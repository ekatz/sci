#include "Midi.h"
#include "MidiDriver.h"
#include "Mutex.h"
#include "Sound.h"
#include <stdarg.h>

#if defined(__IOS__)
#include "Timer.h"
#include <mach/mach_time.h>
#endif

#ifdef PlaySound
#undef PlaySound
#endif

#define ARRAY_ERASE(arr, size, pos)                                            \
    if ((pos) != ((size)-1))                                                   \
        memmove((arr) + (pos),                                                 \
                (arr) + (pos) + 1,                                             \
                ((size) - ((pos) + 1)) * sizeof(*arr));                        \
    arr[size - 1] = 0

#define ARRAY_INSERT(arr, size, pos)                                           \
    if ((pos) != ((size)-1))                                                   \
    memmove(                                                                   \
      (arr) + (pos) + 1, (arr) + (pos), ((size) - ((pos) + 1)) * sizeof(*arr))

// Controllers.
#define MODCTRL     1   // Modulation controller
#define VOLCTRL     7   // Volume controller
#define PANCTRL     10  // Pan controller
#define DAMPRCTRL   64  // Sustain pedal controller
#define NUMNOTES    75  // Maximum note reassignment controller
#define RESTARTFLAG 76  // Song "restart on pause" controller
#define CHPRIORITY  77  // Channel priority (Amiga/Mac)
#define CURNOTE     78  // Current single voice note
#define MUTECTRL    78  // Channel mute controller
#define CHNLSET     79  // Play track on specified channel
#define REVERBMODE  80  // Reverb setting controller (MT-32)
#define NOISECTRL   81  // SSG noise controller (NEC-9801 FM card)
#define ENDPOINT    82  // Loop end point
#define DATAINC     96  // Data increment (cues)
#define ALLNOFF     123 // All notes off controller

#define DAMPER_FLAG   (1 << 15)

#define MIDI_MAXBYTE  (127)
#define MIDI_LSB(n16) ((uint8_t)((n16)&0x7F))
#define MIDI_MSB(n16) ((uint8_t)(((n16) >> 7) & 0x7F))

#define GetByte()                                                              \
    *data++;                                                                   \
    ++node->tIndex[track]

static int PatchReq(void);
static int Init(char *patch);
static void Terminate(void);
static void ProcessSounds(bool dec);
static void SoundOn(uint8_t bar);
static uint MasterVol(uint8_t bar);
static uint8_t SetReverb(uint8_t mode);
static uint PlaySound(Sound *node, uint8_t bedSound);
static void EndSound(Sound *node);
static void PauseSound(Sound *node, bool stopStart);
static void FadeSound(Sound *node, uint8_t vol, uint8_t ticks, uint8_t steps);
static void HoldSound(Sound *node, uint8_t where);
static void MuteSound(Sound *node, bool mute);
static void ChangeVol(Sound *node, uint8_t vol);
static void ChangePri(Sound *node, uint8_t pri);
static uint8_t GetSignal(Sound *node);
static uint8_t GetSignalRset(Sound *node);
static uint16_t GetDataInc(Sound *node);
static void GetSMPTE(Sound *node, uint8_t *min, uint8_t *sec, uint8_t *frame);
static void SendNoteOff(Sound  *node,
                        uint8_t channel,
                        uint8_t note,
                        uint8_t velocity);
static void SendNoteOn(Sound  *node,
                       uint8_t channel,
                       uint8_t note,
                       uint8_t velocity);
static void SendController(Sound  *node,
                           uint8_t channel,
                           uint8_t controller,
                           uint8_t value);
static void SendPChange(Sound *node, uint8_t channel, uint8_t program);
static void SendPBend(Sound *node, uint8_t channel, uint16_t pbend);
static void RestoreSound(Sound *node);
static void ParseNode(Sound *node, uint8_t playPos);
static void DoEnd(Sound *node);
static void FixupHeader(Sound *node);
static uint8_t *HandleCommand(uint8_t  command,
                              uint8_t  channel,
                              Sound   *node,
                              uint8_t *data,
                              uint8_t  track);
static uint8_t *NoteOff(uint8_t  channel,
                        Sound   *node,
                        uint8_t *data,
                        uint8_t  track);
static uint8_t *NoteOn(uint8_t  channel,
                       Sound   *node,
                       uint8_t *data,
                       uint8_t  track);
static uint8_t *PolyAfterTch(uint8_t  channel,
                             Sound   *node,
                             uint8_t *data,
                             uint8_t  track);
static uint8_t *Controller(uint8_t  channel,
                           Sound   *node,
                           uint8_t *data,
                           uint8_t  track);
static uint8_t *ProgramChange(uint8_t  channel,
                              Sound   *node,
                              uint8_t *data,
                              uint8_t  track);
static uint8_t *ChnlAfterTch(uint8_t  channel,
                             Sound   *node,
                             uint8_t *data,
                             uint8_t  track);
static uint8_t *PitchBend(uint8_t  channel,
                          Sound   *node,
                          uint8_t *data,
                          uint8_t  track);
static uint8_t *SysEx(uint8_t  command,
                      Sound   *node,
                      uint8_t *data,
                      uint8_t  track);
static uint8_t *ControlChnl(uint8_t  command,
                            Sound   *node,
                            uint8_t *data,
                            uint8_t  track);
static uint8_t *SkipMidi(uint8_t  command,
                         Sound   *node,
                         uint8_t *data,
                         uint8_t  track);
static uint8_t ScaleVolume(uint8_t cVol, uint8_t sVol);
static uint8_t FindNode(const Sound *node);
static void BackupList(uint8_t numVoices);
static void RestoreList(uint8_t *numVoices);
static void SwapChannels(uint8_t ch1, uint8_t ch2);
static uint8_t LookupChannel(uint8_t nodeChannel);
static uint8_t LookupOpenChannel(void);
static uint8_t PreemptChannel(uint8_t *numVoices);
static void UpdateChannel(Sound *node, uint8_t num, uint8_t channel);
static void DoChannelList(void);
static void DoFade(Sound *node, uint8_t playPos);
static void DoChangeVol(Sound  *node,
                        uint8_t playPos,
                        uint8_t newVol,
                        bool    vRequest);
static uint8_t DoVolRequests(void);
static void SoundServer(void);
static void SampleActive(Sound *node);
static void DoSamples(void);

#define LISTSIZE 16
static Sound *PlayList[LISTSIZE]   = { 0 };
static Sound *SampleList[LISTSIZE] = { 0 };
static Sound *ChNodes[LISTSIZE]    = { 0 };

// static uint16_t EndSnds[LISTSIZE * 2] = { 0 };
//
// static uint8_t Holding[LISTSIZE] = { 0 };
static uint8_t ChList[LISTSIZE] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                    0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t ChBed[LISTSIZE] = { 0 };
static uint8_t ChPri[LISTSIZE] = { 0 };
static uint8_t ChVoice[LISTSIZE] = { 0 };
static uint8_t ChNew[LISTSIZE] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                   0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t ChBedCopy[LISTSIZE] = { 0 };
static uint8_t ChPriCopy[LISTSIZE] = { 0 };
static uint8_t ChVoiceCopy[LISTSIZE] = { 0 };
static uint8_t ChNewCopy[LISTSIZE] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                       0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t ChOld[LISTSIZE] = { 15, 15, 15, 15, 15, 15, 15, 15,
                                   15, 15, 15, 15, 15, 15, 15, 15 };
static uint8_t VolRequest[LISTSIZE] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0xFF, 0xFF, 0xFF };

static uint8_t processSnds = 0;     // ON\OFF switch for SoundServer
static uint8_t loChnl      = 0;     // Lowest channel supported by driver
static uint8_t hiChnl      = 15;    // Highest channel supported by driver
static int8_t  devID       = 0;     // ID of the current music device
static bool    ghostChnl   = false; // Current channel is a ghost channel
static uint8_t s_numVoices = 0;     // Number of voices supported by driver
static uint8_t s_numDACs   = 0;     // Number of Digital-Analog Converters (DAC)
                                    // available
static uint8_t revDefault  = 0;     // Default reverb mode
static uint8_t vLeftCopy   = 0;     // Copy of voicesLeft
static bool    updateChnls = false; // Flag telling next interrupt to call
                                    // DoChannelList
static uint8_t requestChnl = 0;     // Current VolRequest channel
// static uint8_t oldReqChnl  = 0;     // requestChnl end reference
// static uint8_t oldRecCom   = 0;     // Receiving mode running status
static bool    restoring   = false; // Tells ParseNode not to send anything
// static uint8_t testNode    = 0;    // Flag signaling ParseNode to look at
                                      // MIDI port
