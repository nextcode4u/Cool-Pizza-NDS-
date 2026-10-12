# Runtime architecture

Cool Pizza DS is an ARM9 C application using devkitPro's bundled ARM7 runtime.
Game data is compiled into C tables; PCM audio is packed into NitroFS.

## Components

| File | Responsibility |
| --- | --- |
| `source/game.c` | Physics, attacks, enemy states, scoring, powers and wave progression |
| `source/render.c` | Bitmap rendering, sprites/font, scenery and menus |
| `source/main.c` | DS input, refresh clock, simulation catch-up and screen presentation |
| `source/audio.c` | Streamed music, cached effects and fly buzzing loop |
| `source/save.c` | FAT score/play-count persistence |
| `source/original_data.c` | Generated graphics, animation, paths and scene configuration |
| `include/game.h` | Simulation state and input interface |
| `include/original.h` | Compiled asset data structures |

## Update loop

The VBlank interrupt advances the refresh counter. The main loop runs the elapsed
number of 1/60-second simulation steps, with a maximum of eight catch-up steps.
Button presses are applied once; held steering is applied across those steps.
Sound requests are combined before audio service.

Rendering uses a RAM bitmap and DMA transfer into double-buffered top-screen
VRAM. The bottom screen is refreshed on state changes and periodically during
play. Simulation timing is independent of the number of rendered frames.

## Enemies and waves

Wave configuration creates enemies with authored properties, movement curves,
paths and sprite clips. Entrance transforms are initialized before the first
render. Attack states control projectiles and timed damage areas.

Collision-entry tracking records the difference between entering an attack area
and remaining inside it. Shield consumption uses the same contact tracking.

Fly figure-eight motion and sweeper hover share the current simulation time.
Each type samples its hover once per tick and reuses it across active enemies.

## Audio and storage

Music streams from NitroFS through maxmod. Mono effects are loaded into a cache
and mixed with stereo music. A separate voice carries the fly buzzing loop.

`source/save.c` stores best score and play count in a checksum-protected record
at `fat:/coolpizza.stats` when writable FAT storage is available.

## Related documentation

- [Gameplay](fidelity.md)
- [Asset conversion](recovery.md)
- [Build guide](build.md)
- [Testing](emulator-testing.md)
- [Credits](../CREDITS.md)
