#include "Picture.h"
#include "Graphics.h"
#include "Menu.h"
#include "Palette.h"
#include "Window.h"

#define xOrg 160
#define yOrg 95

#define WIPEWIDE 8
#define WIPEHIGH 5
#define SCRNHIGH 190
#define SCRNWIDE 320
#define XWIPES   40
#define YWIPES   40

uint     g_picNotValid = 0;
RWindow *g_picWind     = NULL;
RRect    g_picRect     = { BARSIZE, 0, MAXHEIGHT, MAXWIDTH };

uint g_showPicStyle = 0;
uint g_showMap      = VMAP;

static uint8_t s_priTable[200] = { 0 };
static int     s_priHorizon = 0, s_priBottom = 0;

static void HWipe(int wipeDir, uint mapSet, bool black);
static void VWipe(int wipeDir, uint mapSet, bool black);
static void Iris(int dir, uint mapSet, bool black);
static void Dissolve(uint mapSet, bool black);
static void Shutter(int hDir, int vDir, uint mapSet, bool black);
static void ShowBlackBits(RRect *r, uint mapSet);

void InitPicture(void)
{
    PriTable(42, SCRNHIGH);
}

void ShowPic(uint mapSet, uint style)
{
    RRect      r;
    RGrafPort *oldPort;
    int        i;
    RPalette   workPalette;

    RGetPort(&oldPort);
    RSetPort(&g_picWind->port);

    RHideCursor();

    switch (style & ~PICFLAGS) {
        case PIXELDISSOLVE:
            if ((style & BLACKOUT) != 0) {
                Dissolve((mapSet | PDFLAG | PDBOFLAG), true);
            }
            SetCLUT(&g_sysPalette);
            Dissolve((mapSet | PDFLAG), false);
            break;

        case DISSOLVE:
            if ((style & BLACKOUT) != 0) {
                Dissolve(mapSet, true);
            }
            SetCLUT(&g_sysPalette);
            Dissolve(mapSet, false);
            break;

        case IRISOUT:
            if ((style & BLACKOUT) != 0) {
                Iris(-1, mapSet, true);
            }
            SetCLUT(&g_sysPalette);
            Iris(1, mapSet, 0);
            break;

        case IRISIN:
            if ((style & BLACKOUT) != 0) {
                Iris(1, mapSet, true);
            }
            SetCLUT(&g_sysPalette);
            Iris(-1, mapSet, 0);
            break;

        case WIPERIGHT:
            if ((style & BLACKOUT) != 0) {
                HWipe(-WIPEWIDE, mapSet, true);
            }
            SetCLUT(&g_sysPalette);
            HWipe(WIPEWIDE, mapSet, false);
            break;

        case WIPELEFT:
            if ((style & BLACKOUT) != 0) {
                HWipe(WIPEWIDE, mapSet, true);
            }
            SetCLUT(&g_sysPalette);
            HWipe(-WIPEWIDE, mapSet, false);
            break;

        case WIPEDOWN:
            if ((style & BLACKOUT) != 0) {
                VWipe(-WIPEHIGH, mapSet, true);
            }
            SetCLUT(&g_sysPalette);
            VWipe(WIPEHIGH, mapSet, false);
            break;

        case WIPEUP:
            if ((style & BLACKOUT) != 0) {
                VWipe(WIPEHIGH, mapSet, true);
            }
            SetCLUT(&g_sysPalette);
            VWipe(-WIPEHIGH, mapSet, false);
            break;

        case HSHUTTER:
            if ((style & BLACKOUT) != 0) {
                Shutter(-WIPEWIDE, 0, mapSet, true);
            }
            SetCLUT(&g_sysPalette);
            Shutter(WIPEWIDE, 0, mapSet, false);
            break;

        case VSHUTTER:
            if ((style & BLACKOUT) != 0) {
                Shutter(0, -WIPEHIGH, mapSet, true);
            }
            SetCLUT(&g_sysPalette);
            Shutter(0, WIPEHIGH, mapSet, false);
            break;

        case FADEOUT:
            // Fade out current palette.
            GetCLUT(&workPalette);
            for (i = MAX_INTENSITY; i >= 0; i -= 10) {
                SetPalIntensity(&workPalette, 1, 255, i);
                SetCLUT(&workPalette);
            }

            // Change the visible data.
            SetPalIntensity(&g_sysPalette, 0, 256, 0);
            SetCLUT(&g_sysPalette);
            RSetRect(&r,
                     0,
                     0,
                     g_rThePort->portRect.right,
                     g_rThePort->portRect.bottom);
            ShowBits(&r, mapSet);

            // Now ramp-up and assert system palette.
            for (i = 10; i <= MAX_INTENSITY; i += 10) {
                SetPalIntensity(&g_sysPalette, 0, 256, i);
                SetCLUT(&g_sysPalette);
            }
            break;

        case SCROLLRIGHT:
        case SCROLLLEFT:
        case SCROLLUP:
        case SCROLLDOWN:
            SetCLUT(&g_sysPalette);
            ShiftScreen(g_rThePort->portRect.top + g_rThePort->origin.v,
                        g_rThePort->portRect.left + g_rThePort->origin.h,
                        g_rThePort->portRect.bottom + g_rThePort->origin.v,
                        g_rThePort->portRect.right + g_rThePort->origin.h,
                        (int)(style & ~PICFLAGS) - SCROLLRIGHT);
            break;

        // Simply draw the whole screen.
        default:
            SetCLUT(&g_sysPalette);
            RSetRect(&r,
                     0,
                     0,
                     g_rThePort->portRect.right,
                     g_rThePort->portRect.bottom);
            ShowBits(&r, mapSet);
            break;
    }

    RShowCursor();

    RSetPort(oldPort);
}

