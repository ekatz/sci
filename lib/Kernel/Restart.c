#include "Restart.h"
#include "Animate.h"
#include "Menu.h"
#include "Midi.h"
#include "PMachine.h"
#include "Resource.h"
#include "Sound.h"

#define ret(val) g_acc = ((uintptr_t)(val))

int     g_gameRestarted = 0;
jmp_buf g_restartBuf;

void KRestartGame(argList)
{
#ifndef NOT_IMPL
    DoSound(SProcess, FALSE);
    g_gameRestarted = 1;
    g_gameStarted   = FALSE;

    KillAllSounds();
    DisposeAllScripts();
    DisposeLastCast();

    // Free all memory.
    ResUnLoad(RES_MEM, ALL_IDS);

    // Regenerate the 'patchable resources' table.
    InitPatches();

    InitMenu();
    DoSound(SProcess, TRUE);
    DoSound(SSetReverb, 0);
    g_reverbDefault = 0;

    // Now restore the stack and restart the PMachine.
    longjmp(g_restartBuf, 1);
#endif
}

void KGameIsRestarting(argList)
{
    ret(g_gameRestarted);
    if (argCount != 0 && arg(1) == FALSE) {
        g_gameRestarted = 0;
    }
}
