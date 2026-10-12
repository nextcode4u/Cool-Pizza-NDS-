# Testing

## Host tests

A C compiler with AddressSanitizer and UBSan support is required.

```sh
make -f Makefile.host test
```

The suite checks physics, attacks, scoring, shield collision entry, pause,
wave selection, motion/animation data and extended simulation. It also exercises
transitions through every authored wave and checks entrance poses before the
first rendered frame.

## melonDS checks

Requirements: Linux melonDS 1.1, Xvfb, xdotool, ImageMagick, ffmpeg, Pillow and
devkitARM's `nm`. Build the ROM and prepare the Python environment using the
[build guide](build.md).

```sh
make -j2
make -f Makefile.host layout
.venv/bin/python tools/verify_port.py tools/.local/melonds/AppRun
```

The test starts an isolated emulator in standard DS mode with direct boot and
software graphics. It checks boot, audio output, enemy movement/animation,
pause/resume, ramp launch, attacks, menus, touchscreen input and steering.
The `layout` target supplies offsets for reading game state through the debugger.

The emulator argument can point to another extracted Linux AppImage bundle.
`--display :93` is the default; select another display when needed.
The script expects DS BIOS/firmware files under
`Downloads/melonDS-1.1-windows-x86_64/` and copies them into its test configuration.

| Test keyboard key | DS input |
| --- | --- |
| X | A |
| Space | Start |
| A / D | Left / Right |
| Y | Y / power |
| B | B / back |
| V | X / scores |

## Output

Results are written under `build/port-checks/`.

| File | Contents |
| --- | --- |
| `report.json` | Result, ROM hash, game state, timing counters and capture paths |
| Numbered PNG files | Emulator screenshots |
| `audio.raw` | 48 kHz stereo signed 16-bit SDL audio capture |
| `emulated-audio.wav` | First 12 seconds of captured audio |
| `emulator.log`, `xvfb.log` | Diagnostics |

Timing is recorded separately for simulation ticks, rendered frames and emulated
refreshes. The simulation check compares ticks against the emulated refresh
clock rather than assuming the emulator runs at exactly 60 wall-clock FPS.

## Renderer preview

```sh
make -f Makefile.host preview
./build/preview 0 120
```

The optional arguments are an authored wave ID (0–46) and a tick count.
The preview writes `build/preview.ppm` using the game's software renderer.
