"""Verify the running original-data port, including actual emulated audio.
Run with the recovery Python environment and the extracted melonDS AppImage.
Requires Xvfb, xdotool, ImageMagick, ffmpeg, Pillow. The GDB connection only reads.
"""
from pathlib import Path
import argparse,json,os,socket,struct,subprocess,time,hashlib,shutil
from PIL import Image,ImageChops
ROOT=Path(__file__).resolve().parents[1]

class Remote:
    def __init__(self,port):
        self.s=socket.create_connection(('127.0.0.1',port),timeout=3)
        self.s.settimeout(3)
        self.s.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1)
        self.s.sendall(b'+')
        self.s.sendall(b'$c#63')
        if self.s.recv(1)!=b'+':raise RuntimeError('GDB continue was not acknowledged')
    def read(self,address,length):
        if length>512:
            return b''.join(self.read(address+i,min(512,length-i)) for i in range(0,length,512))
        cmd=f'm{address:x},{length:x}'.encode()
        self.s.sendall(b'$'+cmd+b'#'+f'{sum(cmd)&255:02x}'.encode())
        while True:
            first=self.s.recv(1)
            if not first:raise EOFError('GDB stub closed the connection')
            if first==b'$':break
        response=b''
        while True:
            c=self.s.recv(1)
            if not c:raise EOFError('GDB stub ended the packet')
            if c==b'#':break
            response+=c
        self.s.recv(2);self.s.sendall(b'+')
        if response.startswith(b'E'):raise RuntimeError('GDB memory read failed: '+response.decode())
        expanded=bytearray();i=0
        while i<len(response):
            if response[i]==42:
                expanded.extend(bytes([expanded[-1]])*(response[i+1]-29));i+=2
            else:expanded.append(response[i]);i+=1
        return bytes.fromhex(expanded.decode())
    def close(self):self.s.close()

