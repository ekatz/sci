#ifndef MOUSE_H
#define MOUSE_H

#include "GrTypes.h"

extern bool   g_haveMouse;
extern int    g_mousePosY;
extern int    g_mousePosX;
extern ushort g_buttonState;

// Put local point in mouse coords.
void SetMouse(RPoint *pt);

// Return interrupt level position in local coords of mouse in the point.
ushort RGetMouse(RPoint *pt);

// Return interrupt level position in global coords of mouse in the point.
ushort CurMouse(RPoint *pt);

#endif // MOUSE_H
