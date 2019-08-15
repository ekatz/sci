#include "Graphics.h"
#include "Animate.h"
#include "Cels.h"
#include "Display.h"
#include "ErrMsg.h"
#include "Kernel.h"
#include "PMachine.h"
#include "Picture.h"
#include "Resource.h"
#include "Window.h"

#define BRUSHSIZE_MASK 7

// Rectangular brush bit.
#define RECTBRUSH 0x10

// Airbrush bit in brushSize.
#define AIRBRUSH  0x20

#define LOWCODE   0xf0
#define ENDCODE   0xff

#define PENOFF    0xff

// Pixel masks for virtual bitmaps.
#define ODDON     0x0f
#define ODDOFF    0xf0
#define EVENON    ODDOFF
#define EVENOFF   ODDON

// Bytes per scanline.
#define VROWBYTES MAXWIDTH

static void SetColor(void);
static void ClearColor(void);
static void SetPriority(void);
static void ClearPriority(void);
static void DoShortBrush(void);
static void DoMedLine(void);
static void DoAbsLine(void);
static void DoShortLine(void);
static void DoAbsFill(void);
static void SetBrSize(void);
static void DoAbsBrush(void);
static void SetControl(void);
static void ClearControl(void);
static void DoMedBrush(void);
static void DoSpecial(void);

static void (*s_funcTable[])() = {
    SetColor,      // 0xf0
    ClearColor,    // 0xf1
    SetPriority,   // 0xf2
    ClearPriority, // 0xf3
    DoShortBrush,  // 0xf4
    DoMedLine,     // 0xf5
    DoAbsLine,     // 0xf6
    DoShortLine,   // 0xf7
    DoAbsFill,     // 0xf8
    SetBrSize,     // 0xf9
    DoAbsBrush,    // 0xfa
    SetControl,    // 0xfb
    ClearControl,  // 0xfc
    DoMedBrush,    // 0xfd
    DoSpecial      // 0xfe
};

static void OldSetPalette(void);
static void DoBitMap(void);
static void SetCLUTPalette(void);
static void SetPriTable(void);
static void SetPriBands(void);

static void (*s_specTable[])() = {
    OldSetPalette,  // 0x00
    DoBitMap,       // 0x01
    SetCLUTPalette, // 0x02
    SetPriTable,    // 0x03
    SetPriBands     // 0x04
};

static uint8_t s_oldColorTable[] = { 0x00, 0x32, 0x22, 0x2A, 0x0A, 0x3A, 0x12,
                                     0x05, 0x03, 0x35, 0x25, 0x2D, 0x0D, 0x3D,
                                     0x1D, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44,
                                     0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,
                                     0xCC, 0xDD, 0xEE, 0xFF };

static uint16_t s_brushPattern0[] = {
    0x4000 // 0100000000000000b
};

static uint16_t s_brushPattern1[] = {
    0x2000, // 0010000000000000b
    0x7000, // 0111000000000000b
    0x2000  // 0010000000000000b
};

static uint16_t s_brushPattern2[] = {
    0x3800, // 0011100000000000b
    0x7C00, // 0111110000000000b
    0x7C00, // 0111110000000000b
    0x7C00, // 0111110000000000b
    0x3800  // 0011100000000000b
};

static uint16_t s_brushPattern3[] = {
    0x1C00, // 0001110000000000b
    0x3E00, // 0011111000000000b
    0x7F00, // 0111111100000000b
    0x7F00, // 0111111100000000b
    0x7F00, // 0111111100000000b
    0x3E00, // 0011111000000000b
    0x1C00  // 0001110000000000b
};

static uint16_t s_brushPattern4[] = {
    0x0E00, // 0000111000000000b
    0x3F80, // 0011111110000000b
    0x7FC0, // 0111111111000000b
    0x7FC0, // 0111111111000000b
    0x7FC0, // 0111111111000000b
    0x7FC0, // 0111111111000000b
    0x7FC0, // 0111111111000000b
    0x3F80, // 0011111110000000b
    0x0E00  // 0000111000000000b
};

static uint16_t s_brushPattern5[] = {
    0x0700, // 0000011100000000b
    0x1FC0, // 0001111111000000b
    0x3FE0, // 0011111111100000b
    0x3FE0, // 0011111111100000b
    0x7FF0, // 0111111111110000b
    0x7FF0, // 0111111111110000b
    0x7FF0, // 0111111111110000b
    0x3FE0, // 0011111111100000b
    0x3FE0, // 0011111111100000b
    0x1FC0, // 0001111111000000b
    0x0F80, // 0000111110000000b
    0x0700  // 0000011100000000b
};

static uint16_t s_brushPattern6[] = {
    0x07C0, // 0000011111000000b
    0x1FF0, // 0001111111110000b
    0x3FF8, // 0011111111111000b
    0x3FF8, // 0011111111111000b
    0x7FFC, // 0111111111111100b
    0x7FFC, // 0111111111111100b
    0x7FFC, // 0111111111111100b
    0x7FFC, // 0111111111111100b
    0x7FFC, // 0111111111111100b
    0x3FF8, // 0011111111111000b
    0x3FF8, // 0011111111111000b
    0x1FF0, // 0001111111110000b
    0x07C0  // 0000011111000000b
};

static uint16_t s_brushPattern7[] = {
    0x03E0, // 0000001111100000b
    0x0FF8, // 0000111111111000b
    0x1FFC, // 0001111111111100b
    0x3FFE, // 0011111111111110b
    0x3FFE, // 0011111111111110b
    0x7FFF, // 0111111111111111b
    0x7FFF, // 0111111111111111b
    0x7FFF, // 0111111111111111b
    0x7FFF, // 0111111111111111b
    0x7FFF, // 0111111111111111b
    0x3FFE, // 0011111111111110b
    0x3FFE, // 0011111111111110b
    0x1FFC, // 0001111111111100b
    0x0FF8, // 0000111111111000b
    0x03E0  // 0000001111100000b
};

static uint16_t *s_burshPatterns[] = {
    s_brushPattern0,
    s_brushPattern1,
    s_brushPattern2,
    s_brushPattern3,
    s_brushPattern4,
    s_brushPattern5,
    s_brushPattern6,
    s_brushPattern7
};

// Handles to virtual bitmaps (save to dispose of at endgraph).
uint8_t *g_vHndl  = NULL;
uint8_t *g_pcHndl = NULL;

// Working variables for Line.
static int s_penY = 0;
static int s_penX = 0;
static int s_endY = 0;
static int s_endX = 0;

// Active vMaps.
static uint s_mapSet = 0;

static uint s_brushSize = 0;

static bool s_oldCode = false;

static uint8_t  *s_picPtr = NULL; // Picture data pointer
static RPalette *s_palPtr = NULL; // Palette data pointer

static int s_lineStartY = 0;
static int s_lineStartX = 0;
static int s_lineEndY   = 0;
static int s_lineEndX   = 0;

static uint8_t s_vColor = 0;
static uint8_t s_pColor = 0;
static uint8_t s_cColor = 0;

// Mirroring flags
static bool s_mirrorX = false;
// static bool s_mirrorY = false;

static uint s_picOverlayMask = 0;

static RRect s_bounds = { 0, 0, MAXHEIGHT, MAXWIDTH };

static RGrafPort s_defPort = { 0 };

uint16_t g_baseTable[MAXHEIGHT] = { 0 };

RGrafPort *g_rThePort = NULL;

// Normalized rectangle.
RRect g_theRect = { 0 };

