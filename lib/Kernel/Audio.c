#if defined(__APPLE__)
#include <MacTypes.h>
#endif
#include "Audio.h"
#include "ErrMsg.h"
#include "FileIO.h"
#include "Input.h"
#include "Kernel.h"
#include "PMachine.h"
#include "ResName.h"
#include "Resource.h"
#include "Timer.h"

#if defined(__WINDOWS__)
#pragma comment(lib, "Winmm.lib")
#elif defined(__IOS__)
#include <AudioToolbox/AudioToolbox.h>
#endif

#define CDAUDIOMAP        "CDAUDIO.MAP"
#define AUDIO_BUFFER_SIZE (28 * 1024)

#pragma pack(push, 2)
typedef struct ResAudEntry {
    uint16_t resId;
    uint32_t offset : 28;
    uint32_t volNum : 4;
    uint32_t length;
} ResAudEntry;
#pragma pack(pop)

static bool   s_audioInit     = false;
static bool   s_audioPlaying  = false;
static bool   s_audioStopping = false;
static bool   s_memCheck      = false;
static uint   s_audioStopTick = 0;
static int    s_fd            = -1;
static uint   s_fileSize      = 0;
static uint   s_dataRead      = 0;
static bool   s_useAudio      = true;
static bool   s_audioDrv      = false;
static uint   s_audioRate     = 0;
static int    s_audioType     = 0;
static Handle s_audioMap      = NULL;
static ushort s_audioVolNum   = (ushort)-1;
static ushort s_dacType       = (ushort)-1;
static size_t s_playingNum    = (size_t)-1;

#if defined(__WINDOWS__)
extern HWND         g_hWndMain;
static WAVEFORMATEX waveFormat    = { 0 };
static HWAVEOUT     s_hWaveOut    = NULL;
static WAVEHDR      s_waveOutHdr0 = { 0 }, s_waveOutHdr1 = { 0 };
static char        *s_audioBuffer0 = NULL, *s_audioBuffer1 = NULL;
static uint8_t      s_audioBufferId = 0;
#elif defined(__IOS__)
static AudioComponentInstance s_outputInstance;
static char                  *s_audioBuffer       = NULL;
static uint                   s_audioBufferOffset = 0;

static int ReadAudioFile(void *buffer, uint size);

static OSStatus AudioCallback(void                       *inRefCon,
                              AudioUnitRenderActionFlags *ioActionFlags,
                              const AudioTimeStamp       *inTimeStamp,
                              UInt32                      inBusNumber,
                              UInt32                      inNumberFrames,
                              AudioBufferList            *ioData)
{
    AudioBuffer *abuf;
    void        *ptr;
    UInt32       remaining, len;
    UInt32       i;

    if (!s_audioPlaying || s_audioStopping || s_dataRead == 0) {
        for (i = 0; i < ioData->mNumberBuffers; ++i) {
            abuf = &ioData->mBuffers[i];
            memset(abuf->mData, 0x80, abuf->mDataByteSize);
        }

        AudioOutputUnitStop(s_outputInstance);
        s_audioPlaying = false;
        return noErr;
    }

    for (i = 0; i < ioData->mNumberBuffers; ++i) {
        abuf      = &ioData->mBuffers[i];
        remaining = abuf->mDataByteSize;
        ptr       = abuf->mData;
        while (remaining > 0) {
            if (s_audioBufferOffset >= s_dataRead) {
                s_audioBufferOffset = 0;
                ReadAudioFile(s_audioBuffer, AUDIO_BUFFER_SIZE);
                if (s_dataRead == 0) {
                    memset(ptr, 0x80, remaining);
                    while (++i < ioData->mNumberBuffers) {
                        abuf = &ioData->mBuffers[i];
                        memset(abuf->mData, 0x80, abuf->mDataByteSize);
                    }

                    AudioOutputUnitStop(s_outputInstance);
                    s_audioPlaying = false;
                    break;
                }
            }

            len = s_dataRead - s_audioBufferOffset;
            if (len > remaining) {
                len = remaining;
            }
            memcpy(ptr, s_audioBuffer + s_audioBufferOffset, len);
            ptr = (uint8_t *)ptr + len;
            remaining -= len;
            s_audioBufferOffset += len;
        }
    }

    /*
    if (s_audioBufferId == 0)
    {
        s_audioBufferId = 1;
        s_waveOutHdr1.dwBufferLength = s_dataRead;
        res = waveOutWrite(s_hWaveOut, &s_waveOutHdr1, sizeof(WAVEHDR));
        if (res != MMSYSERR_NOERROR)
        {
            PanicAudio(res);
        }
        memcpy(ioData->mBuffers[0].mData)
        ReadAudioFile(s_audioBuffer0, AUDIO_BUFFER_SIZE);
    }
    else
    {
        s_audioBufferId = 0;
        s_waveOutHdr0.dwBufferLength = s_dataRead;
        res = waveOutWrite(s_hWaveOut, &s_waveOutHdr0, sizeof(WAVEHDR));
        if (res != MMSYSERR_NOERROR)
        {
            PanicAudio(res);
        }
        ReadAudioFile(s_audioBuffer1, AUDIO_BUFFER_SIZE);
    }*/

    return noErr;
}
#endif

