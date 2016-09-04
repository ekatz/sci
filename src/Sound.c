#include "Sound.h"
#include "ErrMsg.h"
#include "Kernel.h"
#include "Midi.h"
#include "PMachine.h"
#include "Resource.h"
#include "Selector.h"

int g_reverbDefault = 0;

static List s_soundList      = LIST_INITIALIZER;
static int  s_numberOfVoices = 0;
static int  s_numberOfDACs   = 0;
static int  s_devID          = 0;

bool InitSoundDriver(void)
{
    int patchNum;
    //  Handle patchHandle;

    patchNum = DoSound(SPatchReq, &s_numberOfVoices, &s_numberOfDACs, &s_devID);

#if 0
    // Load patch (if one is needed).
    if (patchNum != -1)
    {
        if ((patchNum & 0x7f) == 10)
        {
            patchHandle = DoLoad(RES_PATCH, (patchNum & 0x7f));
        }
        else
        {
            patchHandle = ResLoad(RES_PATCH, (patchNum & 0x7f));
        }

        // If bit 7 of the patch number is set, then the patch will need to be locked permanently in hunk.
        if ((patchNum & 0x80) != 0)
        {
            ResLock(RES_PATCH, (patchNum & 0x7f), TRUE);
            LockHandle(patchHandle);
        }
    }
#endif

    if (DoSound(SInit, NULL) == -1) {
        RAlert(E_NO_MUSIC);
        return false;
    }

    // Initialize sound driver.
    InitList(&s_soundList);
    //   InstallSoundServer();

    DoSound(SProcess, TRUE);

    return true;
}

void TermSndDrv(void)
{
    KillAllSounds();
    DoSound(STerminate);
}

void KillAllSounds(void)
{
    Sound *sn;
#if 0
    Handle theHandle;
#endif

    while (!EmptyList(&s_soundList)) {
        sn = FromNode(FirstNode(&s_soundList), Sound);
        DoSound(SEnd, sn);
        ResLock(RES_SOUND, sn->sNumber, false);
#if 0
        if (theHandle = (Handle)GetProperty((Obj *)GetKey(ToNode(sn)), s_handle))
        {
            if ((int)theHandle != 1)
            {
                CriticalHandle(theHandle, false);
                UnlockHandle(theHandle);
            }
        }
#endif
        DeleteNode(&s_soundList, ToNode(sn));
    }
}

void RestoreAllSounds(void)
{
    Sound *sn;
    Obj   *soundObj;
    Handle sHandle;
    int    soundId;

    //
    // For every node on the sound list,
    // load the resource in the s_number property.
    // If the sState property of the node is non-zero,
    // restart the sound using the SRestore function in MIDI.C.
    //

    sn = FromNode(FirstNode(&s_soundList), Sound);

    while (sn != NULL) {
        soundObj = (Obj *)GetKey(ToNode(sn));
        soundId  = GetProperty(soundObj, s_number);
        if (soundId != 0) {
            ResLoad(RES_SOUND, soundId);
        }

        if (sn->sState != 0) {
            sHandle = ResLoad(RES_SOUND, soundId);

            //          CriticalHandle(sHandle, true);
            ResLock(RES_SOUND, soundId, true);

            SetProperty(soundObj, s_handle, (uintptr_t)sHandle);
            sn->sPointer = (uint8_t *)sHandle;
            DoSound(SRestore, sn);
            //          if(sn->sSample)
            //          {
            //              LockHandle(sHandle);
            //          }

            UpdateCues(soundObj);
        }

        sn = FromNode(NextNode(ToNode(sn)), Sound);
    }

    // Reset the default reverb mode.
    DoSound(SSetReverb, g_reverbDefault);
}

void SuspendSounds(bool onOff)
{
    //
    // Use the SProcess function of MIDI.C to ignore or honor calls to
    // SoundServer. A TRUE value in onOff will cause sounds to be suspended,
    // while a FALSE value will cause them to continue.
    //

    DoSound(SProcess, (int)!onOff);
}

