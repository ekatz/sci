#ifndef MIDI_H
#define MIDI_H

#include "Types.h"

// MIDI Parser Functions.
#define SPatchReq      0
#define SInit          1
#define STerminate     2
#define SProcess       3
#define SSoundOn       4
#define SRestore       5
#define SMasterVol     6
#define SSetReverb     7
#define SPlay          8
#define SEnd           9
#define SPause         10
#define SFade          11
#define SHold          12
#define SMute          13
#define SChangeVol     14
#define SChangePri     15
#define SGetSignal     16
#define SGetDataInc    17
#define SGetSYMPTE     18
#define SNoteOff       19
#define SNoteOn        20
#define SController    21
#define SPChange       22
#define SPBend         23
#define SAskDriver     24
#define SGetSignalRset 25

// MIDI Commands.
#define NOTEOFF    0x80 //  Note off
#define NOTEON     0x90 //  Note on
#define POLYAFTER  0xa0 //  Poly aftertouch
#define CONTROLLER 0xb0 //  Controller
#define PCHANGE    0xc0 //  Program change
#define CHNLAFTER  0xd0 //  Channel aftertouch
#define PBEND      0xe0 //  Pitch bend
#define SYSEX      0xf0 //  System exclusive (start of message)
#define EOX        0xf7 //  System exclusive (end of transmission)
#define TIMINGOVER 0xf8 //  Timing overflow (counts as 240)
#define ENDTRK     0xfc //  End of track

int DoSound(int function, ...);

void InstallSoundServer(void);

#endif // MIDI_H
