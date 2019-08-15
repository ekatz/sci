#ifndef SCI_KERNEL_RESTART_H
#define SCI_KERNEL_RESTART_H

#include "sci/Kernel/Kernel.h"
#include <setjmp.h>

extern int     g_gameRestarted;
extern jmp_buf g_restartBuf;

void KRestartGame(argList);
void KGameIsRestarting(argList);

#endif // SCI_KERNEL_RESTART_H
