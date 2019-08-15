#include "Dialog.h"
#include "Cels.h"
#include "ErrMsg.h"
#include "Event.h"
#include "Graphics.h"
#include "Picture.h"
#include "Resource.h"
#include "Selector.h"
#include "Text.h"
#include "Timer.h"
#include "Window.h"

static bool  s_curOn   = false;
static uint  s_flash   = 0;
static RRect s_curRect = { 0 };

static void DrawSelector(Obj *item);
static void FlashCursor(void);
static void ToggleCursor(void);
static void SetFlash(void);
static void TextEdit(Obj *item, Obj *evt);

void InitDialog(boolfptr proc)
{
    if (proc != NULL) {
        // Load and lock FONT.000.
        ResLoad(RES_FONT, 0);
        ResLock(RES_FONT, 0, true);

        // Now set the alert procedure.
        SetAlertProc(proc);
    }
}

void DrawCursor(const RRect *box, const char *textBuf, uint cursor)
{
    const char *text;

    if (!s_curOn) {
        s_curRect.left   = box->left + (int16_t)RTextWidth(textBuf, 0, cursor);
        s_curRect.top    = box->top;
        s_curRect.bottom = s_curRect.top + (int16_t)GetPointSize();

        // We flash the character cel unless we are at end of string.
        text = textBuf + cursor;
        s_curRect.right =
          s_curRect.left + ((*text != '\0') ? (int16_t)RCharWidth(*text) : 1);

        ToggleCursor();
    }
    s_curOn = true;
    SetFlash();
}

void EraseCursor(void)
{
    if (s_curOn) {
        ToggleCursor();
    }
    s_curOn = false;
    SetFlash();
}

static void FlashCursor(void)
{
    if (s_flash < RTickCount()) {
        ToggleCursor();
        s_curOn = !s_curOn;
        SetFlash();
    }
}

static void ToggleCursor(void)
{
    RInvertRect(&s_curRect);
    ShowBits(&s_curRect, VMAP);
}

static void SetFlash(void)
{
    s_flash = 30L + RTickCount();
}

int EditControl(Obj *item, Obj *evt)
{
    uint oldFont;

    // If this is a NULL control (zero) exit.
    if (item == NULL) {
        return 0;
    }

    switch (GetProperty(item, s_type)) {
        case dEdit:
            oldFont = GetFont();
            RSetFont(GetProperty(item, s_font));

            // We are ready to deal with the event.
            if (evt == NULL) {
                EraseCursor();
            } else {
                switch (GetProperty(evt, s_type)) {
                    case keyDown:
                        switch (GetProperty(evt, s_message)) {
                            case ESC:
                            case CR:
                                EraseCursor();
                                break;

#if 0
                case CTRL_CHAR:
                    // Ignore | since it has special meaning to text Output routines.
                    break;
#endif

                            default:
                                TextEdit(item, evt);
                                break;
                        }
                        break;

                    default:
                        TextEdit(item, evt);
                        break;
                }
            }

            // Restore old font.
            RSetFont(oldFont);
            break;
    }
    return 0;
}

