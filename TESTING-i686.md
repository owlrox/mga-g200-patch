# Testing the G200 EXA fixes on real hardware (i686)

## What you have

- `mga_drv.so` (this directory) — 32-bit i386 build of the fixed driver.
  ELF 32-bit LSB, Intel 80386, not stripped (debug info kept on purpose).
- Built from this repo at commit `ad426a5` (branch `g200-exa-fixes`) in an
  i386 Debian 12 (bookworm) container.

Fixes in this build:

- EXA offscreen memory limit restored to a 4096-byte guard
  (`mga_exa_memory_limit()` in `src/mga_layout.c`). Covers the
  `exa_offscreen.c` eviction-path overrun (measured up to 319 bytes
  with 192-byte alignment; still present in upstream xserver master).
- Offscreen base starts past the visible framebuffer:
  `displayWidth * cpp * virtualY` (not virtualX), rounded up to 192.
- DRI buffer layout rounds the texture heap offset DOWN, so the heap
  never runs past the top of VRAM and over the hardware cursor.
- Texture pitch checked against the 11-bit TEXCTL field before use
  (`mga_texture_pitch_ok`), with software fallback.

## Load it on the test machine (i686)

1. Copy `mga_drv.so` to the test machine (scp or USB).

2. Find the module dir (Debian/Ubuntu: `/usr/lib/xorg/modules/drivers/`):

       find / -name mga_drv.so 2>/dev/null

3. Backup the old driver and install the new one (as root):

       cp /usr/lib/xorg/modules/drivers/mga_drv.so \
          /usr/lib/xorg/modules/drivers/mga_drv.so.orig
       cp mga_drv.so /usr/lib/xorg/modules/drivers/mga_drv.so

4. From a text console, with X not running on the mga device, run:

       sh scripts/realhw-test.sh 60

   Use `0` instead of `60` to skip the Xorg smoke test.
   The script runs the unit tests, then a 60 s Xorg run on :99 using
   the freshly built driver, and captures lspci, dmesg, the Xorg log
   and a post-run dmesg.

## Rollback

    mv /usr/lib/xorg/modules/drivers/mga_drv.so.orig \
       /usr/lib/xorg/modules/drivers/mga_drv.so

## Alternative: don't overwrite the installed driver

    Xorg :99 -modulepath /path/to/dir-with-mga_drv.so -config myconfig.conf

Note: `-modulepath` replaces the whole module search path; you may need
to include the standard dirs too. Overwriting in place (step 3) is
simpler and the `.orig` backup is the rollback.

## Feedback

Send back the file printed at the end of the test run:

    mga-hw-results-<timestamp>.txt

It contains lspci, dmesg before/after, the unit test results, and the
full Xorg log.
