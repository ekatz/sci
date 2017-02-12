#ifndef RESTART_H
#define RESTART_H

#include "Kernel.h"
#include <setjmp.h>

extern int     g_gameRestarted;
extern jmp_buf g_restartBuf;

void KRestartGame(argList);
uintptr_t KGameIsRestarting(argList);

#endif // RESTART_H