void DrawControl(Obj *item)
{
    RRect       r, ur;
    const char *text = NULL;
    uint        font = 0, oldFont, type, state;

    type = (uint)GetProperty(item, s_type);
    if (type == dEdit) {
        EraseCursor();
    }
    state = (uint)GetProperty(item, s_state);

    // All items need their now seen rectangles sized externally.
    RectFromNative(GetPropAddr(item, s_nowSeen), &r);

    // Get some properties up front.
    if (RespondsTo(item, s_text)) {
        text = (const char *)GetProperty(item, s_text);
    }
    if (RespondsTo(item, s_font)) {
        font = GetProperty(item, s_font);
    }

    // Leave ur set to encompass the entire area affected.
    switch (type) {
        case dSelector:
        case dScroller:
            RInsetRect(&r, -1, -1);
            RCopyRect(&r, &ur);
            DrawSelector(item);
            break;

        case dButton:
            // Ensure that minimum control area is clear.
            RInsetRect(&r, -1, -1);
            REraseRect(&r);
            //      RFillRect(&r, VMAP, 8);  // Used to be green, now lt black
            RFrameRect(&r);
            RCopyRect(&r, &ur);

            // Box is sized 1 larger in interface.
            RInsetRect(&r, 2, 2);
            // PenColor(0);
            if ((dActive & state) != 0) {
                RTextFace(PLAIN);
            } else {
                RTextFace(DIM);
            }
            RTextBox(text, false, &r, TEJUSTCENTER, font);
            RTextFace(0);

            // Back it out for frame.
            RInsetRect(&r, -1, -1);
            break;

        case dText:
            RInsetRect(&r, -1, -1);
            REraseRect(&r);
            RInsetRect(&r, 1, 1);
            RTextBox(text, false, &r, (uint)GetProperty(item, s_mode), font);
            RCopyRect(&r, &ur);
            break;

        case dIcon:
            RCopyRect(&r, &ur);
            DrawCel(
              ResLoad(RES_VIEW, GetProperty(item, s_view)),
              GetProperty(item, s_loop),
              GetProperty(item, s_cel),
              &r,
              INVALIDPRI, // needed for warren GetProperty(item, s_priority),
              NULL);
            if ((dFrameRect & state) != 0) {
                RFrameRect(&r);
            }
            break;

        case dEdit:
            // Ensure that minimum control area is clear.
            REraseRect(&r);
            RInsetRect(&r, -1, -1);
            RFrameRect(&r);
            RCopyRect(&r, &ur);
            RInsetRect(&r, 1, 1);
            RTextBox(text, false, &r, TEJUSTLEFT, font);
            break;

        default:
            break;
    }

    // Mark a "selected" control.
    if ((dSelected & state) != 0) {
        switch (type) {
            case dEdit:
                oldFont = GetFont();
                RSetFont(font);
                DrawCursor(&r, text, (uint)GetProperty(item, s_cursor));
                RSetFont(oldFont);
                break;

            case dSelector:
                break;

            case dIcon:
                break;

            default:
                RFrameRect(&r);
                break;
        }
    }

    // Show the item's complete rectangle if NOT picNotValid.
    if (g_picNotValid == 0) {
        ShowBits(&ur, VMAP);
    }
}

static void DrawSelector(Obj *item)
{
    int16_t     oldTop;
    uint        i;
    RRect       r;
    const char *str;
    uint        oldFont, font;
    uint8_t     fore, back;
    uint        width, len;

    RectFromNative(GetPropAddr(item, s_nowSeen), &r);
    // Ensure that minimum control area is clear.
    REraseRect(&r);
    RInsetRect(&r, -1, -1);
    RFrameRect(&r);
    RTextBox("\x18", false, &r, TEJUSTCENTER, 0);
    oldTop = r.top;
    r.top  = r.bottom - 10;
    RTextBox("\x19", false, &r, TEJUSTCENTER, 0);
    r.top = oldTop;
    RInsetRect(&r, 0, 10);
    RFrameRect(&r);
    RInsetRect(&r, 1, 1);

    oldFont = GetFont();
    RSetFont(font = (uint)GetProperty(item, s_font));

    // Get colors for inverting.
    back = g_rThePort->bkColor;
    fore = g_rThePort->fgColor;

    // Ready to draw contents of selector box.
    r.bottom = r.top + (int16_t)GetPointSize();
    str      = (const char *)GetProperty(item, s_topString);
    width    = (uint)GetProperty(item, s_x);
    for (i = 0; i < (uint)GetProperty(item, s_y); i++) {
        REraseRect(&r);
        if (*str != '\0') {
            RMoveTo(r.left, r.top);
            // Allow for both fixed length and null-terminated strings
            // (this relies on the fact that the entire array of selector
            // strings must be terminated with a null.
            len = (uint)strlen(str);
            RDrawText(str, 0, width < len ? width : len);
            // Invert current selection line.
            if ((str == (const char *)GetProperty(item, s_cursor)) &&
                (GetProperty(item, s_type) != dScroller)) {
                RInvertRect(&r);
            }

            // Now set them back regardless.
            PenColor(fore);
            RBackColor(back);

            // Advance pointer to start of next string.
            str += GetProperty(item, s_x);
        }
        ROffsetRect(&r, 0, (int)GetPointSize());
    }

    // Restore ports current font.
    RSetFont(oldFont);
}

