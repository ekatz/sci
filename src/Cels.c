#include "Cels.h"
#include "Graphics.h"
#include "Palette.h"
#include "Window.h"

// Maximum "normal" priority.
#define MAXPRI         0x10

#define REPEATC        0x80
#define REPSKIP        0x40
#define UNIQUE         0x00
#define REMAPOFFSETINC 0xff

static int       s_endX      = 0;
static int       s_endY      = 0;
static int       s_penX      = 0;
static int       s_penY      = 0;
static int       s_hOff      = 0;
static int       s_vOff      = 0;
static uint8_t   s_skip      = 0;
static uint8_t   s_viewFlags = 0;
static RPalette *s_palPtr    = NULL;
static uint      s_absPri    = INVALIDPRI;
static bool      s_mirrored  = false;

// This routine is a utility function to setup the output rectangles and
// priority.
static void DrawSetup(int      x,
                      int      y,
                      Cel     *cel,
                      uint     priority,
                      uint8_t *priComp,
                      uint8_t *priStore);

static void Expand256(Cel *cel);
static void Expand16(Cel *cel);

// The main cel drawing loop for non-scaled views.
static void DrawCelZ(int x, int y, Cel *cel, RPalette *pal, uint priority);

static void DrawCel256(Cel *cel, uint8_t priComp, uint8_t priStore);

// Since there is no direct way to tell when a cel line ends directly from the
// RLE data, it is necessary to decode a line of data to bypass it and advance
// the appropriate pointers. This routine does that for all cel types.
static uint8_t *EatLine(uint8_t *data, int lineWidth);

static uint8_t SkipLine(Cel *cel, int vOffset, int hOffset);

// Draw requested cel of loop on color vMap in priority at X/Y
// offset pt to g_rThePort and clip drawing to the portRect.
void DrawCel(View        *view,
             uint         loopNum,
             uint         celNum,
             const RRect *r,
             uint         priority,
             RPalette    *pal)
{
    Cel    *cel;
    RPoint *pt = (RPoint *)&(r->top);

    cel = GetCelPointer(view, loopNum, celNum);

    if (s_palPtr != NULL) {
        RSetPalette(s_palPtr, PAL_MATCH);
    }

    DrawCelZ(pt->h, pt->v, cel, s_palPtr, priority);
}

void DrawBitMap(int       x,
                int       y,
                bool      mirror,
                Cel      *cel,
                uint      priority,
                RPalette *pal)
{
    s_mirrored  = mirror;
    s_viewFlags = COLOR256;
    DrawCelZ(x, y, cel, pal, priority);
}

uint GetNumLoops(View *view)
{
    return view->numLoops;
}

uint GetNumCels(View *view, uint loopNum)
{
    Loop *loop;

    if (loopNum >= (uint)view->numLoops) {
        loopNum = view->numLoops - 1;
    }
    loop = (Loop *)((byte *)view + view->loopOff[loopNum]);
    return loop->numCels;
}

int GetCelWide(View *view, uint loopNum, uint celNum)
{
    Cel *cel = GetCelPointer(view, loopNum, celNum);
    return cel->xDim;
}

int GetCelHigh(View *view, uint loopNum, uint celNum)
{
    Cel *cel = GetCelPointer(view, loopNum, celNum);
    return cel->yDim;
}

void GetCelRect(View  *view,
                uint   loopNum,
                uint   celNum,
                int    x,
                int    y,
                int    z,
                RRect *rect)
{
    Cel *cel;
    int  off;

    cel = GetCelPointer(view, loopNum, celNum);

    // Offset horizontal position of passed point.
    x -= cel->xDim / 2;

    // xOff is subtracted from this to move it right or left of point.
    off = cel->xOff;

    // If MIRRORED we negate this value to offset in proper direction.
    if (s_mirrored) {
        off = -off;
    }

    x += off;
    rect->left = (int16_t)x;

    // Use this to set right of rectangle.
    rect->right = rect->left + cel->xDim;

    // Now do vertical offset (moves it down) unsigned.
    off = (int)((uint8_t)cel->yOff);

    // The rectangle that encompasses this point has a bottom one line lower.
    // Z represents height above ground so we subtract from Y to move it up.
    y = y - z + 1;
    y += off;
    rect->bottom = (int16_t)y;

    // Now fix top for height of cel.
    rect->top = rect->bottom - cel->yDim;
}

void GetCelRectNative(View      *view,
                      uint       loopNum,
                      uint       celNum,
                      int        x,
                      int        y,
                      int        z,
                      uintptr_t *nrect)
{
    RRect rect;
    GetCelRect(view, loopNum, celNum, x, y, z, &rect);
    RectToNative(&rect, nrect);
}

