# Building Cool Pizza DS

## Requirements

- devkitPro with devkitARM, libnds, calico, maxmod, libfat and DS tools.
- Python 3.10 or newer.
- ffmpeg for audio conversion.
- A Cool Pizza **version 1.4 IPA** for local asset preparation.

The original game assets are not included in the repository. Place the IPA in
`Downloads/` before running the preparation scripts. The soundtrack ZIP may
also be placed there for extraction; ROM audio comes from the IPA.

## Prepare assets

Run these commands from the repository root:

```sh
python3 -m venv .venv
.venv/bin/pip install -r tools/recovery-requirements.txt
.venv/bin/python tools/recover_assets.py
.venv/bin/python tools/decode_behaviours.py
.venv/bin/python tools/prepare_art.py
.venv/bin/python tools/compile_original.py
.venv/bin/python tools/prepare_audio.py
```

The scripts generate `source/original_data.c` and `nitro/audio/*.pcm` from the
original sprites, scene data and audio. Both are required for the ROM build.
Prepared assets can be reused when changing the C runtime.

## Compile

Set the SDK paths for your installation:

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
make -j2
```

The build produces `coolpizza.nds` and `coolpizza.elf`. Music and effects are
packed into the ROM through NitroFS.

For a clean rebuild:

```sh
make clean
make -j2
```

Cleaning removes `build/`, the ELF and the ROM. It preserves prepared assets,
original downloads and local tools.

## Test

```sh
make -f Makefile.host test
```

See [testing](emulator-testing.md) for melonDS checks and renderer previews.

## Directories

| Path | Contents |
| --- | --- |
| `source/`, `include/` | Game runtime and interfaces |
| `tests/` | Host-side simulation tests |
| `tools/` | Asset preparation and test utilities |
| `docs/` | Project documentation |
| `Downloads/` | Local IPA, optional soundtrack and emulator BIOS files |
| `recovered/` | Extracted assets and intermediate records |
| `nitro/` | Generated audio embedded in the ROM |
| `tools/.local/` | Local tools, including the extracted Linux melonDS bundle |
| `build/` | Compilation, preview and test outputs |

Local assets, generated files, ROMs and test tools are excluded by `.gitignore`.