static void InitAudioDrv(void);
static int  ReadAudioFile(void *buffer, uint size);
static uint FindAudEntry(Handle    audioMap,
                         ushort   *volNum,
                         uint32_t *offset,
                         size_t    resNum);
static void PanicAudio(int err) {}

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

bool SelectAudio(size_t num)
{
    char     fileName[64];
    uint32_t offset;
    size_t   len;
    int      fd;

    g_acc       = 0;
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

        g_acc = (len * 60) / s_audioRate;
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

        g_acc = (len * 4) / 5;
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

uint AudioLoc(void)
{
    struct {
        uintptr_t loc;
    } audArgs;

    audArgs.loc = 0;
    AudioDrv(A_LOC, (uintptr_t)(&audArgs));
    return (uint)audArgs.loc;
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

int AudioDrv(int function, uintptr_t qualifier)
{
#if defined(__WINDOWS__)
    MMRESULT res;
#endif

    switch (function) {
        case A_INIT:
            if (s_audioInit) {
                return 0;
            }

#if defined(__WINDOWS__)
            if (waveOutGetNumDevs() == 0) {
                LogError("No audio device!");
                return -1;
            }

            waveFormat.wFormatTag      = WAVE_FORMAT_PCM;
            waveFormat.nChannels       = 1;
            waveFormat.nSamplesPerSec  = 11025;
            waveFormat.nAvgBytesPerSec = 11025;
            waveFormat.nBlockAlign     = 1;
            waveFormat.wBitsPerSample  = 8;
            waveFormat.cbSize          = 0;
            res                        = waveOutOpen(
              &s_hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL);
            if (res != MMSYSERR_NOERROR) {
                PanicAudio(res);
            }

            s_audioBuffer0 = (char *)malloc(2 * AUDIO_BUFFER_SIZE);
            s_audioBuffer1 = s_audioBuffer0 + AUDIO_BUFFER_SIZE;
#elif defined(__IOS__)
            s_audioBuffer = (char *)malloc(AUDIO_BUFFER_SIZE);
#endif
            InitAudioDrv();
            return 0;

        case A_SELECT:
            s_fileSize = (uint)((uintptr_t *)qualifier)[2];
            if (((uintptr_t *)qualifier)[0] != 0) {
                if (s_fd != -1) {
                    close(s_fd);
                }
                s_fd =
                  fileopen((const char *)((uintptr_t *)qualifier)[0], O_RDONLY);
                if (s_fd == -1) {
                    return -1;
                }
            }
            if (lseek(s_fd, (int)((uintptr_t *)qualifier)[1], SEEK_SET) == -1) {
                return -1;
            }

            return 0;

        case A_WPLAY:
            s_memCheck = true;
#if defined(__WINDOWS__)
            ReadAudioFile(s_audioBuffer0, 2 * AUDIO_BUFFER_SIZE);
#elif defined(__IOS__)
            ReadAudioFile(s_audioBuffer, AUDIO_BUFFER_SIZE);
#endif
            return 0;

        case A_PLAY:
            if (!s_audioInit) {
                InitAudioDrv();
            }
            if (s_memCheck) {
                s_memCheck = false;
            } else {
#if defined(__WINDOWS__)
                ReadAudioFile(s_audioBuffer0, 2 * AUDIO_BUFFER_SIZE);
#elif defined(__IOS__)
                ReadAudioFile(s_audioBuffer, AUDIO_BUFFER_SIZE);
#endif
            }
            s_audioPlaying  = true;
            s_audioStopTick = RTickCount();
#if defined(__WINDOWS__)
            if ((unsigned __int16)s_dataRead >= AUDIO_BUFFER_SIZE) {
                s_waveOutHdr0.dwBufferLength = AUDIO_BUFFER_SIZE;
                s_dataRead -= AUDIO_BUFFER_SIZE;
            } else {
                s_waveOutHdr0.dwBufferLength = s_dataRead;
                s_dataRead                   = 0;
            }
            s_audioBufferId = 0;
            res = waveOutWrite(s_hWaveOut, &s_waveOutHdr0, sizeof(WAVEHDR));
            if (res != MMSYSERR_NOERROR) {
                PanicAudio(res);
            }
#elif defined(__IOS__)
            s_audioBufferOffset = 0;
            if (AudioOutputUnitStart(s_outputInstance)) {
                LogError("Unable to start audio unit.");
            }
#endif
            return 0;

        case A_STOP:
            s_audioStopping = true;
            while (s_audioPlaying) {
                PollInputEvent();
            }
            s_audioStopping = false;
            return 0;

        case A_RATE:
            s_audioRate = (uint)((uintptr_t *)qualifier)[0];
            return (int)s_audioRate;

        case A_FILLBUFF:
#if defined(__WINDOWS__)
            if (s_audioPlaying) {
                if (s_dataRead != 0 && !s_audioStopping) {
                    if (s_audioBufferId == 0) {
                        s_audioBufferId              = 1;
                        s_waveOutHdr1.dwBufferLength = s_dataRead;
                        res                          = waveOutWrite(
                          s_hWaveOut, &s_waveOutHdr1, sizeof(WAVEHDR));
                        if (res != MMSYSERR_NOERROR) {
                            PanicAudio(res);
                        }
                        ReadAudioFile(s_audioBuffer0, AUDIO_BUFFER_SIZE);
                    } else {
                        s_audioBufferId              = 0;
                        s_waveOutHdr0.dwBufferLength = s_dataRead;
                        res                          = waveOutWrite(
                          s_hWaveOut, &s_waveOutHdr0, sizeof(WAVEHDR));
                        if (res != MMSYSERR_NOERROR) {
                            PanicAudio(res);
                        }
                        ReadAudioFile(s_audioBuffer1, AUDIO_BUFFER_SIZE);
                    }
                } else {
                    waveOutReset(s_hWaveOut);
                    s_audioPlaying = false;
                }
            }
#else
            assert(false);
#endif
            return 0;

        case A_LOC:
            if (s_audioPlaying) {
                ((uintptr_t *)qualifier)[0] = RTickCount() - s_audioStopTick;
            } else {
                ((uintptr_t *)qualifier)[0] = (uintptr_t)-1;
            }
            return (int)((uintptr_t *)qualifier)[0];

        case A_TERMINATE:
#if defined(__WINDOWS__)
            if (s_hWaveOut != NULL) {
                waveOutReset(s_hWaveOut);
                waveOutClose(s_hWaveOut);
                s_hWaveOut     = NULL;
                s_audioPlaying = false;
            }
            if (s_audioBuffer0 != NULL) {
                free(s_audioBuffer0);
                s_audioBuffer1 = s_audioBuffer0 = NULL;
            }
#elif defined(__IOS__)
            if (s_audioInit) {
                AudioOutputUnitStop(s_outputInstance);
                AudioComponentInstanceDispose(s_outputInstance);
                s_audioInit = false;
            }
            if (s_audioBuffer != NULL) {
                free(s_audioBuffer);
                s_audioBuffer = NULL;
            }
#endif
            return 0;

        default:
            return 0;
    }
}

static void InitAudioDrv(void)
{
#if defined(__WINDOWS__)
    MMRESULT res;

    res = waveOutOpen(&s_hWaveOut,
                      WAVE_MAPPER,
                      &waveFormat,
                      (DWORD_PTR)g_hWndMain,
                      0,
                      CALLBACK_WINDOW);
    if (res != MMSYSERR_NOERROR) {
        PanicAudio(res);
    }

    s_waveOutHdr0.lpData          = s_audioBuffer0;
    s_waveOutHdr0.dwBufferLength  = AUDIO_BUFFER_SIZE;
    s_waveOutHdr0.dwBytesRecorded = 0;
    s_waveOutHdr0.dwUser          = 0;
    s_waveOutHdr0.dwFlags         = 0;
    s_waveOutHdr0.dwLoops         = 0;
    res = waveOutPrepareHeader(s_hWaveOut, &s_waveOutHdr0, sizeof(WAVEHDR));
    if (res != MMSYSERR_NOERROR) {
        PanicAudio(res);
    }

    s_waveOutHdr1.lpData          = s_audioBuffer1;
    s_waveOutHdr1.dwBufferLength  = AUDIO_BUFFER_SIZE;
    s_waveOutHdr1.dwBytesRecorded = 0;
    s_waveOutHdr1.dwUser          = 0;
    s_waveOutHdr1.dwFlags         = 0;
    s_waveOutHdr1.dwLoops         = 0;
    res = waveOutPrepareHeader(s_hWaveOut, &s_waveOutHdr1, sizeof(WAVEHDR));
    if (res != MMSYSERR_NOERROR) {
        PanicAudio(res);
    }
#elif defined(__IOS__)
    struct AURenderCallbackStruct callback = {};
    AudioComponent                outputComp;
    AudioComponentDescription     desc;
    AudioStreamBasicDescription   streamFormat;

    streamFormat.mFormatID         = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags      = kLinearPCMFormatFlagIsPacked;
    streamFormat.mChannelsPerFrame = 1;
    streamFormat.mSampleRate       = 11025;
    streamFormat.mBitsPerChannel   = 8;
    streamFormat.mFramesPerPacket  = 1;
    streamFormat.mBytesPerFrame =
      streamFormat.mBitsPerChannel * streamFormat.mChannelsPerFrame / 8;
    streamFormat.mBytesPerPacket =
      streamFormat.mBytesPerFrame * streamFormat.mFramesPerPacket;

    // Locate the default output audio unit.
    desc.componentType         = kAudioUnitType_Output;
    desc.componentSubType      = kAudioUnitSubType_RemoteIO;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags        = 0;
    desc.componentFlagsMask    = 0;

    outputComp = AudioComponentFindNext(NULL, &desc);
    if (outputComp == NULL) {
        LogError("Failed to open default audio device.");
    }

    if (AudioComponentInstanceNew(outputComp, &s_outputInstance) != noErr) {
        LogError("Failed to open default audio device.");
    }

    if (AudioUnitInitialize(s_outputInstance) != noErr) {
        LogError("Unable to initialize audio unit instance.");
    }

    if (AudioUnitSetProperty(s_outputInstance,
                             kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Input,
                             0,
                             &streamFormat,
                             sizeof(streamFormat))) {
        LogError("Failed to set audio unit input property.");
    }

    callback.inputProc = AudioCallback;
    if (AudioUnitSetProperty(s_outputInstance,
                             kAudioUnitProperty_SetRenderCallback,
                             kAudioUnitScope_Input,
                             0,
                             &callback,
                             sizeof(callback))) {
        LogError("Unable to attach an IOProc to the selected audio unit.");
    }
#endif

    s_audioInit = true;
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

static int ReadAudioFile(void *buffer, uint size)
{
    int count;

    s_dataRead = 0;
    if (s_fileSize == 0) {
        return 0;
    }

    if (size > s_fileSize) {
        size = s_fileSize;
    }

    count = read(s_fd, buffer, size);
    if (count == -1) {
        LogError("Read failed!");
        return -1;
    }

    s_dataRead = (uint)count;
    s_fileSize -= s_dataRead;
    return count;
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
        g_acc = 0;
        return;
    }

    switch (arg(1)) {
        case WPLAY:
            s_playingNum = (size_t)-1;
            if (SelectAudio(arg(2))) {
                s_playingNum = arg(2);
                word_1E476   = g_acc;
                AudioStop(0);
                AudioWPlay();
            }
            break;

        case PLAY:
            if (s_playingNum == (size_t)-1 || s_playingNum != arg(2)) {
                if (!SelectAudio(arg(2))) {
                    break;
                }
                AudioStop(0);
            } else {
                g_acc = word_1E476;
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
            g_acc = AudioLoc();
            break;

        case RATE:
            AudioRate((uint)arg(2));
            break;

        case VOLUME:
            AudioVol((int)arg(2));
            break;

        case DACFOUND:
            g_acc = SelectDAC((ushort)arg(2));
            break;

        case 10:
            Audio_14F05(arg(2));
            break;

        default:
            break;
    }
}