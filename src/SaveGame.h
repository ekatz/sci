#ifndef SAVEGAME_H
#define SAVEGAME_H

#include "Kernel.h"

void KSaveGame(argList);
void KRestoreGame(argList);
uintptr_t KGetSaveDir(argList);
void KCheckSaveGame(argList);
void KGetSaveFiles(argList);

#endif // SAVEGAME_H
