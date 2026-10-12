"""Recover all named tk2d sprite quads from their original atlases.

Run after recover_assets.py and decode_behaviours.py, with Pillow installed.
UVs and sprite dimensions come from the game's serialized collection data.
"""
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'recovered'


def recover_sprites():
    index=json.loads((OUT/'decoded/index.json').read_text())
    inventory=json.loads((OUT/'inventory.json').read_text())
    textures={(r['file'],r['path_id']):r for r in inventory if r['type']=='Texture2D'}
    sprites=[]
    for item in index:
        if item['class']!='tk2dSpriteCollectionData': continue
        data=json.loads((ROOT/item['export']).read_text())['fields']
        dest=OUT/'sprites'/data['spriteCollectionName'];dest.mkdir(parents=True,exist_ok=True)
        for sprite_id,sprite in enumerate(data['spriteDefinitions']):
            if not sprite['name']: continue
            if len(sprite['positions'])!=4 or len(sprite['uvs'])!=4:
                raise ValueError('Complex sprite requires separate mesh reconstruction')
            ref=data['textures'][sprite['materialId']]
            if ref['file_id']!=0: raise ValueError('External atlas reference')
            atlas=Image.open(ROOT/textures[(item['file'],ref['path_id'])]['export']).convert('RGBA')
            positions=sprite['positions'];uv=sprite['uvs']
            xmin=min(v[0] for v in positions);xmax=max(v[0] for v in positions)
            ymin=min(v[1] for v in positions);ymax=max(v[1] for v in positions)
            w=round((xmax-xmin)/sprite['texelSize'][0]);h=round((ymax-ymin)/sprite['texelSize'][1])
            quad=[]
            for x,y in [(xmin,ymax),(xmin,ymin),(xmax,ymin),(xmax,ymax)]:
                vertex=min(range(4),key=lambda i:abs(positions[i][0]-x)+abs(positions[i][1]-y))
                quad.extend((uv[vertex][0]*atlas.width,(1-uv[vertex][1])*atlas.height))
            im=atlas.transform((w,h),Image.Transform.QUAD,quad,Image.Resampling.NEAREST)
            path=dest/(sprite['name']+'.png');im.save(path)
            sprites.append({'collection':data['spriteCollectionName'],'sprite_id':sprite_id,
                            'name':sprite['name'],'width':w,'height':h,
                            'export':str(path.relative_to(ROOT)),
                            'source':item['export'],'positions':positions})
    (OUT/'sprites/index.json').write_text(json.dumps(sprites,indent=2)+'\n')
    return sprites


if __name__=='__main__':
    sprites=recover_sprites()
    print(f'Recovered {len(sprites)} named sprites from original atlas UVs.')
