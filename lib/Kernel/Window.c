#include "sci/Kernel/Window.h"
#include "sci/Kernel/Animate.h"
#include "sci/Kernel/Graphics.h"
#include "sci/Kernel/Menu.h"
#include "sci/Kernel/Picture.h"
#include "sci/Kernel/Text.h"
#include "sci/Utils/ErrMsg.h"
#include "sci/Utils/Trig.h"

#define TITLEBAR BARSIZE // lines in title bar

static List       s_windowList = LIST_INITIALIZER;
static RGrafPort *s_wmgrPort   = NULL;

static void SaveBackground(RWindow *wind);

void InitWindow(void)
{
    s_wmgrPort = (RGrafPort *)malloc(sizeof(RGrafPort));
    ROpenPort(s_wmgrPort);
    RSetPort(s_wmgrPort);
    RSetOrigin(0, g_picRect.top);
    s_wmgrPort->portRect.bottom = MAXHEIGHT - g_picRect.top;
    MoveRect(&g_rThePort->portRect, 0, 0);

    // Init the window list.
    InitList(&s_windowList);
    AddToFront(&s_windowList, ToNode(s_wmgrPort));
}

void EndWindow(void)
{
    free(s_wmgrPort);
}

RGrafPort *RGetWmgrPort(void)
{
    return s_wmgrPort;
}

void RBeginUpdate(RWindow *uWind)
{
    RWindow   *wind;
    RGrafPort *oldPort;

    RGetPort(&oldPort);
    RSetPort(RGetWmgrPort());

    wind = FromNode(LastNode(&s_windowList), RWindow);

    while (uWind != wind) {
        SaveBackground(wind);
        wind = FromNode(PrevNode(ToNode(wind)), RWindow);
    }

    RSetPort(oldPort);
}

void REndUpdate(RWindow *uWind)
{
    RGrafPort *oldPort;
    Node      *uWindNode;

    RGetPort(&oldPort);
    RSetPort(RGetWmgrPort());

    uWindNode = ToNode(uWind);
    while (uWindNode != LastNode(&s_windowList)) {
        uWindNode = NextNode(uWindNode);
        uWind     = FromNode(uWindNode, RWindow);
        SaveBackground(uWind);
    }

    // Re-Animate bits of the entire port
    // ReAnimate(&g_rThePort->portRect);

    RSetPort(oldPort);
}

RWindow *RNewWindow(RRect      *fr,
                    const char *title,
                    uint        type,
                    uint        priority,
                    bool        visible)
{
    RWindow *wind;
    RRect    frame;
    RRect   *wr;
    int16_t  top, left;
    uint     mapSet = VMAP;

    // Allocate a structure for the window and add it to front of list.
    wind = (RWindow *)malloc(sizeof(RWindow));
    if (wind == NULL) {
        RAlert(E_OPEN_WINDOW);
        return NULL;
    }

    memset(wind, 0, sizeof(RWindow));
    AddToEnd(&s_windowList, ToNode(wind));

    // Initialize.
    ROpenPort(&wind->port);
    RCopyRect(fr, &frame);
    RCopyRect(fr, &wind->port.portRect);
    wind->wType      = type;
    wind->vUnderBits = 0;
    wind->pUnderBits = 0;
    wind->title      = NULL;
    wind->visible    = false;

    // Determine mapSet we will save.
    if ((type & NOSAVE) == 0) {
        if (priority != INVALIDPRI) {
            mapSet |= PMAP;
        }
        wind->mapSet = mapSet;
    } else {
        wind->mapSet = 0;
    }

    // Get a copy of the title.
    if (title != NULL && (type & TITLED) != 0) {
        wind->title = strdup(title);
        if (wind->title == NULL) {
            free(wind);
            RAlert(E_OPEN_WINDOW);
            return NULL;
        }
    }

    // Size the frame according to type.
    RCopyRect(fr, &frame);

    // Adjust frame unless this is a custom window.
    if (wind->wType != (CUSTOMWIND | NOSAVE)) {
        if ((type & NOBORDER) == 0) {
            RInsetRect(&frame, -1, 0);
            if ((type & TITLED) != 0) {
                frame.top -= TITLEBAR;
                ++frame.bottom;
            } else {
                --frame.top;
            }

            // Add room for drop shadow.
            ++frame.right;
            ++frame.bottom;
        }
    }

    RCopyRect(&frame, &wind->frame);

    //
    // Try to get the window onto the screen, if possible.
    // These are ordered so that for a window too large to fit,
    // the bottom and left of the window will be on the screen.
    //
    fr   = &wind->frame;
    wr   = &s_wmgrPort->portRect;
    top  = fr->top;
    left = fr->left;
    if (fr->top < wr->top) {
        MoveRect(fr, fr->left, wr->top);
    }
    if (fr->bottom > wr->bottom) {
        MoveRect(fr, fr->left, fr->top - fr->bottom + wr->bottom);
    }
    if (fr->right > wr->right) {
        MoveRect(fr, fr->left - fr->right + wr->right, fr->top);
    }
    if (fr->left < wr->left) {
        MoveRect(fr, wr->left, fr->top);
    }
    wr = &wind->port.portRect;
    MoveRect(wr, wr->left + fr->left - left, wr->top + fr->top - top);

    // If the window is visible, draw it.
    if (visible) {
        RDrawWindow(wind);
    }

    // Make this window current port and massage portRect into content.
    RSetPort(&wind->port);
    RSetOrigin(g_rThePort->portRect.left,
               g_rThePort->portRect.top + s_wmgrPort->origin.v);
    MoveRect(&g_rThePort->portRect, 0, 0);
    return wind;
}