// static uint8_t UARTmode    = 0;    // Flag showing MPU in UART mode

#define DisableSoundServer() ++processSnds
#define EnableSoundServer()  --processSnds

static Mutex s_mutex;

int DoSound(int function, ...)
{
    int     res = 0;
    Sound  *node;
    va_list args;
    va_start(args, function);
    LockMutex(&s_mutex);

    // If the function was SMasterVol, SProcesss, SSetReverb, SSoundOn, SInit or
    // STerminate, then the first argument was an int value, not an address.

    switch (function) {
        case SPatchReq:
            res                  = PatchReq();
            *va_arg(args, int *) = (int)s_numVoices;
            *va_arg(args, int *) = (int)s_numDACs;
            *va_arg(args, int *) = (int)devID;
            break;

        case SInit:
            res = Init(va_arg(args, char *));
            break;

        case STerminate:
            Terminate();
            break;

        case SProcess:
            ProcessSounds(va_arg(args, int) != 0);
            break;

        case SSoundOn:
            SoundOn(va_arg(args, uint8_t));
            break;

        case SRestore:
            RestoreSound(va_arg(args, Sound *));
            break;

        case SMasterVol:
            res = (int)MasterVol(va_arg(args, uint8_t));
            break;

        case SSetReverb:
            res = (int)SetReverb(va_arg(args, uint8_t));
            break;

        case SPlay:
            node = va_arg(args, Sound *);
            PlaySound(node, va_arg(args, uint8_t));
            break;

        case SEnd:
            EndSound(va_arg(args, Sound *));
            break;

        case SPause:
            node = va_arg(args, Sound *);
            PauseSound(node, va_arg(args, int) != 0);
            break;

        case SFade: {
            uint8_t vol, ticks, steps;
            node  = va_arg(args, Sound *);
            vol   = va_arg(args, uint8_t);
            ticks = va_arg(args, uint8_t);
            steps = va_arg(args, uint8_t);
            FadeSound(node, vol, ticks, steps);
            break;
        }

        case SHold:
            node = va_arg(args, Sound *);
            HoldSound(node, va_arg(args, uint8_t));
            break;

        case SMute:
            node = va_arg(args, Sound *);
            MuteSound(node, va_arg(args, int) != 0);
            break;

        case SChangeVol:
            node = va_arg(args, Sound *);
            ChangeVol(node, va_arg(args, uint8_t));
            break;

        case SChangePri:
            node = va_arg(args, Sound *);
            ChangePri(node, va_arg(args, uint8_t));
            break;

        case SGetSignal:
            res = (int)GetSignal(va_arg(args, Sound *));
            break;

        case SGetSignalRset:
            res = (int)GetSignalRset(va_arg(args, Sound *));
            break;

        case SGetDataInc:
            res = (int)GetDataInc(va_arg(args, Sound *));
            break;

        case SGetSYMPTE: {
            uint8_t *min, *sec, *frame;
            node  = va_arg(args, Sound *);
            min   = va_arg(args, uint8_t *);
            sec   = va_arg(args, uint8_t *);
            frame = va_arg(args, uint8_t *);
            GetSMPTE(node, min, sec, frame);
            break;
        }

        case SNoteOff: {
            uint8_t channel, note, velocity;
            node     = va_arg(args, Sound *);
            channel  = va_arg(args, uint8_t);
            note     = va_arg(args, uint8_t);
            velocity = va_arg(args, uint8_t);
            SendNoteOff(node, channel, note, velocity);
            break;
        }

        case SNoteOn: {
            uint8_t channel, note, velocity;
            node     = va_arg(args, Sound *);
            channel  = va_arg(args, uint8_t);
            note     = va_arg(args, uint8_t);
            velocity = va_arg(args, uint8_t);
            SendNoteOn(node, channel, note, velocity);
            break;
        }

        case SController: {
            uint8_t channel, controller, value;
            node       = va_arg(args, Sound *);
            channel    = va_arg(args, uint8_t);
            controller = va_arg(args, uint8_t);
            value      = va_arg(args, uint8_t);
            SendController(node, channel, controller, value);
            break;
        }

        case SPChange: {
            uint8_t channel, program;
            node    = va_arg(args, Sound *);
            channel = va_arg(args, uint8_t);
            program = va_arg(args, uint8_t);
            SendPChange(node, channel, program);
            break;
        }

        case SPBend: {
            uint8_t  channel;
            uint16_t pbend;
            node    = va_arg(args, Sound *);
            channel = va_arg(args, uint8_t);
            pbend   = va_arg(args, uint16_t);
            SendPBend(node, channel, pbend);
            break;
        }

        default:
            break;
    }

    UnlockMutex(&s_mutex);
    va_end(args);
    return res;
}

// Determine Patch requirements.
static int PatchReq(void)
{
    uint req    = Driver(DPatchReq, 0, 0, 0);
    s_numVoices = (uint8_t)(req >> 16);
    devID       = (int8_t)(req >> 24);
    s_numDACs   = ((uint8_t)(req >> 8) & 0xF0) >> 4;
    return (int8_t)req;
}

// Initialize Sound Driver.
static int Init(char *patch)
{
    // Call Init function of Driver and set loChnl & hiChnl with return value.
    uint res = Driver(DInit, 0, 0, 0);
    loChnl   = (uint8_t)(res >> 16);
    hiChnl   = (uint8_t)((res >> 16) >> 8);

    // Reset reverb mode.
    Driver(DSetReverb, 0, 0, 0);
    return (int16_t)res;
}

// Terminate Driver.
static void Terminate(void)
{
    // Leave the devices volume at maximum.
    Driver(DMasterVol, 15, 0, 0);

    // Terminate the driver.
    Driver(DTerminate, 0, 0, 0);
}

// Inc or Dec Process Counter.
static void ProcessSounds(bool dec)
{
    if (!dec) {
        processSnds++;
    } else {
        if (processSnds != 0) {
            processSnds--;
        }
    }
}

// Turn Sound On or Off.
static void SoundOn(uint8_t bar)
{
    Driver(DSoundOn, bar, 0, 0);
}

// Change Master Volume.
static uint MasterVol(uint8_t bar)
{
    // If the volume is 255, then we just want a return value.
    if (bar != 255) {
        // Ensure that the volume is 15 or less.
        if (bar > 15) {
            bar = 15;
        }
    }
    return Driver(DMasterVol, bar, 0, 0);
}

// Change Reverb mode.
static uint8_t SetReverb(uint8_t mode)
{
    uint8_t def;

    // If the mode is 255, then we just want a return value.
    if (mode == 255) {
        return (uint8_t)Driver(DSetReverb, mode, 0, 0);
    }

    def = revDefault;

    // Insure that reverb mode is 10 or less.
    if (mode <= 10) {
        revDefault = mode;

        // See if the first node on the PlayList is set to default reverb.
        // If so, tell the driver to change the reverb mode.
        if (PlayList[0] != NULL && PlayList[0]->sReverbMode == 127) {
            Driver(DSetReverb, mode, 0, 0);
        }
    }
    return def;
}

