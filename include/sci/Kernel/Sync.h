#ifndef SYNC_H
#define SYNC_H

#include "Object.h"

// sync cue structure
typedef struct Sync {
    uint16_t time; // absolute frame time to cue
    uint16_t cue;  // cue number to send at frame time
} Sync;

enum syncFuncs { STARTSYNC, NEXTSYNC, STOPSYNC };

void KDoSync(uintptr_t *args);

#endif // SYNC_H
