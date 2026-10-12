"""Compile recovered scene, sprite, animation, path and wave data for the DS.
Requires the three recovery scripts to have run. Assets stay in local generated
files; the original archives remain the source of truth.
"""
import json,math,UnityPy
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'recovered'
def load(p): return json.loads(p.read_text())
decoded={(r['file'],r['path_id']):load(ROOT/r['export']) for r in load(OUT/'decoded/index.json')}
objects={}
for p in (OUT/'objects').glob('*.json'):
    if not p.name.startswith(('mainData_', 'sharedassets0.assets_')): continue
    parts=p.stem.split('_');objects[(parts[0],int(parts[1]))]=load(p)
inventory=load(OUT/'inventory.json')
by_go={};go_trans={}
for key,d in decoded.items():by_go.setdefault((key[0],d['source']['game_object']['path_id']),[]).append((key,d))
for key,d in objects.items():
    if 'm_LocalPosition' in d:go_trans[(key[0],d['m_GameObject']['m_PathID'])]=key
def ref(file,r):
    n=r.get('m_PathID',r.get('path_id',0));f=r.get('m_FileID',r.get('file_id',0))
    return ((file if f==0 else 'sharedassets0.assets'),n)
def fields(key): return decoded[key]['fields']
def world(tkey):
    t=objects[tkey];p=t['m_LocalPosition'];s=t['m_LocalScale'];rot=t['m_LocalRotation']
    # Mirror sign from 180-degree Y rotations is significant for ramps.
    sx=s['x']*(1-2*(rot['y']**2+rot['z']**2));sy=s['y']*(1-2*(rot['x']**2+rot['z']**2))
    parent=ref(tkey[0],t['m_Father'])
    if parent[1]:
        x,y,psx,psy=world(parent);return x+p['x']*psx,y+p['y']*psy,psx*sx,psy*sy
    return p['x'],p['y'],sx,sy
def descendants(tkey):
    yield tkey
    for child in objects[tkey]['m_Children']:yield from descendants(ref(tkey[0],child))
def ff(v):
    if not math.isfinite(v): raise ValueError('Nonfinite data')
    s=f'{v:.9g}';return s+('f' if '.' in s or 'e' in s else '.0f')
def init(values):
    return '{'+','.join(ff(v) if isinstance(v,float) else str(v) for v in values)+'}'
code=['/* Generated from Cool Pizza 1.4. Do not edit. */','#include "original.h"']
def array(typ,name,rows):
    code.append(f'static const {typ} {name}[] = {{\n'+',\n'.join(rows or ['{0}'])+'\n};')

# Sprite crops preserve trimmed quad coordinates so animation pivots do not jump.
sprites=[];sprite_map={}
collections={k:fields(k) for k,d in decoded.items() if d['source']['script']=='tk2dSpriteCollectionData'}
for r in load(OUT/'sprites/index.json'):
    collection=next(k for k,d in collections.items() if d['spriteCollectionName']==r['collection'])
    definition=collections[collection]['spriteDefinitions'][r['sprite_id']]
    im=Image.open(ROOT/r['export']).convert('RGBA');pos=definition['positions']
    x0=min(p[0] for p in pos);y0=min(p[1] for p in pos)
    w=max(p[0] for p in pos)-x0;h=max(p[1] for p in pos)-y0
    target=(max(1,round(w*85.333333)),max(1,round(h*85.333333)))
    im=im.resize(target,Image.Resampling.LANCZOS)
    pixels=[0 if a<90 else 0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10) for r,g,b,a in im.get_flattened_data()]
    name=f'sprite_pixels_{len(sprites)}';array('uint16_t',name,[','.join(hex(x) for x in pixels[i:i+32]) for i in range(0,len(pixels),32)])
    sprite_map[(collection,r['sprite_id'])]=len(sprites)
    sprites.append(init([im.width,im.height,x0,y0,w,h,name]))
