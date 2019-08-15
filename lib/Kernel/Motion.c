#include "sci/Kernel/Motion.h"
#include "sci/Kernel/Animate.h"
#include "sci/Kernel/Cels.h"
#include "sci/Kernel/Graphics.h"
#include "sci/Kernel/Picture.h"
#include "sci/Kernel/Resource.h"
#include "sci/Kernel/Selector.h"
#include "sci/Kernel/View.h"
#include "sci/PMachine/PMachine.h"
#include "sci/Utils/ErrMsg.h"

void KBaseSetter(argList)
{
    Obj  *actor;
    int   theY;
    View *view;

    actor = (Obj *)arg(1);
    view  = (View *)ResLoad(RES_VIEW, IndexedProp(actor, actView));

    GetCelRectNative(view,
                     (uint)IndexedProp(actor, actLoop),
                     (uint)IndexedProp(actor, actCel),
                     (int)IndexedProp(actor, actX),
                     (int)IndexedProp(actor, actY),
                     (int)IndexedProp(actor, actZ),
                     IndexedPropAddr(actor, actBR));

    theY                            = 1 + (int)IndexedProp(actor, actY);
    IndexedProp(actor, actBRBottom) = (uintptr_t)theY;
    IndexedProp(actor, actBR) =
      (uintptr_t)(theY - (int)IndexedProp(actor, actYStep));
}

void KDirLoop(argList)
{
#ifndef NOT_IMPL
#error Not finished
#endif
}

void KInitBresen(argList)
{
    Obj *motion, *client;
    bool xAxis;
    int  DX, DY, toX, toY, dx, dy, incr, i1, i2, di;
    int  tdx, tdy, watchDog;
    int  skipFactor;

    motion     = (Obj *)arg(1);
    client     = (Obj *)IndexedProp(motion, motClient);
    skipFactor = (argCount >= 2) ? (int)arg(2) : 1;

    toX = (int)IndexedProp(motion, motX);
    toY = (int)IndexedProp(motion, motY);

    tdx = (int)IndexedProp(client, actXStep) * skipFactor;
    tdy = (int)IndexedProp(client, actYStep) * skipFactor;

    watchDog = (tdx > tdy) ? tdx : tdy;
    watchDog *= 2;

    // Get distances to be moved.
    DX = toX - (int)IndexedProp(client, actX);
    DY = toY - (int)IndexedProp(client, actY);

    // Compute basic step sizes.
    while (true) {
        dx = tdx;
        dy = tdy;

        if (abs(DX) >= abs(DY)) {
            // Then motion will be along the x-axis.
            xAxis = true;
            if (DX < 0) {
                dx = -dx;
            }
            dy = (DX) ? dx * DY / DX : 0;
        } else {
            // Our major motion is along the y-axis.
            xAxis = false;
            if (DY < 0) {
                dy = -dy;
            }
            dx = (DY) ? dy * DX / DY : 0;
        }

        // Compute increments and decision variable.
        i1   = (xAxis) ? 2 * (dx * DY - dy * DX) : 2 * (dy * DX - dx * DY);
        incr = 1;
        if ((xAxis && DY < 0) || (!xAxis && DX < 0)) {
            i1   = -i1;
            incr = -1;
        }

        i2 = i1 - 2 * ((xAxis) ? DX : DY);
        di = i1 - ((xAxis) ? DX : DY);

        if ((xAxis && DX < 0) || (!xAxis && DY < 0)) {
            i1 = -i1;
            i2 = -i2;
            di = -di;
        }

        // Limit x step to avoid over stepping Y step size.
        if (xAxis && tdx > tdy) {
            if (tdx != 0 && abs(dy + incr) > tdy) {
                if ((--watchDog) == 0) {
                    RAlert(E_BRESEN);
                    exit(1);
                }
                --tdx;
                continue;
            }
        }
#if 0
        // DON'T modify X
        else
        {
            if (tdy && abs(dx + incr) > tdx)
            {
                --tdy;
                continue;
            }
        }
#endif

        break;
    }

    // Set the various variables we've computed.
    IndexedProp(motion, motDX)    = (uintptr_t)dx;
    IndexedProp(motion, motDY)    = (uintptr_t)dy;
    IndexedProp(motion, motI1)    = (uintptr_t)i1;
    IndexedProp(motion, motI2)    = (uintptr_t)i2;
    IndexedProp(motion, motDI)    = (uintptr_t)di;
    IndexedProp(motion, motIncr)  = (uintptr_t)incr;
    IndexedProp(motion, motXAxis) = (uintptr_t)xAxis;
}

