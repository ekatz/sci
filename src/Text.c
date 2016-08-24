#include "Text.h"
#include "Event.h"
#include "Graphics.h"

// Return count of chars that fit in pixel length.
static uint GetLongest(char const **str, uint max);

uint RStringWidth(const char *str)
{
    return RTextWidth(str, 0, (uint)strlen(str));
}

void DrawString(const char *str)
{
    RDrawText(str, 0, (uint)strlen(str));
}

void ShowString(const char *str)
{
    ShowText(str, 0, (uint)strlen(str));
}

#define DEFWIDE 192

void RTextSize(RRect *r, const char *text, int font, int def)
{

    uint        length, longest, height, oldFont;
    const char *str;
    const char *first;

    // We are sizing this text in the font requested.
    oldFont = GetFont();

    if (font != -1) {
        RSetFont(font);
    }

    r->top  = 0;
    r->left = 0;

    if (def < 0) {
        // We don't want word wrap.
        r->bottom = (int16_t)GetPointSize();
        r->right  = (int16_t)RStringWidth(text);
    } else {
        if (def == 0) {
            // Use default width.
            r->right = DEFWIDE;
        } else {
            r->right = (int16_t)def;
        }

        //		do {
        // Get a local pointer to text.
        str     = text;
        height  = 0;
        longest = 0;
        while (*str != '\0') {
            first  = str;
            length = RTextWidth(first, 0, GetLongest(&str, (uint)r->right));
            height++;
            if (length > longest) {
                longest = length;
            }
        }
        height *= GetPointSize();

        if (def == 0 && r->right > (int16_t)longest) {
            r->right = (int16_t)longest;
        }
        r->bottom = (int16_t)height;
        //		} while (0);	// r->right / 2 < r->bottom);	 test
        //for ratio
    }

    // Restore old font.
    RSetFont(oldFont);
}

static uint GetLongest(char const **str, uint max)
{
    const char *last;
    const char *first;
    char        c;
    uint        count = 0, lastCount = 0;

    first = last = *str;

    // Find a HARD terminator or LAST SPACE that fits on line.
    while (true) {
        c = *(*str);
        if (c == CR) {
            if (*(*str + 1) == LF) {
                (*str)++; // so we don't see it later.
            }
            if (lastCount != 0 && max < RTextWidth(first, 0, count)) {
                *str = last;
                return lastCount;
            } else {
                (*str)++;     // so we don't see it later.
                return count; // caller sees end of string.
            }
        }

        if (c == LF) {
            if ((*(*str + 1) == CR) && (*(*str + 2) != LF)) // for 68k.
            {
                (*str)++; // so we don't see it later.
            }
            if (lastCount && max < RTextWidth(first, 0, count)) {
                *str = last;
                return lastCount;
            } else {
                (*str)++;     // so we don't see it later.
                return count; // caller sees end of string.
            }
        }

        if (c == '\0') {
            if (lastCount && max < RTextWidth(first, 0, count)) {
                *str = last;
                return lastCount;
            } else {
                return count; // caller sees end of string.
            }
        }

        if (c == SP) // check word wrap.
        {
            if (max >= RTextWidth(first, 0, count)) {
                last = *str;
                ++last; // so we don't see space again.
                lastCount = count;
            } else {
                *str = last;
                // eliminate trailing spaces
                while (**str == ' ') {
                    ++(*str);
                }
                return lastCount;
            }
        }

        // All is still cool.
        ++count;
        (*str)++;

        {
            // We may never see a space to break on.
            if (lastCount == 0 && RTextWidth(first, 0, count) > max) {
                last += --count;
                *str = last;
                return count;
            }
        }
    }
}

void RTextBox(const char  *text,
              bool         show,
              const RRect *box,
              uint         mode,
              int          font)
{
    const char *first;
    const char *str;
    uint        length, height = 0, wide, count, oldFont;
    int         xPos;

    // We are printing this text in the font requested.
    oldFont = GetFont();

    if (font != -1) {
        RSetFont((uint)font);
    }

    wide = (uint)(box->right - box->left);

    str = text;
    while (*str != '\0') {
        first  = str;
        count  = GetLongest(&str, wide);
        length = RTextWidth(first, 0, count);

        // Determine justification and draw the line.
        switch (mode) {
            case TEJUSTCENTER:
                xPos = (int)(wide - length) / 2;
                break;

            case TEJUSTRIGHT:
                xPos = (int)(wide - length);
                break;

            case TEJUSTLEFT:
            default:
                xPos = 0;
                break;
        }

        RMoveTo((int)box->left + xPos, (int)box->top + (int)height);
        if (show) {
            ShowText(first, 0, count);
        } else {
            RDrawText(first, 0, count);
        }
        height += GetPointSize();
    }

    // Restore old font.
    RSetFont(oldFont);
}

void RDrawText(const char *str, uint first, uint count)
{
    const char *last;

    str += first;
    last = str + count;
    while (str < last) {
        RDrawChar(*str++);
    }
}

void ShowText(const char *str, uint first, uint count)
{
    RRect r;

    r.top    = g_rThePort->pnLoc.v;
    r.bottom = r.top + (int16_t)GetPointSize();
    r.left   = g_rThePort->pnLoc.h;
    RDrawText(str, first, count);

    r.right = g_rThePort->pnLoc.h;
    ShowBits(&r, VMAP);
}

const char *GetTextPointer(uintptr_t native)
{
    if ((intptr_t)native < 0x10000) {
        return (const char *)g_scriptHeap + (uint16_t)native;
    } else {
        return (const char *)native;
    }
}