// Add Sound to Playlist.
static uint PlaySound(Sound *node, uint8_t bedSound)
{
    uint8_t *data, *track;
    uint8_t  n;
    uint     i;

    for (i = 0; i < LISTSIZE; ++i) {
        // If the node is already on the PlayList, then do an End before adding
        // it again.
        if (PlayList[i] == node) {
            DoEnd(node);
            DoChannelList();
            break;
        }
    }

    // Set the state property of the node to TRUE (which means the sound is
    // playing).
    node->sState = 1;

    //* *That funky bed sound thing.
    if (bedSound != 0) {
        ++node->sState;
    }

    // Set up the resource header.
    FixupHeader(node);

    // Initialize all index, rest, PriVoice, and channel parameters.
    for (i = 0; i < (LISTSIZE - 1); ++i) {
        node->tIndex[i]       = 13;
        node->tLoopPoint[i]   = 3;
        node->tRest[i]        = 0;
        node->tLoopRest[i]    = 0;
        node->cDamprPbend[i]  = 0x2000;
        node->tChannel[i]     = (uint8_t)-1;
        node->tCommand[i]     = 0;
        node->tLoopCommand[i] = 0;
        node->cPriVoice[i]    = (uint8_t)-1;
        node->cModulation[i]  = 0;
        node->cProgram[i]     = (uint8_t)-1;
        node->cVolume[i]      = (uint8_t)-1;
        node->cPan[i]         = (uint8_t)-1;
        node->cCurNote[i]     = (uint8_t)-1;
        node->cFlags[i]       = 0;
        node->cMute[i]        = 0;
    }
    node->tChannel[15]     = (uint8_t)-1;
    node->tCommand[15]     = 0;
    node->tLoopCommand[15] = 0;
    node->sSample          = 0;
    node->sHold            = 0;
    node->sReverbMode      = 127;
    node->tIndex[15]       = 13;
    node->tLoopPoint[15]   = 3;
    node->tRest[15]        = 0;
    node->sLoopTime        = 0;

    data = node->sPointer;

    // If there is priority information in the resource, and the sFixedPri flag
    // is not set, then set the sPriority property to the priority in the
    // resource.
    if (data[32] != (uint8_t)-1 && node->sFixedPri == 0) {
        // TODO: node->sPriority = data[32];
        node->sPriority = 0;
    }

    // Set channel values and rest for tracks.
    // Set PriVoice, Volume and Pan for channels.
    for (i = 0; i < LISTSIZE; ++i) {
        uint16_t offset;
        uint8_t  channel;

        offset = ((uint16_t *)data)[i];
        if (offset == 0) {
            break;
        }

        track   = data + offset;
        channel = track[0];
        if (channel == 0xFE) {
            if (s_numDACs) {
                node->sSample = (uint8_t)i + 1;
                break;
            }

            node->tIndex[i]     = 0;
            node->tLoopPoint[i] = 0;
            node->tChannel[i]   = 0xFE;
        } else {
            node->tChannel[i] = channel;
            node->tCommand[i] = channel | CONTROLLER;
            node->tRest[i]    = (track[12] != TIMINGOVER)
                               ? (ushort)track[12]
                               : (ushort)((0x80 << 8) | 240);
            node->tChannel[i] &= 0x0F;
            if ((channel & 0x10) != 0) {
                node->tIndex[i] = 3;
                node->tRest[i]  = 0;
                node->cFlags[channel & 0xF] |= 2;
            } else {
                n = channel & 0x0F;
                if ((channel & 0x20) != 0) {
                    node->cFlags[n] |= 1;
                }

                if ((channel & 0x40) != 0) {
                    node->cMute[n] = 1;
                }

                if (n == (LISTSIZE - 1)) {
                    if (node->sReverbMode == 127) {
                        node->sReverbMode = track[8];
                        continue;
                    }
                } else {
                    if (node->cPriVoice[n] == (uint8_t)-1) {
                        node->cPriVoice[n] = track[1];
                    }
                    if (node->cProgram[n] == (uint8_t)-1) {
                        node->cProgram[n] = track[4];
                    }
                    if (node->cVolume[n] == (uint8_t)-1) {
                        node->cVolume[n] = track[8];
                    }
                }

                if (node->cPan[n] == (uint8_t)-1) {
                    node->cPan[n] = track[11];
                }
            }
        }
    }

    //* *That funky bed sound thing.
    if (node->sState == 2) {
        for (i = 0; i < (LISTSIZE - 1); ++i) {
            node->cFlags[i] |= 1;
        }
    }

    for (i = 0; i < LISTSIZE; ++i) {
        if (PlayList[i] == NULL) {
            break;
        }

        if (PlayList[i]->sPriority <= node->sPriority) {
            // If the spot was in the middle of the play list, then move
            // everyone down one.
            ARRAY_INSERT(PlayList, LISTSIZE, i);
            break;
        }
    }

    if (i < LISTSIZE) {
        // Place node on play list where 'i' is pointing to.
        PlayList[i] = node;

        // If this sound is restoring, don't reset critical values.
        if (!restoring) {
            node->sDataInc   = 0;
            node->sTimer     = 0;
            node->sSignal    = 0;
            node->sFadeDest  = 0;
            node->sFadeTicks = 0;
            node->sFadeCount = 0;
            node->sFadeSteps = 0;
            node->sPause     = 0;
            DoChannelList();
        }
    }

    // Return the PlayList position of the node.
    return i;
}

// End a sound.
static void EndSound(Sound *node)
{
    // Take node off PlayList, and update ChList.
    DoEnd(node);
    DoChannelList();
}

// Pause/Unpause a sound.
static void PauseSound(Sound *node, bool stopStart)
{
    if (node != NULL) {
        if (stopStart) {
            node->sPause++;
        } else {
            if (node->sPause != 0) {
                node->sPause--;
            }
        }
    } else {
        //
        // If the node pointer is set to NULL, then pause all sounds on the
        // PlayList.
        //

        uint i;

        for (i = 0; i < LISTSIZE; ++i) {
            node = PlayList[i];
            if (node != NULL) {
                if (stopStart) {
                    node->sPause++;
                } else {
                    if (node->sPause != 0) {
                        node->sPause--;
                    }
                }
            } else {
                if (i != 0) {
                    break;
                }
            }
        }
    }
    DoChannelList();
}

// Fade a sound to given level.
static void FadeSound(Sound *node, uint8_t vol, uint8_t ticks, uint8_t steps)
{
    // If the volume of the node is already there, then don't bother.
    if (node->sVolume != vol) {
        // Set fade properties in the sound node.
        node->sFadeDest  = vol;
        node->sFadeTicks = ticks;
        node->sFadeSteps = steps;
        node->sFadeCount = 0;
    }
}

// Hold/Release a sound.
static void HoldSound(Sound *node, uint8_t where)
{
    // Set sHold property of node.
    node->sHold = where;
}

// Mute/Unmute an entire node.
static void MuteSound(Sound *node, bool mute)
{
    uint i;

    DisableSoundServer();

    // Change the mute flag of every channel in the node.
    // Decide whether to mute or unmute the node.
    if (mute) {
        for (i = 0; i < 15; ++i) {
            if ((node->cMute[i] & 0xF0) != 0xF0) {
                node->cMute[i] += 0x10;
            }
        }
    } else {
        for (i = 0; i < 15; ++i) {
            if ((node->cMute[i] & 0xF0) != 0) {
                node->cMute[i] -= 0x10;
            }
        }
    }
    DoChannelList();

    EnableSoundServer();
}

// Change sound node volume.
static void ChangeVol(Sound *node, uint8_t vol)
{
    DoChangeVol(node, FindNode(node), vol, false);
}

// Change sound node priority.
static void ChangePri(Sound *node, uint8_t pri)
{
    uint8_t i;

    // If it's the same as before, then don't bother.
    if (node->sPriority != pri) {
        // Store new priority.
        node->sPriority = pri;

        i = FindNode(node);

        // If it is on the PlayList.
        if (i != (uint8_t)-1) {
            // Remove the node from the PlayList.
            ARRAY_ERASE(PlayList, LISTSIZE, i);

            // Put node back on in order of priority.
            for (i = 0; i < LISTSIZE; ++i) {
                if (PlayList[i]->sPriority <= pri) {
                    ARRAY_INSERT(PlayList, LISTSIZE, i);
                    break;
                }
            }
            PlayList[i] = node;

            // Update ChList.
            DoChannelList();
        }
    }
}

// Get Signal value of a node.
static uint8_t GetSignal(Sound *node)
{
    return node->sSignal;
}

// Get Signal value of a node and reset it.
static uint8_t GetSignalRset(Sound *node)
{
    uint8_t signal = node->sSignal;
    node->sSignal  = 0;
    return signal;
}

// Get DataInc value of a node.
static uint16_t GetDataInc(Sound *node)
{
    return node->sDataInc;
}

// Get SMPTE value of a node.
static void GetSMPTE(Sound *node, uint8_t *min, uint8_t *sec, uint8_t *frame)
{
    uint16_t tmp;

    // Convert to minutes.
    *min = (uint8_t)(node->sTimer / 3600);
    tmp  = (node->sTimer % 3600);

    // Convert to seconds.
    *sec   = (uint8_t)(tmp / 60);
    *frame = (uint8_t)((tmp % 60) / 2);
}

// Send a NoteOff to a given channel in a given node.
static void SendNoteOff(Sound  *node,
                        uint8_t channel,
                        uint8_t note,
                        uint8_t velocity)
{
    uint8_t pos, nodeChannel;
    uint    i;

    DisableSoundServer();

    pos = FindNode(node);
    // If -1 is returned, then the node is not on the PlayList at all.
    if (pos != (uint8_t)-1) {
        // Reset the CurNote.
        node->cCurNote[channel] = (uint8_t)-1;

        // Combine the PlayList position and the channel number (this byte will
        // represent the node/channel we are sending to).
        nodeChannel = (pos << 4) | channel;

        // Search for an entry for this node/channel on the ChList.
        // The position of this entry on the list will tell us the physical
        // channel that this channel was mapped to. If no such entry exists,
        // then this channel is not currently being played (possibly preempted).
        for (i = 0; i < LISTSIZE; ++i) {
            if (ChList[i] == nodeChannel) {
                // Call the Sound Driver's NoteOff function.
                Driver(DNoteOff, (uint8_t)i, note, velocity);
                break;
            }
        }
    }

    EnableSoundServer();
}