void RHiliteControl(Obj *item)
{
    RRect r;

    // All items need their now seen rectangles sized externally.
    RectFromNative(GetPropAddr(item, s_nowSeen), &r);

    // Leave r set to encompass the entire area.
    switch (GetProperty(item, s_type)) {
        case dText:
        case dButton:
        case dEdit:
        case dIcon:
        case dMenu:
            RInvertRect(&r);
            break;

        default:
            break;
    }

    // Show the rectangle we affected.
    ShowBits(&r, VMAP);
}

void RGlobalToLocal(RPoint *mp)
{
    mp->h -= g_rThePort->origin.h;
    mp->v -= g_rThePort->origin.v;
}

void RLocalToGlobal(RPoint *mp)
{
    mp->h += g_rThePort->origin.h;
    mp->v += g_rThePort->origin.v;
}

static void TextEdit(Obj *item, Obj *evt)
{
    uint         cursor;
    RRect        box;
    REventRecord theEvent;

    // Get properties into locals.
    RectFromNative(GetPropAddr(item, s_nowSeen), &box);
    ObjToEvent(evt, &theEvent);

    cursor = EditText(&box,
                      (char *)GetProperty(item, s_text),
                      (uint)GetProperty(item, s_cursor),
                      (uint)GetProperty(item, s_max),
                      &theEvent);

    SetProperty(item, s_cursor, cursor);
}

uint EditText(RRect *box, char *text, uint cursor, uint max, REventRecord *evt)
{
    RPoint mp;
    ushort msg;
    uint   oldCursor, i;
    bool delete, changed;
    uint textLen;

    oldCursor = cursor;
    changed   = false;
    delete    = false;
    textLen   = (uint)strlen(text);

    switch (evt->type) {
        case keyDown:
            switch (msg = evt->message) {
                case HOMEKEY:
                    // Beginning of line.
                    cursor = 0;
                    break;

                case ENDKEY:
                    // End of line.
                    cursor = textLen;
                    break;

                case CLEARKEY: // Control C.
                               // Clear line.
                    cursor  = 0;
                    *text   = 0;
                    changed = true;
                    break;

                case BS:
                    // Destructive backspace.
                    delete = true;
                    if (cursor != 0) {
                        --cursor;
                    }
                    break;

                case LEFTARROW:
                    // Non-destructive backspace.
                    if (cursor != 0) {
                        --cursor;
                    }
                    break;

                case DELETE:
                    // Delete at cursor.
                    if (cursor != textLen) {
                        delete = true;
                    }
                    break;

                case RIGHTARROW:
                    if (cursor < textLen) {
                        ++cursor;
                    }
                    break;

                default:
                    if (msg >= (ushort)' ' && msg < (ushort)257) {
                        // Insert this key and advance cursor
                        // if we have room in buffer AND we won't try to
                        // draw outside of our enclosing rectangle.
                        if ((textLen + 1) < max &&
                            (int)(RCharWidth((char)msg) + RStringWidth(text)) <
                              (int)(box->right - box->left)) {
                            changed = true;
                            // Shift it up one.
                            for (i = textLen; (int)i >= (int)cursor; i--) {
                                *(text + i + 1) = *(text + i);
                            }
                            *(text + cursor) = (char)msg;
                            ++(cursor);
                        }
                    }
                    break;
            }

            // If delateAt, we delete the character at cursor.
            if (delete) {
                changed = true;
                // Collapse the string from cursor on.
                for (i = cursor; i < textLen; i++) {
                    *(text + i) = *(text + i + 1);
                }
            }

            break;

        case mouseDown:
            // Move cursor to closest character division.
            mp.h = evt->where.h;
            mp.v = evt->where.v;

            if (RPtInRect(&mp, box)) {
                for (cursor = textLen;
                     cursor != 0 &&
                     (box->left + (int16_t)RTextWidth(text, 0, cursor) - 1) >
                       mp.h;
                     --cursor)
                    ;
            }
            break;

        default:
            break;
    }

    if (changed) {
        // If we have changed we redraw the entire field in the text box.
        EraseCursor();
        REraseRect(box);
        RTextBox(text, false, box, TEJUSTLEFT, -1);
        ShowBits(box, VMAP);
        DrawCursor(box, text, cursor);
    } else if (oldCursor == cursor) {
        // Cursor is in the same place -- keep flashing.
        FlashCursor();
    } else {
        // Cursor has moved -- ensure it is on at new position.
        EraseCursor();
        DrawCursor(box, text, cursor);
    }

    return cursor;
}
