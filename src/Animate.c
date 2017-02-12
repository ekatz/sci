#include "Animate.h"
#include "Cels.h"
#include "Dialog.h"
#include "Globals.h"
#include "Graphics.h"
#include "Object.h"
#include "PMachine.h"
#include "Picture.h"
#include "Resource.h"
#include "Selector.h"

#define PLENTYROOM 1 // only applies to this module
#define SORTONZTOO   // only applies to this module
#define NOSAVEBITS -1

typedef struct AniObj {
    Node      link;
    uint      v;
    uint      l;
    uint      c;
    uint      p;
    RPalette *pal;
    Handle    u;
    RRect     r;
} AniObj;

#define LIST_INVALID_NODE ((Node *)-1)

#define IsListInitialized(list) ((list)->head != LIST_INVALID_NODE)
#define UnInitList(list)        FirstNode(list) = LIST_INVALID_NODE

static List s_lastCast = { LIST_INVALID_NODE };

// Sort cast array by y property.
static void SortCast(Obj *cast[], int casty[], uint size);

// Redo all "stopped" actors.
static void ReDoStopped(Obj *cast[], bool castShow[], uint size);

void Animate(List *cast, bool doit)
{
    AniObj    *dup;
    Obj       *him;
    Node      *it;
    RGrafPort *oldPort;
    View      *view;
    RRect      ur, rns, rls;
    uint       i, castSize;
    uint       signal, loopNum, celNum;
    uint       showAll = g_picNotValid;
    Obj       *castID[200];
    int        castY[200];
    bool       castShow[200];

    if (cast == NULL) {
        // No castList -- just show picture (if necessary).
        DisposeLastCast();
        if (g_picNotValid != 0) {
            ShowPic(g_showMap, g_showPicStyle);
            g_picNotValid = 0;
        }
        return;
    }

    // Invoke doit: method for each member of the cast unless they are
    // identified as a static view.
    if (doit) {
        for (it = FirstNode(cast); it != NULL; it = NextNode(it)) {
            if (g_globalVars[g_fastCast] != 0) {
                return;
            }

            him = (Obj *)GetKey(it);

            if ((IndexedProp(him, actSignal) & staticView) == 0) {
                InvokeMethod(him, s_doit, 0);
            }
        }
    }

    RGetPort(&oldPort);
    RSetPort(&g_picWind->port);

    // Dispose the old castList (if any) and get a new one.
    DisposeLastCast();
    InitList(&s_lastCast);

    //
    // The screen is clean except for stopped actors.
    // Validate LOOP/CEL and update nowSeen for ALL actors in the cast.
    // Set castID array for all subsequent access to the cast.
    //
    castSize = 0;
    for (it = FirstNode(cast); it != NULL; it = NextNode(it)) {
        him = (Obj *)GetKey(it);

        // Save their object IDs in castID array.
        castID[castSize] = him;

        // Save their y positions in castY array.
        castY[castSize] = IndexedProp(him, actY);

        // Zero out every show (no matter what sort does).
        castShow[castSize] = 0;

        ++castSize;

        signal = (uint)IndexedProp(him, actSignal);
        // This handle will be valid for a while.
        view = (View *)ResLoad(RES_VIEW, IndexedProp(him, actView));

        // Check valid loop and cel.
        loopNum = (uint)IndexedProp(him, actLoop);
        if (loopNum >= GetNumLoops(view)) {
            loopNum                   = 0;
            IndexedProp(him, actLoop) = loopNum;
        }

        celNum = IndexedProp(him, actCel);
        if (celNum >= GetNumCels(view, loopNum)) {
            celNum                   = 0;
            IndexedProp(him, actCel) = celNum;
        }

        GetCelRectNative(view,
                         loopNum,
                         celNum,
                         (int)IndexedProp(him, actX),
                         (int)IndexedProp(him, actY),
                         (int)IndexedProp(him, actZ),
                         IndexedPropAddr(him, actNS));

        // If fixPriority bit in signal is clear we get priority for this line.
        if ((signal & FIXPRI) == 0) {
            IndexedProp(him, actPri) = CoordPri((int)IndexedProp(him, actY));
        }

        // Deal with the "signal" bits affecting stop updated actors in cast.
        // Turn off invalid bits for either condition
        if ((signal & NOUPDATE) != 0) {
            // STOPPED actors only care about STARTUPD, FORCEUPD and HIDE/HIDDEN
            // bits.
            if (((signal & (STARTUPD | FORCEUPD)) != 0) ||
                ((signal & HIDE) != 0 && (signal & HIDDEN) == 0) ||
                ((signal & HIDE) == 0 && (signal & HIDDEN) != 0) ||
                ((signal & VIEWADDED) != 0)) {
                ++showAll;
            }
            signal &= ~STOPUPD;
        } else {
            // nonSTOPPED actors care about being stopped or added.
            if ((signal & STOPUPD) != 0 || (signal & VIEWADDED) != 0) {
                ++showAll;
            }
            signal &= ~FORCEUPD;
        }

        // Update changes to signal.
        IndexedProp(him, actSignal) = signal;
    }

    // Sort the actor list by y coord.
    SortCast(castID, castY, castSize);

    // If showAll is true we must save a "clean" underbits of all stopped
    // actors. Any newly stopped actors are treated equally.
    if (showAll != 0) {
        ReDoStopped(castID, castShow, castSize);
    }

    // Draw nonSTOPPED nonHIDDEN cast from front to back of sorted list
    // (forward).
    for (i = 0; i < castSize; ++i) {
        him    = castID[i];
        signal = (uint)IndexedProp(him, actSignal);

        // This handle will be valid for a while.
        view = (View *)ResLoad(RES_VIEW, IndexedProp(him, actView));

        // All stopped actors have already been drawn.
        // A HIDE actor is not drawn.
        // A VIEWADDED is not drawn.
        if ((signal & (VIEWADDED | NOUPDATE | HIDE)) == 0) {
            RectFromNative(IndexedPropAddr(him, actNS), &rns);
            IndexedProp(him, actUB) =
              (uintptr_t)SaveBits(&rns, PMAP | VMAP | CMAP);

            DrawCel(view,
                    (uint)IndexedProp(him, actLoop),
                    (uint)IndexedProp(him, actCel),
                    &rns,
                    (uint)IndexedProp(him, actPri),
                    (RPalette *)GetProperty(him, s_palette));

            castShow[i] = true;

            // This actor was drawn so we must unHIDE him.
            if ((signal & HIDDEN) != 0 && (signal & HIDE) == 0) {
                signal &= ~HIDDEN;
                // Update changes to signal.
                IndexedProp(him, actSignal) = signal;
            }

            // Add an entry to ReAnimate list.
            dup      = (AniObj *)malloc(sizeof(AniObj));
            dup->v   = (uint)IndexedProp(him, actView);
            dup->l   = (uint)IndexedProp(him, actLoop);
            dup->c   = (uint)IndexedProp(him, actCel);
            dup->p   = (uint)IndexedProp(him, actPri);
            dup->pal = (RPalette *)GetProperty(him, s_palette);
            RCopyRect(&rns, &dup->r);
            AddToEnd(&s_lastCast, ToNode(dup));
        }
    }

    // Do we show the screen here.
    if (g_picNotValid != 0) {
        ShowPic(g_showMap, g_showPicStyle);
        g_picNotValid = 0;
    }

    // Now once more through the list to show the bits we have drawn.
    for (i = 0; i < castSize; ++i) {
        him    = castID[i];
        signal = (uint)IndexedProp(him, actSignal);

        // "stopped" actors don't need showing, unless a newStop has been done.
        // Hidden actors don't need it either.
        if ((castShow[i]) || ((signal & HIDDEN) == 0 &&
                              ((signal & NOUPDATE) == 0 || showAll != 0))) {
            RectFromNative(IndexedPropAddr(him, actLS), &rls);
            RectFromNative(IndexedPropAddr(him, actNS), &rns);

            // Update visual screen.
            if (RSectRect(&rls, &rns, &ur)) {

                // Show the union.
                RUnionRect(&rls, &rns, &ur);

                ShowBits(&ur, g_showMap);
            } else {
                // Show each separately.
                ShowBits(&rns, g_showMap);
                ShowBits(&rls, g_showMap);
            }

            RectToNative(&rns, IndexedPropAddr(him, actLS));

            // Now reflect hidden status.
            if ((signal & HIDE) != 0 && (signal & HIDDEN) == 0) {
                signal |= HIDDEN;
            }
        }

        // Update any changes.
        IndexedProp(him, actSignal) = signal;
    }

    //
    // Now after all our hard work we are going to erase the entire list,
    // except for NOUPDATE actors.
    //
    for (i = castSize - 1; (int)i >= 0; --i) {
        him = castID[i];

        //
        // Erase him from background.
        //

        signal = (uint)IndexedProp(him, actSignal);
        if (!((signal & NOUPDATE) != 0 || (signal & HIDDEN) != 0)) {
            RestoreBits((Handle)IndexedProp(him, actUB));
            IndexedProp(him, actUB) = 0;
        }

        // Invoke the delete method of certain objects.
        if ((signal & delObj) != 0) {
            InvokeMethod(him, s_delete, 0);
        }
    }

    // Restore current port.
    RSetPort(oldPort);
}