// Allocate a sound node for the object being initialized (if there isn't
// already one), load the sound resource specified in the s_number property, and
// set up the node properties.
void InitSnd(Obj *soundObj)
{
    Sound *sn;
    size_t soundId;

    soundId = GetProperty(soundObj, s_number);
    if (soundId != 0) {
        ResLoad(RES_SOUND, soundId);
    }

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn == NULL) {
        sn = malloc(sizeof(Sound));
        if (sn == NULL) {
            return;
        }
        memset(sn, 0, sizeof(Sound));
        AddKeyToEnd(&s_soundList, ToNode(sn), soundObj);
        SetProperty(soundObj, s_nodePtr, (uintptr_t)sn);
    } else {
        StopSnd(soundObj); // TODO: This is not in the Windows version!
    }

    sn->sLoop = FALSE;
    if ((uint8_t)GetProperty(soundObj, s_loop) == (uint8_t)-1) {
        sn->sLoop = TRUE;
    }
    sn->sPriority = (uint8_t)GetProperty(soundObj, s_priority);
    sn->sVolume   = (uint8_t)GetProperty(soundObj, s_vol);
    sn->sSignal   = 0;
    sn->sDataInc  = 0;
}

void KillSnd(Obj *soundObj)
{
    Sound *sn;

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    StopSnd(soundObj);

    if (sn != NULL) {
        DeleteNode(&s_soundList, ToNode(sn));
        free(sn);
    }

    SetProperty(soundObj, s_nodePtr, 0);
}

void PlaySnd(Obj *soundObj, int how)
{
    Sound *sn;
    Handle sHandle;
    size_t soundId;

    //
    // Load the resource in the s_number property of the object,
    // lock the resource, and start the sound playing.
    //

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn == NULL) {
        InitSnd(soundObj);
        sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    }

    if (sn != NULL) {
        if (GetProperty(soundObj, s_handle) != 0) {
            StopSnd(soundObj);
        }
        soundId = GetProperty(soundObj, s_number);
        // why is the following not set in InitSnd?
        sn->sNumber = (short)soundId;

        sHandle = ResLoad(RES_SOUND, soundId);
        if (sHandle == NULL) {
            return;
        }
        //      CriticalHandle(sHandle, true);
        ResLock(RES_SOUND, soundId, true);

        SetProperty(soundObj, s_handle, (uintptr_t)sHandle);
        SetProperty(soundObj, s_signal, 0);
        SetProperty(soundObj, s_min, 0);
        SetProperty(soundObj, s_sec, 0);
        SetProperty(soundObj, s_frame, 0);

        sn->sPriority = (uint8_t)GetProperty(soundObj, s_priority);

        //
        // The following few lines should be removed after KQ5 cd and Jones cd
        // ship....
        //
        sn->sVolume = (uint8_t)GetProperty(soundObj, s_vol);
        if (GetProperty(soundObj, s_loop) == -1) {
            sn->sLoop = TRUE;
        } else {
            sn->sLoop = FALSE;
        }

        //
        // In the future, the volume property will be set to a value that will
        // be passed into this function.
        //

        sn->sPointer = (uint8_t *)sHandle;
        ChangeSndState(soundObj);
        DoSound(SPlay, sn, how);

        SetProperty(soundObj, s_priority, (uintptr_t)sn->sPriority);
    }
    //  else
    //  {
    //      SetProperty(soundObj, s_signal, -1);
    //  }
}

void StopSnd(Obj *soundObj)
{
    Sound *sn, *searchSn;

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn != NULL) {
        //
        // Search every sound in the sound list to make sure that there
        // are no other sounds playing off of the same resource as the one we
        // are now stopping. If there is, then we do not want to unlock the
        // resource when we stop the sound.
        //

        searchSn = FromNode(FirstNode(&s_soundList), Sound);
        while (searchSn != NULL) {
            if (searchSn != sn && searchSn->sPointer == sn->sPointer &&
                GetProperty((Obj *)GetKey(ToNode(searchSn)), s_handle) != 0) {
                break;
            }
            searchSn = FromNode(NextNode(ToNode(searchSn)), Sound);
        }

        DoSound(SEnd, sn);

        if (searchSn == NULL && GetProperty(soundObj, s_handle) != 0) {
            //          Handle theHandle;

            ResLock(RES_SOUND, sn->sNumber, false);
            //          if ((theHandle = (Handle)GetProperty(soundObj,
            //          s_handle)) != NULL)
            //          {
            //              if ((intptr_t)theHandle != 1)
            //              {
            //                  CriticalHandle(theHandle, false);
            //                  UnlockHandle(theHandle);
            //              }
            //          }
        }
    }

    SetProperty(soundObj, s_handle, 0);
    SetProperty(soundObj, s_signal, (uintptr_t)-1);
}