// Send a NoteOn to a given channel in a given node.
static void SendNoteOn(Sound  *node,
                       uint8_t channel,
                       uint8_t note,
                       uint8_t velocity)
{
    uint8_t pos, nodeChannel;
    uint    i;

    DisableSoundServer();

    pos = FindNode(node);
    // If -1 is returned, then the node is not on the PlayList at all.
    if (pos != (uint8_t)-1) {
        // Fill the node's CurNote field with the note number specified.
        node->cCurNote[channel] = note;

        // Combine the PlayList position and the channel number (this byte will
        // represent the node/channel we are sending to).
        nodeChannel = (pos << 4) | channel;

        // Search for an entry for this node/channel on the ChList.
        // The position of this entry on the list will tell us the physical
        // channel that this channel was mapped to. If no such entry exists,
        // then this channel is not currently being played (possibly preempted).
        for (i = 0; i < LISTSIZE; ++i) {
            if (ChList[i] == nodeChannel) {
                // Call the Sound Driver's NoteOn function.
                Driver(DNoteOn, (uint8_t)i, note, velocity);
                break;
            }
        }
    }

    EnableSoundServer();
}

// Send Controller to a node.
static void SendController(Sound  *node,
                           uint8_t channel,
                           uint8_t controller,
                           uint8_t value)
{
    uint8_t pos, nodeChannel;
    uint    i;

    // If the node isn't on the PlayList, exit the procedure.
    pos = FindNode(node);
    if (pos == (uint8_t)-1) {
        return;
    }

    switch (controller) {
        case VOLCTRL:
            node->cVolume[channel] = value;
            value                  = ScaleVolume(value, node->sVolume);
            break;

        case PANCTRL:
            node->cPan[channel] = value;
            break;

        case MODCTRL:
            node->cModulation[channel] = value;
            break;

        case DAMPRCTRL:
            if (value != 0) {
                node->cDamprPbend[channel] |= DAMPER_FLAG;
            } else {
                node->cDamprPbend[channel] &= ~DAMPER_FLAG;
            }
            break;

        case MUTECTRL:
            if (value != 0) {
                if ((node->cMute[channel] & 0xF0) != 0xF0) {
                    node->cMute[channel] += 0x10;
                    DoChannelList();
                }
            } else {
                if ((node->cMute[channel] & 0xF0) != 0) {
                    node->cMute[channel] -= 0x10;
                    DoChannelList();
                }
            }
            return;

        case 127:
            node->cProgram[channel] = value;
            break;

        default:
            break;
    }

    // Look for the node/channel on the ChList.
    nodeChannel = (pos << 4) | channel;
    for (i = 0; i < LISTSIZE; ++i) {
        if (ChList[i] == nodeChannel) {
            if (controller == 127) {
                Driver(DProgramChange, (uint8_t)i, value, 0);
            } else {
                Driver(DController, (uint8_t)i, controller, value);
            }
            break;
        }
    }
}

// Send a ProgramChange to a given channel in a given node.
static void SendPChange(Sound *node, uint8_t channel, uint8_t program)
{
    uint8_t pos, nodeChannel;
    uint    i;

    DisableSoundServer();

    pos = FindNode(node);
    // If -1 is returned, then the node is not on the PlayList at all.
    if (pos != (uint8_t)-1) {
        // Store the new program in the node.
        node->cCurNote[channel] = program;

        // Combine the PlayList position and the channel number (this byte will
        // represent the node/channel we are sending to).
        nodeChannel = (pos << 4) | channel;

        // Search for an entry for this node/channel on the ChList.
        // The position of this entry on the list will tell us the physical
        // channel that this channel was mapped to. If no such entry exists,
        // then this channel is not currently being played (possibly preempted).
        for (i = 0; i < LISTSIZE; ++i) {
            if (ChList[i] == nodeChannel) {
                // Call the Sound Driver's ProgramChange function.
                Driver(DProgramChange, (uint8_t)i, program, 0);
                break;
            }
        }
    }

    EnableSoundServer();
}

// Send Pitch Bend to a node.
static void SendPBend(Sound *node, uint8_t channel, uint16_t pbend)
{
    uint8_t  pos, nodeChannel;
    uint16_t damprPbend;
    uint     i;

    pos = FindNode(node);
    // If -1 is returned, then the node is not on the PlayList at all.
    if (pos != (uint8_t)-1) {
        damprPbend = pbend;

        if ((node->cDamprPbend[channel] & DAMPER_FLAG) != 0) {
            damprPbend |= DAMPER_FLAG; // Damper
        }
        node->cDamprPbend[channel] = damprPbend;

        // Combine the PlayList position and the channel number (this byte will
        // represent the node/channel we are sending to).
        nodeChannel = (pos << 4) | channel;

        for (i = 0; i < LISTSIZE; ++i) {
            if (ChList[i] == nodeChannel) {
                Driver(DPitchBend,
                       (uint8_t)i,
                       MIDI_LSB(node->cDamprPbend[channel]),
                       MIDI_MSB(node->cDamprPbend[channel]));
                break;
            }
        }
    }
}

// Put sound back on Playlist.
static void RestoreSound(Sound *node)
{
    uint8_t  savMuteFlag[15];
    uint8_t  bedSound, holding;
    uint16_t prevTimer;
    uint8_t  prevLoop;
    uint     i;

    //* *That funky bed sound thing
    bedSound = node->sState - 1;

    // Save the values of the node which will be overwritten by the restore, and
    // that must remain intact.
    for (i = 0; i < 15; ++i) {
        savMuteFlag[i] = node->cMute[i] & 0xF0;
    }

    holding   = node->sHold;
    restoring = true;
    i         = PlaySound(node, bedSound);

    // Reset timer and set Loop value.
    prevTimer    = node->sTimer;
    node->sTimer = 0;
    prevLoop     = node->sLoop;
    node->sLoop  = TRUE;

    while (prevTimer != node->sTimer) {
        uint16_t unparsedTimer = node->sTimer;

        ParseNode(node, (uint8_t)i);

        if (unparsedTimer == node->sTimer) {
            break;
        }
        if (unparsedTimer > node->sTimer) {
            prevTimer -= unparsedTimer - node->sTimer;
        }
    }

    // Put old loop value back.
    node->sLoop = prevLoop;

    // Clear restore flag.
    restoring = false;

    // Restore the values of the node that were previously saved.
    for (i = 0; i < 15; ++i) {
        node->cMute[i] |= savMuteFlag[i];
    }

    node->sHold = holding;

    DoChannelList();
}

// Parse a node.
static void ParseNode(Sound *node, uint8_t playPos)
{
    uint8_t *data;
    uint8_t  realChnl; // The channel# data is being sent on
    uint8_t  trackChannel, channel, command;
    uint16_t rest;
    uint8_t  track, i;

    // Put the PlayList position of the node in the 4 hi-bits of playPos.
    playPos <<= 4;

    // Increment the timer.
    ++node->sTimer;

    for (track = 0; track < LISTSIZE; ++track) {
        trackChannel = node->tChannel[track];
        if (trackChannel == (uint8_t)-1) {
            break;
        }

        // Skip any sample tracks, marked with 0xfe.
        if (trackChannel == 0xfe) {
            continue;
        }

        //
        // Check to see if the current node-channel is on the ChList, or if the
        // channel is a ghost channel.
        //
        realChnl  = (uint8_t)-1;
        ghostChnl = false;
        if ((node->cFlags[trackChannel] & 2) != 0) {
            realChnl  = trackChannel;
            ghostChnl = true;
        } else {
            uint8_t nodeChannel = playPos | (trackChannel & 0x0F);
            for (i = 0; i < LISTSIZE; ++i) {
                if (ChList[i] == nodeChannel) {
                    realChnl = i;
                    break;
                }
            }
        }

        // Point "data" to the location in the track data where we last left
        // off.
        data = node->sPointer + ((uint16_t *)node->sPointer)[track];
        data += node->tIndex[track];

        // Check if the track is frozen.
        if (node->tIndex[track] == 0) {
            continue;
        }

        // Check for a rest count.
        if (node->tRest[track] != 0) {
            // Decrement the rest count and move on to the next track.
            if (--node->tRest[track] == (0x80 << 8)) {
                rest = GetByte();
                if (rest == TIMINGOVER) {
                    rest = (0x80 << 8) | 240;
                }
                node->tRest[track] = rest;
            }
        } else {
            while (true) {
                // Read next byte in MIDI stream, and determine if it's running
                // status or not.
                command = GetByte();
                if (command < 0x80) {
                    command = node->tCommand[track];
                    --data;
                    --node->tIndex[track];
                } else {
                    node->tCommand[track] = command;
                }

                if (command == ENDTRK) {
                    node->tIndex[track] = 0;
                    break;
                }

                // Split command and channel.
                channel = command & 0x0F;
                command = command & 0xF0;

                // If we're on channel 16, parse with the ControlChnl procedure.
                if (channel == 15) {
                    data = ControlChnl(command, node, data, track);
                } else {
                    // Get the real Channel #.
                    channel = realChnl;
                    data = HandleCommand(command, channel, node, data, track);
                }

                // If we return with a 0 index, then we hit an end point.
                if (node->tIndex[track] == 0) {
                    break;
                }

                // Check for timing.
                rest = GetByte();
                if (rest != 0) {
                    if (rest == TIMINGOVER) {
                        rest = (0x80 << 8) | 240;
                    }
                    node->tRest[track] = rest - 1;
                    break;
                }
            }
        }
    }

    for (track = 0; track < LISTSIZE; ++track) {
        if (node->tChannel[track] == (uint8_t)-1) {
            break;
        }

        if (node->tIndex[track] != 0) {
            return;
        }
    }

    if (node->sHold || node->sLoop) {
        node->sTimer = node->sLoopTime;
        memcpy(node->tIndex, node->tLoopPoint, sizeof(node->tIndex));
        memcpy(node->tRest, node->tLoopRest, sizeof(node->tRest));
        memcpy(node->tCommand, node->tLoopCommand, sizeof(node->tCommand));
    } else {
        DoEnd(node);
        updateChnls = true;
    }
}