void ReAnimate(const RRect *badRect)
{
    RGrafPort *oldPort;
    AniObj    *dupPtr;
    View      *view;
    RRect      bRect;
    Node      *dup;

    // Bad rect may be in Local Coords of another window (port),
    // we must make them local to this port.
    RCopyRect(badRect, &bRect);

    RLocalToGlobal((RPoint *)&(bRect.top));
    RLocalToGlobal((RPoint *)&(bRect.bottom));
    RGetPort(&oldPort);
    RSetPort(&g_picWind->port);
    RGlobalToLocal((RPoint *)&(bRect.top));
    RGlobalToLocal((RPoint *)&(bRect.bottom));

    // This is valid only if we have a s_lastCast.
    if (IsListInitialized(&s_lastCast)) {
        for (dup = FirstNode(&s_lastCast); dup != NULL; dup = NextNode(dup)) {
            dupPtr = FromNode(dup, AniObj);

            view = (View *)ResLoad(RES_VIEW, dupPtr->v);

            dupPtr->u = SaveBits(&dupPtr->r, VMAP | PMAP);
            DrawCel(
              view, dupPtr->l, dupPtr->c, &dupPtr->r, dupPtr->p, dupPtr->pal);
        }

        // Show the disturbed rectangle.
        ShowBits(&bRect, g_showMap);

        // Now erase them again.
        for (dup = LastNode(&s_lastCast); dup != NULL; dup = PrevNode(dup)) {
            dupPtr = FromNode(dup, AniObj);
            RestoreBits(dupPtr->u);
        }
    } else {
        // Show the disturbed rectangle.
        ShowBits(&bRect, g_showMap);
    }

    RSetPort(oldPort);
}