def texture_sprite(pid,target):
    rec=next(r for r in inventory if r['file']=='sharedassets0.assets' and r['path_id']==pid)
    im=Image.open(ROOT/rec['export']).convert('RGBA');im.thumbnail(target,Image.Resampling.LANCZOS)
    pixels=[0 if a<90 else 0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10) for r,g,b,a in im.get_flattened_data()]
    name=f'sprite_pixels_{len(sprites)}';array('uint16_t',name,[','.join(hex(x) for x in pixels[i:i+32]) for i in range(0,len(pixels),32)])
    idx=len(sprites);sprites.append(init([im.width,im.height,0.,0.,im.width/96.,im.height/96.,name]));return idx
logo=texture_sprite(64,(174,118));start=texture_sprite(40,(94,24));scores=texture_sprite(54,(70,24));dead=texture_sprite(66,(210,24));win=texture_sprite(42,(210,65))
company=texture_sprite(30,(148,76));tettix=texture_sprite(49,(148,44))
code.append(f'const int art_company={company},art_tettix={tettix};')
# Recover the game's BLACKOUT MIDNIGHT font instead of substituting a system font.
env=UnityPy.load(str(OUT/'ipa/Payload/coolpizza.app/Data/sharedassets0.assets'))
font=next(o.read_typetree() for o in env.objects if o.type.name=='Font' and o.path_id==225)
atlas_path=next(ROOT/r['export'] for r in inventory if r['file']=='sharedassets0.assets' and r['path_id']==font['m_Texture']['m_PathID'])
atlas=Image.open(atlas_path).convert('RGBA');glyphs=['{-1,4,0}']*128
for character in font['m_CharacterRects']:
    ascii=character['index']
    if not 0<=ascii<128:continue
    uv=character['uv'];vert=character['vert'];advance=max(1,round(character['width']/3))
    if not uv['width'] or not uv['height']:glyphs[ascii]=init([-1,advance,0]);continue
    x=uv['x']*atlas.width;y=(1-uv['y']-uv['height'])*atlas.height
    im=atlas.crop((round(x),round(y),round(x+uv['width']*atlas.width),round(y+uv['height']*atlas.height)))
    w=max(1,round(abs(vert['width'])/3));h=max(1,round(abs(vert['height'])/3))
    im=im.resize((w,h),Image.Resampling.LANCZOS)
    pixels=[0xffff if a>90 else 0 for r,g,b,a in im.get_flattened_data()]
    name=f'sprite_pixels_{len(sprites)}';array('uint16_t',name,[','.join(hex(x) for x in pixels)])
    glyphs[ascii]=init([len(sprites),advance,round(-vert['y']/3)])
    sprites.append(init([w,h,0.,0.,w/96.,h/96.,name]))
code.append('const Glyph original_font[128] = {\n'+',\n'.join(glyphs)+'\n};')
code.append('const Sprite original_sprites[] = {\n'+',\n'.join(sprites)+'\n};')
code.append(f'const int original_sprite_count = {len(sprites)};')
code.append(f'const int art_logo={logo},art_start={start},art_scores={scores},art_dead={dead},art_win={win};')
def named_sprite(name):
    for k,d in collections.items():
        for i,s in enumerate(d['spriteDefinitions']):
            if s['name']==name:return sprite_map[k,i]
    raise KeyError(name)
code.append('const int art_power[4] = '+init([named_sprite(n) for n in ['jump-text','shot-text','nuke-text','shield-text']])+';')
code.append(f'const int art_meter={named_sprite("meter-back")};')
clips=[];clip_map={}
for key,d in decoded.items():
    if d['source']['script']!='tk2dSpriteAnimation':continue
    for i,clip in enumerate(d['fields']['clips']):
        frames=[sprite_map[(ref(key[0],f['spriteCollection']),f['spriteId'])] for f in clip['frames']]
        name=f'clip_frames_{len(clips)}';array('short',name,[','.join(map(str,frames))])
        clip_map[key,i]=len(clips)
        clips.append(init([json.dumps(clip['name']),name,len(frames),clip['loopStart'],clip['wrapMode'],clip['fps']]))
