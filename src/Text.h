#ifndef TEXT_H
#define TEXT_H

#include "GrTypes.h"

// Text justification.
#define TEJUSTLEFT   0
#define TEJUSTCENTER 1
#define TEJUSTRIGHT  (-1)

uint RStringWidth(const char *str);

// Draw and this string.
void DrawString(const char *str);

// Draw and show this string.
void ShowString(const char *str);

// Make r large enough to hold text.
void RTextSize(RRect *r, const char *text, int font, int def);

// Put the text to the box in mode requested.
void RTextBox(const char  *text,
              bool         show,
              const RRect *box,
              uint         mode,
              int          font);

void RDrawText(const char *str, uint first, uint count);

void ShowText(const char *str, uint first, uint count);

const char *GetTextPointer(uintptr_t native);

#endif // TEXT_H