void KDoBresen(argList)
{
    Obj     *motion, *client;
    int      x, y, toX, toY, i1, i2, di, si1, si2, sdi;
    int      dx, dy, incr;
    bool     xAxis;
    uint     moveCount;
    uint8_t *aniState[500];

    motion = (Obj *)arg(1);
    client = (Obj *)IndexedProp(motion, motClient);
    g_acc  = 0;

    IndexedProp(client, actSignal) =
      IndexedProp(client, actSignal) & (~blocked);

    moveCount = (uint)IndexedProp(motion, motMoveCnt) + 1;
    if ((uint)IndexedProp(client, actMoveSpeed) < moveCount) {
        // Get properties in variables for speed and convenience.
        x     = (int)IndexedProp(client, actX);
        y     = (int)IndexedProp(client, actY);
        toX   = (int)IndexedProp(motion, motX);
        toY   = (int)IndexedProp(motion, motY);
        xAxis = (bool)IndexedProp(motion, motXAxis);
        dx    = (int)IndexedProp(motion, motDX);
        dy    = (int)IndexedProp(motion, motDY);
        incr  = (int)IndexedProp(motion, motIncr);
        si1 = i1 = (int)IndexedProp(motion, motI1);
        si2 = i2 = (int)IndexedProp(motion, motI2);
        sdi = di = (int)IndexedProp(motion, motDI);

        IndexedProp(motion, motXLast) = (uintptr_t)x;
        IndexedProp(motion, motYLast) = (uintptr_t)y;

        // Save the current animation state before moving the client.
        memcpy(aniState,
               client->vars,
               sizeof(uintptr_t) * OBJHEADER(client)->varSelNum);

        if ((xAxis && (abs(toX - x) <= abs(dx))) ||
            (!xAxis && (abs(toY - y) <= abs(dy)))) {
            //
            // We're within a step size of the destination -- set client's x & y
            // to it.
            //

            x = toX;
            y = toY;
        } else {
            //
            // Move one step.
            //

            x += dx;
            y += dy;
            if (di < 0) {
                di += i1;
            } else {
                di += i2;
                if (xAxis) {
                    y += incr;
                } else {
                    x += incr;
                }
            }
        }

        // Update client's properties.
        IndexedProp(client, actX) = (uintptr_t)x;
        IndexedProp(client, actY) = (uintptr_t)y;

        // Check position validity for this cel.
        if ((g_acc = InvokeMethod(client, s_cantBeHere, 0)) != 0) {
            // Client can't be here -- restore the original state and mark the
            // client as blocked.
            memcpy(client->vars,
                   aniState,
                   sizeof(uintptr_t) * OBJHEADER(client)->varSelNum);
            i1 = si1;
            i2 = si2;
            di = sdi;

            IndexedProp(client, actSignal) =
              blocked | IndexedProp(client, actSignal);
        }

        IndexedProp(motion, motI1)      = (uintptr_t)i1;
        IndexedProp(motion, motI2)      = (uintptr_t)i2;
        IndexedProp(motion, motDI)      = (uintptr_t)di;
        IndexedProp(motion, motMoveCnt) = moveCount;

        if (x == toX && y == toY) {
            InvokeMethod(motion, s_moveDone, 0);
        }
    } else {
        IndexedProp(motion, motMoveCnt) = moveCount;
    }
}

void KDoAvoider(argList) {}

void KSetJump(argList)
{
#ifndef NOT_IMPL
#error Not finished
#endif
}

// Determine and return legality of actors position.
// This code checks controls AND base rect intersection.
void KCantBeHere(argList)
{
    Obj       *him  = (Obj *)arg(1);
    List      *cast = (List *)arg(2);
    Node      *node;
    Obj       *me;
    RRect      r, chkR;
    RGrafPort *oldPort;

    RGetPort(&oldPort);
    RSetPort(&g_picWind->port);
    RectFromNative(IndexedPropAddr(him, actBR), &r);

    //
    // (s_illegalBits) are the bits that the object cannot be on.
    // Anding this with the bits the object is on tells us which
    // bits the object is on but shouldn't be on.
    // If this is zero, the position is valid.
    //

    g_acc = 0;
    if (!(g_acc = (OnControl(CMAP, &r) & IndexedProp(him, actIllegalBits)))) {
        // Controls were legal, do I care about other actors?
        // If I am hidden or ignoring actors my position is legal 8.
        if ((IndexedProp(him, actSignal) & (ignrAct | HIDDEN)) == 0) {
            // Default to no hits.
            g_acc = 0;
            // Now the last thing we care about is our rectangles.
            for (node = FirstNode(cast); node != NULL; node = NextNode(node)) {
                me = (Obj *)((KNode *)node)->nVal;

                // Can't hit myself.
                if (him == me) {
                    continue;
                }

                // Can't hit if I'm as below.
                if ((IndexedProp(me, actSignal) &
                     (NOUPDATE | ignrAct | HIDDEN)) != 0) {
                    continue;
                }

                // Check method from actor.

                // If our rectangles intersect we are done.
                RectFromNative(IndexedPropAddr(me, actBR), &chkR);
                if (r.left >= chkR.right || r.right <= chkR.left ||
                    r.top >= chkR.bottom || r.bottom <= chkR.top) {
                    continue;
                } else {
                    g_acc = (uintptr_t)me;
                    break;
                }
            }
        }
    }

    RSetPort(oldPort);
}