uint CoordPri(int y)
{
    if (y >= s_priBottom) {
        y = s_priBottom - 1;
    }
    return (int)s_priTable[y];
}

int PriCoord(uint p)
{
    int i;

    if (p >= 15) {
        return s_priBottom;
    }

    for (i = 0; i < s_priBottom; ++i) {
        if (s_priTable[i] >= (uint8_t)p) {
            break;
        }
    }
    return i;
}

void PriTable(int x, int y)
{
    int priBands;
    int priority;
    int n;

    s_priBottom  = y;
    s_priHorizon = x;

    priBands = ((y - x) * 2000) / 14;

    for (n = 0; n < 200; ++n) {
        if (n < x) {
            priority = 0;
        } else {
            priority = (((n - x) * 2000) / priBands) + 1;
            if ((uint)priority > 14) {
                priority = 14;
            }
        }

        s_priTable[n] = (uint8_t)priority;
    }
}

void PriBands(uint16_t *priPtr)
{
    int  priBands = 14;
    int  priority = 0;
    uint n        = 0;

    while (priority < priBands) {
        while (n < (uint)*priPtr) {
            s_priTable[n] = (uint8_t)priority;
            ++n;
        }
        ++priPtr;
        ++priority;
    }

    while (n < 200) {
        s_priTable[n] = (uint8_t)priority;
        ++n;
    }

    s_priBottom  = 190;
    s_priHorizon = PriCoord(1);
}

static void HWipe(int wipeDir, uint mapSet, bool black)
{
    RRect r;
    int   i;
    ulong ticks;

    RSetRect(&r, 0, 0, WIPEWIDE, g_rThePort->portRect.bottom);
    if (wipeDir < 0) {
        ROffsetRect(&r, g_rThePort->portRect.right - WIPEWIDE, 0);
    }

    for (i = 0; i < XWIPES; ++i) {
        if (black) {
            ShowBlackBits(&r, mapSet);
        } else {
            ShowBits(&r, mapSet);
        }
        ROffsetRect(&r, wipeDir, 0);

#ifndef __WINDOWS__
        // Wait until enough time has passed
        ticks = RTickCount();
        while (ticks == RTickCount())
            ;
#endif
    }
}

static void VWipe(int wipeDir, uint mapSet, bool black)
{
    RRect r;
    int   i;
    ulong ticks;

    RSetRect(&r, 0, 0, g_rThePort->portRect.right, WIPEHIGH);
    if (wipeDir < 0) {
        ROffsetRect(&r, 0, g_rThePort->portRect.bottom - WIPEHIGH);
    }

    for (i = 0; i < YWIPES; ++i) {
        if (black) {
            ShowBlackBits(&r, mapSet);
        } else {
            ShowBits(&r, mapSet);
        }
        ROffsetRect(&r, wipeDir, 0);

#ifndef __WINDOWS__
        // Wait until enough time has passed.
        ticks = RTickCount();
        while (ticks == RTickCount())
            ;
#endif
    }
}

