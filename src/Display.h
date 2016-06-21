#ifndef DISPLAY_H
#define DISPLAY_H

#include "GrTypes.h"
#include "Palette.h"

extern HDC   g_hDcWnd;
extern HGLRC g_hWgl;

void InitDisplay(void);
void EndDisplay(void);

void Display(int      top,
             int      left,
             int      bottom,
             int      right,
             uint8_t *bits,
             uint     mapSet);

void GetDisplayCLUT(RPalette *pal);

void SetDisplayCLUT(const RPalette *pal);

bool DisplayCursor(bool show);

#endif // DISPLAY_H
