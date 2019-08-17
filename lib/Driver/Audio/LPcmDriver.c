#if defined(__APPLE__)
#include <MacTypes.h>
#endif
#include "sci/Driver/Audio/LPcmDriver.h"
#include "sci/Driver/Input/Input.h"
#include "sci/Logger/Log.h"
#include "sci/Utils/FileIO.h"
#include "sci/Utils/Timer.h"

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

static int ReadAudioFile(void *buffer, uint size);

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
static void PanicAudio(int err) {}

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
