#include "sci/Kernel/Palette.h"
#include "sci/Kernel/Graphics.h"
#include "sci/Kernel/Kernel.h"
#include "sci/Kernel/Picture.h"
#include "sci/Kernel/Resource.h"
#include "sci/PMachine/PMachine.h"
#include "sci/Utils/Timer.h"

static struct PaletteCycleStruct {
    uint   timestamp[MAXPALCYCLE];
    int8_t start[MAXPALCYCLE];
} s_palCycleTable;

RPalette g_sysPalette = { 0 };

// Integrate this palette into the current system palette observing the various
// rules of combination expressed by the "flags" element of each palette color
// value & mode.
static void InsertPalette(RPalette *srcPal, RPalette *dstPal, int mode);

// Given R/G/B values, returns the index of this color.
static uint Match(const Guns *theGun, const RPalette *pal, int leastDist);

static void ResetPaletteFlags(RPalette *pal,
                              uint      first,
                              uint      last,
                              uint8_t   flags);
static void SetPaletteFlags(RPalette *pal,
                            uint      first,
                            uint      last,
                            uint8_t   flags);

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

uint8_t PalMatch(uint8_t r, uint8_t g, uint8_t b)
{
    Guns aGun;

    aGun.r = r;
    aGun.g = g;
    aGun.b = b;
    return (uint8_t)Match(&aGun, &g_sysPalette, -1L);
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

static void ResetPaletteFlags(RPalette *pal,
                              uint      first,
                              uint      last,
                              uint8_t   flags)
{
    uint i;

    for (i = first; i < last; i++) {
        // **********************
        // Don't know if this is true yet
        // if (PAL_IN_USE & flags && pal->gun[i].flags & PAL_MATCHED)
        //
        pal->gun[i].flags &= ~flags;
    }
}

static void SetPaletteFlags(RPalette *pal, uint first, uint last, uint8_t flags)
{
    uint i;

    for (i = first; i < last; i++) {
        pal->gun[i].flags |= flags;
    }
}

void KPalette(argList)
{
#define ret(val) g_acc = ((uintptr_t)(val))

    Guns      aGun;
    RPalette *pal;
    uint      count;
    int       index, replace;
    int8_t    start;
    uint      ticks;

    switch (arg(1)) {
        // Gets a palette resource number.
        case PALLoad:
            SetResPalette((uint)arg(2), (int)arg(3));
            break;

            // Gets a start, end, & bits to set in sysPalette.
        case PALSet:
            SetPaletteFlags(
              &g_sysPalette, (uint)arg(2), (uint)arg(3), (uint8_t)arg(4));
            break;

            // Gets a start, end, & bits to reset in sysPalette
        case PALReset:
            ResetPaletteFlags(
              &g_sysPalette, (uint)arg(2), (uint)arg(3), (uint8_t)arg(4));
            break;

            // Gets a start, end and intensity.
        case PALIntensity:
            SetPalIntensity(
              &g_sysPalette, (int)arg(2), (int)arg(3), (int)arg(4));
            if (argCount < 5 || arg(5) == FALSE) {
                SetCLUT(&g_sysPalette);
            }
            break;

        // Given R/G/B values, returns the index of this color.
        case PALMatch:
            aGun.r = (uint8_t)arg(2);
            aGun.g = (uint8_t)arg(3);
            aGun.b = (uint8_t)arg(4);
            ret(Match(&aGun, &g_sysPalette, -1));
            break;

        case PALCycle:
            count = (uint)-1;
            while ((count += 3) < argCount) {
                start = (int8_t)arg(count);
                // Is range passed already in palCycleTable?
                for (index = 0; index < MAXPALCYCLE; index++) {
                    if (s_palCycleTable.start[index] == start) {
                        break;
                    }
                }
                // No--replace oldest range with new range.
                if (s_palCycleTable.start[index] != start) {
                    ticks   = 0x7fffffff;
                    replace = 0;
                    for (index = 0; index < MAXPALCYCLE; index++) {
                        if (s_palCycleTable.timestamp[index] < ticks) {
                            ticks   = s_palCycleTable.timestamp[index];
                            replace = index;
                        }
                    }
                    index                        = replace;
                    s_palCycleTable.start[index] = start;
                }
                // Is it time to cycle yet?
                if ((RTickCount() - s_palCycleTable.timestamp[index]) >=
                    (uint)abs((int)arg(count + 2))) {
                    s_palCycleTable.timestamp[index] = RTickCount();
                    if ((int)arg(count + 2) > 0) {
                        // Cycle forward.
                        index = start;
                        aGun  = g_sysPalette.gun[index];
                        for (index++; index < (int)arg(count + 1); index++) {
                            g_sysPalette.gun[index - 1] =
                              g_sysPalette.gun[index];
                        }
                        g_sysPalette.gun[index - 1] = aGun;
                    } else {
                        // Cycle reverse.
                        index = (int)arg(count + 1) - 1;
                        aGun  = g_sysPalette.gun[index];
                        for (; index > (int)start; index--) {
                            g_sysPalette.gun[index] =
                              g_sysPalette.gun[index - 1];
                        }
                        g_sysPalette.gun[index] = aGun;
                    }
                }
            }
            // Actually set the palette.
            SetCLUT(&g_sysPalette);
            break;

        // Copies systemPalette into a new pointer and returns pointer.
        case PALSave:
            pal = (RPalette *)malloc(sizeof(RPalette));
            if (pal != NULL) {
                memset(pal, 0, sizeof(RPalette));
                GetCLUT(pal);
            }
            ret(pal);
            break;

        // Copies saved palette to systemPalette and disposes pointer.
        case PALRestore:
            pal = (RPalette *)arg(2);
            if (pal != NULL) {
                RSetPalette(pal, PAL_REPLACE);
                free(pal);
            }
            ret(pal);
            break;
    }
}
