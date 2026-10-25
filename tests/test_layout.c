/*
 * Unit tests for src/mga_layout.c -- pure G200 layout math, no Xorg
 * SDK or hardware needed.  Run with `make check` in this directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include "mga_layout.h"

static int failures;
static int checks;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void
test_constants(void)
{
    /* LCM(128, 192) = 384: covers 32bpp 32-pixel granularity and the
     * PW24 64-pixel granularity, and is a multiple of 3 so 24bpp byte
     * pitches convert exactly to pixels.
     */
    CHECK(MGA_EXA_PITCH_ALIGN == 384);
    CHECK(MGA_EXA_OFFSET_ALIGN == 192);
    CHECK(MGA_DRI_BUFFER_ALIGN == 0x00000fff);
}

static void
test_offscreen_base(void)
{
    /* 800x600@24 on G200: Roundings[24] is 64, so displayWidth is 832,
     * not virtualX = 800.  The visible framebuffer occupies
     * 832*600*3 = 1497600 bytes; offscreen must start there.  The old
     * virtualX-based value (1440000) sat inside the visible framebuffer
     * and corrupted the bottom scanlines.
     */
    unsigned long base = mga_exa_offscreen_base(832, 600, 24,
                                                MGA_EXA_OFFSET_ALIGN);
    CHECK(base == 1497600);
    CHECK(base > 800u * 600u * 3u);

    /* Rounds up to offsetAlign (192 is not a power of two). */
    CHECK(mga_exa_offscreen_base(41, 10, 8, 192) == 576);

    /* Already aligned stays put. */
    CHECK(mga_exa_offscreen_base(640, 480, 32, 192) == 1228800);
}

static void
test_texture_pitch(void)
{
    /* Fits: 2000 px at 32bpp = 8000 bytes, rounded up to 384 -> 8064
     * bytes = 2016 pixels, inside the 11-bit field (2047 max).
     */
    CHECK(mga_texture_pitch_ok(2000, 32, MGA_EXA_PITCH_ALIGN) == 1);

    /* 2048 px at 32bpp = 8192 bytes -> 8448 aligned = 2112 pixels:
     * over the field, must be rejected.
     */
    CHECK(mga_texture_pitch_ok(2048, 32, MGA_EXA_PITCH_ALIGN) == 0);

    /* 3000 px at 24bpp = 9000 bytes -> 9216 aligned = 3072 pixels:
     * over the field.
     */
    CHECK(mga_texture_pitch_ok(3000, 24, MGA_EXA_PITCH_ALIGN) == 0);

    CHECK(mga_texture_pitch_ok(640, 24, MGA_EXA_PITCH_ALIGN) == 1);
    CHECK(mga_texture_pitch_ok(100, 32, MGA_EXA_PITCH_ALIGN) == 1);
}

static void
test_memory_limit(void)
{
    /* The guard must absorb exa_offscreen.c's allocation overshoot:
     * the eviction path computes the alignment slack from the free
     * area's size BEFORE merging in more space, then carves from the
     * enlarged area's end, so the pixmap data can overrun the area end.
     * Measured overshoot against the real xserver 21.1.7 allocator:
     * 319 bytes with a 192-byte alignment.  The old driver code reserved
     * a full 4096-byte guard page; keep that.
     */
    CHECK(mga_exa_memory_limit(8388608u - 1024u, 0) == 8387584u - 4096u);

    /* DRI back buffer below FbUsableSize: capped, then the guard. */
    CHECK(mga_exa_memory_limit(8388608u - 1024u, 3145728u) == 3145728u - 4096u);

    /* DRI back offset above FbUsableSize or unset: no cap, just the guard. */
    CHECK(mga_exa_memory_limit(8388608u - 1024u, 0) == 8387584u - 4096u);
    CHECK(mga_exa_memory_limit(8388608u - 1024u, 8388608u) == 8387584u - 4096u);

    /* Tiny framebuffer: never underflow.  The guard is reserved only
     * when more than 2 * MGA_EXA_GUARD remains.
     */
    CHECK(mga_exa_memory_limit(100, 0) == 100);
    CHECK(mga_exa_memory_limit(4096, 0) == 4096);
    CHECK(mga_exa_memory_limit(2 * 4096, 0) == 2 * 4096);
    CHECK(mga_exa_memory_limit(2 * 4096 + 1, 0) == 2 * 4096 + 1 - 4096);
}

static void
test_dri_layout_regression(void)
{
    /* Regression: the texture heap must end at or below fbTop.  With
     * the old round-UP of (fbTop - textureSize) this layout gave a
     * heap top of 1000448 > fbTop = 1000000, running past the top of
     * video RAM and over the hardware cursor.
     */
    MgaDriLayout l;
    long fbTop = 1000000;
    long bufferSize = 102400;   /* 4K-aligned, as the caller computes it */
    long maxlines = 10;
    long widthBytes = 16000;

    mga_dri_layout(&l, fbTop, bufferSize, maxlines, widthBytes);

    CHECK(l.textureSize == 635200);
    CHECK(l.textureOffset == 364544);
    CHECK(l.depthOffset == 262144);
    CHECK(l.backOffset == 159744);

    CHECK(l.textureOffset + l.textureSize <= fbTop);
    CHECK(l.backOffset + bufferSize == l.depthOffset);
    CHECK(l.depthOffset + bufferSize == l.textureOffset);
}

static void
test_dri_layout_8mb(void)
{
    /* Typical G200: 8MB card, 1024x768x16, 1KB hardware cursor at the
     * top of VRAM already excluded from FbUsableSize.
     */
    MgaDriLayout l;
    long fbTop = 8388608 - 1024;
    long bufferSize = 768 * 2048;      /* 4K-aligned */
    long maxlines = fbTop / 2048;      /* 4095 */
    long widthBytes = 2048;

    mga_dri_layout(&l, fbTop, bufferSize, maxlines, widthBytes);

    CHECK(l.textureSize == 2096128);
    CHECK(l.textureOffset == 6291456);
    CHECK(l.depthOffset == 4718592);
    CHECK(l.backOffset == 3145728);

    CHECK(l.textureOffset + l.textureSize == fbTop);
    CHECK(l.backOffset + bufferSize == l.depthOffset);
    CHECK(l.depthOffset + bufferSize == l.textureOffset);
}

static void
test_dri_layout_degenerate(void)
{
    /* Too little memory for a usable heap: the heap is disabled, not
     * wrapped around the top of VRAM.
     */
    MgaDriLayout l;
    long fbTop = 1048576;
    long bufferSize = 786432;
    long maxlines = 512;
    long widthBytes = 1024;

    mga_dri_layout(&l, fbTop, bufferSize, maxlines, widthBytes);

    CHECK(l.textureSize == 0);
    CHECK(l.textureOffset == 0);
}

int
main(void)
{
    test_constants();
    test_offscreen_base();
    test_texture_pitch();
    test_memory_limit();
    test_dri_layout_regression();
    test_dri_layout_8mb();
    test_dri_layout_degenerate();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