code.append('const SpriteClip original_clips[] = {\n'+',\n'.join(clips)+'\n};')
code.append(f'const int original_clip_count={len(clips)};')
def animation_named(lib,name):
    key=('sharedassets0.assets',lib)
    i=next(i for i,c in enumerate(fields(key)['clips']) if c['name']==name)
    return clip_map[key,i]
code.append('const int player_clips[6] = '+init([animation_named(963,n) for n in ['idle','left','right','attack_left','attack_right','jump']])+';')
code.append('const int bomb_clip='+str(animation_named(954,'bomb'))+',reticle_clip='+str(animation_named(967,'reticle'))+';')
ghost_library=next(k for k,d in decoded.items() if d['source']['script']=='tk2dSpriteAnimation' and any(c['name']=='shield' for c in d['fields']['clips']))
code.append(f'const int ghost_clip={clip_map[ghost_library,0]},shield_clip='+str(next(clip_map[ghost_library,i] for i,c in enumerate(fields(ghost_library)['clips']) if c['name']=='shield'))+';')

def hermite(keys,t,axis=None):
    def val(k,n):return k[n][axis] if axis else k[n]
    if t<=keys[0]['time']:return val(keys[0],'value')
    if t>=keys[-1]['time']:return val(keys[-1],'value')
    i=next(i for i in range(len(keys)-1) if keys[i+1]['time']>=t);a,b=keys[i:i+2]
    dt=b['time']-a['time'];u=(t-a['time'])/dt
    av,bv=val(a,'value'),val(b,'value');out,ins=val(a,'outSlope'),val(b,'inSlope')
    if not math.isfinite(out) or not math.isfinite(ins):return av
    return (2*u**3-3*u*u+1)*av+(u**3-2*u*u+u)*dt*out+(-2*u**3+3*u*u)*bv+(u**3-u*u)*dt*ins
motions=[];motion_map={}
for key,d in objects.items():
    if 'm_PositionCurves' not in d:continue
    all_curves=d['m_PositionCurves']+d['m_ScaleCurves']+d['m_FloatCurves']
    duration=max([k['time'] for c in all_curves for k in c['curve']['m_Curve']]+[e['time'] for e in d['m_Events']]+[0.])
    positions=next((c['curve']['m_Curve'] for c in d['m_PositionCurves'] if c['path']==''),None)
    scales=next((c['curve']['m_Curve'] for c in d['m_ScaleCurves'] if c['path']==''),None)
    angle=next((c['curve']['m_Curve'] for c in d['m_FloatCurves'] if c['path']=='' and c['attribute']=='localEulerAngles.z'),None)
    flags=int(bool(positions))+2*bool(scales)+4*bool(angle)
    count=max(1,math.ceil(duration*60)+1);samples=[]
    for i in range(count):
        t=min(duration,i/60)
        samples.append(init([hermite(positions,t,'x') if positions else 0.,hermite(positions,t,'y') if positions else 0.,hermite(scales,t,'x') if scales else 1.,hermite(scales,t,'y') if scales else 1.,hermite(angle,t) if angle else 0.]))
    m=len(motions);array('MotionSample',f'motion_samples_{m}',samples)
    events=[init([e['time']]) for e in d['m_Events'] if e['functionName'] in ('Attack','DoAttack')]
    array('MotionEvent',f'motion_events_{m}',events)
    motion_map[key]=m;motions.append(init([f'motion_samples_{m}',f'motion_events_{m}',count,len(events),d['m_WrapMode'],flags,float(duration)]))
code.append('const Motion original_motions[] = {\n'+',\n'.join(motions)+'\n};')
code.append(f'const int original_motion_count={len(motions)};')

