#ifndef MIDIDRIVER_H
#define MIDIDRIVER_H

#include "Types.h"

// Driver Function Definitions.
#define DPatchReq      0
#define DInit          1
#define DTerminate     2
#define DService       3
#define DNoteOff       4
#define DNoteOn        5
#define DPolyAfterTch  6
#define DController    7
#define DProgramChange 8
#define DChnlAfterTch  9
#define DPitchBend     10
#define DSetReverb     11
#define DMasterVol     12
#define DSoundOn       13
#define DSamplePlay    14
#define DSampleEnd     15
#define DSampleCheck   16
#define DAskDriver     17

uint Driver(int      function,
            uint16_t channel,
            uint8_t  data1,
            uint8_t  data2); // ax, ch, cl

#endif // MIDIDRIVER_H
