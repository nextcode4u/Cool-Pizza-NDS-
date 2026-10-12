"""Catalog user-provided archives and export Unity 3.5 textures and IL.

Usage: python tools/recover_assets.py
Requires UnityPy, Pillow, dnfile, dncil (see tools/recovery-requirements.txt).
Never executes the app or any recovered managed code.
"""
from pathlib import Path
import collections
import hashlib
import json
import plistlib
import re
import struct
import zipfile

import UnityPy
import dnfile
from PIL import Image
from dncil.cil.body import CilMethodBody
from dncil.cil.body.reader import CilMethodBodyReaderBytes

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "recovered"


def safe_name(value):
    return re.sub(r"[^a-zA-Z0-9_.-]+", "_", str(value))


def save_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, default=str) + "\n")


def extract():
    archives = []
    for path in sorted((ROOT / "Downloads").iterdir()):
        if path.suffix.lower() not in (".ipa", ".zip"):
            continue
        dest = OUT / ("ipa" if path.suffix.lower() == ".ipa" else "soundtrack")
        with zipfile.ZipFile(path) as archive:
            for item in archive.infolist():
                if item.is_dir() or "__MACOSX" in item.filename or ".DS_Store" in item.filename:
                    continue
                target = (dest / item.filename).resolve()
                if not target.is_relative_to(dest.resolve()):
                    raise ValueError(f"Unsafe path: {item.filename}")
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(item))
        archives.append({"file": str(path.relative_to(ROOT)),
                         "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    save_json(OUT / "archives.json", archives)


def export_il(dll):
    pe = dnfile.dnPE(str(dll))
    owner = {}
    for cls in pe.net.mdtables.TypeDef:
        for field in cls.FieldList:
            owner[id(field.row)] = str(cls.TypeName)
        for method in cls.MethodList:
            owner[id(method.row)] = str(cls.TypeName)

    def resolve(operand):
        value = getattr(operand, "value", None)
        if value is None:
            return str(operand) if operand is not None else ""
        table_id, index = value >> 24, value & 0xffffff
        if table_id == 0x70:
            entry = pe.net.user_strings.get(index)
            return repr(entry.value) if entry else str(operand)
        table = pe.net.mdtables.tables.get(table_id)
        if table and 0 < index <= len(table.rows):
            row = table.rows[index - 1]
            if table_id == 0x2b:
                row = row.Method.row
            name = str(getattr(row, "Name", getattr(row, "TypeName", "")))
            parent = owner.get(id(row))
            if not parent and hasattr(row, "Class"):
                ref = row.Class.row
                parent = str(getattr(ref, "TypeName", getattr(ref, "Name", "")))
            return f"{parent + '::' if parent else ''}{name} [0x{value:08x}]"
        return str(operand)

    dest = OUT / "code"
    dest.mkdir(exist_ok=True)
    metadata = []
    for cls in pe.net.mdtables.TypeDef:
        name = str(cls.TypeName)
        fields = [{"name": str(f.row.Name), "signature": f.row.Signature.value.hex(),
                   "public": f.row.Flags.fdPublic, "static": f.row.Flags.fdStatic}
                  for f in cls.FieldList]
        methods = []
        text = [f"// Disassembled IL: {cls.TypeNamespace}.{name}", "// Original recovered binary; not C# source."]
        for entry in cls.MethodList:
            m = entry.row
            methods.append({"name": str(m.Name), "rva": m.Rva})
            if not m.Rva:
                continue
            text.append(f"\n.method {m.Name} (RVA 0x{m.Rva:x})")
            try:
                body = CilMethodBody(CilMethodBodyReaderBytes(pe.get_data(m.Rva)))
                for instruction in body.instructions:
                    text.append(f"  {instruction.offset:04x}: {instruction.opcode.name:16} {resolve(instruction.operand)}")
            except Exception as exc:
                text.append(f"// FAILED: {exc}")
        (dest / (safe_name(name) + ".il")).write_text("\n".join(text) + "\n")
        metadata.append({"class": name, "namespace": str(cls.TypeNamespace),
                         "fields": fields, "methods": methods})
    save_json(dest / "classes.json", metadata)


def export_unity(data):
    env = UnityPy.load(str(data / "mainData"), str(data / "sharedassets0.assets"),
                       str(data / "unity default resources"))
    inventory, errors, scenes, scripts = [], [], [], {}
    counts = collections.Counter()
    for folder in ("textures", "audio", "objects", "behaviours"):
        (OUT / folder).mkdir(exist_ok=True)
    for obj in env.objects:
        if obj.type.name == "MonoScript":
            scripts[(Path(obj.assets_file.name).name, obj.path_id)] = obj.read().m_ClassName
    for obj in env.objects:
        kind = obj.type.name
        counts[kind] += 1
        file = Path(obj.assets_file.name).name
        record = {"file": file, "path_id": obj.path_id, "type": kind, "bytes": obj.byte_size}
        key = f"{safe_name(file)}_{obj.path_id}"
        try:
            if kind == "Texture2D":
                tex = obj.read()
                name = safe_name(tex.m_Name)
                dest = OUT / "textures" / f"{key}_{name}.png"
                image = tex.image
                # This iOS Unity 3.5 build's 16-bit pixels are RGBA4444.
                # UnityPy treats format 2 as ARGB4444, rotating every channel
                # and turning transparent white into opaque yellow.
                if tex.m_TextureFormat == 2 and obj.assets_file.unity_version.startswith('3.5.'):
                    r, g, b, a = image.split()
                    image = Image.merge('RGBA', (a, r, g, b))
                image.save(dest)
                record.update(name=tex.m_Name, width=tex.m_Width, height=tex.m_Height,
                              export=str(dest.relative_to(ROOT)))
            elif kind == "AudioClip":
                # Unity 3.5 stores AIFF/WAV inline (stream=1) or MP3 in resS
                # (stream=2). Modern UnityPy sample conversion misses these.
                raw = obj.get_raw_data()
                n = struct.unpack_from('<i', raw)[0]
                name = raw[4:4+n].decode('utf-8')
                pos = (4+n+3) & ~3
                stream, size = struct.unpack_from('<ii', raw, pos+12)
                if stream == 2:
                    offset = struct.unpack_from('<I', raw, pos+20)[0]
                    with (data / 'sharedassets0.assets.resS').open('rb') as audio:
                        audio.seek(offset); blob = audio.read(size)
                else:
                    blob = raw[pos+20:pos+20+size]
                if len(blob) != size: raise ValueError('Truncated audio payload')
                ext = '.aiff' if blob[:4] == b'FORM' else '.wav' if blob[:4] == b'RIFF' else '.ogg' if blob[:4] == b'OggS' else '.mp3'
                dest = OUT / 'audio' / f'{key}_{safe_name(name)}{ext}'
                dest.write_bytes(blob)
                record.update(name=name, stream=stream, exports=[str(dest.relative_to(ROOT))])
            elif kind == "MonoBehaviour":
                # Player builds omit script type trees. Keep exact bytes for a
                # schema-aware decoder. PPtrs in this version are two int32s.
                raw = obj.get_raw_data()
                go_file, go_id = struct.unpack_from('<ii', raw)
                script_file, script_id = struct.unpack_from('<ii', raw, 12)
                if script_file:
                    external = obj.assets_file.externals[script_file - 1].path
                    script_source = Path(external).name
                else:
                    script_source = file
                cls = scripts.get((script_source, script_id), "unknown")
                dest = OUT / "behaviours" / f"{key}_{safe_name(cls)}.bin"
                dest.write_bytes(raw)
                record.update(script=cls, game_object={"file_id": go_file, "path_id": go_id},
                              export=str(dest.relative_to(ROOT)))
            elif kind in ("GameObject", "Transform", "MonoManager", "AnimationClip",
                          "Animation", "BoxCollider", "Rigidbody", "Material", "Mesh",
                          "Camera", "AudioSource", "MeshRenderer"):
                tree = obj.read_typetree()
                dest = OUT / "objects" / f"{key}_{kind}.json"
                save_json(dest, tree)
                if kind == "GameObject":
                    record["name"] = tree.get("m_Name")
                    scenes.append({**record, "components": tree.get("m_Component")})
                record["export"] = str(dest.relative_to(ROOT))
        except Exception as exc:
            record["error"] = str(exc)
            errors.append(record.copy())
        inventory.append(record)
    save_json(OUT / "inventory.json", inventory)
    save_json(OUT / "scene-objects.json", scenes)
    save_json(OUT / "export-errors.json", errors)
    print("Object types:", dict(counts))
    print("Exports with errors:", len(errors), "(see recovered/export-errors.json)")


if __name__ == "__main__":
    extract()
    app = next((OUT / "ipa" / "Payload").glob("*.app"))
    info = plistlib.loads((app / "Info.plist").read_bytes())
    save_json(OUT / "app-info.json", info)
    export_il(app / "Data" / "Managed" / "Assembly-CSharp.dll")
    export_unity(app / "Data")