static void DoEnd(Sound *node)
{
    uint i;

    i = 0;
    while (node != PlayList[i]) {
        ++i;
        if (i == LISTSIZE) {
            return;
        }
    }

    ARRAY_ERASE(PlayList, LISTSIZE, i);

    node->sSignal = (unsigned char)-1;
    node->sState  = 0;
    if (node->sSample >= 0x80) {
        uint8_t *ptr = node->sPointer +
                       ((uint16_t *)node->sPointer)[(node->sSample & 0x0F) - 1];
        Driver(DSampleEnd, ptr, 0, 0);
    }
}

static void FixupHeader(Sound *node)
{
    uint16_t kludgeHdr[LISTSIZE];
    uint8_t  priInfo;
    uint8_t *data;

    if (node->sPointer != NULL) {
        data = node->sPointer;

        // Make sure that this resource hasn't already been fixed up.
        if (!(data[33] == 0xFC && data[34] == 0xFD && data[35] == 0xFE)) {
            // Clear header buffer.
            memset(kludgeHdr, 0, sizeof(kludgeHdr));

            // If there is a properties entry in the header, pull the properties
            // information out of it.
            priInfo = (uint8_t)-1;
            if (data[0] == 0xF0) {
                priInfo = data[1];
                data += 8;
            }

            // Find device ID in header.
            while (*data != (uint8_t)-1) {
                int8_t id = (int8_t)*data++;
                if (id == devID) {
                    // Construct the Kludged header.
                    uint i = 0;
                    while (*data++ != (uint8_t)-1) {
                        data += 1;
                        kludgeHdr[i] = *(uint16_t *)data;

                        data += 4;
                        i++;
                    }
                    break;
                }

                while (*data++ != (uint8_t)-1) {
                    data += 1 + 4;
                }
            }

            // Write the header onto the sound resource.
            memcpy(node->sPointer, kludgeHdr, sizeof(kludgeHdr));

            // Put the priority information into the new header.
            node->sPointer[32] = priInfo;

            // Flag this resource as an already "fixed-up" resource.
            node->sPointer[33] = 0xFC;
            node->sPointer[34] = 0xFD;
            node->sPointer[35] = 0xFE;
        }
    }
}

static uint8_t *HandleCommand(uint8_t  command,
                              uint8_t  channel,
                              Sound   *node,
                              uint8_t *data,
                              uint8_t  track)
{
    // Switch on the value of the command to any of the eight MIDI handling
    // procedures.
    switch (command) {
        case NOTEOFF:
            data = NoteOff(channel, node, data, track);
            break;

        case NOTEON:
            data = NoteOn(channel, node, data, track);
            break;

        case POLYAFTER:
            data = PolyAfterTch(channel, node, data, track);
            break;

        case CONTROLLER:
            data = Controller(channel, node, data, track);
            break;

        case PCHANGE:
            data = ProgramChange(channel, node, data, track);
            break;

        case CHNLAFTER:
            data = ChnlAfterTch(channel, node, data, track);
            break;

        case PBEND:
            data = PitchBend(channel, node, data, track);
            break;

        case SYSEX:
            data = SysEx(command, node, data, track);
            break;

        // If the command wasn't any of the above, then shut down the track
        // before we do any serious damage.
        default:
            node->tIndex[track] = 0;
            break;
    }
    return data;
}

// Turn Note off.
static uint8_t *NoteOff(uint8_t  channel,
                        Sound   *node,
                        uint8_t *data,
                        uint8_t  track)
{
    uint8_t note, velocity;
    uint8_t trackChannel;

    note     = GetByte();
    velocity = GetByte();

    trackChannel = node->tChannel[track] & 0x0F;
    if (channel < LISTSIZE && node->cCurNote[trackChannel] == note) {
        node->cCurNote[trackChannel] = (uint8_t)-1;
    }

    if (channel != (uint8_t)-1 && !restoring) {
        Driver(DNoteOff, channel & 0x0F, note, velocity);
    }
    return data;
}

// Turn Note on.
static uint8_t *NoteOn(uint8_t  channel,
                       Sound   *node,
                       uint8_t *data,
                       uint8_t  track)
{
    uint8_t note, velocity;
    uint8_t trackChannel;

    note     = GetByte();
    velocity = GetByte();

    trackChannel = node->tChannel[track] & 0x0F;
    if (velocity != 0) {
        // TODO: This "if" (and all others like it) is not in the latest
        // version.
        if (channel < LISTSIZE) {
            node->cCurNote[trackChannel] = note;
        }
        if (channel != (uint8_t)-1 && !restoring) {
            Driver(DNoteOn, channel & 0x0F, note, velocity);
        }
    } else {
        if (channel < LISTSIZE && node->cCurNote[trackChannel] == note) {
            node->cCurNote[trackChannel] = (uint8_t)-1;
        }
        if (channel != (uint8_t)-1 && !restoring) {
            Driver(DNoteOff, channel & 0x0F, note, velocity);
        }
    }
    return data;
}

// Do Poly AfterTouch.
static uint8_t *PolyAfterTch(uint8_t  channel,
                             Sound   *node,
                             uint8_t *data,
                             uint8_t  track)
{
    uint8_t note, pressure;

    note     = GetByte();
    pressure = GetByte();

    if (channel != (uint8_t)-1 && !restoring) {
        Driver(DPolyAfterTch, channel, note, pressure);
    }
    return data;
}

// Do Control Change.
static uint8_t *Controller(uint8_t  channel,
                           Sound   *node,
                           uint8_t *data,
                           uint8_t  track)
{
    uint8_t controller, value;
    uint8_t trackChannel;

    controller = GetByte();
    value      = GetByte();

    if (!ghostChnl || ChList[channel & 0xF] == (uint8_t)-1) {
        trackChannel = node->tChannel[track] & 0x0F;
        switch (controller) {
            case VOLCTRL:
                node->cVolume[trackChannel] = value;
                value                       = ScaleVolume(value, node->sVolume);
                // TODO: this should be '16' instead of '32'.
                if (channel >= 32) {
                    return data;
                }
                // Clear volume request.
                VolRequest[channel] = (uint8_t)-1;
                break;

            case PANCTRL:
                node->cPan[trackChannel] = value;
                break;

            case MODCTRL:
                node->cModulation[trackChannel] = value;
                break;

            case DAMPRCTRL:
                if (value != 0) {
                    node->cDamprPbend[trackChannel] |= DAMPER_FLAG;
                } else {
                    node->cDamprPbend[trackChannel] &= ~DAMPER_FLAG;
                }
                break;

            case NUMNOTES:
                node->cPriVoice[trackChannel] =
                  value | node->cPriVoice[trackChannel] & 0xF0;
                updateChnls = true;
                break;

            case CURNOTE:
                node->cMute[trackChannel] =
                  (value != 0) | node->cMute[trackChannel] & 0xF0;
                updateChnls = true;
                break;
        }

        if (channel != (uint8_t)-1 && !restoring) {
            Driver(DController, channel & 0x0F, controller, value);
        }
    }
    return data;
}

