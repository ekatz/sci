#ifndef SCI_KERNEL_AUDIO_H
#define SCI_KERNEL_AUDIO_H

#include "sci/Kernel/Kernel.h"

enum audioFuncs {
    WPLAY = 1,
    PLAY,
    STOP,
    PAUSE,
    RESUME,
    LOC,
    RATE,
    VOLUME,
    DACFOUND,
    CDREDBOOK,
    QUEUE
};

bool InitAudioDriver(void);

// Prepare the sound for playback.
void AudioWPlay();

// Declare the audio playback rate.
uint AudioRate(uint hertz);

void KDoAudio(argList);

#endif // SCI_KERNEL_AUDIO_H