void RDrawWindow(RWindow *wind)
{
    RRect      r;
    RGrafPort *oldPort;
    uint8_t    oldPen;

    if (wind->visible) {
        return;
    }
    wind->visible = true;

    // draw the window in master port according to type.
    RGetPort(&oldPort);
    RSetPort(s_wmgrPort);

    // All drawing is done in window manager colors.
    PenColor(BRDCOLOR);

    // Save bits under frame (global coords) and do priority fill if needed.
    if ((wind->wType & NOSAVE) == 0) {
        wind->vUnderBits = SaveBits(&wind->frame, VMAP);

        if ((wind->mapSet & PMAP) != 0) {
            wind->pUnderBits = SaveBits(&wind->frame, PMAP);

            // VCOLOR is ignored.
            RFillRect(&wind->frame, PMAP, 0, 15, 0);
        }
    }

    /* A custom window is draw by the user */
    if (wind->wType != (CUSTOMWIND | NOSAVE)) {
        // If nothing else we fill the entire frame.
        RCopyRect(&wind->frame, &r);

        // Now we work on the frame.
        if ((wind->wType & NOBORDER) == 0) {
            // Drop shadow.
            --r.right;
            --r.bottom;
            ROffsetRect(&r, 1, 1);
            RFrameRect(&r);
            ROffsetRect(&r, -1, -1);
            RFrameRect(&r);

            // If we are titled.
            if ((wind->wType & TITLED) != 0) {
                r.bottom = r.top + TITLEBAR;
                RFrameRect(&r);
                RInsetRect(&r, 1, 1);
                RFillRect(&r, VMAP, BRDCOLOR, 0, 0);
                oldPen = g_rThePort->fgColor;
                PenColor(vWHITE);
                if (wind->title != NULL) {
                    RTextBox(wind->title, true, &r, TEJUSTCENTER, 0);
                }
                PenColor(oldPen);

                // Return &r to where we were.
                RCopyRect(&wind->frame, &r);
                r.top += TITLEBAR - 1;
                --r.right;
                --r.bottom;
            }
            RInsetRect(&r, 1, 1);
        }

        // Fill content region (r) in background color of the windows port.
        if ((wind->wType & NOSAVE) == 0) {
            RFillRect(&r, VMAP, wind->port.bkColor, 0, 0);
        }

        // Show what we have wrought.
        ShowBits(&wind->frame, VMAP);
    }
    RSetPort(oldPort);
}

