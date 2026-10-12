# Asset formats and conversion

The asset tools target **Cool Pizza 1.4**, built with **Unity 3.5.7f6** and
serialization version 9. The app's bundle identifier is
`com.secretlibrary.coolpizza`.

## Pipeline

| Script | Input / output |
| --- | --- |
| `recover_assets.py` | Extracts local archives, exports Unity objects/textures/audio and disassembles managed IL |
| `decode_behaviours.py` | Decodes serialized script fields into JSON |
| `prepare_art.py` | Reconstructs named sprites from atlas UVs and quad geometry |
| `compile_original.py` | Builds C tables for sprites, fonts, animation, paths, waves and enemy properties |
| `prepare_audio.py` | Converts IPA music/effects to DS PCM files |

See [building](build.md) for commands and dependencies.

## Source data

| Data | Version 1.4 contents |
| --- | --- |
| Textures | 47 game textures, including six sprite atlases |
| Atlas sprites | 144 named sprites |
| Sprite animation | 25 clips across 11 libraries |
| Unity motion | 47 animation clips |
| Waves | 47 Wave and 47 RampWave records |
| Enemies | 196 scene instances |
| Paths | 36 iTween paths |
| Audio | Four music tracks and 23 other payloads |

The generated sprite tables also contain standalone images and font glyphs.
Wave definitions are combined into the game's 42-entry run sequence.

## Graphics

The old iOS texture layout uses RGBA4444. The exporter corrects the channel order
before writing PNGs. Sprite crops use the original quad vertices and UVs,
including rotated atlas entries. Geometry and pivots are retained for animation.

The C compiler converts images to DS 16-bit bitmap pixels and scales sprites for
the 256-pixel-wide playfield. The original Blackout Midnight font is converted
from the Unity font atlas.

Motion curves are sampled at 60 Hz using Hermite interpolation. Paths are
sampled into 241-point tables. Clip tables preserve frame rate and wrap mode.

## Audio

The IPA stores AIFF, WAV and MP3 payloads both inline and in the external `.resS`
file. Audio conversion produces signed 16-bit PCM at 22.05 kHz: stereo for music
and mono for effects.

| Music file | Screen / state |
| --- | --- |
| `title.pcm` | Intro and title |
| `level.pcm` | Gameplay |
| `death.pcm` | Death sequence |
| `scores.pcm` | Scores and completion |

All PCM files are embedded in NitroFS. Effects are cached on demand rather than
loading every effect into DS memory at startup.

## Intermediate files

`recovered/` contains archive hashes, object inventories, decoded script records,
IL listings, sprite crops and conversion manifests. `port-audit.json` describes
compiled scene data; `ds-audio.json` lists converted audio files.
These files are local build inputs and are excluded from the source repository.
