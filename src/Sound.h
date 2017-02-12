#ifndef SOUND_H
#define SOUND_H

#include "List.h"
#include "Object.h"

// Defines for flags property of sound objects.
#define mNOPAUSE  0x0001
#define mFIXEDPRI 0x0002

enum SndFuncs {
    MASTERVOL,
    SOUNDON,
    RESTORESND,
    NUMVOICES,
    NUMDACS,
    SUSPEND,
    INITSOUND,
    KILLSOUND,
    PLAYSOUND,
    STOPSOUND,
    PAUSESOUND,
    FADESOUND,
    HOLDSOUND,
    MUTESOUND,
    SETVOL,
    SETPRI,
    SETLOOP,
    UPDATECUES,
    MIDISEND,
    SETREVERB,

    CHANGESNDSTATE
};

// Sound Node Definition.
typedef struct Sound {
    KeyedNode      link;           // Node header
    short          sNumber;        // SCI sound number
    uint8_t       *sPointer;       // Pointer to sound resource
    unsigned short tIndex[16];     // Index for 16 tracks
    unsigned short tLoopPoint[16]; // Loop RPoint for 16 tracks
    unsigned short tRest[16];      // Rest count for 16 tracks
    unsigned short tLoopRest[16];  // Rest count at loop for 16 tracks
    unsigned char  tChannel[16];   // Channel #'s for 16 tracks
    unsigned char  tCommand[16]; // Last command for 16 tracks (running status)
    unsigned char  tLoopCommand[16]; // Command at loop for 16 tracks
    unsigned short
                   cDamprPbend[15]; // Damper pedal and pitch bend for 15 channels
    unsigned char  cPriVoice[15]; // Priority & voice allocation for 15 channels
    unsigned char  cModulation[15]; // Modulation values for 15 channels
    unsigned char  cPan[15];        // Pan values for 15 channels
    unsigned char  cVolume[15];     // Volume values for 15 channels
    unsigned char  cProgram[15];    // Program values for 15 channels
    unsigned char  cCurNote[15];    // Current note being played
    unsigned char  cFlags[15];      // Channel Flags (Locked, Ghost)
    unsigned char  cMute[15];       // Channel mute counter (0 = not muted)
    unsigned short sDataInc;        // Current Data Increment cue value
    unsigned short sTimer;          // Age of sound (in 60th secs)
    unsigned short sLoopTime;       // Timer loop point
    unsigned char  sSignal;         // Sound signal
    unsigned char  sState;          // Flag is set if sound is playing
    unsigned char  sHold;           // Sound hold/release flag
    unsigned char  sFixedPri;       // Flags not to use sound file priority
    unsigned char  sPriority;       // Sound priority
    unsigned char  sLoop;           // Sound loops
    unsigned char  sVolume;         // Sound volume
    unsigned char  sReverbMode;     // Reverb setting
    unsigned char  sFadeDest;       // Fade destination
    unsigned char  sFadeTicks;      // Clock ticks before next fade
    unsigned char  sFadeCount;      // Fade tick counter
    unsigned char  sFadeSteps;      // Fade steps
    unsigned char  sPause;          // Pause flag
    unsigned char  sSample;         // Sample track + 1
} Sound;

extern int g_reverbDefault;

bool InitSoundDriver(void);

void TermSndDrv(void);

// Stop and delete each node in the sound list.
void KillAllSounds(void);

// Stop the sound and delete it's node.
void KillSnd(Obj *soundObj);

void RestoreAllSounds(void);

void SuspendSounds(bool onOff);

void InitSnd(Obj *soundObj);

void PlaySnd(Obj *soundObj, int how);

void StopSnd(Obj *soundObj);

// If the Object parameter is NULL, then we want to pause/unpause every node.
// If it is nonzero, pause/unpause only the node belonging to that object.
void PauseSnd(Obj *soundObj, bool stopStart);

// Fade the node belonging to the object specified.
void FadeSnd(Obj *soundObj, int newVol, int fTicks, int fSteps, int fEnd);

// Set the hold property of the sound node belonging to the specified object.
void HoldSnd(Obj *soundObj, int where);

// Change the volume of the sound node and the sound object to the value passed
// in.
void SetSndVol(Obj *soundObj, int newVol);

// Set the priority of the sound node to the value in newPri.
void SetSndPri(Obj *soundObj, int newPri);

// Set the loop property of the sound node and the sound object to the value
// passed in.
void SetSndLoop(Obj *soundObj, int newLoop);

// Copy the current cue, clock, and volume information from the sound node to
// it's object.
void UpdateCues(Obj *soundObj);

// Send MIDI a command to any channel of the node belonging to the specified
// object.
void MidiSend(Obj *soundObj, int channel, int command, int value1, int value2);

void ChangeSndState(Obj *soundObj);

uintptr_t KDoSound(uintptr_t *args);

#endif // SOUND_H
