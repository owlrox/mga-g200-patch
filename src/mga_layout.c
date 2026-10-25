/*
 * Pure layout math for the Matrox G200/G400/G550 driver, split out of
 * mga_exa.c and mga_storm.c so it can be unit-tested without the Xorg
 * server SDK or hardware.
 */

/* Deliberately no config.h include: this file must compile standalone
 * (config.h pulls in xorg-server.h) and uses nothing from it.
 */

#include "mga_layout.h"

void
mga_dri_layout(MgaDriLayout *out, long fbTop, long bufferSize,
               long maxlines, long widthBytes)
{
    long textureSize;

    /* Try for front, back, depth, and two framebuffers worth of
     * pixmap cache.  Should be enough for a fullscreen background
     * image plus some leftovers.
     */
    textureSize = fbTop - 5 * bufferSize;

    /* If that gives us less than half the available memory, let's
     * be greedy and grab some more.  Sorry, I care more about 3D
     * performance than playing nicely, and you'll get around a full
     * framebuffer's worth of pixmap cache anyway.
     */
    if (textureSize < fbTop / 2) {
        textureSize = fbTop - 4 * bufferSize;
    }

    /* Check to see if there is more room available after the maximum
     * scanline for textures.
     */
    if (fbTop - maxlines * widthBytes - bufferSize * 2 > textureSize) {
        textureSize = fbTop - maxlines * widthBytes - bufferSize * 2;
    }

    /* Set a minimum usable local texture heap size.  This will fit
     * two 256x256x32bpp textures.
     */
    if (textureSize < 512 * 1024) {
        out->textureOffset = 0;
        out->textureSize = 0;
    } else {
        out->textureSize = textureSize;

        /* Reserve space for textures.  Round the offset DOWN so the
         * heap ends at or below fbTop; rounding up made the heap
         * extend past the top of video RAM, over the hardware cursor.
         */
        out->textureOffset = (fbTop - textureSize) & ~MGA_DRI_BUFFER_ALIGN;
    }

    /* Reserve space for the shared depth buffer */
    out->depthOffset = (out->textureOffset - bufferSize +
                        MGA_DRI_BUFFER_ALIGN) & ~MGA_DRI_BUFFER_ALIGN;

    /* Reserve space for the shared back buffer */
    out->backOffset = (out->depthOffset - bufferSize +
                       MGA_DRI_BUFFER_ALIGN) & ~MGA_DRI_BUFFER_ALIGN;
}

unsigned long
mga_exa_offscreen_base(long displayWidth, long virtualY, int bpp,
                       long offsetAlign)
{
    /* The screen pixmap pitch is displayWidth, which xf86ValidateModes
     * rounds up to the hardware pitch granularity and which is usually
     * larger than virtualX.  The visible framebuffer therefore occupies
     * displayWidth * cpp * virtualY bytes, and offscreen memory must
     * start past it.  Round up to offsetAlign; it need not be a power
     * of two, so use the modulo form, not a mask.
     */
    unsigned long fb_bytes = (unsigned long)displayWidth *
                             (unsigned long)virtualY *
                             (unsigned long)(bpp / 8);

    return (fb_bytes + offsetAlign - 1) -
           ((fb_bytes + offsetAlign - 1) % offsetAlign);
}

unsigned long
mga_exa_memory_limit(unsigned long fbUsable, unsigned long driBackOffset)
{
    unsigned long limit = fbUsable;

    /* The DRI back buffer, when present, bounds the EXA area the same
     * way the XAA path limits the FBManager to below the back buffer.
     */
    if (driBackOffset && driBackOffset < limit)
        limit = driBackOffset;

    /* exa_offscreen.c's eviction path computes the alignment slack from
     * the free area's size BEFORE the follow-up kick-outs merge more
     * space into it, then carves the allocation from the end of the
     * ENLARGED area.  The carved area's start is therefore misaligned,
     * and the pixmap data can overrun the area end by more than
     * (MGA_EXA_OFFSET_ALIGN - 1) bytes (measured: 319) -- into the next
     * allocation, or past the end of VRAM when the area is the last one
     * (where the hardware cursor lives), or into the DRI buffers when
     * they cap memorySize.  Keep MGA_EXA_GUARD bytes of slack below the
     * limit so such an allocation stays inside VRAM; reserve it only
     * when more than twice that remains, so a tiny framebuffer is never
     * underflowed.  Verified against the real xserver 21.1.7
     * exa/exa_offscreen.c; still present in upstream master as of
     * 2026-10.
     */
    if (limit > 2 * MGA_EXA_GUARD)
        limit -= MGA_EXA_GUARD;

    return limit;
}

int
mga_texture_pitch_ok(int width, int bpp, int pitchAlign)
{
    /* MGAREG_TEXCTL carries the texture pitch in an 11-bit field (2047
     * pixels max).  Compute the pitch EXA will assign to this pixmap
     * and report whether it fits; with EXA_OFFSCREEN_ALIGN_POT unset
     * the pitch is the pixmap width in bytes rounded up to
     * pixmapPitchAlign.
     */
    int pitch = (width * bpp + 7) / 8;

    pitch = (pitch + pitchAlign - 1) / pitchAlign * pitchAlign;
    return pitch / (bpp / 8) <= 2047;
}
