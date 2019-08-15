#ifndef SCI_KERNEL_SAVEGAME_H
#define SCI_KERNEL_SAVEGAME_H

#include "sci/Kernel/Kernel.h"

void KSaveGame(argList);
void KRestoreGame(argList);
void KGetSaveDir(argList);
void KCheckSaveGame(argList);
void KGetSaveFiles(argList);

#endif // SCI_KERNEL_SAVEGAME_H
