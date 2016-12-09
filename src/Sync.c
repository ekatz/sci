#include "Sync.h"
#include "ErrMsg.h"
#include "Kernel.h"
#include "PMachine.h"
#include "Resource.h"
#include "Selector.h"

static bool   s_syncing    = false;
static Handle s_syncHandle = NULL;
static uint   s_syncNum, s_syncIndex;

static void StopSync();

static void StartSync(Obj *theSync, uint num)
{
    if (s_syncHandle != NULL) {
        StopSync();
    }

    if ((s_syncHandle = ResLoad(RES_SYNC, num)) != NULL) {
        ResLock(RES_SYNC, s_syncNum = num, true);
        IndexedProp(theSync, syncCue) = 0;
        s_syncIndex                   = 0;
        s_syncing                     = true;
    } else {
        IndexedProp(theSync, syncCue) = (uint16_t)-1;
    }
}

static void NextSync(Obj *theSync)
{
    Sync      tsync;
    uint16_t *dp;

    if (s_syncHandle == NULL || s_syncIndex == (uint)-1) {
        return;
    }

    dp = (uint16_t *)s_syncHandle;
    tsync.time = dp[s_syncIndex++];
    if (tsync.time == (uint16_t)-1) {
        StopSync();
        tsync.cue   = (uint16_t)-1;
        s_syncIndex = (uint)-1;
    } else {
        tsync.cue = dp[s_syncIndex++];
    }
    IndexedProp(theSync, syncTime) = (uintptr_t)((int16_t)tsync.time);
    IndexedProp(theSync, syncCue)  = (uintptr_t)((int16_t)tsync.cue);
}

static void StopSync()
{
    if (s_syncHandle == NULL) {
        return;
    }

    ResUnLoad(RES_SYNC, s_syncNum);
    s_syncHandle = NULL;
    s_syncing    = false;
}

void KDoSync(argList)
{
    switch (arg(1)) {
        case STARTSYNC:
            StartSync((Obj *)arg(2), (uint)arg(3));
            break;

        case NEXTSYNC:
            NextSync((Obj *)arg(2));
            break;

        case STOPSYNC:
            StopSync();
            break;
    }
}