Cel *GetCelPointer(View *view, uint loopNum, uint celNum)
{
    byte *pal = NULL;
    Loop *loop;

    s_viewFlags = view->vFlags;
    if ((s_viewFlags & COLOR256) != 0) {
        if (view->paletteOffset != 0x100 && view->paletteOffset != 0) {
            pal = (byte *)view + view->paletteOffset;
        }
    }
    s_palPtr = (RPalette *)pal;

    if (loopNum >= (uint)view->numLoops) {
        loopNum = view->numLoops - 1;
    }
    s_mirrored = ((uint)view->mirrorBits & (1U << loopNum)) != 0;
    if (s_mirrored) {
        --loopNum;
    }
    loop = (Loop *)((byte *)view + view->loopOff[loopNum]);

    if (celNum >= (uint)loop->numCels) {
        celNum = loop->numCels - 1;
    }

    return (Cel *)((byte *)view + loop->celOff[celNum]);
}

bool RIsItSkip(View *view, uint loopNum, uint celNum, int vOffset, int hOffset)
{
    Cel    *cel;
    uint8_t color;

    cel   = GetCelPointer(view, loopNum, celNum);
    color = SkipLine(cel, vOffset, hOffset);
    return (color == cel->skipColor);
}

static void DrawSetup(int      x,
                      int      y,
                      Cel     *cel,
                      uint     priority,
                      uint8_t *priComp,
                      uint8_t *priStore)
{
    x += g_rThePort->origin.h;
    s_penX = x;
    s_hOff = x;
    y += g_rThePort->origin.v;
    s_penY = y;

    g_theRect.top    = g_rThePort->portRect.top + g_rThePort->origin.v;
    g_theRect.bottom = g_rThePort->portRect.bottom + g_rThePort->origin.v;
    g_theRect.left   = g_rThePort->portRect.left + g_rThePort->origin.h;
    g_theRect.right  = g_rThePort->portRect.right + g_rThePort->origin.h;

    // Build priority compare and store values.
    s_absPri = priority;
    if (priority < MAXPRI) {
        *priStore = (uint8_t)(priority << 4);

        // F in low nibble ensures it will be higher than any control.
        *priComp = *priStore | 0x0f;
    } else {
        *priStore = 0xff;
        *priComp  = 0xff;
    }

    s_skip = cel->skipColor;
    s_endX = cel->xDim;
    s_endY = cel->yDim;

    if ((s_viewFlags & COLOR256) != 0) {
        Expand256(cel);
    } else {
        assert(0);
        Expand16(cel);
    }
}

static void Expand256(Cel *cel)
{
    uint8_t *dataBegin;
    uint8_t *data;
    uint8_t *buf;
    uint8_t *bufEnd;
    uint8_t  dbyte;
    uint     run;
    uint     num;
    uint     width, height;

    if (s_mirrored) {
        if (cel->compressType != 0) {
            return;
        }
    } else {
        if (cel->compressType == 0) {
            return;
        }
    }

    cel->compressType = ~cel->compressType;

    width  = (uint)cel->xDim;
    bufEnd = (uint8_t *)alloca(width * 2);
    bufEnd += width * 2;
    dataBegin = cel->data;

    for (height = (uint)cel->yDim; height != 0; --height) {
        data = dataBegin;
        buf  = bufEnd;

        num = 0;
        while (num < width) {
            // Get the control character from the control buffer.
            dbyte = *data++;
            run   = (uint)(dbyte & ~(REPEATC | REPSKIP));
            num += run;

            // Repeat same byte.
            if ((dbyte & REPEATC) != 0) {
                if ((dbyte & REPSKIP) == 0) {
                    // Get the color byte and point to next color.
                    *--buf = *data++;
                }
            }
            // Unique bytes.
            else {
                while (run != 0) {
                    *--buf = *data++;
                    --run;
                }
            }

            *--buf = dbyte;
        }

        num = (uint)(bufEnd - buf);
        memcpy(dataBegin, buf, num);
        dataBegin += num;
    }
}

static void Expand16(Cel *cel)
{
    uint8_t *data;
    uint8_t *buf;
    uint8_t  dbyte;
    uint     width, height;
    uint     num, i;

    if (s_mirrored) {
        if (cel->compressType != 0) {
            return;
        }
    } else {
        if (cel->compressType == 0) {
            return;
        }
    }

    width = (uint)cel->xDim;
    buf   = (uint8_t *)alloca(width * 2);
    data  = cel->data;

    for (height = (uint)cel->yDim; height != 0; --height) {
        i   = 0;
        num = 0;
        while (num < width) {
            dbyte  = data[i];
            buf[i] = dbyte;
            ++i;

            // Repeat count in the high nibble.
            num += dbyte >> 4;
        }

        while (i != 0) {
            --i;
            *data++ = buf[i];
        }
    }
}