// Do Program Change.
static uint8_t *ProgramChange(uint8_t  channel,
                              Sound   *node,
                              uint8_t *data,
                              uint8_t  track)
{
    uint8_t program;

    program = GetByte();

    if (!ghostChnl || ChList[channel & 0x0F] == (uint8_t)-1) {
        // Save program in sound node.
        node->cProgram[node->tChannel[track] & 0x0F] = program;

        if (channel != (uint8_t)-1 && !restoring) {
            Driver(DProgramChange, channel & 0x0F, program, 0);
        }
    }
    return data;
}

// Do Channel Aftertouch.
static uint8_t *ChnlAfterTch(uint8_t  channel,
                             Sound   *node,
                             uint8_t *data,
                             uint8_t  track)
{
    uint8_t pressure;

    pressure = GetByte();

    if (channel != (uint8_t)-1 && !restoring) {
        Driver(DChnlAfterTch, channel, pressure, 0);
    }
    return data;
}

// Do Pitch Bend.
static uint8_t *PitchBend(uint8_t  channel,
                          Sound   *node,
                          uint8_t *data,
                          uint8_t  track)
{
    uint8_t  lsb, msb;
    uint16_t damprPbend;
    uint8_t  trackChannel;

    lsb = GetByte();
    msb = GetByte();

    if (!ghostChnl || ChList[channel & 0x0F] == (uint8_t)-1) {
        trackChannel = node->tChannel[track] & 0x0F;

        damprPbend = ((uint16_t)msb << 7) | (uint16_t)lsb;

        if ((node->cDamprPbend[trackChannel] & DAMPER_FLAG) != 0) {
            damprPbend |= DAMPER_FLAG; // Damper
        }
        node->cDamprPbend[trackChannel] = damprPbend;

        if (channel != (uint8_t)-1 && !restoring) {
            Driver(DPitchBend, channel & 0x0F, lsb, msb);
        }
    }
    return data;
}

// Process SysEx.
static uint8_t *SysEx(uint8_t  command,
                      Sound   *node,
                      uint8_t *data,
                      uint8_t  track)
{
    // We don't support it.
    return SkipMidi(command, node, data, track);
}

// Control Channel Commands.
static uint8_t *ControlChnl(uint8_t  command,
                            Sound   *node,
                            uint8_t *data,
                            uint8_t  track)
{
    if (command == PCHANGE) {
        uint8_t  cue;
        uint16_t rest;

        cue = GetByte();

        // If it's a loop set, set the loop point.
        if (cue == 127) {
            rest = GetByte();
            if (rest == TIMINGOVER) {
                rest = (0x80 << 8) | 240;
            }

            node->tRest[track]    = rest;
            node->tCommand[track] = PCHANGE | 15;

            memcpy(node->tLoopPoint, node->tIndex, sizeof(node->tIndex));
            memcpy(node->tLoopRest, node->tRest, sizeof(node->tRest));
            memcpy(node->tLoopCommand, node->tCommand, sizeof(node->tCommand));
            node->sLoopTime = node->sTimer;

            // Go back 1 byte.
            node->tIndex[track]--;
            data--;

            node->tRest[track] = 0;
        } else if (!restoring) {
            // Set the signal property of the sound node.
            node->sSignal = cue;
        }
    } else if (command == CONTROLLER) {
        uint8_t controller, value;

        controller = GetByte();
        value      = GetByte();

        if (controller == REVERBMODE) {
            if (value == 0x7F) {
                value = revDefault;
            }

            node->sReverbMode = value;
            Driver(DSetReverb, value, 0, 0);
        } else if (controller == DATAINC) {
            if (!restoring) {
                ++node->sDataInc;
            }
        } else if (controller == ENDPOINT && value == node->sHold) {
            memset(node->tIndex, 0, sizeof(node->tIndex));
        }
    } else {
        // If it's not a valid Ch. 16 command, then skip it.
        data = SkipMidi(command, node, data, track);
    }
    return data;
}

// Skip next MIDI Command.
static uint8_t *SkipMidi(uint8_t  command,
                         Sound   *node,
                         uint8_t *data,
                         uint8_t  track)
{
    if (command == SYSEX) {
        uint8_t b;
        do {
            b = GetByte();
        } while (b != EOX);
    } else {
        if (command != PCHANGE && command != CHNLAFTER) {
            GetByte();
        }
        GetByte();
    }
    return data;
}

// Scale Volume Controller.
static uint8_t ScaleVolume(uint8_t cVol, uint8_t sVol)
{
    //
    // cVol   x
    // ---- = ----
    // 127    sVol
    //

    uint8_t vol = (((cVol + 1) * (sVol + 1)) >> 8) << 1;
    return (vol != 0) ? (vol - 1) : 0;
}

// Find a node on the playlist.
static uint8_t FindNode(const Sound *node)
{
    uint8_t i;

    for (i = 0; i < LISTSIZE; ++i) {
        if (PlayList[i] == node) {
            return i;
        }
    }
    return (uint8_t)-1;
}

static void BackupList(uint8_t numVoices)
{
    memcpy(ChNewCopy, ChNew, sizeof(ChNew));
    memcpy(ChPriCopy, ChPri, sizeof(ChPri));
    memcpy(ChVoiceCopy, ChVoice, sizeof(ChVoice));
    memcpy(ChBedCopy, ChBed, sizeof(ChBed));
    vLeftCopy = numVoices;
}

static void RestoreList(uint8_t *numVoices)
{
    memcpy(ChNew, ChNewCopy, sizeof(ChNew));
    memcpy(ChPri, ChPriCopy, sizeof(ChPri));
    memcpy(ChVoice, ChVoiceCopy, sizeof(ChVoice));
    memcpy(ChBed, ChBedCopy, sizeof(ChBed));
    *numVoices = vLeftCopy;
}

static void SwapChannels(uint8_t ch1, uint8_t ch2)
{
    uint8_t tmp;

    tmp        = ChNew[ch1];
    ChNew[ch1] = ChNew[ch2];
    ChNew[ch2] = tmp;

    tmp        = ChPri[ch1];
    ChPri[ch1] = ChPri[ch2];
    ChPri[ch2] = tmp;

    tmp          = ChVoice[ch1];
    ChVoice[ch1] = ChVoice[ch2];
    ChVoice[ch2] = tmp;

    tmp        = ChBed[ch1];
    ChBed[ch1] = ChBed[ch2];
    ChBed[ch2] = tmp;
}

static uint8_t LookupChannel(uint8_t nodeChannel)
{
    uint8_t *p;

    p = (uint8_t *)memchr(ChNew, (int)(char)nodeChannel, LISTSIZE);
    if (p == NULL) {
        return (uint8_t)-1;
    }
    return (uint8_t)(p - ChNew);
}

static uint8_t LookupOpenChannel(void)
{
    uint8_t *list, *p;

    list = &ChNew[loChnl];
    p    = (uint8_t *)memchr(list, -1, hiChnl - loChnl + 1);
    if (p == NULL) {
        return (uint8_t)-1;
    }
    return (uint8_t)(p - ChNew);
}

// Preempt a channel off of the ChNew list.
static uint8_t PreemptChannel(uint8_t *numVoices)
{
    uint8_t channel, pri, n;

    channel = (uint8_t)-1;
    pri     = 0;
    for (n = 0; n < LISTSIZE; ++n) {
        if (pri < ChPri[n]) {
            pri     = ChPri[n];
            channel = n;
        }
    }

    if (channel != (uint8_t)-1) {
        *numVoices += ChVoice[channel];
        ChNew[channel]   = (uint8_t)-1;
        ChVoice[channel] = 0;
        ChPri[channel]   = 0;
        ChBed[channel]   = FALSE;
    }

    return channel;
}

static void UpdateChannel(Sound *node, uint8_t num, uint8_t channel)
{
    uint8_t volume, damper;

    Driver(DController, num, ALLNOFF, 0);
    Driver(DController, num, NUMNOTES, (node->cPriVoice[channel] & 0xF));
    Driver(DProgramChange, num, node->cProgram[channel], 0);
    VolRequest[num] = (uint8_t)-1;

    volume = ScaleVolume(node->cVolume[channel], node->sVolume);
    Driver(DController, num, VOLCTRL, volume);
    Driver(DController, num, PANCTRL, node->cPan[channel]);
    Driver(DController, num, MODCTRL, node->cModulation[channel]);

    damper = 0;
    if ((node->cDamprPbend[channel] & DAMPER_FLAG) != 0) {
        damper = MIDI_MAXBYTE;
    }
    Driver(DController, num, DAMPRCTRL, damper);
    Driver(DPitchBend,
           num,
           MIDI_LSB(node->cDamprPbend[channel]),
           MIDI_MSB(node->cDamprPbend[channel]));

    Driver(DController, num, CURNOTE, node->cCurNote[channel]);
}

