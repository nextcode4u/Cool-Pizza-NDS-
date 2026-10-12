"""Convert original embedded tracks/effects into streamable DS PCM assets."""
import json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
inventory=json.loads((ROOT/'recovered/inventory.json').read_text())
dest=ROOT/'nitro/audio';dest.mkdir(parents=True,exist_ok=True)
tracks={151:'title',138:'level',148:'death',152:'scores'}
manifest=[]
for r in inventory:
    if r['type']!='AudioClip':continue
    pid=r['path_id'];src=ROOT/r['exports'][0]
    name=tracks.get(pid,f'sfx{pid}')
    channels=2 if pid in tracks else 1
    output=dest/(name+'.pcm')
    subprocess.run(['ffmpeg','-v','error','-y','-i',str(src),'-ac',str(channels),'-ar','22050',
                    '-f','s16le',str(output)],check=True)
    manifest.append({'id':pid,'name':r['name'],'output':str(output.relative_to(ROOT)),
                     'channels':channels,'rate':22050,'frames':output.stat().st_size//(2*channels)})
(ROOT/'recovered/ds-audio.json').write_text(json.dumps(manifest,indent=2)+'\n')
effects=sum((ROOT/r['output']).stat().st_size for r in manifest if r['channels']==1)
print(f'Prepared {len(manifest)} audio files. In-memory effects: {effects} bytes; music streams from NitroFS.')