static void InitGraph(void);
static void EndGraph(void);
static void ClearScreen(void);
static void DispatchGrf(void);
static void GetAbsXY(int *x, int *y);
static void GetMedXY(int *x, int *y);
static void GetShortXY(int *x, int *y);
static void DrawLine(int x0, int y0, int x1, int y1);
static void DrawHLine(void);
static void DrawVLine(void);
static void DrawDLine(void);
static void DrawBrush(int x, int y, uint brush, uint spray);
static void Fill(int x, int y);
static void DoFill(int xScan,
                   int leftSide,
                   int rightSide,
                   int prevLeftSide,
                   int prevRightSide,
                   int yScan,
                   int direction);
static void FillUp(int xScan,
                   int leftSide,
                   int rightSide,
                   int oldLeftSide,
                   int oldRightSide,
                   int yScan,
                   int direction);
static void FillDown(int xScan,
                     int leftSide,
                     int rightSide,
                     int oldLeftSide,
                     int oldRightSide,
                     int yScan,
                     int direction);
static bool CheckDot(int penX, uint lineBase);
static int CheckRight(int left, uint lineBase);
static int CheckLeft(int right, uint lineBase);
static uint8_t *GetSegment(uint mapSet);
static void SetMaps(void);
static uint SetBase(void);
static void FastDot(uint base);
static void DoDot(uint base);
static void FillLines(uint base, int left, int right);
static uint SizeRect(const RRect *rect);
static void SOffsetRect(RRect *rect, const RGrafPort *port);
static void GetTheRect(const RRect *rect);
static void StdChar(char cCode);
static void ClearChar(char cCode);

bool CInitGraph(void)
{
    // Initialize the display driver.
    InitDisplay();

    // Call resolution specific initialization.
    InitGraph();

    // Create default port.
    g_rThePort = &s_defPort;
    ROpenPort(g_rThePort);
    return true;
}

void CEndGraph(void)
{
    EndGraph();
    EndDisplay();
}

const RRect *GetBounds(void)
{
    return &s_bounds;
}

void ShowBits(const RRect *rect, uint mapSet)
{
    RRect    showRect;
    uint8_t *hndl;

    if (!RSectRect(rect, &g_rThePort->portRect, &showRect)) {
        return;
    }

    SOffsetRect(&showRect, g_rThePort);

    hndl = GetSegment(mapSet);
    Display(showRect.top,
            showRect.left,
            showRect.bottom,
            showRect.right,
            hndl,
            mapSet);
}

void DrawPic(Handle hndl, bool clear, uint mirror)
{
    g_picNotValid = 1;
    if (clear) {
        // Force 0 priority
        s_picOverlayMask = 0;
        ClearScreen();
    } else {
        // Allow any VALID priority
        s_picOverlayMask = 0xF;
    }

    s_mirrorX = ((mirror & HMIRROR) != 0);
    s_picPtr  = (uint8_t *)hndl;

    s_brushSize = 0;
    s_oldCode   = false;

    // Set default color/priority/control
    s_vColor = 0;
    s_pColor = PENOFF;
    s_cColor = PENOFF;

    DispatchGrf();

    s_mirrorX = false;
}

void LoadBits(uint num)
{
    RPalette *pal;

    g_picNotValid = 1;
    RHideCursor();
    pal = (RPalette *)ResLoad(RES_BITMAP, num);
    RSetPalette(pal, PAL_REPLACE);
    DrawBitMap(0, 0, false, (Cel *)((byte *)pal + PAL_FILE_SIZE), 0, pal);
    RShowCursor();
}

void ROpenPort(RGrafPort *port)
{
    RGrafPort *prevPort;

    prevPort   = g_rThePort;
    g_rThePort = port;

    port->fontNum  = 0;
    port->fontSize = 8;
    RSetFont(0);

    g_rThePort = prevPort;

    port->origin.v = 0;
    port->origin.h = 0;
    port->pnLoc.h  = 0;
    port->pnLoc.v  = 0;
    port->txFace   = 0;
    port->fgColor  = vBLACK;
    port->bkColor  = vWHITE;
    port->pnMode   = SRCOR;

    memcpy(&port->portRect, &s_bounds, sizeof(RRect));
}

void RGetPort(RGrafPort **port)
{
    *port = g_rThePort;
}

void RSetPort(RGrafPort *port)
{
    g_rThePort = port;
}

void RSetOrigin(int x, int y)
{
    g_rThePort->origin.h = (int16_t)(x & (~1));
    g_rThePort->origin.v = (int16_t)y;
}

void RMoveTo(int x, int y)
{
    g_rThePort->pnLoc.h = (int16_t)x;
    g_rThePort->pnLoc.v = (int16_t)y;
}

void RMove(int x, int y)
{
    g_rThePort->pnLoc.h += (int16_t)x;
    g_rThePort->pnLoc.v += (int16_t)y;
}

void RPenMode(uint8_t mode)
{
    g_rThePort->pnMode = mode;
}

void RTextFace(uint face)
{
    g_rThePort->txFace = face;
}

uint GetPointSize(void)
{
    return g_rThePort->fontSize;
}

void SetPointSize(uint size)
{
    g_rThePort->fontSize = size;
}

uint GetFont(void)
{
    return g_rThePort->fontNum;
}

void RSetFont(uint fontNum)
{
    Font *font;

    font = (Font *)ResLoad(RES_FONT, fontNum);
    if (font != NULL) {
        g_rThePort->fontSize = font->pointSize;
        g_rThePort->fontNum  = fontNum;
    }
}

uint RTextWidth(const char *text, int first, uint count)
{
    Font *font;
    uint  i;
    uint  width = 0;

    font = (Font *)ResLoad(RES_FONT, g_rThePort->fontNum);
    text += first;
    for (i = 0; i < count; ++i) {
        char cCode = text[i];
        width += *((uint8_t *)font + font->charRecs[(uint8_t)cCode]);
    }
    return width;
}

uint RCharWidth(char cCode)
{
    Font *font;

    font = (Font *)ResLoad(RES_FONT, g_rThePort->fontNum);
    return ((uint8_t *)font + font->charRecs[(uint8_t)cCode])[0];
}

uint CharHeight(char cCode)
{
    Font *font;

    font = (Font *)ResLoad(RES_FONT, g_rThePort->fontNum);
    return ((uint8_t *)font + font->charRecs[(uint8_t)cCode])[1];
}

void RDrawChar(char cCode)
{
    ClearChar(cCode);
    StdChar(cCode);

    g_rThePort->pnLoc.h += (int16_t)RCharWidth(cCode);
}

void ShowChar(char cCode)
{
    ClearChar(cCode);
    StdChar(cCode);

    g_theRect.left = g_rThePort->pnLoc.h;
    g_rThePort->pnLoc.h += (int16_t)RCharWidth(cCode);
    g_theRect.right  = g_rThePort->pnLoc.h;
    g_theRect.top    = g_rThePort->pnLoc.v;
    g_theRect.bottom = g_rThePort->pnLoc.v + (int16_t)CharHeight(cCode);
    ShowBits(&g_theRect, VMAP);
}