void AddToPic(List *cast)
{
    Obj       *him;
    Node      *it;
    RGrafPort *oldPort;
    RRect      r;
    View      *view;
    uint       i, castSize, signal, priority;
    int16_t    top;
    Obj       *castID[200];
    int        castY[200];

    if (cast == NULL) {
        return;
    }

    RGetPort(&oldPort);
    RSetPort(&g_picWind->port);

    // Set castID array for all subsequent access to the cast.
    castSize = 0;
    for (it = FirstNode(cast); it != NULL; it = NextNode(it)) {
        him = (Obj *)GetKey(it);

        // Save their object IDs in castID array.
        castID[castSize] = him;

        // Save their y positions in castY array.
        castY[castSize] = (int)GetProperty(him, s_y);
        ++castSize;
    }

    // Sort the actor list by y coord.
    SortCast(castID, castY, castSize);

    // Now draw the objects from back to front.
    for (i = 0; i < castSize; ++i) {
        him = castID[i];

        signal = (uint)GetProperty(him, s_signal);

        // Get the cel rectangle.
        view = (View *)ResLoad(RES_VIEW, GetProperty(him, s_view));
        GetCelRect(view,
                   (uint)GetProperty(him, s_loop),
                   (uint)GetProperty(him, s_cel),
                   (int)GetProperty(him, s_x),
                   (int)GetProperty(him, s_y),
                   (int)GetProperty(him, s_z),
                   &r);

        // Get a priority.
        if (INVALIDPRI == (priority = GetProperty(him, s_priority))) {
            priority = CoordPri(castY[i]);
        }

        DrawCel(view,
                (uint)GetProperty(him, s_loop),
                (uint)GetProperty(him, s_cel),
                &r,
                priority,
                (RPalette *)GetProperty(him, s_palette));

        // If ignrAct bit is not set we draw a control box.
        if ((signal & ignrAct) == 0) {
            // Compute for priority that he is.
            top = (int16_t)PriCoord(priority) - 1;

            // Adjust top accordingly.
            if (top < r.top) {
                top = r.top;
            }

            if (top >= r.bottom) {
                top = r.bottom - 1;
            }

            r.top = top;
            RFillRect(&r, CMAP, 0, 0, 15);
        }
    }
}

