"""Decode public serialized fields in the recovered Unity 3.5 player build.

Only publish objects whose field schema consumes the complete byte record.
Unsupported/private serialization is reported, never guessed as a valid decode.
"""
import json
import struct
from pathlib import Path
import dnfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'recovered'
pe = dnfile.dnPE(str(OUT / 'ipa/Payload/coolpizza.app/Data/Managed/Assembly-CSharp.dll'))
types = {str(t.TypeName): t for t in pe.net.mdtables.TypeDef}
serialized_private = set()
for attribute in pe.net.mdtables.CustomAttribute:
    constructor = attribute.Type.row
    if hasattr(constructor, 'Class') and str(getattr(constructor.Class.row, 'TypeName', '')) == 'SerializeField':
        serialized_private.add(id(attribute.Parent.row))


class Signature:
    def __init__(self, data): self.data, self.pos = data, 0
    def integer(self):
        a = self.data[self.pos]; self.pos += 1
        if a < 128: return a
        if a < 192:
            b = self.data[self.pos]; self.pos += 1
            return ((a & 63) << 8) | b
        b = self.data[self.pos:self.pos+3]; self.pos += 3
        return ((a & 31) << 24) | int.from_bytes(b, 'big')
    def type(self):
        kind = self.integer()
        if kind == 0x1d: return ('array', self.type())
        if kind in (0x11, 0x12):
            code = self.integer()
            table = (pe.net.mdtables.TypeDef, pe.net.mdtables.TypeRef, pe.net.mdtables.TypeSpec)[code & 3]
            row = table.rows[(code >> 2)-1]
            return ('named', str(row.TypeName))
        if kind == 0x15:
            parent = self.type(); count = self.integer()
            args = [self.type() for _ in range(count)]
            if parent[1] == 'List`1' and count == 1: return ('array', args[0])
            raise ValueError('Unsupported generic '+str(parent))
        return ('primitive', kind)


class Reader:
    def __init__(self, data): self.data, self.pos = data, 24
    def read(self, fmt):
        size = struct.calcsize('<'+fmt)
        if self.pos+size > len(self.data): raise ValueError('Record ended')
        value = struct.unpack_from('<'+fmt, self.data, self.pos)
        self.pos += size
        return value[0] if len(value)==1 else list(value)
    def align(self): self.pos = (self.pos+3) & ~3
    def string(self):
        n = self.read('i')
        if n < 0 or self.pos+n > len(self.data): raise ValueError('Invalid string length')
        v = self.data[self.pos:self.pos+n].decode('utf-8');self.pos += n;self.align()
        return v
    def value(self, schema):
        kind, arg = schema
        if kind == 'array':
            count = self.read('i')
            if count < 0 or count > 10000: raise ValueError('Invalid array length')
            result = [self.value(arg) for _ in range(count)]
            self.align();return result
        if kind == 'primitive':
            if arg == 2:
                v = bool(self.read('B'));self.align();return v
            if arg == 14: return self.string()
            fmt = {4:'b',5:'B',6:'h',7:'H',8:'i',9:'I',10:'q',11:'Q',12:'f',13:'d'}.get(arg)
            if not fmt: raise ValueError('Unsupported primitive '+str(arg))
            return self.read(fmt)
        vectors = {'Vector2':2,'Vector3':3,'Vector4':4,'Quaternion':4,'Color':4,'Rect':4,'Bounds':6}
        if arg in vectors: return self.read('f'*vectors[arg])
        if arg not in types:
            # Unity object references are PPtr(fileID, pathID), 32 bits each.
            known = {'GameObject','Transform','AudioClip','AnimationClip','Material','Texture',
                     'Texture2D','Camera','Font','Animation','ParticleSystem','Mesh',
                     'BoxCollider','MeshCollider','GUISkin'}
            if arg not in known: raise ValueError('Unknown engine type '+arg)
            return {'file_id':self.read('i'),'path_id':self.read('i')}
        cls = types[arg]
        base = str(getattr(cls.Extends.row,'TypeName','')) if cls.Extends else ''
        if base == 'Enum': return self.read('i')
        if is_component(cls): return {'file_id':self.read('i'),'path_id':self.read('i')}
        return self.fields(cls)
    def fields(self, cls):
        values = {}
        base = str(getattr(cls.Extends.row,'TypeName','')) if cls.Extends else ''
        if base in types and base != 'Enum': values.update(self.fields(types[base]))
        for item in cls.FieldList:
            f = item.row
            if (not f.Flags.fdPublic and id(f) not in serialized_private) or f.Flags.fdStatic or f.Flags.fdNotSerialized or f.Flags.fdInitOnly:
                continue
            sig = Signature(f.Signature.value);sig.integer() # FIELD prefix
            try: schema = sig.type()
            except ValueError as exc:
                if 'Dictionary' in str(exc): continue # Unity does not serialize dictionaries
                raise
            if schema[0] == 'named' and schema[1] in types:
                target = types[schema[1]]
                if str(getattr(target.Extends.row, 'TypeName', '')) in ('Delegate', 'MulticastDelegate'):
                    continue
            values[str(f.Name)] = self.value(schema)
        return values


def is_component(cls):
    for _ in range(20):
        base = str(getattr(cls.Extends.row,'TypeName','')) if cls.Extends else ''
        if base in ('MonoBehaviour','Component'): return True
        if base == 'Object' and str(getattr(cls.Extends.row,'TypeNamespace','')) == 'UnityEngine':
            return True
        if base not in types: return False
        cls = types[base]
    raise ValueError('Inheritance cycle')


if __name__ == '__main__':
    inventory = json.loads((OUT/'inventory.json').read_text())
    dest=OUT/'decoded';dest.mkdir(exist_ok=True)
    successes, errors = [], []
    for record in inventory:
        name=record.get('script')
        if name not in types: continue
        try:
            raw=(ROOT/record['export']).read_bytes();r=Reader(raw)
            # Name string follows the fixed header; empty in most player objects.
            r.pos=20;r.string()
            values=r.fields(types[name]);r.align()
            if r.pos != len(raw): raise ValueError(f'Consumed {r.pos} of {len(raw)} bytes')
            path=dest/(Path(record['export']).stem+'.json')
            path.write_text(json.dumps({'source':record,'fields':values},indent=2)+'\n')
            successes.append({'class':name,'path_id':record['path_id'],'file':record['file'],
                              'export':str(path.relative_to(ROOT))})
        except Exception as exc:
            errors.append({'class':name,'path_id':record['path_id'],'error':str(exc)})
    (dest/'index.json').write_text(json.dumps(successes,indent=2)+'\n')
    (dest/'errors.json').write_text(json.dumps(errors,indent=2)+'\n')
    print(f'Fully decoded {len(successes)} behaviours; {len(errors)} require further schema work.')