// Draw the character.
static void StdChar(char cCode)
{
    uint16_t theChar;
    uint8_t  color;
    uint     style;
    Font    *font;
    uint     base;

    theChar = (uint16_t)(uint8_t)cCode;
    color   = g_rThePort->fgColor;
    style   = g_rThePort->txFace;

    s_penY = g_rThePort->pnLoc.v + g_rThePort->origin.v;
    s_penX = g_rThePort->pnLoc.h + g_rThePort->origin.h;
    base   = (uint)g_baseTable[s_penY] + (uint)s_penX;

    font = (Font *)ResLoad(RES_FONT, g_rThePort->fontNum);
    if (font->lowChar <= theChar && theChar < font->highChar) {
        uint8_t  cWide, cHigh, cDots, dimMask, pattern;
        uint8_t *data;
        uint8_t *charRecs = (uint8_t *)font + font->charRecs[(uint8_t)cCode];

        cWide = *charRecs++;
        cHigh = *charRecs++;

        while (cHigh != 0) {
            dimMask = 0xff;
            if ((style & DIM) != 0) {
                dimMask &= 0x55;
                if ((s_penY & 1) != 0) {
                    dimMask = ~dimMask;
                }
            }

            pattern = *charRecs++;
            pattern &= dimMask;

            cDots = 0;
            data  = &g_vHndl[base];
            while (true) {
                if ((int8_t)pattern < 0) {
                    *data = color;
                }
                pattern <<=
                  1; // we shift dots out the left and plot left to right.
                data++;
                cDots++;
                if (cDots >= cWide) {
                    break;
                }

                // Check for byte reload.
                if ((cDots & 7) == 0) // 8 bits used?
                {
                    pattern = *charRecs++;
                    pattern &= dimMask;
                }
            }

            // Line of pattern done.
            base += VROWBYTES;

            // See if we have more lines to do.
            s_penY++;
            cHigh--;
        }
    }
}

// If pnMode is SRCCOPY we erase the rectangle (it will occupy width by
// fontSize).
static void ClearChar(char cCode)
{
    if (g_rThePort->pnMode == SRCCOPY) {
        g_theRect.top    = g_rThePort->pnLoc.v;
        g_theRect.bottom = g_theRect.top + (int16_t)g_rThePort->fontSize;
        g_theRect.left   = g_rThePort->pnLoc.h;
        g_theRect.right  = g_rThePort->pnLoc.h + (int16_t)RCharWidth(cCode);
        REraseRect(&g_theRect);
    }
}

void RPaintRect(RRect *rect)
{
    RFillRect(rect, VMAP, g_rThePort->fgColor, 0, 0);
}

void RFillRect(const RRect *rect,
               uint         mapSet,
               uint8_t      color1,
               uint8_t      color2,
               uint8_t      color3)
{
    uint8_t *vMap;
    uint8_t  trMode;
    uint     base;
    uint     inter;
    int      hRun, vRun, i, j;

    // Copy input rectangle to local storage.
    memcpy(&g_theRect, rect, sizeof(RRect));

    // Set local transfer mode.
    trMode = g_rThePort->pnMode;

    // Clip to current portRect.
    if (!RSectRect(&g_theRect, &g_rThePort->portRect, &g_theRect)) {
        return;
    }

    // Make it global.
    SOffsetRect(&g_theRect, g_rThePort);

    // Determine vertical run-count.
    vRun = g_theRect.bottom - g_theRect.top;
    // Determine horizontal run-count.
    hRun = g_theRect.right - g_theRect.left;

    // Set up base address of first line.
    base = g_baseTable[g_theRect.top];
    base += (uint)g_theRect.left;

    // Produces the remainder of byte to advance at end of run.
    inter = (uint)(VROWBYTES - hRun);

    if ((mapSet & VMAP) != 0) {
        vMap = &g_vHndl[base];
        for (i = 0; i < vRun; ++i) {
            if (trMode == srcInvert) {
                for (j = 0; j < hRun; ++j) {
                    if (*vMap == color1) {
                        *vMap = color2;
                    } else if (*vMap == color2) {
                        *vMap = color1;
                    }
                    ++vMap;
                }
            } else {
                memset(vMap, (int)color1, (uint)hRun);
                vMap += (uint)hRun;
            }
            vMap += inter;
        }
    }

    if ((mapSet & (PMAP | CMAP)) == (PMAP | CMAP)) {
        uint8_t pcColor = (color2 << 4) | color3;

        vMap = &g_pcHndl[base];
        for (i = 0; i < vRun; ++i) {
            if (trMode == srcInvert) {
                for (j = 0; j < hRun; ++j) {
                    *vMap ^= pcColor;
                    ++vMap;
                }
            } else {
                memset(vMap, (int)pcColor, (uint)hRun);
                vMap += (uint)hRun;
            }
            vMap += inter;
        }
    } else if ((mapSet & (PMAP | CMAP)) != 0) {
        uint8_t pcColor, mask;
        if ((mapSet & PMAP) != 0) {
            pcColor = color2 << 4;
            mask    = 0x0f;
        } else {
            pcColor = color3;
            mask    = 0xf0;
        }

        vMap = &g_pcHndl[base];
        for (i = 0; i < vRun; ++i) {
            if (trMode == srcInvert) {
                for (j = 0; j < hRun; ++j) {
                    *vMap ^= pcColor;
                    ++vMap;
                }
            } else {
                for (j = 0; j < hRun; ++j) {
                    *vMap = (*vMap & mask) | pcColor;
                    ++vMap;
                }
            }
            vMap += inter;
        }
    }
}

void RInvertRect(RRect *rect)
{
    uint8_t oldMode;

    oldMode            = g_rThePort->pnMode;
    g_rThePort->pnMode = srcInvert;
    RFillRect(rect, VMAP, g_rThePort->fgColor, g_rThePort->bkColor, 0);
    g_rThePort->pnMode = oldMode;
}

void REraseRect(RRect *rect)
{
    RFillRect(rect, VMAP, g_rThePort->bkColor, 0, 0);
}

uint OnControl(uint mapSet, const RRect *rect)
{
    uint8_t *vMap;
    uint     base;
    uint     inter;
    int      hRun, vRun, i, j;
    uint     mask, bit;

    // Get rect into local.
    memcpy(&g_theRect, rect, sizeof(RRect));

    if (!RSectRect(&g_theRect, &g_rThePort->portRect, &g_theRect)) {
        // No rectangle left to check.
        return 0;
    }

    // Make it global.
    SOffsetRect(&g_theRect, g_rThePort);

    // Determine vertical run-count.
    vRun = g_theRect.bottom - g_theRect.top;
    // Determine horizontal run-count.
    hRun = g_theRect.right - g_theRect.left;

    // Set up base address of first line.
    base = g_baseTable[g_theRect.top];
    base += (uint)g_theRect.left;

    // Calculate interline advance value.
    inter = (uint)(VROWBYTES - hRun);

    // Do all the lines.
    mask = 0;
    vMap = &g_pcHndl[base];
    for (i = 0; i < vRun; ++i) {
        // Priority pixel is in high nibble.
        if ((mapSet & PMAP) != 0) {
            for (j = 0; j < hRun; ++j) {
                bit = *vMap >> 4;
                mask |= 1U << bit;
                ++vMap;
            }
        }
        // Control pixel is in low nibble.
        else {
            for (j = 0; j < hRun; ++j) {
                bit = *vMap & ODDON;
                mask |= 1U << bit;
                ++vMap;
            }
        }

        // Advance to next line.
        vMap += inter;
    }
    return mask;
}