paths=[];path_map={}
for key,d in decoded.items():
    if d['source']['script']!='iTweenPath':continue
    f=d['fields'];nodes=f['nodes'];pts=[nodes[0]]+nodes+[nodes[-1]]
    pts[0]=[2*nodes[0][a]-nodes[1][a] for a in range(3)];pts[-1]=[2*nodes[-1][a]-nodes[-2][a] for a in range(3)]
    if nodes[0]==nodes[-1]:pts[0]=nodes[-2];pts[-1]=nodes[1]
    xy=[]
    for i in range(241):
        t=i/240*(len(nodes)-1);seg=min(len(nodes)-2,int(t));u=t-seg
        for axis in range(2):
            a,b,c,dv=[p[axis] for p in pts[seg:seg+4]]
            xy.append(.5*((2*b)+(-a+c)*u+(2*a-5*b+4*c-dv)*u*u+(-a+3*b-3*c+dv)*u*u*u))
    name=f'path_xy_{len(paths)}';array('float',name,[','.join(ff(v) for v in xy)])
    path_map[f['pathName']]=len(paths);paths.append(init([name,241,1,0]))
code.append('const PathData original_paths[] = {\n'+',\n'.join(paths)+'\n};')

def collision(file,gid):
    go=objects[file,gid]
    for kind,r in go['m_Component']:
        if kind==65:
            c=objects[ref(file,r)];return [c['m_Center']['x'],c['m_Center']['y'],c['m_Size']['x']/2,c['m_Size']['y']/2]
    return [0.,0.,.15,.15]
def script_on(file,gid,script):
    return next((x for x in by_go.get((file,gid),[]) if x[1]['source']['script']==script),None)
def root_box(gid):
    tr=objects[go_trans['sharedassets0.assets',gid]];s=tr['m_LocalScale'];p=tr['m_LocalPosition'];c=collision('sharedassets0.assets',gid)
    return [p['x']+c[0]*s['x'],p['y']+c[1]*s['y'],c[2]*abs(s['x']),c[3]*abs(s['y'])]
code.append('const float player_attack_box[4] = '+init(root_box(318))+';')
code.append('const float player_board_box[4] = '+init(root_box(259))+';')
wave_keys=[k for k,d in decoded.items() if d['source']['script']=='Wave'];wave_map={k:i for i,k in enumerate(wave_keys)}
enemies=[];waves=[];audit=[]
for wk in wave_keys:
    wt=go_trans[wk[0],decoded[wk]['source']['game_object']['path_id']];w=fields(wk);first=len(enemies)
    for tk in descendants(wt):
        tr=objects[tk];gid=tr['m_GameObject']['m_PathID']
        for ek,ed in by_go.get((tk[0],gid),[]):
            cls=ed['source']['script']
            if cls not in ['CoinMonster','BombingEyeball','HeatseekingSweeperEyeball','HandMonster','PyramidMonster']:continue
            f=ed['fields'];sprite=script_on(tk[0],gid,'tk2dAnimatedSprite')
            if not sprite:raise ValueError('Enemy lacks sprite')
            sf=sprite[1]['fields'];lib=ref(tk[0],sf['anim']);library=fields(lib)['clips']
            idle=next((i for i,c in enumerate(library) if c['name']=='idle'),sf['clipId']);strike=next((i for i,c in enumerate(library) if c['name']=='attack'),idle)
            parent=ref(tk[0],tr['m_Father']);px,py,psx,psy=world(parent);p=tr['m_LocalPosition'];s=tr['m_LocalScale'];box=collision(tk[0],gid)
            pt=objects[parent];gp=ref(parent[0],pt['m_Father']);gx,gy,gsx,gsy=world(gp) if gp[1] else (0.,0.,1.,1.)
            ox=pt['m_LocalPosition']['x']*gsx;oy=pt['m_LocalPosition']['y']*gsy
            boom=[0.,0.];laser_start=-.5283308
            for child in tr['m_Children']:
                ct=objects[ref(tk[0],child)];cg=objects[tk[0],ct['m_GameObject']['m_PathID']]
                if cg.get('m_Tag')==20020:boom=[ct['m_LocalPosition']['x'],ct['m_LocalPosition']['y']]
                if cg.get('m_Name')=='AttackStart':laser_start=ct['m_LocalPosition']['y']
            easing={'linear':0,'easeinoutback':1,'easeinoutcubic':2,'easeoutquad':3,'easeinquad':4}.get(f['iTweenEaseType'].lower(),0)
            vals=[px,py,psx,psy,p['x'],p['y'],s['x'],s['y'],*box,f['attackAnimationSpeed'],f['lifetimeInSeconds'],f.get('timeSpentTracking',0.),f.get('timeBeforeLaser',0.),f.get('timeLaserIsActive',0.),f.get('trackingSpeed',0.),f.get('tellLengthInSeconds',0.),f.get('secondsToImpact',0.),f.get('secondsToReturnToOrigin',0.),f.get('airPullStrength',0.),f.get('groundPullStrength',0.),ox,oy,gsx,gsy,*boom,laser_start,2 if f['iTweenLoopType'].lower()=='pingpong' else 1,easing,['CoinMonster','BombingEyeball','HeatseekingSweeperEyeball','HandMonster','PyramidMonster'].index(cls),f['hitPoints'],f['scoreValue'],clip_map[lib,idle],clip_map[lib,strike],motion_map.get(ref(tk[0],f['entranceAnimation']),-1),motion_map.get(ref(tk[0],f['attackPatternAnimation']),-1),motion_map.get(ref(tk[0],f['exitAnimation']),-1),path_map.get(f['iTweenPathName'],-1),f['attackSoundEffect']['path_id'],f['enemyDestroyedSound']['path_id']]
            enemies.append(init(vals));audit.append({'wave':objects[wk[0],decoded[wk]['source']['game_object']['path_id']]['m_Name'],'object':gid,'class':cls,'hp':f['hitPoints'],'score':f['scoreValue']})
    rw=script_on(wk[0],decoded[wk]['source']['game_object']['path_id'],'RampWave');rf=rw[1]['fields'] if rw else {}
    mask=sum(1<<int(k.split('_')[1],2) for k,v in rf.items() if k.startswith('formation_') and v)
    name=objects[wk[0],decoded[wk]['source']['game_object']['path_id']]['m_Name']
    waves.append(init([json.dumps(name),first,len(enemies)-first,w['timeBetweenRamps'],w['rampOnlyWaveLifeTime'],w['secondsBetweenWaveCompletionAndDestruction'],rf.get('rampAlertTimeInSeconds',0.),str(mask)+'ULL',int(w['isRampOnlyWave'])]))