def main():
    args=argparse.ArgumentParser();args.add_argument('emulator',type=Path);args.add_argument('--display',default=':93');opt=args.parse_args()
    out=ROOT/'build/port-checks';out.mkdir(exist_ok=True)
    config=out/'config/melonDS';config.mkdir(parents=True,exist_ok=True)
    src=ROOT/'Downloads/melonDS-1.1-windows-x86_64'
    for f in ['biosnds7.bin','biosnds9.bin','dsfirmware.bin']:shutil.copy2(src/f,config/f)
    (config/'melonDS.toml').write_text(f'''PauseLostFocus = false
TargetFPS = 60.0
LimitFPS = true
AudioSync = false
[Emu]
ConsoleType = 0
DirectBoot = true
ExternalBIOSEnable = true
[DS]
BIOS7Path = "{config/'biosnds7.bin'}"
BIOS9Path = "{config/'biosnds9.bin'}"
FirmwarePath = "{config/'dsfirmware.bin'}"
[3D]
Renderer = 0
[Screen]
UseGL = false
[Gdb]
Enabled = true
[Instance0]
EnableCheats = false
[Instance0.Keyboard]
A = 88
Start = 32
Left = 65
Right = 68
Y = 89
B = 66
X = 86
[Instance0.Gdb]
Enabled = true
[Instance0.Gdb.ARM9]
Port = 3333
BreakOnStartup = false
[Instance0.Gdb.ARM7]
Port = 3334
BreakOnStartup = false
[Instance0.Window0]
Enabled = true
ScreenFilter = false
ScreenLayout = 0
ScreenSizing = 0
ScreenGap = 0
''')
    exe=opt.emulator.resolve();env=os.environ.copy();env.update(DISPLAY=opt.display,QT_QPA_PLATFORM='xcb',
       XDG_CONFIG_HOME=str(config.parent),SDL_AUDIODRIVER='disk',SDL_DISKAUDIOFILE=str(out/'audio.raw'),SDL_DISKAUDIODELAY='10')
    env['LD_LIBRARY_PATH']=str(exe.parent/'usr/lib');env['QT_QPA_PLATFORM_PLUGIN_PATH']=str(exe.parent/'usr/plugins')
    nm=subprocess.check_output(['/opt/devkitpro/devkitARM/bin/arm-none-eabi-nm',str(ROOT/'coolpizza.elf')],text=True)
    address=int(next(l.split()[0] for l in nm.splitlines() if l.endswith(' b game')),16)
    render_address=int(next(l.split()[0] for l in nm.splitlines() if l.endswith(' b rendered_frames')),16)
    refresh_address=int(next(l.split()[0] for l in nm.splitlines() if l.endswith(' b vblank_ticks')),16)
    layout=json.loads((ROOT/'build/game-layout.json').read_text());record=[];remote=None;emu=None
    xvfb=subprocess.Popen(['Xvfb',opt.display,'-screen','0','1024x900x24','-nolisten','tcp'],stdout=subprocess.DEVNULL,stderr=(out/'xvfb.log').open('w'))
    def run(*a):return subprocess.run(a,env=env,check=True,capture_output=True,text=True,timeout=10).stdout
    try:
        time.sleep(.4)
        emu=subprocess.Popen([str(exe),str(ROOT/'coolpizza.nds')],env=env,cwd=out,stdout=(out/'emulator.log').open('w'),stderr=subprocess.STDOUT)
        deadline=time.monotonic()+15;window=None
        while time.monotonic()<deadline:
            if emu.poll() is not None:raise RuntimeError('Emulator exited; inspect emulator.log')
            p=subprocess.run(['xdotool','search','--onlyvisible','--name','melonDS 1.1'],env=env,capture_output=True,text=True)
            if p.returncode==0:window=p.stdout.strip().splitlines()[-1];break
            time.sleep(.2)
        if not window:raise RuntimeError('No emulator window')
        run('xdotool','windowfocus',window);time.sleep(1)
        run('import','-window',window,str(out/'boot-before-debug.png'))
        remote=Remote(3333)
        def state():
            raw=remote.read(address,layout['enemies']+layout['enemy_size']*8);d={}
            for k in ['phase','frame','wave','clock','score','combo','attack','clip','clip_tick','sounds','wave_count','wave_id']:
                d[k]=struct.unpack_from('<I' if k=='sounds' else '<i',raw,layout[k])[0]
            for k in ['x','y','vy']:d[k]=struct.unpack_from('<f',raw,layout[k])[0]
            enemies=[]
            for i in range(8):
                offset=layout['enemies']+i*layout['enemy_size']
                values=struct.unpack_from('<9i10f',raw,offset)
                if values[0]:enemies.append({'config':values[1],'stage':values[2],'hp':values[3],'clip':values[4],
                    'clip_tick':values[5],'x':values[11],'y':values[12]})
            d['enemies']=enemies;return d
        def capture(label):
            path=out/(label+'.png');run('import','-window',window,str(path));d=state();record.append({'capture':str(path.relative_to(ROOT)),'state':d});return d,Image.open(path).convert('RGB')
        def key(k,duration=.16):run('xdotool','keydown',k);time.sleep(duration);run('xdotool','keyup',k)
        splash,_=capture('00-intro')
        deadline=time.monotonic()+12
        while state()['phase']==6 and time.monotonic()<deadline:time.sleep(.15)
        title,_=capture('01-title');assert title['phase']==0
        raw_audio=out/'audio.raw';assert raw_audio.exists()
        assert any(raw_audio.read_bytes()),'Audio output is silent on title'
        key('space');begun,_=capture('02-start');assert begun['phase']==1
        # Real runtime must advance sprite frames and motion coordinates.
        time.sleep(.35);moving,_=capture('03-moving')
        assert moving['frame']>begun['frame']
        assert moving['enemies'] and begun['enemies']
        assert moving['enemies'][0]['clip_tick']>begun['enemies'][0]['clip_tick']
        assert moving['enemies'][0]['x']!=begun['enemies'][0]['x'] or moving['enemies'][0]['y']!=begun['enemies'][0]['y']
        key('space');paused,image=capture('04-pause');assert paused['phase']==2
        key('d',.25);key('x');later,later_image=capture('05-paused-input')
        assert later['frame']==paused['frame'] and later['x']==paused['x']
        assert ImageChops.difference(image.crop((0,19,256,image.height)),later_image.crop((0,19,256,later_image.height))).getbbox() is None
        key('space');resume,_=capture('06-resume');assert resume['phase']==1
        # Follow the central introductory ramp and tap rapidly during flight.
        deadline=time.monotonic()+8;airborne=False;scored=False
        while time.monotonic()<deadline:
            current=state()
            if current['phase']==3:break
            if current['y']>-.5:
                airborne=True;key('x',.05);time.sleep(.05)
            else:time.sleep(.08)
            scored=scored or current['score']>0
        flight,_=capture('07-flight');assert airborne,'No ramp launch was observed'
        key('b');menu,_=capture('08-menu');assert menu['phase']==0
        # Verify actual touch events, rather than only keyboard input.
        run('xdotool','mousemove','--window',window,'128','290');run('xdotool','mousedown','1');time.sleep(.15);run('xdotool','mouseup','1')
        touched,_=capture('09-touch-start');assert touched['phase']==1
        time.sleep(.3);key('d',.2);steering,_=capture('10-steer');assert steering['x']>.2
        # Sample once before/after, without polling, so the debugger does not
        # distort the runtime's steady-state frame rate.
        def counter(address):
            before=time.monotonic()
            value=struct.unpack('<I',remote.read(address,4))[0]
            return value,(before+time.monotonic())/2
        # Timestamp each small read itself. A full state read includes several
        # GDB round trips and biases short timing intervals.
        frame_start,frame_time=counter(address+layout['frame'])
        render_start,render_time=counter(render_address)
        refresh_start,refresh_time=counter(refresh_address)
        time.sleep(3)
        frame_end,frame_end_time=counter(address+layout['frame'])
        render_end,render_end_time=counter(render_address)
        refresh_end,refresh_end_time=counter(refresh_address)
        measured_fps=(frame_end-frame_start)/(frame_end_time-frame_time)
        render_fps=(render_end-render_start)/(render_end_time-render_time)
        refresh_fps=(refresh_end-refresh_start)/(refresh_end_time-refresh_time)
        ticks_per_refresh=measured_fps/refresh_fps
        # Emulator pacing can deviate from wall-clock 60 Hz, particularly with
        # disk audio. The game must track the actual emulated refresh clock.
        assert .93<ticks_per_refresh<1.07,f'Simulation skipped refreshes: {ticks_per_refresh:.3f} ticks/refresh'
        final_audio=raw_audio.read_bytes();assert len(final_audio)>100000 and any(final_audio[-100000:])
        audio_bytes=len(final_audio)
        subprocess.run(['ffmpeg','-v','error','-y','-f','s16le','-ar','48000','-ac','2','-i',str(raw_audio),'-t','12',str(out/'emulated-audio.wav')],check=True)
        report={'result':'passed','rom_sha256':hashlib.sha256((ROOT/'coolpizza.nds').read_bytes()).hexdigest(),
          'emulator':'melonDS 1.1 Linux','audio_bytes':audio_bytes,'airborne':airborne,'score_observed':scored,
          'simulation_ticks_per_second':measured_fps,'rendered_frames_per_second':render_fps,
          'emulated_refreshes_per_second':refresh_fps,'simulation_ticks_per_refresh':ticks_per_refresh,
          'checks':['boot','audible title/level PCM output','enemy animation advances','enemy movement advances','pause freeze',
                    'resume','ramp launch','keyboard attacks','menu','touch start','steering'],'captures':record}
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print('ROM movement, animation, ramp, input, pause and audio-output checks passed.',flush=True)
    finally:
        if remote:remote.close()
        for p in [emu,xvfb]:
            if p and p.poll() is None:
                p.terminate()
                try:p.wait(timeout=3)
                except subprocess.TimeoutExpired:p.kill();p.wait()

if __name__=='__main__':main()