Handle SaveBits(const RRect *rect, uint mapSet)
{
    uint     size;
    uint     bytes;
    Handle   hndl;
    uint8_t *bits;
    uint8_t *vMap;
    uint     base;
    uint     inter;
    int      hRun, vRun, i;

    // Address the rectangle.
    GetTheRect(rect);

    // Clip the rectangle.
    if (!RSectRect(&g_theRect, &g_rThePort->portRect, &g_theRect)) {
        return NULL;
    }

    SOffsetRect(&g_theRect, g_rThePort);

    size = SizeRect(&g_theRect);

    bytes = 0;
    if ((mapSet & VMAP) != 0) {
        bytes += size;
    }

    if ((mapSet & (PMAP | CMAP)) != 0) {
        bytes += size;

        // Make sure we do not have an overflow.
        if (bytes < size) {
            Panic(E_SAVEBITS);
        }
    }

    // Header size.
    bytes += sizeof(RRect) + sizeof(uint16_t);

    // Make sure we do not have an overflow.
    if (bytes < (sizeof(RRect) + sizeof(uint16_t))) {
        Panic(E_SAVEBITS);
    }

    hndl = ResLoad(RES_MEM, bytes);
    if (hndl == NULL) {
        Panic(E_SAVEBITS);
    }
    bits = (uint8_t *)hndl;

    // Initialize Header.
    memcpy(bits, &g_theRect, sizeof(RRect));
    bits += sizeof(RRect);

    *((uint16_t *)bits) = (uint16_t)mapSet;
    bits += sizeof(uint16_t);

    vRun = g_theRect.bottom - g_theRect.top;
    hRun = g_theRect.right - g_theRect.left;

    base = g_baseTable[g_theRect.top];
    base += (uint)g_theRect.left;

    // Inter-line advance (difference in width and run-count).
    inter = (uint)(VROWBYTES - hRun);

    if ((mapSet & VMAP) != 0) {
        vMap = &g_vHndl[base];
        for (i = 0; i < vRun; ++i) {
            memcpy(bits, vMap, (uint)hRun);
            bits += (uint)hRun;
            vMap += (uint)hRun + inter;
        }
    }

    if ((mapSet & (PMAP | CMAP)) != 0) {
        vMap = &g_pcHndl[base];
        for (i = 0; i < vRun; ++i) {
            memcpy(bits, vMap, (uint)hRun);
            bits += (uint)hRun;
            vMap += (uint)hRun + inter;
        }
    }

    return hndl;
}

void RestoreBits(Handle hndl)
{
    uint     mapSet;
    RRect   *rect;
    uint8_t *bits;
    uint8_t *vMap;
    uint     base;
    uint     inter;
    int      hRun, vRun, i;

    if (hndl == NULL) {
        return;
    }

    if (FindResEntry(RES_MEM, (size_t)hndl) == NULL) {
        return;
    }

    rect = (RRect *)hndl;
    bits = (uint8_t *)hndl + sizeof(RRect);

    mapSet = *((uint16_t *)bits);
    bits += sizeof(uint16_t);

    vRun = rect->bottom - rect->top;
    hRun = rect->right - rect->left;

    base = g_baseTable[rect->top];
    base += (uint)rect->left;

    // Inter-line advance (difference in width and run-count).
    inter = (uint)(VROWBYTES - hRun);

    if ((mapSet & VMAP) != 0) {
        vMap = &g_vHndl[base];
        for (i = 0; i < vRun; ++i) {
            memcpy(vMap, bits, (uint)hRun);
            bits += (uint)hRun;
            vMap += (uint)hRun + inter;
        }
    }

    if ((mapSet & (PMAP | CMAP)) != 0) {
        vMap = &g_pcHndl[base];
        for (i = 0; i < vRun; ++i) {
            memcpy(vMap, bits, (uint)hRun);
            bits += (uint)hRun;
            vMap += (uint)hRun + inter;
        }
    }

    ResUnLoad(RES_MEM, (size_t)hndl);
}

void UnloadBits(Handle hndl)
{
    if (hndl == NULL) {
        return;
    }

    if (*((uint16_t *)((uint8_t *)hndl + 12)) == 0x4321) {
        // Not implemented in this version!
        assert(0);
    }

    ResUnLoad(RES_MEM, (size_t)hndl);
}

void PenColor(uint8_t color)
{
    g_rThePort->fgColor = color;
}

void RBackColor(uint8_t color)
{
    g_rThePort->bkColor = color;
}

void GetCLUT(RPalette *pal)
{
    GetDisplayCLUT(pal);
}

void SetCLUT(const RPalette *pal)
{
    SetDisplayCLUT(pal);
}

bool RHideCursor(void)
{
    return DisplayCursor(false);
}

bool RShowCursor(void)
{
    return DisplayCursor(true);
}

void ShakeScreen(int cnt, int dir)
{
#ifndef NOT_IMPL
#error Not finished
#endif
}

void ShiftScreen(int rtop, int rleft, int rbot, int rright, int dir){
#ifndef NOT_IMPL
#error Not finished
#endif
}

uint FastMatch(const RPalette *pal,
               uint8_t         redV,
               uint8_t         greenV,
               uint8_t         blueV,
               uint            psize,
               uint            least)
{
    uint sum, i, leastIndex;

    // Default to NO match.
    leastIndex = psize;

    for (i = 0; i < psize; ++i) {
        // Only match active entries.
        if ((pal->gun[i].flags & PAL_IN_USE) != 0) {
            sum = (uint)abs((int)pal->gun[i].r - (int)redV);

            // Need we examine green?
            if (sum <= least) {
                sum += (uint)abs((int)pal->gun[i].g - (int)greenV);

                // Need we examine blue?
                if (sum <= least) {
                    sum += (uint)abs((int)pal->gun[i].b - (int)blueV);

                    // Our current index will be closest match if the sum of
                    // differences is less than current least. Compare to
                    // current least.
                    if (sum <= least) {
                        // Update least and least index.
                        least      = sum;
                        leastIndex = i;
                    }
                }
            }
        }
    }
    return leastIndex;
}

// Allocate virtual bitmaps and resolution specific initialization;
static void InitGraph(void)
{
    int  i, n;
    uint mapSize;

    for (i = 0, n = s_bounds.bottom - s_bounds.top; i < n; ++i) {
        g_baseTable[i] = (uint16_t)(i * VROWBYTES);
    }

    // Allocate memory for visual & shared priority/control virtual bitmaps.
    mapSize = SizeRect(&s_bounds);

    // Get a handle (and dereference it) for each map in turn.
    g_vHndl  = (uint8_t *)malloc(mapSize);
    g_pcHndl = (uint8_t *)malloc(mapSize);

    // Clear all vMaps to default colors.
    memset(g_vHndl, 0, mapSize);
    memset(g_pcHndl, 0, mapSize);
}

// Free allocated virtual bitmap's memory.
static void EndGraph(void)
{
    free(g_vHndl);
    free(g_pcHndl);
}

// Return segment of a virtual bitmap.
static uint8_t *GetSegment(uint mapSet)
{
    if ((mapSet & (CMAP | PMAP)) != 0) {
        return g_pcHndl;
    } else {
        return g_vHndl;
    }
}

// Set up local colors into nibbles and set mapSet for active bitmaps.
static void SetMaps(void)
{
    s_mapSet = 0;

    if (s_vColor != PENOFF) {
        s_mapSet |= VMAP;
    }

    if (s_pColor != PENOFF) {
        s_mapSet |= PMAP;
    }

    if (s_cColor != PENOFF) {
        s_mapSet |= CMAP;
    }
}

// Setup for FastDot, which is for constant color dots at the same y coordinate.
static uint SetBase(void)
{
    return g_baseTable[s_penY];
}

