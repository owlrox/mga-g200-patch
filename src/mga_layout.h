/*
 * Pure layout math for the Matrox G200/G400/G550 driver, split out of
 * mga_exa.c and mga_storm.c so it can be unit-tested without the Xorg
 * server SDK or hardware.
 */

#ifndef MGA_LAYOUT_H
#define MGA_LAYOUT_H

/* DRI buffers are aligned to this boundary (same value as
 * MGA_BUFFER_ALIGN in mga_dri.h, duplicated here to keep this header
 * free of Xorg includes).
 */
#define MGA_DRI_BUFFER_ALIGN 0x00000fff

/* EXA alignment constants, as programmed in mgaExaInit().
 *
 * pixmapOffsetAlign: in PW24 mode the hardware needs "3 64-bytes",
 * so 192.
 *
 * pixmapPitchAlign: pitch alignment is in sets of 32 pixels.  Cover
 * both 32bpp (32 pixels = 128 bytes) and PW24 (64 pixels = 192 bytes):
 * LCM(128, 192) = 384.  384 is also a multiple of 3, so at 24bpp the
 * byte pitch always converts exactly to pixels for MGAREG_PITCH, which
 * is programmed in pixels (see mgaGetPixmapPitch).
 */
#define MGA_EXA_OFFSET_ALIGN 192
#define MGA_EXA_PITCH_ALIGN  384

/* Guard below the EXA memory limit, absorbing exa_offscreen.c's
 * allocation overshoot (see mga_exa_memory_limit).  The old driver
 * code reserved a full 4096-byte guard page; keep that size.
 */
#define MGA_EXA_GUARD 4096

/* DRI offscreen buffers, laid out top-down from fbTop (FbUsableSize,
 * which already excludes the hardware cursor at the top of video RAM).
 *
 * bufferSize: size of one back/depth buffer, already aligned.
 * maxlines * widthBytes: maximum scanline area for the front buffer.
 *
 * Invariants (for 4K-aligned bufferSize and a non-degenerate heap):
 *   textureOffset + textureSize <= fbTop   (heap never overruns fbTop)
 *   backOffset + bufferSize == depthOffset
 *   depthOffset + bufferSize == textureOffset
 */
typedef struct {
    long textureSize;
    long textureOffset;
    long depthOffset;
    long backOffset;
} MgaDriLayout;

void
mga_dri_layout(MgaDriLayout *out, long fbTop, long bufferSize,
               long maxlines, long widthBytes);

/* First byte of EXA offscreen memory: past the visible framebuffer,
 * which occupies displayWidth * cpp * virtualY bytes (the screen
 * pixmap pitch is displayWidth, usually larger than virtualX), rounded
 * up to offsetAlign.  offsetAlign need not be a power of two.
 */
unsigned long
mga_exa_offscreen_base(long displayWidth, long virtualY, int bpp,
                       long offsetAlign);

/* The memorySize to hand to EXA: min(fbUsable, driBackOffset) less a
 * guard of MGA_EXA_GUARD bytes (reserved only when more than twice
 * that remains, so a tiny framebuffer is never underflowed).
 * driBackOffset == 0 (or >= fbUsable) means DRI is not in the way.
 *
 * The guard covers a bug in exa/exa_offscreen.c: the eviction path
 * computes the alignment slack from the free area's size BEFORE the
 * follow-up kick-outs merge more space into it, then carves the
 * allocation from the end of the ENLARGED area.  The carved area's
 * start is therefore misaligned, and the pixmap data can overrun the
 * area end by more than (MGA_EXA_OFFSET_ALIGN - 1) bytes (measured:
 * 319) -- into the next allocation, or past the top of VRAM when the
 * area is the last one (where the hardware cursor lives).  Verified
 * against the real xserver 21.1.7 exa/exa_offscreen.c; still present
 * in upstream master as of 2026-10.
 */
unsigned long
mga_exa_memory_limit(unsigned long fbUsable, unsigned long driBackOffset);

/* True if the EXA pitch EXA will assign to a pixmap of width x bpp
 * (rounded up to pitchAlign) fits the 11-bit MGAREG_TEXCTL pitch field
 * (2047 pixels max).  Calling code must fall back to software when
 * this returns false; PrepareSourceTexture silently truncates larger
 * pitches.
 */
int
mga_texture_pitch_ok(int width, int bpp, int pitchAlign);

#endif /* MGA_LAYOUT_H */