code.append('const EnemyConfig original_enemies[] = {\n'+',\n'.join(enemies)+'\n};')
code.append('const WaveConfig original_waves[] = {\n'+',\n'.join(waves)+'\n};')
code.append(f'const int original_wave_count={len(waves)},original_enemy_count={len(enemies)};')
dm=next(d['fields'] for d in decoded.values() if d['source']['script']=='DungeonMaster')
def wave_id(n):return wave_map[ref('mainData',dm[n])]
code.append(f'const int tutorial_wave={wave_id("justAttack")};')
groups=[['tutorialOneMonster','tutorialLotsaFlies','pacifistDrivebyCutscene'],['tutorialTwoMonsters','deathFromAboveBombersOnly','pingPongDriveby','bomberLine','carpetBombers','sweeperTriad','sweeperSwipers','sweeperSwipersWithMidBomber','middleSweepers','circleOfEyeballs','bigCircleOfEyeballs'],['handjob1','handjob3','handjob2','drivebyHandjob','crazyW','squareWave','circleOfDeath','assholeEyeballs','bomberSidemovers','doubleCircleOfEyeballs','interleavedBombers','bomberTriad','tutorialThreeMonstersAvoidBombs','elevators','invertedDoubleCircleEyeballs','groupsSideBombers'],['deathFromAboveMixed','fourCoffinsAndASweeper','withinReach','outOfReach','outerOfReach','theWholeGang','bouncingEyeballs','bouncingEyeballsInterleaved','tallToLongEyeballs']]
for label,group in zip(['original_opening','original_pool1','original_pool2','original_pool3'],groups):code.append(f'const int {label}[] = '+init([wave_id(n) for n in group])+';')
code.append('const int original_pool_sizes[3] = '+init([len(g) for g in groups[1:]])+';')
code.append('const int original_fixed[5] = '+init([wave_id(n) for n in ['intermissionLotsaFlies','pyramidWave','expandContractEyeballs','bouncingEyeballsPyramidPal','justSkateDemoOver']])+';')

