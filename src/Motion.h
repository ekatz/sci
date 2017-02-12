#ifndef MOTION_H
#define MOTION_H

#include "Kernel.h"

void KBaseSetter(argList);
void KDirLoop(argList);
uintptr_t KCantBeHere(argList);
void KInitBresen(argList);
uintptr_t KDoBresen(argList);
void KDoAvoider(argList);
void KSetJump(argList);

#endif // MOTION_H
