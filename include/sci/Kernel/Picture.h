#ifndef PICTURE_H
#define PICTURE_H

#include "Window.h"

// Show pic styles.
#define HSHUTTER      0
#define VSHUTTER      1
#define WIPELEFT      2
#define WIPERIGHT     3
#define WIPEUP        4
#define WIPEDOWN      5
#define IRISIN        6
#define IRISOUT       7
#define DISSOLVE      8
#define PIXELDISSOLVE 9
#define FADEOUT       10

// The relative order of these defines is important for ShowPic to work
// correctly.
#define SCROLLRIGHT   11
#define SCROLLLEFT    12
#define SCROLLUP      13
#define SCROLLDOWN    14

#define DONTSHOW 0x1000
#define VMIRROR  0x2000
#define HMIRROR  0x4000
#define BLACKOUT 0x8000
// 256 different styles and 8 different flags
#define PICFLAGS (~0xff)

#define PDFLAG   0x8000
#define PDBOFLAG 0x4000

extern uint     g_picNotValid;
extern RWindow *g_picWind;
extern RRect    g_picRect;

extern uint     g_showPicStyle;
extern uint     g_showMap;

void InitPicture(void);

void ShowPic(uint mapSet, uint style);

// Return priority corresponding to this point.
uint CoordPri(int y);

// Return lowest Y of this priority band.
int PriCoord(uint p);

void PriTable(int x, int y);

// Each priority band to be set in the table is specified in data.
void PriBands(uint16_t *priPtr);

#endif // PICTURE_H
