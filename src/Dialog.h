#ifndef DIALOG_H
#define DIALOG_H

#include "Event.h"

// Item types
#define dButton   1
#define dText     2
#define dEdit     3
#define dIcon     4
#define dMenu     5
#define dSelector 6
#define dScroller 7

// Control states
#define dActive    0x0001
#define dExit      0x0002
#define dBold      0x0004
#define dSelected  0x0008
#define dIconTop   0x0010
#define dFrameRect 0x0020

void InitDialog(boolfptr proc);

void DrawCursor(const RRect *box, const char *textBuf, uint cursor);

void EraseCursor(void);

int EditControl(Obj *item, Obj *evt);

// Draw this control.
void DrawControl(Obj *item);

// Highlight (or un-highlight) the control.
void RHiliteControl(Obj *item);

// Make this global coord local.
void RGlobalToLocal(RPoint *mp);

// Make this local coord global.
void RLocalToGlobal(RPoint *mp);

uint EditText(RRect *box, char *text, uint cursor, uint max, REventRecord *evt);

#endif // DIALOG_H