// Update channel list.
static void DoChannelList(void)
{
    Sound  *node;
    uint8_t channel = 0, newChannel;
    uint8_t nodeChannel;
    uint8_t reverb;
    uint8_t numVoices, nodeVoices;
    uint8_t pri;
    uint8_t n, i;
    uint    track;

    DisableSoundServer();

    updateChnls = false;

    // Clear channel tables.
    memset(ChList, -1, sizeof(ChList));
    memset(ChVoice, 0, sizeof(ChVoice));
    memset(ChBed, 0, sizeof(ChBed));
    memset(ChPri, 0, sizeof(ChPri));
    memset(ChNew, -1, sizeof(ChNew));

    SampleList[0] = NULL; // Sample Kludge!

    // Empty play list?
    if (PlayList[0] == NULL) {
        memset(ChList, -1, sizeof(ChList));
    } else {
        //
        // Set the reverb mode to the value in the first node's sReverbMode
        // value. 127 means default reverb mode which can be set by the apps
        // programmer.
        //

        reverb = PlayList[0]->sReverbMode;
        if (reverb == 127) {
            reverb = revDefault;
        }
        Driver(DSetReverb, reverb, 0, 0);

        ////////////////////////////////
        //* **DoChannelList PASS 1 ***//
        ////////////////////////////////
        numVoices = s_numVoices;
        for (n = 0; n < LISTSIZE; ++n) {
            bool blewIt = false;

            // Get next sound node off PlayList.
            node = PlayList[n];
            if (node == NULL) {
                break;
            }

            // If the sound is paused, don't allocate any channels for it.
            if (node->sPause) {
                continue;
            }

            if (node->sSample != 0) {
                if (SampleList[0] == NULL) {
                    SampleList[0] = node;
                }
                continue;
            }

            BackupList(numVoices);

            for (track = 0; track < LISTSIZE; ++track) {
                // Get the channel of the next track from node (Tracks should
                // already be prioritized in the sound node).
                channel = node->tChannel[track];
                if (channel == 0xFF) {
                    continue;
                }

                // If the channel value is 0xFE, then it's a sample.
                if (channel == 0xFE) {
                    continue;
                }

                // If it's channel 16, then it doesn't belong on the ChList.
                if (channel == 15) {
                    continue;
                }

                // If bit 1 of the cFlags array is set, then it's  a ghost
                // channel.
                if ((node->cFlags[channel] & 2) != 0) {
                    continue;
                }

                // If the cMuted property is TRUE, then the channel is muted.
                if (node->cMute[channel] != 0) {
                    continue;
                }

                nodeChannel = channel | (n << 4); // channelId
                nodeVoices  = node->cPriVoice[channel] & 0xF;
                pri         = node->cPriVoice[channel] >> 4;
                if (pri) {
                    pri = (n + 1) * LISTSIZE - pri;
                }

                // If bit 0 is set, then it's a locked channel.
                if ((node->cFlags[channel] & 1) != 0 &&
                    ChNew[channel] == (uint8_t)-1) {
                    newChannel = channel;
                } else {
                    //
                    // Look for an open channel.
                    // While looking, make sure that this node/channel is not
                    // already on the list.
                    //

                    if (LookupChannel(nodeChannel) != (uint8_t)-1) {
                        continue;
                    }

                    newChannel = LookupOpenChannel();

                    // There were no channels open.
                    if (newChannel == (uint8_t)-1) {
                        // If we're a preemptable channel, then we can blow it
                        // off and proceed to the next node.
                        if (pri) {
                            break;
                        }

                        // If we're non-preemptable, then we have to preempt
                        // someone or back out.
                        newChannel = PreemptChannel(&numVoices);
                        if (newChannel == (uint8_t)-1) {
                            blewIt = true;
                            break;
                        }
                    }
                }

                // Check for enough voices.
                if (nodeVoices > numVoices) {
                    // There weren't enough voices.
                    // If we're preemptable, blow it off and go to the next
                    // track.
                    if (pri != 0) {
                        continue;
                    }

                    do {
                        // Keep preempting channels until there are enough
                        // voices. If we run out of preemptable channels, then
                        // we have to back out.
                        newChannel = PreemptChannel(&numVoices);
                        if (newChannel == (uint8_t)-1) {
                            break;
                        }
                    } while (nodeVoices > numVoices);

                    if (newChannel == (uint8_t)-1) {
                        blewIt = true;
                        break;
                    }
                }

                // Put the channel on the list.
                ChNew[newChannel]   = nodeChannel;
                ChVoice[newChannel] = nodeVoices;
                numVoices -= nodeVoices;
                ChPri[newChannel] = pri;

                // If bit 0 is set, then it's a locked channel.
                if ((node->cFlags[channel] & 1) != 0) {
                    ChBed[newChannel] = TRUE;

                    // If we're a bed sound, insure that we're on the right
                    // channel
                    if (newChannel != channel) {
                        // If the channel in our desired position is not a bed
                        // channel, just swap with him.
                        if (ChBed[channel]) {
                            // If we're preemptable, we can take ourselves off
                            // the list and go on to the next track.
                            if (pri != 0) {
                                ChNew[newChannel]   = (uint8_t)-1;
                                ChPri[newChannel]   = 0;
                                ChVoice[newChannel] = 0;
                                ChBed[newChannel]   = FALSE;
                                numVoices += nodeVoices;
                            } else {
                                // If the other bed channel is not preemptable,
                                // then we have to back out.
                                if (ChPri[channel] == 0) {
                                    blewIt = true;
                                    break;
                                }

                                // The other bed channel was preemptable, so we
                                // can steal his channel.
                                numVoices += ChVoice[channel];
                                ChNew[newChannel]   = (uint8_t)-1;
                                ChVoice[newChannel] = 0;
                                ChPri[newChannel]   = 0;
                                ChBed[newChannel]   = FALSE;

                                ChNew[channel]   = nodeChannel;
                                ChPri[channel]   = pri;
                                ChVoice[channel] = nodeVoices;
                                numVoices -= nodeVoices;
                            }
                        } else {
                            SwapChannels(newChannel, channel);
                        }
                    }
                } else {
                    ChBed[newChannel] = FALSE;
                }
            }

            if (blewIt) {
                RestoreList(&numVoices);
            }
        }

        ////////////////////////////////
        //* **DoChannelList PASS 2 ***//
        ////////////////////////////////
        for (n = 0; n < LISTSIZE; ++n) {
            if (ChNew[n] == (uint8_t)-1) {
                continue;
            }

            // Is the next channel a bed channel?
            if (ChBed[n]) {
                //
                // It was a bed channel, so put it on the list in the same
                // position.
                //

                nodeChannel = ChNew[n];
                ChNew[n]    = (uint8_t)-1;
                ChList[n]   = nodeChannel;

                // See if the node/channel that was previously there was the
                // same. If not, Update the channel.
                node    = PlayList[nodeChannel >> 4];
                channel = nodeChannel & 0x0F;
                if (ChOld[n] != channel || ChNodes[n] != node) {
                    UpdateChannel(node, n, channel);
                }
                continue;
            }

            nodeChannel = ChNew[n];
            node        = PlayList[nodeChannel >> 4];
            channel     = nodeChannel & 0x0F;

            for (i = loChnl; i <= hiChnl; ++i) {
                // Look at next node on ChNodes list and see if it is the same.
                // If so, look at the channel number in ChOld and see if it is
                // the same.
                if (ChNodes[i] == node && ChOld[i] == channel) {
                    // If this ChList position is going to be occupied by a bed
                    // channel, then leave this channel for Pass3.
                    if (!ChBed[i]) {
                        // Copy this node/channel to the ChList, at the position
                        // it used to be at.
                        ChList[i] = ChNew[n];
                        ChNew[n]  = (uint8_t)-1;
                    }
                    break;
                }
            }
        }

        ////////////////////////////////
        //* **DoChannelList PASS 3 ***//
        ////////////////////////////////
        for (n = 0; n < LISTSIZE; ++n) {
            if (ChNew[n] != (uint8_t)-1) {
                // Locate the last open channel on the ChList.
                for (channel = hiChnl; (int8_t)loChnl <= (int8_t)channel;
                     --channel) {
                    if (ChList[channel] == (uint8_t)-1) {
                        break;
                    }
                }

                // Copy the channel to ChList, and update the channel.
                nodeChannel     = ChNew[n];
                ChList[channel] = nodeChannel;

                node = PlayList[nodeChannel >> 4];
                UpdateChannel(node, channel, nodeChannel & 0x0F);
            }
        }
    }

    ////////////////////////////////////
    //* **CleanUp Removed Channels ***//
    ////////////////////////////////////
    n = LISTSIZE;
    while (n != 0) {
        --n;

        // If any channels were active before but are not now,
        // turn it's notes off and reset it's NUMNOTES.
        if (ChOld[n] != (LISTSIZE - 1) && ChList[n] == (uint8_t)-1) {
            Driver(DController, n, DAMPRCTRL, 0);
            Driver(DController, n, ALLNOFF, 0);
            Driver(DController, n, NUMNOTES, 0);
        }
    }

    // Make the ChOld list from the ChList for use by the next call to this
    // procedure.
    for (n = 0; n < LISTSIZE; ++n) {
        ChOld[n] = ChList[n] & 0x0F;
    }

    // Construct the ChNodes list from the ChList and the PlayList for use in
    // the next call to this procedure.
    for (n = 0; n < LISTSIZE; ++n) {
        nodeChannel = ChList[n];
        if (nodeChannel != (uint8_t)-1) {
            ChNodes[n] = PlayList[nodeChannel >> 4];
        } else {
            ChNodes[n] = NULL;
        }
    }

    EnableSoundServer();
}

