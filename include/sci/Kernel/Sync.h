#ifndef SCI_KERNEL_SYNC_H
#define SCI_KERNEL_SYNC_H

#include "sci/PMachine/Object.h"

// sync cue structure
typedef struct Sync {
    uint16_t time; // absolute frame time to cue
    uint16_t cue;  // cue number to send at frame time
} Sync;

enum syncFuncs { STARTSYNC, NEXTSYNC, STOPSYNC };

void KDoSync(uintptr_t *args);

#endif // SCI_KERNEL_SYNC_H
