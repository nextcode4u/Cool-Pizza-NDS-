"""Convert the original iOS app icon into the DS cartridge banner bitmap.

The committed assets are ready to build. This script is only needed to regenerate
those files from the original version 1.4 IPA's primary icon.
"""
from pathlib import Path
import io
import struct
import zlib
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'recovered/ipa/Payload/coolpizza.app/app_icon_114x114.png'
OUTPUT = ROOT / 'assets'


def png_chunk(kind, payload):
    return (struct.pack('>I', len(payload)) + kind + payload +
            struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))


def read_ios_icon(path):
    raw = path.read_bytes()
    assert raw[:8] == b'\x89PNG\r\n\x1a\n', 'Expected PNG'
    chunks = []
    offset = 8
    while offset < len(raw):
        length = struct.unpack_from('>I', raw, offset)[0]
        kind = raw[offset + 4:offset + 8]
        payload = raw[offset + 8:offset + 8 + length]
        assert len(payload) == length, 'Truncated PNG chunk'
        chunks.append((kind, payload))
        offset += length + 12
    if not any(kind == b'CgBI' for kind, _ in chunks):
        return Image.open(io.BytesIO(raw)).convert('RGBA')
    header = next(payload for kind, payload in chunks if kind == b'IHDR')
    assert header[8:] == bytes([8, 6, 0, 0, 0]), 'Expected non-interlaced RGBA8'
    compressed = b''.join(payload for kind, payload in chunks if kind == b'IDAT')
    # Apple's optimized PNG uses raw DEFLATE and premultiplied BGRA pixels.
    pixels = zlib.decompress(compressed, -15)
    png = (raw[:8] + png_chunk(b'IHDR', header) +
           png_chunk(b'IDAT', zlib.compress(pixels)) + png_chunk(b'IEND', b''))
    decoded = Image.open(io.BytesIO(png)).convert('RGBA')
    rgba = []
    for b, g, r, a in decoded.get_flattened_data():
        if a:
            rgba.append((min(255, (r * 255 + a // 2) // a),
                         min(255, (g * 255 + a // 2) // a),
                         min(255, (b * 255 + a // 2) // a), a))
        else:
            rgba.append((0, 0, 0, 0))
    decoded.putdata(rgba)
    return decoded


if __name__ == '__main__':
    OUTPUT.mkdir(exist_ok=True)
    icon = read_ios_icon(SOURCE)
    icon.save(OUTPUT / 'ios-icon.png')
    # DS banners use 32x32 pixels and 16 palette entries. Index zero is transparent.
    small = icon.resize((32, 32), Image.Resampling.LANCZOS)
    flat = Image.new('RGB', small.size, 'black')
    flat.paste(small, mask=small.getchannel('A'))
    quantized = flat.quantize(colors=15, method=Image.Quantize.MEDIANCUT,
                             dither=Image.Dither.NONE)
    banner = Image.new('P', (32, 32))
    banner.putpalette([255, 0, 255] + quantized.getpalette()[:45] + [0] * 720)
    banner.putdata([0 if a < 96 else p + 1
                    for p, a in zip(quantized.get_flattened_data(), small.getchannel('A').get_flattened_data())])
    palette = banner.getpalette()[:48]
    image = list(banner.get_flattened_data())
    packed = bytes((image[y * 32 + x] << 4) | image[y * 32 + x + 1]
                   for y in range(31, -1, -1) for x in range(0, 32, 2))
    colors = bytes(v for i in range(16)
                   for v in (palette[i * 3 + 2], palette[i * 3 + 1], palette[i * 3], 0))
    offset = 14 + 40 + len(colors)
    header = struct.pack('<2sIHHI', b'BM', offset + len(packed), 0, 0, offset)
    info = struct.pack('<IiiHHIIiiII', 40, 32, 32, 1, 4, 0, len(packed),
                       2835, 2835, 16, 16)
    (OUTPUT / 'icon.bmp').write_bytes(header + info + colors + packed)
    print('Prepared the original iOS icon and 32x32 DS banner bitmap.')