// Quick draw of points of constant color at the same y coordinate.
static void FastDot(uint base)
{
    // Adword in offset of startX.
    base += (uint)s_penX;

    // Now plot each active bitMap.
    if ((s_mapSet & VMAP) != 0) {
        g_vHndl[base] = s_vColor; // color 0-256 in byte
    }

    if ((s_mapSet & PMAP) != 0) {
        g_pcHndl[base] &= EVENOFF;
        g_pcHndl[base] |= s_pColor; // 0-F in hi nibble only
    }

    if ((s_mapSet & CMAP) != 0) {
        g_pcHndl[base] &= ODDOFF;
        g_pcHndl[base] |= s_cColor; // 0-F in low nibble
    }
}

static void DoDot(uint base)
{
    FastDot(base);
}

// Fill an entire line in all specified pens.
static void FillLines(uint base, int left, int right)
{
    uint length;
    uint i, n;

    if (left > right) {
        return;
    }

    length = (uint)(right - left);
    base += (uint)left;
    n = base + length;

    if ((s_mapSet & VMAP) != 0) {
        for (i = base; i < n; ++i) {
            g_vHndl[i] = s_vColor;
        }
    }

    // Try for a straight store to shared priority/control map.
    if ((s_mapSet & (PMAP | CMAP)) == (PMAP | CMAP)) {
        for (i = base; i < n; ++i) {
            g_pcHndl[i] = s_pColor | s_vColor;
        }
    } else if ((s_mapSet & PMAP) != 0) {
        for (i = base; i < n; ++i) {
            g_pcHndl[i] &= EVENOFF;
            g_pcHndl[i] |= s_pColor;
        }
    } else if ((s_mapSet & CMAP) != 0) {
        for (i = base; i < n; ++i) {
            g_pcHndl[i] &= ODDOFF;
            g_pcHndl[i] |= s_cColor;
        }
    }
}

// Fill area enclosing seed point in port's pen color
// Uses stack hungry recursive (logic limited) scan technique
// seed point (penX/Y) determines background color.
// Fill is limited by boundaries contained in theRect.
static void Fill(int x, int y)
{
    uint base;

    if (s_mirrorX) {
        x = -(x - MAXWIDTH + 1);
    }

    // Offset input coords.
    s_penX = x + (int)g_rThePort->origin.h;
    s_penY = y + (int)g_rThePort->origin.v;

    // Copy portRect to local and offset it for clipping.
    memcpy(&g_theRect, &g_rThePort->portRect, sizeof(RRect));

    // Add the origin of g_rThePort to g_theRect.
    g_theRect.top += g_rThePort->origin.v;
    g_theRect.bottom += g_rThePort->origin.v;
    g_theRect.left += g_rThePort->origin.h;
    g_theRect.right += g_rThePort->origin.h;

    // Set up plotting s_mapSet.
    SetMaps();

    // Are there any maps to be plotted in?
    if ((s_mapSet & (VMAP | PMAP | CMAP)) == 0) {
        return;
    }

    // Abort any attempt to fill in background color of guide map.
    if ((s_mapSet & VMAP) != 0) {
        if (s_vColor == vWHITE) {
            return;
        }
    } else if ((s_mapSet & PMAP) != 0) {
        if (s_pColor == 0) {
            return;
        }
    } else {
        if (s_cColor == 0) {
            return;
        }
    }

    // If seed point is filled we abort.
    base = SetBase();
    if (!CheckDot(s_penX, base)) {
        return;
    }

#define UP   -1
#define DN   1
#define DONE 0

    // Put DONE in the first frame of the fill stack.
    DoFill(s_penX, s_penX, s_penX, s_penX, s_penX, s_penY, DONE);
}

static void DoFill(int xScan,
                   int leftSide,
                   int rightSide,
                   int prevLeftSide,
                   int prevRightSide,
                   int yScan,
                   int direction)
{
    int  oldLeftSide, oldRightSide;
    int  penY;
    uint base;

    oldRightSide = rightSide;
    oldLeftSide  = leftSide;

    base      = SetBase();
    rightSide = CheckRight(s_penX, base);    // returns right end of scan
    leftSide  = CheckLeft(s_penX, base) + 1; // returns left end of scan

    // Line ends have been found, fill line between end points.
    FillLines(base, leftSide, rightSide);

    // Now we scan above while xScan < rightSide.
    penY = s_penY;
    FillUp(leftSide,
           leftSide,
           rightSide,
           oldLeftSide,
           oldRightSide,
           penY,
           direction);

    // Scan to left on bottom of line.
    FillDown(leftSide,
             leftSide,
             rightSide,
             oldLeftSide,
             oldRightSide,
             penY,
             direction);

    if (direction == DN) {
        FillDown(rightSide,
                 oldLeftSide,
                 oldRightSide,
                 prevLeftSide,
                 prevRightSide,
                 yScan,
                 direction);
    } else if (direction == UP) {
        FillUp(rightSide,
               oldLeftSide,
               oldRightSide,
               prevLeftSide,
               prevRightSide,
               yScan,
               direction);
    }
}

static void FillUp(int xScan,
                   int leftSide,
                   int rightSide,
                   int oldLeftSide,
                   int oldRightSide,
                   int yScan,
                   int direction)
{
    uint base;

    // Look above this line for holes.
    s_penY = yScan - 1;

    // If the next line up is off the top of the port, it's time to start
    // looking down.
    if (s_penY < g_theRect.top) {
        return;
    }

    base = SetBase();
    do {
        s_penX = xScan;

        while (true) {
            int x = xScan;
            if (xScan >= rightSide) {
                return;
            }
            xScan++;

            if (!(direction == DN && oldLeftSide < x && x < oldRightSide)) {
                break;
            }

            // We're in between so we swap them.
            xScan = oldRightSide;
        }
    } while (!CheckDot(s_penX, base));

    DoFill(xScan, leftSide, rightSide, oldLeftSide, oldRightSide, yScan, UP);
}

static void FillDown(int xScan,
                     int leftSide,
                     int rightSide,
                     int oldLeftSide,
                     int oldRightSide,
                     int yScan,
                     int direction)
{
    uint base;

    s_penY = yScan + 1;

    // If the next line down is off the bottom of the port, it's time to start
    // returning from the recursion.
    if (s_penY >= g_theRect.bottom) {
        return;
    }

    base = SetBase();
    do {
        s_penX = xScan;

        while (true) {
            int x = xScan;
            if (xScan >= rightSide) {
                return;
            }
            xScan++;

            if (!(direction == UP && oldLeftSide < x && x < oldRightSide)) {
                break;
            }

            xScan = oldRightSide;
        }
    } while (!CheckDot(s_penX, base));

    DoFill(xScan, leftSide, rightSide, oldLeftSide, oldRightSide, yScan, DN);
}

// Check for an unfilled point in Fill.
static bool CheckDot(int penX, uint lineBase)
{
    // Check for a dot out of map coords.
    if (penX < (int)g_theRect.left || penX >= (int)g_theRect.right) {
        return false;
    }

    if ((s_mapSet & VMAP) != 0) {
        return (g_vHndl[lineBase + penX] == vWHITE);
    }

    if ((s_mapSet & PMAP) != 0) {
        return ((g_pcHndl[lineBase + penX] & EVENON) == 0);
    }

    return ((g_pcHndl[lineBase + penX] & ODDON) == 0);
}

