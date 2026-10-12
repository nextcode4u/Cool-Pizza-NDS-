# Contributing

Changes should preserve Cool Pizza's wave layouts, attack rhythm, scoring and
visual style. Original game credits belong in [CREDITS.md](CREDITS.md).

## Development

Prepare local assets and install the SDK using [the build guide](docs/build.md).
For gameplay or rendering changes:

```sh
make -j2
make -f Makefile.host test
```

Use the existing C style and keep generated data changes in the asset preparation
scripts. `source/original_data.c` and `nitro/` are regenerated locally.

## Bug reports

Include the device or emulator, ROM version/hash, affected wave or enemy and
steps to reproduce. Screenshots or a short recording help explain visual issues.
For behavior differences, include a reference to the original game when possible.

## Pull requests

Describe the resulting behavior and the checks run. Keep game assets, IPA files,
audio, BIOS/firmware, emulator bundles and build outputs out of pull requests.
