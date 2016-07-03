#ifndef WINDOW_H
#define WINDOW_H

#include "GrTypes.h"

#pragma warning(push)
#pragma warning(disable : 4201) // nameless struct/union

#define BRDCOLOR   vBLACK
#define NOSAVE     1
#define NOBORDER   2
#define TITLED     4
#define STDWIND    0
#define CUSTOMWIND 0x80

typedef struct RWindow {
    union {
        Node      link;
        RGrafPort port;
    };
    RRect  frame;      // visible portion of window
    uint   wType;      // variation in drawing window
    uint   mapSet;     // maps we are in
    Handle vUnderBits; // pointer to saved bits in visual map
    Handle pUnderBits; // pointer to saved bits in priority map
    char  *title;      // pointer to name
    bool   visible;    // non-zero if window is drawn
} RWindow;

// Init window manager port and window list.
void InitWindow(void);

// Clean up window system preparatory to exit.
void EndWindow(void);

RGrafPort *RGetWmgrPort(void);

// Set up for a window update.
void RBeginUpdate(RWindow *uWind);

// End update phase of RWindow Manager.
void REndUpdate(RWindow *uWind);

// Open and return pointer to window.
RWindow *RNewWindow(RRect      *fr,
                    const char *title,
                    uint        type,
                    uint        priority,
                    bool        visible);

// Draw this window and set its visible flag.
void RDrawWindow(RWindow *wind);

// Dispose window and all allocated storage.
void RDisposeWindow(RWindow *wind, bool eraseOnly);

// Return true if this is front window.
bool RFrontWindow(RWindow *wind);

// Put this window at end of list.
void RSelectWindow(RWindow *wind);

RRect *RectFromNative(uintptr_t *native, RRect *rect);
RPoint *PointFromNative(uintptr_t *native, RPoint *pt);
uintptr_t *RectToNative(RRect *rect, uintptr_t *native);
uintptr_t *PointToNative(RPoint *pt, uintptr_t *native);

// Draw a frame around this rect
void RFrameRect(RRect *r);

// Make a rectangle from these values.
void RSetRect(RRect *rect, int left, int top, int right, int bottom);

void RCopyRect(const RRect *src, RRect *dst);

// Set ru to a rectangle encompassing both.
void RUnionRect(const RRect *r1, const RRect *r2, RRect *ru);

// True if point is in rect.
bool RPtInRect(const RPoint *p, const RRect *r);

// Return pseudo angle 0-359.
int RPtToAngle(const RPoint *sp, const RPoint *dp);

#pragma warning(pop)

#endif // WINDOW_H
