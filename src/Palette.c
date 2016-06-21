#include "Palette.h"
#include "Graphics.h"
#include "Picture.h"
#include "Resource.h"
#include "Timer.h"

RPalette g_sysPalette = { 0 };

// Integrate this palette into the current system palette observing the various
// rules of combination expressed by the "flags" element of each palette color
// value & mode.
static void InsertPalette(RPalette *srcPal, RPalette *dstPal, int mode);

// Given R/G/B values, returns the index of this color.
static uint Match(const Guns *theGun, const RPalette *pal, int leastDist);

void InitPalette(void)
{
    int i;
    for (i = 0; i < PAL_CLUT_SIZE; i++) {
        g_sysPalette.gun[i].flags    = 0;
        g_sysPalette.gun[i].r        = 0;
        g_sysPalette.gun[i].g        = 0;
        g_sysPalette.gun[i].b        = 0;
        g_sysPalette.palIntensity[i] = MAX_INTENSITY;
    }
    g_sysPalette.gun[0].flags = PAL_IN_USE;

    // Set up index 255 as white.
    g_sysPalette.gun[255].r     = 255;
    g_sysPalette.gun[255].g     = 255;
    g_sysPalette.gun[255].b     = 255;
    g_sysPalette.gun[255].flags = PAL_IN_USE;

    SetResPalette(999, PAL_REPLACE);
}

void RSetPalette(RPalette *srcPal, int mode)
{
    uint32_t valid;

    // Save to note any change requiring a SetCLUT call.
    valid = g_sysPalette.valid;

    if (mode == PAL_REPLACE || srcPal->valid != g_sysPalette.valid) {
        InsertPalette(srcPal, &g_sysPalette, mode);

        // Validate the source palette.
        srcPal->valid = g_sysPalette.valid;
        if (valid != g_sysPalette.valid && g_picNotValid == 0) {
            SetCLUT(&g_sysPalette);
        }
    }
}

void SetResPalette(uint num, int mode)
{
    Handle hPal;

    hPal = ResLoad(RES_PAL, num);
    if (hPal != NULL) {
        RSetPalette((RPalette *)hPal, mode);
    }
}

void SetPalIntensity(RPalette *palette, int first, int last, int intensity)
{
    int i;

    // Protect first and last entry.
    if (first == 0) {
        ++first;
    }

    if (last > (PAL_CLUT_SIZE - 1)) {
        last = PAL_CLUT_SIZE - 1;
    }

    for (i = first; i < last; i++) {
        palette->palIntensity[i] = (int16_t)intensity;
    }
}

static void InsertPalette(RPalette *srcPal, RPalette *dstPal, int mode)
{
    int      i;
    uint     iMatch;
    uint8_t  flags;
    uint32_t valid;

    valid = RTickCount();

    for (i = 1; i < (PAL_CLUT_SIZE - 1); ++i) {
        // Set remapping index to none by default.
        srcPal->mapTo[i] = (uint8_t)i;

        flags = srcPal->gun[i].flags;

        // Only deal with active entries.
        if ((flags & PAL_IN_USE) == 0) {
            continue;
        }

        if (mode == PAL_REPLACE) {
            dstPal->valid  = valid;
            dstPal->gun[i] = srcPal->gun[i];
            continue;
        }

        // If this value is in use we must update valid element.
        if ((dstPal->gun[i].flags & PAL_IN_USE) == 0) {
            dstPal->valid  = valid;
            dstPal->gun[i] = srcPal->gun[i];
            continue;
        }

        iMatch = Match(&srcPal->gun[i], dstPal, 0);
        if (iMatch < PAL_CLUT_SIZE) {
            srcPal->mapTo[i] = (uint8_t)iMatch;
            continue;
        }

        for (iMatch = 0; iMatch < PAL_CLUT_SIZE; ++iMatch) {
            if ((dstPal->gun[iMatch].flags & PAL_IN_USE) == 0) {
                srcPal->mapTo[i] = (uint8_t)iMatch;

                dstPal->valid       = valid;
                dstPal->gun[iMatch] = srcPal->gun[i];
                break;
            }
        }

        if (iMatch == PAL_CLUT_SIZE) {
            iMatch           = Match(&srcPal->gun[i], dstPal, -1);
            srcPal->mapTo[i] = (uint8_t)iMatch;
            dstPal->gun[iMatch].flags |= PAL_MATCHED;
        }
    }
}

static uint Match(const Guns *theGun, const RPalette *pal, int leastDist)
{
    return FastMatch(
      pal, theGun->r, theGun->g, theGun->b, PAL_CLUT_SIZE, (uint)leastDist);
}