static void Iris(int dir, uint mapSet, bool black)
{
    RRect r;
    int   i, j, done;
    ulong ticks;

    if (dir > 0) {
        i = 1;
    } else {
        i = XWIPES / 2;
    }

    for (done = 0; done < (XWIPES / 2); ++done) {
        for (j = 0; j < 4; ++j) {
            // Make the rectangle for this show.
            switch (j) {
                case 0: // top bar
                    RSetRect(&r, 0, 0, WIPEWIDE * i * 2, WIPEHIGH);
                    MoveRect(&r, xOrg - (i * WIPEWIDE), yOrg - (i * WIPEHIGH));
                    break;

                case 1: // left bar
                    RSetRect(&r, 0, 0, WIPEWIDE, WIPEHIGH * (2 * (i - 1)));
                    MoveRect(&r,
                             xOrg - (i * WIPEWIDE),
                             yOrg - (i * WIPEHIGH) + WIPEHIGH);
                    break;

                case 2: // bottom bar
                    RSetRect(&r, 0, 0, WIPEWIDE * i * 2, WIPEHIGH);
                    MoveRect(&r,
                             xOrg - (i * WIPEWIDE),
                             yOrg + (i * WIPEHIGH) - WIPEHIGH);
                    break;

                case 3: // right bar
                    RSetRect(&r, 0, 0, WIPEWIDE, WIPEHIGH * (2 * (i - 1)));
                    MoveRect(&r,
                             xOrg + (i * WIPEWIDE) - WIPEWIDE,
                             yOrg - (i * WIPEHIGH) + WIPEHIGH);
                    break;
            }

            if (black) {
                ShowBlackBits(&r, mapSet);
            } else {
                ShowBits(&r, mapSet);
            }
        }
        i += dir;

#ifndef __WINDOWS__
        // Wait until enough time has passed.
        ticks = RTickCount();
        while (ticks == RTickCount())
            ;
#endif
    }
}

// Bring in random rectangles or pixels.
static void Dissolve(uint mapSet, bool black)
{
#define RANDMASK 0x240

    int   i, j, x, y, rand = 1234;
    RRect r;
    ulong ticks;

    if (mapSet & PDFLAG) {
        RSetRect(&r,
                 g_rThePort->portRect.left,
                 g_rThePort->portRect.top,
                 g_rThePort->portRect.right,
                 g_rThePort->portRect.bottom);
        ShowBits(&r, mapSet);
        return;
    }

    rand &= RANDMASK;

    for (j = 0; j < 64; ++j) {
        for (i = 0; i < 16; ++i) {
            if (rand & 1) {
                rand /= 2;
                rand ^= RANDMASK;
            } else {
                rand /= 2;
            }

            // Use rand to pick a rectangle.
            x = rand % 40;
            y = rand / 40;
            RSetRect(&r, x * 8, y * 8, (x * 8) + 8, (y * 8) + 8);
            if (black) {
                ShowBlackBits(&r, mapSet);
            } else {
                ShowBits(&r, mapSet);
            }
        }

#ifndef __WINDOWS__
        // Wait until enough time has passed.
        ticks = RTickCount();
        while (ticks == RTickCount())
            ;
#endif
    }

    // Do rectangle 0.
    RSetRect(&r, 0 * 8, 0 * 8, (0 * 8) + 8, (0 * 8) + 8);
    if (black) {
        ShowBlackBits(&r, mapSet);
    } else {
        ShowBits(&r, mapSet);
    }
}

static void Shutter(int hDir, int vDir, uint mapSet, bool black)
{
    RRect r1, r2;
    int   i;
    ulong ticks;

    if (hDir != 0) {
        // Horizontal shutter.
        RSetRect(&r1, 0, 0, WIPEWIDE, g_rThePort->portRect.bottom);
        RSetRect(&r2, 0, 0, WIPEWIDE, g_rThePort->portRect.bottom);
        if (hDir > 0) {
            MoveRect(&r1, xOrg - WIPEWIDE, 0);
            MoveRect(&r2, xOrg, 0);
        } else {
            MoveRect(&r2, g_rThePort->portRect.right - WIPEWIDE, 0);
        }
    } else {
        RSetRect(&r1, 0, 0, g_rThePort->portRect.right, WIPEHIGH);
        RSetRect(&r2, 0, 0, g_rThePort->portRect.right, WIPEHIGH);
        if (vDir > 0) {
            MoveRect(&r1, 0, yOrg - WIPEHIGH);
            MoveRect(&r2, 0, yOrg);
        } else {
            MoveRect(&r2, 0, g_rThePort->portRect.bottom - WIPEHIGH);
        }
    }

    for (i = 0; i < (XWIPES / 2); ++i) {
        if (black) {
            ShowBlackBits(&r1, mapSet);
            ShowBlackBits(&r2, mapSet);
        } else {
            ShowBits(&r1, mapSet);
            ShowBits(&r2, mapSet);
        }

        // Move the rectangles.
        ROffsetRect(&r1, -hDir, -vDir);
        ROffsetRect(&r2, hDir, vDir);

#ifndef __WINDOWS__
        // Wait until enough time has passed.
        ticks = RTickCount();
        while (ticks == RTickCount())
            ;
#endif
    }
}

static void ShowBlackBits(RRect *r, uint mapSet)
{
    Handle saveBits;

    saveBits = SaveBits(r, mapSet);
    RFillRect(r, mapSet, 0, 0, 0);
    ShowBits(r, mapSet);
    RestoreBits(saveBits);
}