ramps=[];scenery=[]
ramp_tags=['RightFarRamp','RightMiddleRamp','RightNearRamp','LeftNearRamp','LeftMiddleRamp','LeftFarRamp']
# TagManager stores integer tags; GameObject names also identify these prefabs.
for tag in ramp_tags:
    found=None
    for key,d in decoded.items():
        if d['source']['script']!='Ramp':continue
        gid=d['source']['game_object']['path_id'];go=objects[key[0],gid];tk=go_trans[key[0],gid]
        # Parent containers carry the lane names.
        parent=ref(key[0],objects[tk]['m_Father']);pname=objects[parent[0],objects[parent]['m_GameObject']['m_PathID']]['m_Name']
        if pname.replace(' ','').replace('_','').lower().replace('rampcontainer','')==tag.lower().replace('ramp',''):found=(key,tk,parent);break
    if not found:raise ValueError('Missing ramp '+tag)
    key,tk,parent=found;gid=decoded[key]['source']['game_object']['path_id'];px,py,sx,sy=world(parent)
    animation=next(objects[ref(key[0],r)] for typ,r in objects[key[0],gid]['m_Component'] if typ==111)
    lane=tag.replace('Ramp','')
    child=next(k for k in descendants(tk) if objects[k[0],objects[k]['m_GameObject']['m_PathID']]['m_Name'].replace('_','').lower()==('RampSprite'+lane).lower())
    sp=script_on(child[0],objects[child]['m_GameObject']['m_PathID'],'tk2dAnimatedSprite')[1]['fields'];sid=sprite_map[ref(child[0],sp['collection']),sp['_spriteId']]
    child_go=objects[child[0],objects[child]['m_GameObject']['m_PathID']]
    child_anim=next(objects[ref(child[0],r)] for typ,r in child_go['m_Component'] if typ==111)
    visual=motion_map[ref(child[0],child_anim['m_Animation'])]
    library=ref(child[0],sp['anim']);clip=clip_map[library,sp['clipId']]
    b=collision(key[0],gid);ramps.append(init([px,py,sx,sy,b[2],b[3],motion_map[ref(key[0],animation['m_Animation'])],sid,visual,clip]))
code.append(f'const int ramp_alert_clip={animation_named(962,"ramp_alert")};')
code.append('const RampConfig original_ramps[6] = {\n'+',\n'.join(ramps)+'\n};')
for key,d in objects.items():
    if 'm_Animation' not in d or key[0]!='mainData':continue
    gid=d['m_GameObject']['m_PathID'];go=objects[key[0],gid];name=go['m_Name'].lower()
    if 'cloud' not in name and 'horizonline' not in name:continue
    sp=script_on(key[0],gid,'tk2dSprite') or script_on(key[0],gid,'tk2dAnimatedSprite')
    if not sp:continue
    f=sp[1]['fields'];sid=sprite_map[ref(key[0],f['collection']),f['_spriteId']];x,y,sx,sy=world(go_trans[key[0],gid]);scenery.append(init([x,y,sx,sy,motion_map[ref(key[0],d['m_Animation'])],sid]))
code.append('const Scenery original_scenery[] = {\n'+',\n'.join(scenery or ['{0}'])+'\n};')
code.append(f'const int original_scenery_count={len(scenery)};')
(ROOT/'source/original_data.c').write_text('\n'.join(code)+'\n')
(OUT/'port-audit.json').write_text(json.dumps({'waves':len(waves),'enemies':audit,'sprites':len(sprites),'sprite_clips':len(clips),'motions':len(motions),'scenery':len(scenery)},indent=2)+'\n')
print(f'Compiled {len(waves)} wave definitions, {len(enemies)} enemies, {len(clips)} sprite clips, {len(motions)} motion clips.')
