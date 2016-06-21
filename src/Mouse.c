#include "Mouse.h"
#include "Graphics.h"

#define MAXY MAXHEIGHT
#define MAXX MAXWIDTH

bool   g_haveMouse   = true;
int    g_mousePosY   = 0;
int    g_mousePosX   = 0;
ushort g_buttonState = 0;

void SetMouse(RPoint *pt){
#ifndef NOT_IMPL
#error Not finished
#endif
}

ushort RGetMouse(RPoint *pt)
{
    pt->v = (int16_t)g_mousePosY - g_rThePort->origin.v;
    pt->h = (int16_t)g_mousePosX - g_rThePort->origin.h;
    return g_buttonState;
}

ushort CurMouse(RPoint *pt)
{
    pt->v = (int16_t)g_mousePosY;
    pt->h = (int16_t)g_mousePosX;
    return g_buttonState;
}