// Scans right from penX/Y returning X of first collision.
static int CheckRight(int left, uint lineBase)
{
    uint8_t *vMap;
    int      right;

    // Scan full bytes for deviation.
    right = (int)g_theRect.right;
    lineBase += (uint)left;

    if ((s_mapSet & VMAP) != 0) {
        vMap = g_vHndl + lineBase;
        for (; left < right; ++left, ++vMap) {
            if (*vMap != vWHITE) {
                break;
            }
        }
    } else {
        uint8_t mask;

        if ((s_mapSet & PMAP) != 0) {
            mask = EVENON;
        } else {
            mask = ODDON;
        }

        vMap = g_pcHndl + lineBase;
        for (; left < right; ++left, ++vMap) {
            if ((*vMap & mask) != 0) {
                break;
            }
        }
    }

    return left;
}

// Scans left from penX/Y returning X of first collision.
static int CheckLeft(int right, uint lineBase)
{
    uint8_t *vMap;
    int      left;

    // Scan full bytes for deviation.
    left = (int)g_theRect.left;
    lineBase += (uint)right;

    if ((s_mapSet & VMAP) != 0) {
        vMap = g_vHndl + lineBase;
        for (; left <= right; --right, --vMap) {
            if (*vMap != vWHITE) {
                break;
            }
        }
    } else {
        uint8_t mask;

        if ((s_mapSet & PMAP) != 0) {
            mask = EVENON;
        } else {
            mask = ODDON;
        }

        vMap = g_pcHndl + lineBase;
        for (; left <= right; --right, --vMap) {
            if ((*vMap & mask) != 0) {
                break;
            }
        }
    }

    return right;
}

// Erase all VMAPS to background color.
static void ClearScreen(void)
{
    RFillRect(&g_rThePort->portRect, VMAP | PMAP | CMAP, vWHITE, 0, 0);
}

#define Verify()                                                               \
    if ((*s_picPtr) >= LOWCODE)                                                \
    return

// Return next byte of picture data.
#undef GetByte
#define GetByte() (*s_picPtr++)

static void DispatchGrf(void)
{
    uint8_t code;

    while (true) {
        // Get picture codes and jump to proper handler.
        code = GetByte();
        if (code == ENDCODE) {
            return;
        }

        s_funcTable[code & 0x0f]();
    }
}

static void SetColor(void)
{
    uint8_t data;

    Verify();
    data = GetByte();
    if (s_oldCode) {
        data = s_oldColorTable[data];
    }
    s_vColor = data;
}

static void ClearColor(void)
{
    s_vColor = PENOFF;
}

static void SetPriority(void)
{
    uint8_t data;

    Verify();
    data     = GetByte();
    s_pColor = data << 4;
}

static void ClearPriority(void)
{
    s_pColor = PENOFF;
}

static void DoShortBrush(void)
{
    uint8_t spray;
    int     x, y;

    spray = 0;
    if ((s_brushSize & AIRBRUSH) != 0 && (s_brushSize & BRUSHSIZE_MASK) != 0) {
        Verify();
        spray = GetByte();
    }

    Verify();
    GetAbsXY(&s_lineStartX, &s_lineStartY);
    DrawBrush(s_lineStartX, s_lineStartY, s_brushSize, spray);

    while (true) {
        spray = 0;
        if ((s_brushSize & AIRBRUSH) != 0 &&
            (s_brushSize & BRUSHSIZE_MASK) != 0) {
            Verify();
            spray = GetByte();
        }

        Verify();
        GetShortXY(&x, &y);
        s_lineStartX += x;
        s_lineStartY += y;
        DrawBrush(s_lineStartX, s_lineStartY, s_brushSize, spray);
    }
}

static void DoMedBrush(void)
{
    uint8_t data;
    int     x, y;

    data = 0;
    if ((s_brushSize & AIRBRUSH) != 0 && (s_brushSize & BRUSHSIZE_MASK) != 0) {
        Verify();
        data = GetByte();
    }

    Verify();
    GetAbsXY(&s_lineStartX, &s_lineStartY);
    DrawBrush(s_lineStartX, s_lineStartY, s_brushSize, data);

    while (true) {
        data = 0;
        if ((s_brushSize & AIRBRUSH) != 0 &&
            (s_brushSize & BRUSHSIZE_MASK) != 0) {
            Verify();
            data = GetByte();
        }

        Verify();
        GetMedXY(&x, &y);
        s_lineStartX += x;
        s_lineStartY += y;
        DrawBrush(s_lineStartX, s_lineStartY, s_brushSize, data);
    }
}

static void DoAbsBrush(void)
{
    uint8_t data;

    while (true) {
        data = 0;
        if ((s_brushSize & AIRBRUSH) != 0 &&
            (s_brushSize & BRUSHSIZE_MASK) != 0) {
            Verify();
            data = GetByte();
        }

        Verify();
        GetAbsXY(&s_lineStartX, &s_lineStartY);
        DrawBrush(s_lineStartX, s_lineStartY, s_brushSize, data);
    }
}

// Get absolute (3 byte) start and subsequent packed bytes.
static void DoShortLine(void)
{
    int x0, y0, x1, y1;

    Verify();
    GetAbsXY(&s_lineStartX, &s_lineStartY);

    while (true) {
        Verify();

        x0 = s_lineStartX;
        y0 = s_lineStartY;
        GetShortXY(&x1, &y1);
        x1 += s_lineStartX;
        y1 += s_lineStartY;

        s_lineStartX = x1;
        s_lineStartY = y1;
        DrawLine(x0, y0, x1, y1);
    }
}

static void DoMedLine(void)
{
    int x0, y0, x1, y1;

    Verify();
    GetAbsXY(&s_lineStartX, &s_lineStartY);

    while (true) {
        Verify();

        x0 = s_lineStartX;
        y0 = s_lineStartY;
        GetMedXY(&x1, &y1);
        x1 += s_lineStartX;
        y1 += s_lineStartY;

        s_lineStartX = x1;
        s_lineStartY = y1;
        DrawLine(x0, y0, x1, y1);
    }
}

static void DoAbsLine(void)
{
    int x0, y0, x1, y1;

    Verify();
    GetAbsXY(&s_lineStartX, &s_lineStartY);

    while (true) {
        Verify();

        x0 = s_lineStartX;
        y0 = s_lineStartY;
        GetAbsXY(&x1, &y1);
        s_lineStartX = x1;
        s_lineStartY = y1;
        DrawLine(x0, y0, x1, y1);
    }
}

static void DoAbsFill(void)
{
    int x, y;

    while (true) {
        Verify();
        GetAbsXY(&x, &y);
        Fill(x, y);
    }
}

static void SetBrSize(void)
{
    Verify();
    s_brushSize = GetByte();
}

static void SetControl(void)
{
    Verify();
    s_cColor = GetByte();
}

static void ClearControl(void)
{
    s_cColor = PENOFF;
}

// This is an escape function.
// The following byte determines the actual function invoked.
static void DoSpecial(void)
{
    uint8_t code = GetByte();
    assert(code < ARRAYSIZE(s_specTable));
    s_specTable[code]();
}

static void OldSetPalette(void)
{
    s_oldCode = true;

    while (true) {
        Verify();
        GetByte();
        ++s_picPtr;
    }
}

static void DoBitMap(void)
{
    uint size;
    uint priority;
    Cel *cel;

    Verify();
    GetAbsXY(&s_lineStartX, &s_lineStartY);

    size = *(uint16_t *)s_picPtr;
    s_picPtr += sizeof(uint16_t);

    cel = (Cel *)s_picPtr;
    s_picPtr += size;

    priority = s_pColor;
    if (priority != PENOFF) {
        priority >>= 4;
    }
    priority &= s_picOverlayMask;

    DrawBitMap(s_lineStartX, s_lineStartY, s_mirrorX, cel, priority, s_palPtr);
}

