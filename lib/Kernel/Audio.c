#include "sci/Kernel/Audio.h"
#include "sci/Kernel/ResName.h"
#include "sci/Kernel/Resource.h"
#include "sci/Driver/Audio/LPcmDriver.h"
#include "sci/Logger/Log.h"
#include "sci/PMachine/PMachine.h"
#include "sci/Utils/ErrMsg.h"
#include "sci/Utils/FileIO.h"
#include "sci/Utils/Timer.h"

#define CDAUDIOMAP        "CDAUDIO.MAP"

#pragma pack(push, 2)
typedef struct ResAudEntry {
    uint16_t resId;
    uint32_t offset : 28;
    uint32_t volNum : 4;
    uint32_t length;
} ResAudEntry;
#pragma pack(pop)

static bool   s_useAudio    = true;
static bool   s_audioDrv    = false;
static uint   s_audioRate   = 0;
static int    s_audioType   = 0;
static Handle s_audioMap    = NULL;
static ushort s_audioVolNum = (ushort)-1;
static ushort s_dacType     = (ushort)-1;
static size_t s_playingNum  = (size_t)-1;

static uint FindAudEntry(Handle    audioMap,
                         ushort   *volNum,
                         uint32_t *offset,
                         size_t    resNum);

bool InitAudioDriver(void)
{
    if (s_useAudio) {
        s_audioDrv = true;
        if (AudioDrv(A_INIT, 0) != 0) {
            s_audioDrv = false;
            Panic(E_NO_AUDIO);
        }

        if (AudioRate(11025) == 44100) {
            s_audioType = RES_CDAUDIO;
            s_audioMap  = LoadHandle(CDAUDIOMAP);
            if (s_audioMap == NULL) {
                s_audioDrv = false;
                RAlert(E_NO_CD_MAP);
            }
        } else {
            s_audioType = RES_AUDIO;
        }
    }
    return s_audioDrv;
}

void EndAudio(void)
{
    if (s_audioDrv) {
        AudioDrv(A_TERMINATE, 0);
    }
}

bool SelectAudio(size_t num, kArgs acc)
{
    char     fileName[64];
    uint32_t offset;
    size_t   len;
    int      fd;

    *acc       = 0;
    fileName[0] = '\0';
    if (s_audioType == RES_AUDIO && FindPatchEntry(RES_AUDIO, num)) {
        ResNameMake(fileName, RES_AUDIO, num);
        fd = fileopen(fileName, O_RDONLY);
        if (fd == -1) {
            return false;
        }

        offset = 0;
        len    = (size_t)filelength(fd);
        close(fd);
        s_audioVolNum = (ushort)-1;
    } else {
        ushort volNum;

        if (s_audioMap == NULL) {
            return false;
        }

        len = FindAudEntry(s_audioMap, &volNum, &offset, num);
        if (len == 0) {
            return false;
        }

        if (volNum != s_audioVolNum) {
            if (s_audioType == RES_AUDIO) {
                sprintf(fileName, "AUDIO%03u.%03u", s_dacType, volNum);
                fd = ROpenResFile(RES_AUDIO, volNum, fileName);
                if (fd == -1) {
                    return false;
                }
                close(fd);
            }
            s_audioVolNum = volNum;
        }
    }

    if (s_audioType != RES_CDAUDIO) {
        struct {
            uintptr_t path;
            uintptr_t offset;
            uintptr_t len;
        } audArgs;

        audArgs.path   = (*fileName != '\0') ? (uintptr_t)fileName : 0;
        audArgs.offset = offset;
        audArgs.len    = len;
        if (AudioDrv(A_SELECT, (uintptr_t)(&audArgs)) != 0) {
            return false;
        }

        *acc = (len * 60) / s_audioRate;
    } else {
        struct {
            uintptr_t volNum;
            uintptr_t offset;
            uintptr_t len;
        } audArgs;

        audArgs.volNum = s_audioVolNum;
        audArgs.offset = offset;
        audArgs.len    = len;
        if (AudioDrv(A_SELECT, (uintptr_t)(&audArgs)) != 0) {
            return false;
        }

        *acc = (len * 4) / 5;
    }
    return true;
}

void AudioWPlay(void)
{
    AudioDrv(A_WPLAY, 0);
}

void AudioPlay(uint a)
{
    struct {
        uintptr_t x;
    } audArgs;

    audArgs.x = a;
    AudioDrv(A_PLAY, (uintptr_t)(&audArgs));
}

