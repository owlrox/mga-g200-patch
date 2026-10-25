# xf86-video-mga — Matrox G200 driver (patched)

X.Org driver for Matrox G200-series graphics chips, with fixes to the EXA
acceleration path. This tree started from the upstream
[xf86-video-mga](https://gitlab.freedesktop.org/xorg/driver/xf86-video-mga)
sources and carries correctness fixes that upstream does not yet have.

## What's fixed

- **EXA offscreen memory limit** — the offscreen allocator's limit is now
  enforced with a 4096-byte guard (`src/mga_layout.c`), fixing the
  eviction-path overrun where allocations could walk past the end of
  offscreen memory.
- **Offscreen base calculation** — the offscreen area now starts past the
  visible framebuffer using the actual pitch (`displayWidth * cpp`),
  not the virtual width, with correct alignment.
- **DRI buffer layout** — the texture heap offset is rounded down so the
  heap never overruns the top of VRAM into the hardware cursor.
- **Texture pitch validation** — texture pitches are checked against the
  11-bit limit the hardware can express (`mga_texture_pitch_ok`) before
  use, falling back to software where they don't fit.

## Repository layout

```
src/        Driver sources (mga_*.c)
man/        Man page
tests/      Unit tests for the layout/EXA logic
util/       Helper tools
scripts/    realhw-test.sh — hardware test runner
```

## Building

Requires the X.Org server development headers and `pkg-config`.

```sh
./autogen.sh
make
```

The build produces `src/mga_drv.so` (or the `.so` target for your
platform). See `tests/` for the unit tests:

```sh
make check   # or run tests/test_layout directly
```

## Testing on real hardware

`scripts/realhw-test.sh` runs the unit tests and then a full Xorg session
against the freshly built driver, capturing `lspci`, `dmesg`, and the Xorg
log:

```sh
sh scripts/realhw-test.sh 60
```

See `TESTING-i686.md` for the full hardware-test workflow, including how
to install the driver on a test machine and how to roll back.

## License

MIT-style XFree86 license — see `COPYING`. Upstream sources remain under
their original license.
