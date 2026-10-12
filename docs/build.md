# Building Cool Pizza DS

The repository includes all prepared graphics, game data, music and effects.
A normal build requires the Nintendo DS toolchain and Make. No original IPA,
Python asset conversion, soundtrack download or emulator is needed.

## Requirements

Install devkitPro's Nintendo DS development tools: devkitARM, libnds, calico,
maxmod, libfat and DS tools. Installation instructions are available from
[devkitPro](https://devkitpro.org/wiki/Getting_Started).

The commands below use `/opt/devkitpro`. Adjust the SDK paths for your installation.

## Clone and compile

```sh
git clone https://github.com/nextcode4u/Cool-Pizza-NDS-.git
cd Cool-Pizza-NDS-
export DEVKITPRO=/opt/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
make -j2
```

Output: `coolpizza.nds`. Music and effects are embedded through NitroFS.
The build also produces `coolpizza.elf` and intermediate files under `build/`.

For a clean rebuild, run from the repository root:

```sh
make clean
make -j2
```

Cleaning removes `build/`, the ELF and the ROM. The committed graphics/game data
in `source/original_data.c` and audio in `nitro/audio/` are preserved.

## Tests

Host tests require a C compiler with AddressSanitizer and UBSan support:

```sh
make -f Makefile.host test
```

See [testing](emulator-testing.md) for optional melonDS checks and renderer previews.
Emulator testing has additional dependencies; it is separate from building the ROM.

## Optional asset regeneration

The preparation tools can regenerate the committed data from a Cool Pizza
version 1.4 IPA. This requires Python 3.10 or newer, ffmpeg and the original IPA
placed in `Downloads/`. The soundtrack ZIP can also be extracted there; ROM
playback uses the IPA's audio.

```sh
python3 -m venv .venv
.venv/bin/pip install -r tools/recovery-requirements.txt
.venv/bin/python tools/recover_assets.py
.venv/bin/python tools/decode_behaviours.py
.venv/bin/python tools/prepare_art.py
.venv/bin/python tools/compile_original.py
.venv/bin/python tools/prepare_audio.py
```

Run these commands from the repository root. Changes to conversion scripts
should include their regenerated C and PCM outputs.

## Directories

| Path | Contents |
| --- | --- |
| `source/`, `include/` | Runtime code, interfaces and prepared graphics/game data |
| `nitro/audio/` | Prepared music and effects embedded in the ROM |
| `tests/` | Host-side simulation tests |
| `tools/` | Optional asset preparation and test utilities |
| `docs/` | Project documentation |
| `Downloads/` | Local source archives and emulator BIOS files; ignored |
| `recovered/` | Intermediate recovery data; ignored |
| `tools/.local/` | Local tools such as the Linux melonDS bundle; ignored |
| `build/` | Compilation, preview and test outputs; ignored |

Original game credits are in [CREDITS.md](../CREDITS.md).