void RDisposeWindow(RWindow *wind, bool eraseOnly)
{
    RSetPort(s_wmgrPort);
    RestoreBits(wind->vUnderBits); // frees handle
    RestoreBits(wind->pUnderBits); // frees handle
    if (eraseOnly != 0) {
        ShowBits(&wind->frame, VMAP);
    } else {
        ReAnimate(&wind->frame);
    }
    DeleteNode(&s_windowList, ToNode(wind));
    RSelectWindow(FromNode(LastNode(&s_windowList), RWindow));
    if (wind->title != NULL) {
        free(wind->title);
    }
    free(wind);
}

bool RFrontWindow(RWindow *wind)
{
    return (wind == FromNode(LastNode(&s_windowList), RWindow));
}

void RSelectWindow(RWindow *wind)
{
    RWindow *uWind;
    Node    *windNode;

    RSetPort(&wind->port);
    windNode = ToNode(wind);
    if (windNode != LastNode(&s_windowList)) {
        uWind = FromNode(PrevNode(windNode), RWindow);
        RBeginUpdate(uWind);
        MoveToEnd(&s_windowList, windNode);
        REndUpdate(uWind);
    }
    RSetPort(&wind->port);
}

RRect *RectFromNative(uintptr_t *native, RRect *rect)
{
    rect->top    = (int16_t)native[0];
    rect->left   = (int16_t)native[1];
    rect->bottom = (int16_t)native[2];
    rect->right  = (int16_t)native[3];
    return rect;
}

uintptr_t *RectToNative(RRect *rect, uintptr_t *native)
{
    native[0] = (uintptr_t)rect->top;
    native[1] = (uintptr_t)rect->left;
    native[2] = (uintptr_t)rect->bottom;
    native[3] = (uintptr_t)rect->right;
    return native;
}

RPoint *PointFromNative(uintptr_t *native, RPoint *pt)
{
    pt->v = (int16_t)native[0];
    pt->h = (int16_t)native[1];
    return pt;
}

uintptr_t *PointToNative(RPoint *pt, uintptr_t *native)
{
    native[0] = (uintptr_t)pt->v;
    native[1] = (uintptr_t)pt->h;
    return native;
}

void RFrameRect(RRect *r)
{
    RRect rt;

    rt.top    = r->top;
    rt.bottom = r->bottom;
    rt.left = rt.right = r->left;
    ++rt.right;
    RPaintRect(&rt);

    rt.left = rt.right = r->right - 1;
    ++rt.right;
    RPaintRect(&rt);

    rt.left = r->left;
    rt.top = rt.bottom = r->top;
    ++rt.bottom;
    RPaintRect(&rt);

    rt.top = rt.bottom = r->bottom - 1;
    ++rt.bottom;
    RPaintRect(&rt);
}

void RSetRect(RRect *rect, int left, int top, int right, int bottom)
{

    rect->left   = (int16_t)left;
    rect->right  = (int16_t)right;
    rect->top    = (int16_t)top;
    rect->bottom = (int16_t)bottom;
}

void RCopyRect(const RRect *src, RRect *dst)
{
    memcpy(dst, src, sizeof(RRect));
}

void RUnionRect(const RRect *r1, const RRect *r2, RRect *ru)
{

    ru->top    = (r1->top < r2->top) ? r1->top : r2->top;
    ru->left   = (r1->left < r2->left) ? r1->left : r2->left;
    ru->bottom = (r1->bottom > r2->bottom) ? r1->bottom : r2->bottom;
    ru->right  = (r1->right > r2->right) ? r1->right : r2->right;
}

bool RPtInRect(const RPoint *p, const RRect *r)
{
    return (p->v < r->bottom && p->v >= r->top && p->h < r->right &&
            p->h >= r->left);
}

int RPtToAngle(const RPoint *sp, const RPoint *dp)
{
    return ATan(dp->v, sp->h, sp->v, dp->h);
}

static void SaveBackground(RWindow *wind)
{
    Handle uBits;

    if (wind->mapSet != 0 && wind->visible) {
        uBits = SaveBits(&wind->frame, VMAP);
        RestoreBits(wind->vUnderBits);
        wind->vUnderBits = uBits;

        if ((wind->mapSet & PMAP) != 0) {
            uBits = SaveBits(&wind->frame, PMAP);
            RestoreBits(wind->pUnderBits);
            wind->pUnderBits = uBits;
        }
    }
}