// Process Node Fade properties.
static void DoFade(Sound *node, uint8_t playPos)
{
    uint8_t dstVol;

    // If we still have some FadeCount left, count it down.
    if (node->sFadeCount != 0) {
        --node->sFadeCount;
        return;
    }
    node->sFadeCount = node->sFadeTicks;

    dstVol = node->sFadeDest & 0x7F;

    // Fade the sound down.
    if (dstVol < node->sVolume) {
        if ((node->sVolume - dstVol) > node->sFadeSteps) {
            DoChangeVol(node, playPos, node->sVolume - node->sFadeSteps, true);
            return;
        }

        DoChangeVol(node, playPos, dstVol, true);
    }
    // Fade the sound up.
    else if (dstVol > node->sVolume) {
        if ((dstVol - node->sVolume) > node->sFadeSteps) {
            DoChangeVol(node, playPos, node->sFadeSteps + node->sVolume, true);
            return;
        }

        DoChangeVol(node, playPos, dstVol, true);
    }

    node->sSignal    = 0xfe; // KLUDGE!!!
    node->sFadeSteps = 0;

    // Stop the fade and check to see if we need to end the sound.
    if ((node->sFadeDest & 0x80) != 0) {
        DoEnd(node);
        updateChnls = true;
    }
}

// Change Sound Node Volume.
static void DoChangeVol(Sound  *node,
                        uint8_t playPos,
                        uint8_t newVol,
                        bool    vRequest)
{
    uint8_t nodeChannel, vol;
    uint8_t n, i;

    // If its the same volume as before, then don't bother
    if (newVol == node->sVolume) {
        return;
    }

    // Store the new volume.
    node->sVolume = newVol;

    // If the node isn't on the PlayList, then exit.
    if (playPos == (uint8_t)-1) {
        return;
    }

    playPos <<= 4;

    // Change the volume of all real channels occupied by the sound.
    for (i = 0; i < LISTSIZE; ++i) {
        nodeChannel = ChList[i];
        if (nodeChannel != (uint8_t)-1 && (nodeChannel & 0xF0) == playPos) {
            n   = nodeChannel & 0x0F;
            vol = ScaleVolume(node->cVolume[n], node->sVolume);
            if (vRequest) {
                VolRequest[i] = vol;
            } else {
                VolRequest[i] = (uint8_t)-1;
                Driver(DController, i, VOLCTRL, vol);
            }
        }
    }

    // If there are any ghost channels in the node, change their volumes if no
    // one else owns their channels.
    for (i = 0; i < LISTSIZE; ++i) {
        n = node->tChannel[i];
        if (n == (uint8_t)-1) {
            break;
        }

        if ((node->cFlags[n] & 2) != 0 && ChList[n] == (uint8_t)-1) {
            vol = ScaleVolume(node->cVolume[n], node->sVolume);
            if (vRequest) {
                VolRequest[n] = vol;
            } else {
                VolRequest[n] = (uint8_t)-1;
                Driver(DController, n, VOLCTRL, vol);
            }
        }
    }
}

// Process Fade Volume requests.
static uint8_t DoVolRequests(void)
{
    uint8_t vol;
    uint8_t requests, i;

    //
    // Send the next two (or less) volume requests on the VolRequest list.
    //

    requests = 0;
    i        = requestChnl;
    do {
        vol = VolRequest[i];
        if (vol != (uint8_t)-1) {
            VolRequest[i] = (uint8_t)-1;
            Driver(DController, i, VOLCTRL, vol);
            if (++requests == 2) {
                requestChnl = i;
                break;
            }
        }

        if (++i == LISTSIZE) {
            i = 0;
        }
    } while (i != requestChnl);
    return requestChnl;
}

// Parse each node on Playlist.
static void SoundServer(void)
{
    Sound  *node;
    uint8_t n, i;

    if (processSnds != 0) {
        return;
    }

    LockMutex(&s_mutex);

    // Update the channel list if anything happened in the last interrupt to
    // warrant it.
    if (updateChnls) {
        DoChannelList();
    }

    // Fade and parse each node on the PlayList.
    for (i = 0, n = 0; i < LISTSIZE; ++i, ++n) {
        node = PlayList[i];
        if (node == NULL) {
            break;
        }

        if (!node->sPause) {
            if (node->sFadeSteps != 0) {
                DoFade(node, i);

                if (node->sSignal == (uint8_t)-1) {
                    --i;
                    continue;
                }
            }

            if (node->sSample != 0) {
                SampleActive(node);
            } else {
                ParseNode(node, n);
            }

            if (node->sSignal == (uint8_t)-1) {
                --i;
            }
        }
    }

    DoSamples();

    // Volume requests, and service the driver.
    DoVolRequests();
    Driver(DService, 0, 0, 0);
    UnlockMutex(&s_mutex);
}

static void SampleActive(Sound *node)
{
    uint i;

    for (i = 0; i < LISTSIZE; ++i) {
        if (SampleList[i] == node) {
            return;
        }
    }

    DoEnd(node);
    updateChnls = true;
}

static void DoSamples(void)
{
    Sound   *node;
    uint8_t *data;
    uint     i;

    for (i = 0; i < LISTSIZE; ++i) {
        node = SampleList[i];
        if (node == NULL) {
            break;
        }

        ++node->sTimer;

        data = node->sPointer +
               ((uint16_t *)node->sPointer)[(node->sSample & 0x0F) - 1];

        if (node->sSample <= 16) {
            node->sSample |= 0x80;
            Driver(DSamplePlay, data, node->sLoop, node->sVolume);
        } else {
            uint res = Driver(DSampleCheck, data, node->sLoop, 0);
            if ((res & 0xff00) != 0) {
                node->sTimer = 0;
            }

            if ((res & 0x00ff) != 0) {
                node->sSample = 0;
                DoEnd(node);
                updateChnls = true;
            }
        }
    }
}

#if defined(__WINDOWS__)

extern HWND g_hWndMain;
uint        g_sysTime = 0;

static void CALLBACK TimerCallback(UINT      uTimerID,
                                   UINT      uMsg,
                                   DWORD_PTR dwUser,
                                   DWORD_PTR dw1,
                                   DWORD_PTR dw2)
{
    static uint s_timeLag = 25;
    if (--s_timeLag != 0) {
        ++g_sysTime;
    } else {
        s_timeLag = 25;
    }

    SoundServer();
}

#else

void *TimerThread(void *argument)
{
    const uint64_t timeToWait = NanosecondsToAbsoluteTime(15625000ULL);
    uint64_t       now        = mach_absolute_time();
    while (true) {
        mach_wait_until(now + timeToWait);
        now = mach_absolute_time();
        SoundServer();
    }
    return NULL;
}

#endif

void InstallSoundServer(void)
{
    CreateMutex(&s_mutex, true);

#if defined(__WINDOWS__)
    // timeBeginPeriod(16);
    timeSetEvent(16, 1, &TimerCallback, (DWORD_PTR)g_hWndMain, TIME_PERIODIC);
#else
    {
        pthread_t          thread;
        struct sched_param param;
        pthread_attr_t     attr;
        pthread_attr_init(&attr);
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        pthread_attr_setschedparam(&attr, &param);
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        pthread_create(&thread, &attr, TimerThread, NULL);
    }
#endif
}