void PauseSnd(Obj *soundObj, bool stopStart)
{
    Sound *sn;

    if (soundObj == NULL) {
        DoSound(SPause, 0, (int)stopStart);
    } else {
        sn = (Sound *)GetProperty(soundObj, s_nodePtr);
        if (sn != NULL) {
            DoSound(SPause, sn, (int)stopStart);
        }
    }
}

void FadeSnd(Obj *soundObj, int newVol, int fTicks, int fSteps, int fEnd)
{
    Sound *sn;

    if (fEnd != 0) {
        newVol += 128;
    }

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn != NULL) {
        DoSound(SFade, sn, newVol, fTicks, fSteps);
    }
}

// This is another type of loop setting, in which sounds can be looped in the
// middle, and continue on towards the end after being released.
void HoldSnd(Obj *soundObj, int where)
{
    Sound *sn;

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn != NULL) {
        DoSound(SHold, sn, where);
    }
}

void SetSndVol(Obj *soundObj, int newVol)
{
    Sound *sn;

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn != NULL) {
        if (sn->sVolume != ((uint8_t)newVol)) {
            DoSound(SChangeVol, sn, newVol);
            SetProperty(soundObj, s_vol, newVol);
        }
    }
}

void SetSndPri(Obj *soundObj, int newPri)
{
    Sound *sn;

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn != NULL) {
        // If the value is -1, then simply clear the fixed priority flag
        // in the sound node and in the flags property of the sound object.
        if (newPri == -1) {
            sn->sFixedPri = FALSE;
            SetProperty(soundObj,
                        s_flags,
                        (GetProperty(soundObj, s_flags) & (-1 - mFIXEDPRI)));
        }
        // If it is not -1, then set both of those flags.
        else {
            sn->sFixedPri = TRUE;
            SetProperty(
              soundObj, s_flags, (GetProperty(soundObj, s_flags) | mFIXEDPRI));
            DoSound(SChangePri, sn, newPri);
        }
    }
}

void SetSndLoop(Obj *soundObj, int newLoop)
{
    Sound *sn;

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn != NULL) {
        if (newLoop == -1) {
            SetProperty(soundObj, s_loop, (uintptr_t)-1);
            sn->sLoop = TRUE;
        } else {
            SetProperty(soundObj, s_loop, 1);
            sn->sLoop = FALSE;
        }
    }
}

// This function should be called every game cycle (in the sounds check:
// method), or the time and cue information in the sound object will be wrong.
void UpdateCues(Obj *soundObj)
{
    Sound  *sn;
    uint8_t min, sec, frame, signal;

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn != NULL) {
        signal      = sn->sSignal;
        sn->sSignal = 0;
        switch (signal) {
            case 0xff:
                StopSnd(soundObj);
                break;

            case 0x00:
                if ((uint16_t)GetProperty(soundObj, s_dataInc) !=
                    sn->sDataInc) {
                    SetProperty(soundObj, s_dataInc, sn->sDataInc);
                    SetProperty(soundObj, s_signal, (sn->sDataInc + 127));
                }
                break;

            default:
                SetProperty(soundObj, s_signal, signal);
                break;
        }

        DoSound(SGetSYMPTE, sn, &min, &sec, &frame);
        SetProperty(soundObj, s_min, min);
        SetProperty(soundObj, s_sec, sec);
        SetProperty(soundObj, s_frame, frame);

        SetProperty(soundObj, s_vol, sn->sVolume);
    }
}