static void SetCLUTPalette(void)
{
    s_oldCode = false;

    s_palPtr = (RPalette *)s_picPtr;

    // Advance picPtr past embedded palette data;
    s_picPtr += PAL_FILE_SIZE;

    RSetPalette(s_palPtr, PAL_REPLACE);
}

// Get new values to build priority tables.
static void SetPriTable(void)
{
    int x, y;

    x = ((int16_t *)s_picPtr)[0];
    y = ((int16_t *)s_picPtr)[1];
    s_picPtr += sizeof(uint16_t) * 2;

    PriTable(x, y);
}

// Each priority band to be set in the table is specified in data
static void SetPriBands(void)
{
    uint16_t *priPtr = (uint16_t *)s_picPtr;
    s_picPtr += 14;

    PriBands(priPtr);
}

// Unpack next 3 bytes of picture code into a X/Y pair.
static void GetAbsXY(int *x, int *y)
{
    uint8_t data;

    //  Verify();
    data = GetByte();
    // This byte has MSBits of X in upper nibble and MSBits of Y in low nibble.
    *y = (int)(data & 0x0f);
    *x = (int)(data >> 4);
    *x = (*x << 8) | (int)GetByte(); // Get LSBits of X
    *y = (*y << 8) | (int)GetByte(); // Get LSBits of Y
}

// Next two bytes are signed offsets from current (Y is first byte).
static void GetMedXY(int *x, int *y)
{
    uint8_t data;

    //  Verify();
    data = GetByte();

    // Make the Y.
    // The byte containing Y must be twiddled into twos complement.
    if ((data & 0x80) != 0) {
        *y = (int)(-(int8_t)(data & 0x7f));
    } else {
        *y = (int)((int8_t)data);
    }

    // Get the X.
    data = GetByte();
    *x   = (int)((int8_t)data);
}

// Get next byte and unpack it into signed offset.
static void GetShortXY(int *x, int *y)
{
    uint8_t data;

    //  Verify();
    data = GetByte();

    // Do Y (low nibble) first.
    *y = (int)(data & 0x0f);
    if ((*y & 8) != 0) {
        *y = -(*y & 7);
    }

    // Now do X (hi nibble).
    *x = (int)(data >> 4);
    if ((*x & 8) != 0) {
        *x = -(*x & 7);
    }
}

// Plot all the points along a line.
static void DrawLine(int x0, int y0, int x1, int y1)
{
    if (s_mirrorX) {
        x0 = -(x0 - MAXWIDTH + 1);
        x1 = -(x1 - MAXWIDTH + 1);
    }

    // Offset all 4 end points.
    x0 += g_rThePort->origin.h;
    x1 += g_rThePort->origin.h;
    y0 += g_rThePort->origin.v;
    y1 += g_rThePort->origin.v;

    s_penX = x0;
    s_penY = y0;
    s_endX = x1;
    s_endY = y1;

    SetMaps();

    // Optimize for strict vertical/horizontal lines.
    if (s_penY == s_endY) {
        DrawHLine();
    } else if (s_penX == s_endX) {
        DrawVLine();
    } else {
        DrawDLine();
    }
}

// Draw a strictly horizontal line.
static void DrawHLine(void)
{
    uint base;
    int  left, right;

    base = SetBase();

    // Make sure that we're drawing the line left to right.
    if (s_penX <= s_endX) {
        left  = s_penX;
        right = s_endX;
    } else {
        left  = s_endX;
        right = s_penX;
    }

    // Needed to complete the line.
    ++right;
    FillLines(base, left, right);
}

// Draw a strictly vertical line from lesser to greater.
static void DrawVLine(void)
{
    uint base, i, length;
    int  top, bottom;

    // Make sure that we're drawing the line top to bottom.
    if (s_penY <= s_endY) {
        top    = s_penY;
        bottom = s_endY;
    } else {
        top    = s_endY;
        bottom = s_penY;

        s_penY = top;
        s_endY = bottom;
    }

    length = bottom - top; // Number of points to plot.
    ++length;

    base = SetBase();
    base += (uint)s_penX; // Points at byte containing point.

    for (i = 0; i < length; ++i) {
        if ((s_mapSet & VMAP) != 0) {
            g_vHndl[base] = s_vColor;
        }

        if ((s_mapSet & PMAP) != 0) {
            g_pcHndl[base] &= EVENOFF;
            g_pcHndl[base] |= s_pColor;
        }

        if ((s_mapSet & CMAP) != 0) {
            g_pcHndl[base] &= ODDOFF;
            g_pcHndl[base] |= s_cColor;
        }

        // Move to next line down.
        base += VROWBYTES;
    }
}

// Determine X/Y vector (direction of change) and pseudo fractional delta.
static void DrawDLine(void)
{
    uint base, i, n;
    int  xVector, yVector;
    int  xDelta, yDelta;
    int  rollover;

    base = SetBase();
    DoDot(base);

    // Get the change in y coordinates.
    yVector = VROWBYTES;
    yDelta  = s_endY - s_penY;
    if (yDelta < 0) {
        yVector = -yVector;
        yDelta  = -yDelta;
    }

    // Get the change in x coordinates.
    xVector = 1;
    xDelta  = s_endX - s_penX;
    if (xDelta < 0) {
        // Negative motion, complement vector and delta.
        xVector = -xVector;
        xDelta  = -xDelta;
    }

    // Branch depending on magnitude of the deltas.
    if (yDelta < xDelta) {
        n        = (uint)xDelta; // iterations required to reach last point
        rollover = xDelta / 2;   // init rollover counter to smooth out line

        // Now determine which direction X is moving and branch on that.
        if (xVector < 0) {
            for (i = 0; i < n; ++i) {
                rollover += yDelta;
                if (rollover >= xDelta) {
                    // Rollover means increment Y location before plotting.
                    rollover -= xDelta;
                    base += (uint)yVector;
                }

                --s_penX;
                FastDot(base);
            }
        } else {
            for (i = 0; i < n; ++i) {
                rollover += yDelta;
                if (rollover >= xDelta) {
                    // Rollover means increment Y location before plotting.
                    rollover -= xDelta;
                    base += (uint)yVector;
                }

                ++s_penX;
                FastDot(base);
            }
        }
    } else {
        n        = (uint)yDelta; // iterations required to reach last point
        rollover = yDelta / 2;   // init rollover counter to smooth out line

        // Now determine which direction X is moving and branch on that.
        if (xVector < 0) {
            for (i = 0; i < n; ++i) {
                rollover += xDelta;
                if (rollover >= yDelta) {
                    // Rollover means increment Y location before plotting.
                    rollover -= yDelta;
                    --s_penX;
                }

                base += (uint)yVector;
                FastDot(base);
            }
        } else {
            for (i = 0; i < n; ++i) {
                rollover += xDelta;
                if (rollover >= yDelta) {
                    // Rollover means increment Y location before plotting.
                    rollover -= yDelta;
                    ++s_penX;
                }

                base += (uint)yVector;
                FastDot(base);
            }
        }
    }
}