// Stop the audio playback.
void AudioStop(uint a)
{
    struct {
        uintptr_t x;
    } audArgs;

    audArgs.x = a;
    AudioDrv(A_STOP, (uintptr_t)(&audArgs));
}

void AudioPause(void)
{
    if (s_audioDrv) {
        AudioDrv(A_PAUSE, 0);
    }
}

void AudioResume(void)
{
    if (s_audioDrv) {
        AudioDrv(A_RESUME, 0);
    }
}

uint AudioRate(uint hertz)
{
    struct {
        uintptr_t rate;
    } audArgs;

    audArgs.rate = hertz;
    AudioDrv(A_RATE, (uintptr_t)(&audArgs));
    return (uint)audArgs.rate;
}

uintptr_t AudioLoc(void)
{
    struct {
        uintptr_t loc;
    } audArgs;

    audArgs.loc = 0;
    AudioDrv(A_LOC, (uintptr_t)(&audArgs));
    return audArgs.loc;
}

void AudioVol(int vol)
{
    struct {
        uintptr_t vol;
    } audArgs;

    audArgs.vol = (uintptr_t)vol;
    AudioDrv(9, (uintptr_t)(&audArgs));
}

void Audio_14F05(uint a)
{
    struct {
        uintptr_t x;
    } audArgs;

    audArgs.x = a;
    AudioDrv(10, (uintptr_t)(&audArgs));
}

static ushort SelectDAC(ushort dacType)
{
    char fileName[32];

    if (s_audioType == RES_AUDIO && dacType != (ushort)-1) {
        if (s_audioMap != NULL) {
            DisposeResHandle(s_audioMap);
        }
        sprintf(fileName, "AUDIO%03u.MAP", dacType);
        s_audioMap = LoadHandle(fileName);
        if (s_audioMap != NULL) {
            s_audioVolNum = (ushort)-1;
            s_dacType     = dacType;
        } else {
            sprintf(fileName, "AUDIO%03u.MAP", s_dacType);
            s_audioMap = LoadHandle(fileName);
        }
    }
    return s_dacType;
}

static uint FindAudEntry(Handle    audioMap,
                         ushort   *volNum,
                         uint32_t *offset,
                         size_t    resNum)
{
    uint16_t           resId;
    uint               vol;
    const ResAudEntry *entry;
    const ResAudEntry *foundEntry = NULL;

    resId = (uint16_t)resNum;
    vol   = (uint)*volNum;

    for (entry = (ResAudEntry *)audioMap; entry->resId != (uint16_t)-1;
         ++entry) {
        if (entry->resId == resId) {
            *offset = entry->offset;
            if (entry->volNum == vol) {
                return entry->length;
            }

            foundEntry = entry;
        }
    }

    if (foundEntry != NULL) {
        *volNum = (ushort)foundEntry->volNum;
        return foundEntry->length;
    }
    return 0;
}

void KDoAudio(argList)
{
    static uintptr_t word_1E476 = 0;

    if (!s_audioDrv) {
        *acc = 0;
        return;
    }

    switch (arg(1)) {
        case WPLAY:
            s_playingNum = (size_t)-1;
            if (SelectAudio(arg(2), acc)) {
                s_playingNum = arg(2);
                word_1E476   = *acc;
                AudioStop(0);
                AudioWPlay();
            }
            break;

        case PLAY:
            if (s_playingNum == (size_t)-1 || s_playingNum != arg(2)) {
                if (!SelectAudio(arg(2), acc)) {
                    break;
                }
                AudioStop(0);
            } else {
                *acc = word_1E476;
            }
            s_playingNum = (size_t)-1;
            AudioPlay(argCount >= 3 ? arg(3) : 1);
            break;

        case STOP:
            s_playingNum = (size_t)-1;
            AudioStop(argCount >= 2 ? arg(2) : 0);
            break;

        case PAUSE:
            AudioPause();
            break;

        case RESUME:
            AudioResume();
            break;

        case LOC:
            *acc = AudioLoc();
            break;

        case RATE:
            s_audioRate = AudioRate((uint)arg(2));
            break;

        case VOLUME:
            AudioVol((int)arg(2));
            break;

        case DACFOUND:
            *acc = SelectDAC((ushort)arg(2));
            break;

        case 10:
            Audio_14F05(arg(2));
            break;

        default:
            break;
    }
}