void MidiSend(Obj *soundObj, int channel, int command, int value1, int value2)
{
    Sound *sn;

    channel--;

    if (command == PBEND) {
        if (value1 > 8191) {
            value1 = 8191;
        }
        if (value1 < -8192) {
            value1 = -8192;
        }
    } else {
        if (value1 > 127) {
            value1 = 127;
        }
        if (value1 < 0) {
            value1 = 0;
        }

        if (value2 > 127) {
            value2 = 127;
        }
        if (value2 < 0) {
            value2 = 0;
        }
    }

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn != NULL) {
        switch (command) {
            case NOTEOFF:
                DoSound(SNoteOff, sn, channel, value1, value2);
                break;

            case NOTEON:
                DoSound(SNoteOn, sn, channel, value1, value2);
                break;

            case CONTROLLER:
                DoSound(SController, sn, channel, value1, value2);
                break;

            case PCHANGE:
                DoSound(SPChange, sn, channel, value1);
                break;

            case PBEND:
                DoSound(SPBend, sn, channel, value1 + 8192);
                break;
        }
    }
}

// TODO: This function is on it's way out (after KQ5 cd and Jones cd ship).
void ChangeSndState(Obj *soundObj)
{
    Sound  *sn;
    uint8_t val;

    //
    // Update the sLoop, sVolume, and sPriority properties of the sound node
    // to what is currently in those properties of object which the node belongs
    // to.
    //

    sn = (Sound *)GetProperty(soundObj, s_nodePtr);
    if (sn != NULL) {
        sn->sLoop = FALSE;
        if ((uint8_t)GetProperty(soundObj, s_loop) == (uint8_t)-1) {
            sn->sLoop = TRUE;
        }

        val = (uint8_t)GetProperty(soundObj, s_vol);
        if (sn->sVolume != val) {
            DoSound(SChangeVol, sn, val);
        }

        val = (uint8_t)GetProperty(soundObj, s_priority);
        if (sn->sPriority != val) {
            DoSound(SChangePri, sn, val);
        }
    }
}

void KDoSound(argList)
{
#define ret(val) g_acc = ((uintptr_t)(val))

    Obj *soundObj;

    soundObj = (Obj *)arg(2);

    switch (arg(1)) {
        case MASTERVOL:
            if (argCount == 1) {
                ret(DoSound(SMasterVol, 255));
            } else {
                ret(DoSound(SMasterVol, arg(2)));
            }
            break;

        case SOUNDON:
            if (argCount == 1) {
                ret(DoSound(SSoundOn, 255));
            } else {
                ret(DoSound(SSoundOn, arg(2)));
            }
            break;

        case RESTORESND:
            break;

        case NUMVOICES:
            ret(s_numberOfVoices);
            break;

        case NUMDACS:
            ret(s_numberOfDACs);
            break;

        case SUSPEND:
            SuspendSounds(arg(2) != FALSE);
            break;

        case INITSOUND:
            InitSnd(soundObj);
            break;

        case KILLSOUND:
            KillSnd(soundObj);
            break;

        case PLAYSOUND:
            PlaySnd(soundObj, (int)arg(3));
            break;

        case STOPSOUND:
            StopSnd(soundObj);
            break;

        case PAUSESOUND:
            PauseSnd(soundObj, arg(3) != FALSE);
            break;

        case FADESOUND:
            FadeSnd(
              soundObj, (int)arg(3), (int)arg(4), (int)arg(5), (int)arg(6));
            break;

        case HOLDSOUND:
            HoldSnd(soundObj, (int)arg(3));
            break;

        case SETVOL:
            SetSndVol(soundObj, (int)arg(3));
            break;

        case SETPRI:
            SetSndPri(soundObj, (int)arg(3));
            break;

        case SETLOOP:
            SetSndLoop(soundObj, (int)arg(3));
            break;

        case UPDATECUES:
            UpdateCues(soundObj);
            break;

        case MIDISEND:
            MidiSend(
              soundObj, (int)arg(3), (int)arg(4), (int)arg(5), (int)arg(6));
            break;

        case SETREVERB:
            if (argCount == 1) {
                ret(DoSound(SSetReverb, 255));
            } else {
                ret(DoSound(SSetReverb, arg(2)));
            }
            break;

            // This function is on it's way out (after KQ5 cd and Jones cd ship)
        case CHANGESNDSTATE:
            ChangeSndState(soundObj);
            break;
    }
}