// ax = x   bx = y  dx = priority   si = cel    di = pal    es = viewSeg
static void DrawCelZ(int x, int y, Cel *cel, RPalette *pal, uint priority)
{
    uint8_t priComp, priStore;

    s_palPtr = pal;

    DrawSetup(x, y, cel, priority, &priComp, &priStore);

    if ((s_viewFlags & COLOR256) != 0) {
        DrawCel256(cel, priComp, priStore);
    } else {
        assert(0);
        // DrawCel16(cel);
    }
}

static void DrawCel256(Cel *cel, uint8_t priComp, uint8_t priStore)
{
    uint8_t *data;
    uint8_t  dbyte, num;
    uint8_t  priMask;
    uint     base;
    int      runX, penX;

    // DrawSetup256
    if (s_absPri == INVALIDPRI) {
        priStore = 0;
        priMask  = 0xff;
    } else {
        priMask = 0x0f;
    }

    data = cel->data;

    for (; s_endY > 0; ++s_penY, --s_endY) {
        runX = s_endX;
        penX = s_penX;

        if (s_penY >= (int)g_theRect.bottom) {
            break;
        }

        if (s_penY >= (int)g_theRect.top) {
            // StandardLine draws the decoded RLE buffer from aLine to the
            // virtual screen for standard non-scaled views (both normal and
            // mirrored). StandardLine

            //////////////////////////////////////////////////////////////////////////

            // Get an offset for the line from a stored table.
            base = g_baseTable[s_penY];
            // Plus offset to first pixel to be drawn.
            base += (uint)s_hOff;

            while (runX > 0) {
                num = *data++;
                if ((num & REPEATC) != 0) {
                    if ((num & REPSKIP) == 0) {
                        dbyte = *data++;
                        if (s_palPtr != NULL) {
                            dbyte = s_palPtr->mapTo[dbyte];
                        }

                        num &= ~(REPEATC | REPSKIP);

                        while (num != 0) {
                            if (penX >= (int)g_theRect.right) {
                                break;
                            }

                            if (penX >= (int)g_theRect.left) {
                                if (priComp >= g_pcHndl[base]) {
                                    g_pcHndl[base] =
                                      (g_pcHndl[base] & priMask) | priStore;
                                    g_vHndl[base] = dbyte;
                                }
                            }

                            base++;
                            runX--;
                            penX++;
                            num--;
                        }
                    } else {
                        num &= ~(REPEATC | REPSKIP);
                    }
                } else {
                    while (num != 0) {
                        if (penX >= (int)g_theRect.right) {
                            break;
                        }

                        dbyte = *data++;
                        if (dbyte != s_skip) {
                            if (penX >= (int)g_theRect.left) {
                                if (priComp >= g_pcHndl[base]) {
                                    g_pcHndl[base] =
                                      (g_pcHndl[base] & priMask) | priStore;

                                    if (s_palPtr != NULL) {
                                        dbyte = s_palPtr->mapTo[dbyte];
                                    }

                                    g_vHndl[base] = dbyte;
                                }
                            }
                        }

                        base++;
                        runX--;
                        penX++;
                        num--;
                    }

                    data += num;
                }

                runX -= (int)num;
                penX += (int)num;
                base += (uint)num;
            }
            //////////////////////////////////////////////////////////////////////////
        } else {
            // Won't be output so just advance indexes & counters
            data = EatLine(data, runX);
        }
    }
}

static uint8_t *EatLine(uint8_t *data, int lineWidth)
{
    uint8_t dbyte;
    int     runX;

    for (runX = lineWidth; runX > 0; runX -= (int)dbyte) {
        // Get the control character from the control buffer.
        dbyte = *data++;

        // Decode RLE data.
        if ((dbyte & REPEATC) != 0) {
            // It is either repeated color or repeated skip.
            if ((dbyte & REPSKIP) == 0) {
                data++;
            }

            // Here we have a repeat color situation.
            dbyte &= ~(REPEATC | REPSKIP);
        } else {
            // Do run of unique bytes.
            data += dbyte; // use up dbyte color bytes.
        }
    }
    return data;
}

static uint8_t SkipLine(Cel *cel, int vOffset, int hOffset)
{
    uint8_t *data;
    uint8_t  dbyte = 0, num;
    int      run;

    run  = ((int)cel->xDim * vOffset) + hOffset;
    data = cel->data;

    while (run > 0) {
        dbyte = *data++;
        num   = dbyte & ~(REPEATC | REPSKIP);
        run -= (int)num;

        if ((dbyte & (REPEATC | REPSKIP)) != 0) {
            if ((dbyte & REPSKIP) == 0) {
                data++;
            }
        } else {
            data += num;
        }
    }

    return ((dbyte & REPSKIP) != 0) ? cel->skipColor : 0xff;
}
