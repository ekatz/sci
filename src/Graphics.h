#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "GrTypes.h"
#include "Palette.h"

// KGraph function dispatch constants.
#define GLoadBits    1
#define GDetect      2
#define GSetPalette  3
#define GDrawLine    4
#define GFillArea    5
#define GDrawBrush   6
#define GSaveBits    7
#define GRestoreBits 8
#define GEraseRect   9
#define GPaintRect   10
#define GFillRect    11
#define GShowBits    12
#define GReAnimate   13
#define GInitPri     14

extern uint8_t   *g_vHndl;
extern uint8_t   *g_pcHndl;

extern uint16_t   g_baseTable[];
extern RGrafPort *g_rThePort;
extern RRect      g_theRect;

extern uint16_t   g_wordBits[];

// Load video driver and do general initialization.
bool CInitGraph(void);

// End the graphics.
void CEndGraph(void);

const RRect *GetBounds(void);

void ShowBits(const RRect *rect, uint mapSet);

// Draw the picture described by data at hndl, clear is boolean
void DrawPic(Handle hndl, bool clear, uint mirror);

// Load the numbered bitmap.
void LoadBits(uint num);

void ROpenPort(RGrafPort *port);

// Point passed pointer to g_rThePort.
void RGetPort(RGrafPort **port);

// Point g_rThePort to passed address.
void RSetPort(RGrafPort *port);

// Set the origin of g_rThePort to X/Y (x must be even).
void RSetOrigin(int x, int y);

// Move pen location to x/y.
void RMoveTo(int x, int y);

// Move pen location by delta x/y.
void RMove(int x, int y);

// Set pen's transfer mode
void RPenMode(uint8_t mode);

// Set txFace.
void RTextFace(uint face);

// Return point size of port's current font or over ride.
uint GetPointSize(void);

// Override the point setting of the current font.
void SetPointSize(uint size);

// Return the font number of the current port.
uint GetFont(void);

// Try to set the font of this port.
void RSetFont(uint fontNum);

// Return total width of this text.
uint RTextWidth(const char *text, int first, uint count);

// Return width in pixels of the character.
uint RCharWidth(char cCode);

// Return height in pixels of the character.
uint CharHeight(char cCode);

// Draw a graphics character into visual map at current pnLoc and advance pnLoc
// by characters width.
void RDrawChar(char cCode);

// draw a graphics character into visual map at current pnLoc show the bits and
// advance pnLoc.
void ShowChar(char cCode);

// Inset (or outset) pointed to rect.
void RInsetRect(RRect *rect, int x, int y);

void ROffsetRect(RRect *rect, int x, int y);

// Return intersection of passed rectangles in source rectangle.
// If source is inside clip, ok;
// else move clip to src start clipping with top/left.
bool RSectRect(const RRect *src, const RRect *clip, RRect *dst);

// Move rectangle so origin is at x/y.
void MoveRect(RRect *rect, int x, int y);

// Call fill rect to fill in foreground color.
void RPaintRect(RRect *rect);

// Draw the rectangle on screen in the requested bitMaps in requested colors.
// Colors passed are expect in v/p/c order!
void RFillRect(const RRect *rect,
               uint         mapSet,
               uint8_t      color1,
               uint8_t      color2,
               uint8_t      color3);

// Call fill rect to xor white with current color.
void RInvertRect(RRect *rect);

// Call fill rect to fill in background color.
void REraseRect(RRect *rect);

// Save virtual bitmaps bounded by rect, in buffer we allocate locally.
Handle SaveBits(const RRect *rect, uint mapSet);

// Restore virtual bitmaps with info and data from passed handle.
// Dispose of handle.
void RestoreBits(Handle hndl);

// Dispose of handle.
void UnloadBits(Handle hndl);

void GetCLUT(RPalette *pal);

void SetCLUT(const RPalette *pal);

// Tell driver to hide the cursor.
bool RHideCursor(void);

// Tell driver to show cursor on screen.
bool RShowCursor(void);

// Set pen's foreground color.
void PenColor(uint8_t color);

// Set pen's background color.
void RBackColor(uint8_t color);

// Return entry index of palette that is closest to passed R/G/B values.
// Match is determined by least sum of absolute differences.
uint FastMatch(const RPalette *pal,
               uint8_t         redV,
               uint8_t         greenV,
               uint8_t         blueV,
               uint            psize,
               uint            least);

// Shake the screen.
void ShakeScreen(int cnt, int dir);

// Shift the screen area defined by the passed rectangle coordinates in the
// specified direction.
void ShiftScreen(int rtop, int rleft, int rbot, int rright, int dir);

// Common dispatch for various kernel routines.
void KGraph(uintptr_t *args);

#endif // GRAPHICS_H
