#ifndef DIALOG_H
#define DIALOG_H

#include "GrTypes.h"

void InitDialog(boolfptr proc);

// Make this global coord local.
void RGlobalToLocal(RPoint *mp);

// Make this local coord global.
void RLocalToGlobal(RPoint *mp);

#endif // DIALOG_H