static void DrawBrush(int x, int y, uint brush, uint spray)
{
    int       edge, delta;
    int       restartX;
    int       radius, diameter;
    uint16_t  brushBitRight;
    uint16_t  brushBit;
    uint16_t  theMask;
    uint16_t *pattern;
    uint      base;

    if (s_mirrorX) {
        x = -(x - MAXWIDTH + 1);
    }

    s_penX = x;
    s_penY = y;

    SetMaps();

    radius   = (int)(brush & BRUSHSIZE_MASK);
    diameter = radius * 2 + 1;
    pattern  = s_burshPatterns[radius];

    edge = s_penX - radius;
    if (edge < (int)g_rThePort->portRect.left) {
        edge = (int)g_rThePort->portRect.left;
    }

    delta = (int)g_rThePort->portRect.right - diameter;
    if (edge >= delta) {
        edge = delta;
    }

    s_endX = s_penX = edge + g_rThePort->origin.h;
    restartX        = s_penX;

    edge = s_penY - radius;
    if (edge < (int)g_rThePort->portRect.top) {
        edge = (int)g_rThePort->portRect.top;
    }

    delta = (int)g_rThePort->portRect.bottom - diameter;
    if (edge >= delta) {
        edge = delta;
    }

    s_endY = s_penY = edge + g_rThePort->origin.v;

    s_endX += diameter;
    s_endY += diameter;

    brushBitRight = 0x8000; // 1000000000000000b
    brushBitRight >>= diameter;

    spray = (spray | 1) & 0xff;

    if ((brush & RECTBRUSH) != 0) {
        if ((brush & AIRBRUSH) != 0) {
            while (s_penY < s_endY) {
                base = SetBase();

                for (brushBit = brushBitRight; brushBit != 0; brushBit <<= 1) {
                    if ((spray & 1) == 0) {
                        spray >>= 1;
                        spray ^= 0xB8;
                    } else {
                        spray >>= 1;
                    }

                    if ((spray & 1) == 0 && (spray & 2) != 0) {
                        FastDot(base);
                    }

                    ++s_penX;
                }

                s_penX = restartX;
                ++s_penY;
            }
        } else {
            restartX = s_penX;

            while (s_penY < s_endY) {
                DrawHLine();
                s_penX = restartX;
                ++s_penY;
            }
        }
    } else {
        if ((brush & AIRBRUSH) != 0) {
            while (s_penY < s_endY) {
                base    = SetBase();
                theMask = *pattern++;

                for (brushBit = brushBitRight; brushBit != 0; brushBit <<= 1) {
                    if ((brushBit & theMask) == 0) {
                        continue;
                    }

                    if ((spray & 1) == 0) {
                        spray >>= 1;
                        spray ^= 0xB8;
                    } else {
                        spray >>= 1;
                    }

                    if ((spray & 1) == 0 && (spray & 2) != 0) {
                        FastDot(base);
                    }

                    ++s_penX;
                }

                s_penX = restartX;
                ++s_penY;
            }
        } else {
            restartX = --s_endX;

            while (s_penY < s_endY) {
                brushBit = brushBitRight;
                theMask  = *pattern++;

                // Find next fill.
                while ((brushBit & theMask) == 0) {
                    brushBit <<= 1;
                    --s_endX;
                }

                s_penX = s_endX - 1;
                brushBit <<= 1;

                // Find next space.
                while ((brushBit & theMask) != 0) {
                    brushBit <<= 1;
                    --s_penX;
                }

                ++s_penX;
                DrawHLine();

                s_endX = restartX;
                ++s_penY;
            }
        }
    }
}

// Modify rectangle to even pixel size.
// Return storage size in bytes of enclosed pixels.
static uint SizeRect(const RRect *rect)
{
    return (uint16_t)(rect->bottom - rect->top) *
           (uint16_t)(rect->right - rect->left);
}

// Add the origin of the port to the rectangle.
static void SOffsetRect(RRect *rect, const RGrafPort *port)
{
    rect->top += port->origin.v;
    rect->bottom += port->origin.v;

    rect->left += port->origin.h;
    rect->right += port->origin.h;
}

// Get an input rectangle into local.
static void GetTheRect(const RRect *rect)
{
    memcpy(&g_theRect, rect, sizeof(RRect));
}

void RInsetRect(RRect *rect, int x, int y)
{
    rect->left += (int16_t)x;
    rect->right -= (int16_t)x;
    rect->top += (int16_t)y;
    rect->bottom -= (int16_t)y;
}

void ROffsetRect(RRect *rect, int x, int y)
{
    rect->left += (int16_t)x;
    rect->right += (int16_t)x;
    rect->top += (int16_t)y;
    rect->bottom += (int16_t)y;
}

void MoveRect(RRect *rect, int x, int y)
{
    x -= rect->left;
    y -= rect->top;

    rect->left += (int16_t)x;
    rect->right += (int16_t)x;
    rect->top += (int16_t)y;
    rect->bottom += (int16_t)y;
}

bool RSectRect(const RRect *src, const RRect *clip, RRect *dst)
{
    int16_t dim;

    dim = src->top;
    if (dim < clip->top) {
        dim = clip->top;
    } else if (dim > clip->bottom) {
        dim = clip->bottom;
    }
    dst->top = dim;

    dim = src->left;
    if (dim < clip->left) {
        dim = clip->left;
    } else if (dim > clip->right) {
        dim = clip->right;
    }
    dst->left = dim;

    dim = src->bottom;
    if (dim < clip->top) {
        dim = clip->top;
    } else if (dim > clip->bottom) {
        dim = clip->bottom;
    }
    dst->bottom = dim;

    dim = src->right;
    if (dim < clip->left) {
        dim = clip->left;
    } else if (dim > clip->right) {
        dim = clip->right;
    }
    dst->right = dim;

    // Test if the rectangle left is any good.
    return (dst->left < dst->right && dst->top < dst->bottom);
}

void KGraph(argList)
{
#define ret(val) g_acc = ((uintptr_t)(val))

    RRect rect;

    switch ((int)arg(1)) {
        case GLoadBits:
            LoadBits((uint)arg(2));
            break;

        case GDetect:
            ret(256);
            break;

        case GSetPalette:
            // SetResPalette((uint)arg(2), (int)arg(3));
            break;

        case GDrawLine:
            s_vColor = (uint8_t)arg(6);
            s_pColor = (uint8_t)arg(7);
            if ((uint)arg(7) != INVALIDPRI) {
                s_pColor <<= 4;
            }
            s_cColor = (uint8_t)arg(8);
            DrawLine((int)arg(3), (int)arg(2), (int)arg(5), (int)arg(4));
            break;

        case GReAnimate:
            ReAnimate(RectFromNative(&arg(2), &rect));
            break;

        case GFillArea:
            break;

        case GDrawBrush:
            s_vColor = (uint8_t)arg(5);
            s_pColor = (uint8_t)arg(6);
            s_pColor <<= 4;
            s_cColor = (uint8_t)arg(7);
            DrawBrush((int)arg(2), (int)arg(3), (uint)arg(4), (uint)arg(2));
            break;

        case GSaveBits:
            ret(SaveBits(RectFromNative(&arg(2), &rect), (uint)arg(6)));
            break;

        case GRestoreBits:
            RestoreBits((Handle)arg(2));
            break;

        case GEraseRect:
            REraseRect(RectFromNative(&arg(2), &rect));
            break;

        case GPaintRect:
            RPaintRect(RectFromNative(&arg(2), &rect));
            break;

        case GFillRect:
            RFillRect(RectFromNative(&arg(2), &rect),
                      (uint)arg(6),
                      (uint8_t)arg(7),
                      (uint8_t)arg(8),
                      (uint8_t)arg(9));
            break;

        case GShowBits:
            ShowBits(RectFromNative(&arg(2), &rect), (uint)arg(6));
            break;

        default:
            break;
    }
}