void DisposeLastCast(void)
{
    Node *dup;

    if (!IsListInitialized(&s_lastCast)) {
        return;
    }

    while ((dup = FirstNode(&s_lastCast)) != NULL) {
        DeleteNode(&s_lastCast, dup);
        free(FromNode(dup, AniObj));
    }

    UnInitList(&s_lastCast);
}

static void SortCast(Obj *cast[], int casty[], uint size)
{
    bool swapped = true;
    int  i, j;
    Obj *tmp;
    int  tmpint;

    for (j = ((int)size - 1); j > 0; --j) {
        swapped = false;
        for (i = 0; i < j; ++i) {
            // Base it on y position.
            if ((casty[i + 1] < casty[i]) ||
#ifdef SORTONZTOO
                (casty[i + 1] == casty[i] &&
                 GetProperty(cast[i + 1], s_z) < GetProperty(cast[i], s_z))
#endif
            ) {
                tmp         = cast[i];
                cast[i]     = cast[i + 1];
                cast[i + 1] = tmp;

                // Exchange y positions also.
                tmpint       = casty[i];
                casty[i]     = casty[i + 1];
                casty[i + 1] = tmpint;

                swapped = true;
            }
        }

        if (!swapped) {
            break;
        }
    }
}

static void ReDoStopped(Obj *cast[], bool castShow[], uint size)
{
    Obj    *him;
    Handle  underBits;
    RRect   r;
    View   *view;
    uint    i, signal, whichMaps;
    int16_t top;

    // Save windows so that stopUpd'd actor don't show through.
    RBeginUpdate(g_picWind);

    // To ReDo stopped actors, we must TOSS all backgrounds in reverse.
    for (i = size - 1; (int)i >= 0; --i) {
        him    = cast[i];
        signal = (uint)IndexedProp(him, actSignal);
        if ((signal & NOUPDATE) != 0) {
            if ((signal & HIDDEN) == 0) {
                underBits = (Handle)IndexedProp(him, actUB);
                // If a new picture.
                if (g_picNotValid == 1) {
                    if (underBits != NULL) {
                        UnloadBits(underBits);
                    }
                } else {
                    RestoreBits(underBits);
                    castShow[i] = true;
                }

                IndexedProp(him, actUB) = 0;
            }

            // Now is when we catch START updates and HIDE/SHOWs.
            if ((signal & FORCEUPD) != 0) {
                signal &= ~FORCEUPD;
            }

            // Logic must respect transition to UPDATING.
            if ((signal & STARTUPD) != 0) {
                signal &= (~(STARTUPD | NOUPDATE));
            }
        } else {
            // An updating actor that may want to stop.
            if ((signal & STOPUPD) != 0) {
                signal = (signal & ~STOPUPD) | NOUPDATE;
            }
        }

        // Update signal property.
        IndexedProp(him, actSignal) = signal;
    }

    // The screen is absolutely clean at this point.
    // Draw in ANY actors with a view added property and condition signal.
    for (i = 0; i < size; ++i) {
        him    = cast[i];
        signal = (uint)IndexedProp(him, actSignal);
        if ((signal & VIEWADDED) != 0) {
            view = (View *)ResLoad(RES_VIEW, IndexedProp(him, actView));
            RectFromNative(IndexedPropAddr(him, actNS), &r);
            DrawCel(view,
                    (uint)IndexedProp(him, actLoop),
                    (uint)IndexedProp(him, actCel),
                    &r,
                    (uint)IndexedProp(him, actPri),
                    (RPalette *)GetProperty(him, s_palette));

            castShow[i] = true;

            signal &= (~(NOUPDATE | STOPUPD | STARTUPD | FORCEUPD));

            // If ignrAct bit is not set we draw a control box.
            if ((signal & ignrAct) == 0) {
                // Compute for4 priority that he is.
                top = (int16_t)PriCoord((uint)IndexedProp(him, actPri)) - 1;

                /* adjust top accordingly */
                if (top < r.top) {
                    top = r.top;
                }

                if (top >= r.bottom) {
                    top = r.bottom - 1;
                }

                r.top = top;
                RFillRect(&r, CMAP, 0, 0, 15);
            }
        }

        IndexedProp(him, actSignal) = signal;
    }

    // Now save a clean copy of underbits for NOUPDATE / nonHIDDEN actors.
    for (i = 0; i < size; ++i) {
        him    = cast[i];
        signal = (uint)IndexedProp(him, actSignal);
        if ((signal & NOUPDATE) != 0) {
            // Make HIDE/SHOW decision first.
            if ((signal & HIDE) != 0) {
                signal |= HIDDEN;
            } else {
                signal &= ~HIDDEN;
            }
            IndexedProp(him, actSignal) = signal;
            if ((signal & HIDDEN) == 0) {
#ifdef PLENTYROOM
                whichMaps = PMAP | VMAP;
                if ((signal & ignrAct) == 0) {
                    whichMaps |= CMAP;
                }

                IndexedProp(him, actUB) = (uintptr_t)SaveBits(
                  RectFromNative(IndexedPropAddr(him, actNS), &r), whichMaps);
#else
                IndexedProp(him, actUB) = (uintptr_t)SaveBits(
                  RectFromNative(IndexedPropAddr(him, actNS), &r),
                  VMAP | PMAP | CMAP);
#endif
            }
        }
    }

    // Now redraw them at nowSeen from head to tail of list.
    for (i = 0; i < size; i++) {
        him    = cast[i];
        signal = (uint)IndexedProp(him, actSignal);
        if ((signal & NOUPDATE) != 0 && (signal & HIDE) == 0) {
            view = (View *)ResLoad(RES_VIEW, IndexedProp(him, actView));

            //
            // Get nowSeen into local and draw it.
            //
            RectFromNative(IndexedPropAddr(him, actNS), &r);

            DrawCel(view,
                    (uint)IndexedProp(him, actLoop),
                    (uint)IndexedProp(him, actCel),
                    &r,
                    (uint)IndexedProp(him, actPri),
                    (RPalette *)GetProperty(him, s_palette));

            castShow[i] = true;

            // If ignrAct bit is not set we draw a control box.
            if ((signal & ignrAct) == 0) {
                // Compute for4 priority that he is.
                top = (int16_t)PriCoord((uint)IndexedProp(him, actPri)) - 1;

                // Adjust top accordingly.
                if (top < r.top) {
                    top = r.top;
                }

                if (top >= r.bottom) {
                    top = r.bottom - 1;
                }

                r.top = top;
                RFillRect(&r, CMAP, 0, 0, 15);
            }
        }
        IndexedProp(him, actSignal) = signal;
    }

    // Redraw windows over already drawn stopUpd'ers.
    REndUpdate(g_picWind);
}
