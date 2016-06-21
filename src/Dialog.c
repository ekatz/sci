#include "Dialog.h"
#include "ErrMsg.h"
#include "Graphics.h"
#include "Resource.h"

void InitDialog(boolfptr proc)
{
    if (proc != NULL) {
        // Load and lock FONT.000.
        ResLoad(RES_FONT, 0);
        ResLock(RES_FONT, 0, true);

        // Now set the alert procedure.
        SetAlertProc(proc);
    }
}

void RGlobalToLocal(RPoint *mp)
{
    mp->h -= g_rThePort->origin.h;
    mp->v -= g_rThePort->origin.v;
}

void RLocalToGlobal(RPoint *mp)
{
    mp->h += g_rThePort->origin.h;
    mp->v += g_rThePort->origin.v;
}
