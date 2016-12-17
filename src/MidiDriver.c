#include "MidiDriver.h"
#include "Midi.h"

#define MAX_VOLUME_BAR 15

static uint16_t s_noteMasks[128] = { 0 };
static uint8_t byte_2612[16]    = { 127, 127, 127, 127, 127, 127, 127, 127,
                                    127, 127, 127, 127, 127, 127, 127, 127 };
static uint8_t s_mainVolMsb[16] = { 127, 127, 127, 127, 127, 127, 127, 127,
                                    127, 127, 127, 127, 127, 127, 127, 127 };
static uint8_t s_volBarToScale[MAX_VOLUME_BAR + 1] = { 1,    8,    0x11, 0x19,
                                                       0x22, 0x2A, 0x32, 0x3B,
                                                       0x43, 0x4C, 0x54, 0x5C,
                                                       0x65, 0x6D, 0x76, 0x7F };
static bool    s_soundOn      = true;
static uint8_t s_volumeBar    = MAX_VOLUME_BAR;
static uint8_t s_mainVolScale = 127;
static uint    s_uDeviceID    = (uint)-1;

#if 0
static uint s_devID = 22;
static uint s_numVoices = 6;
static uint8_t s_loChnl = 0;
static uint8_t s_hiChnl = 15;
#elif 0
static uint    s_devID     = 11;
static uint    s_numVoices = 6;
static uint8_t s_loChnl    = 12;
static uint8_t s_hiChnl    = 14;
#else
static uint    s_devID     = 7;
static uint    s_numVoices = 16;
static uint8_t s_loChnl    = 0;
static uint8_t s_hiChnl    = 8;
#endif

static HMIDIOUT s_hMidiOut = NULL;

typedef int(__cdecl *DriverFunc)(uint16_t channel,
                                 uint8_t  data1,
                                 uint8_t  data2);
// {bp = func#} ax = channel, ch = data1, cl = data2

static uint __cdecl PatchReq(void);
static uint __cdecl Init(void);
static void __cdecl Terminate(void);
static void __cdecl Service() {}
static void __cdecl NoteOff(uint16_t channel, uint8_t note, uint8_t velocity);
static void __cdecl NoteOn(uint16_t channel, uint8_t note, uint8_t velocity);
static void __cdecl PolyAfterTch(uint16_t channel,
                                 uint8_t  note,
                                 uint8_t  pressure)
{
}
static void __cdecl Controller(uint16_t channel,
                               uint8_t  controller,
                               uint8_t  value);
static void __cdecl ProgramChange(uint16_t channel, uint8_t program);
static void __cdecl ChnlAfterTch(uint16_t channel, uint8_t pressure) {}
static void __cdecl PitchBend(uint16_t channel, uint8_t lsb, uint8_t msb);
static void __cdecl SetReverb(uint8_t mode) {}
static uint __cdecl MasterVol(uint8_t bar);
static void __cdecl SoundOn(uint8_t bar);
static void __cdecl SamplePlay(uint16_t channel, uint8_t loop, uint8_t volume)
{
}
static void __cdecl SampleEnd() {}
static void __cdecl SampleCheck(uint16_t channel, uint8_t loop) {}
static void __cdecl AskDriver() {}

static const DriverFunc s_funcs[] = {
    (DriverFunc)PatchReq,
    (DriverFunc)Init,
    (DriverFunc)Terminate,
    (DriverFunc)Service,
    (DriverFunc)NoteOff,
    (DriverFunc)NoteOn,
    (DriverFunc)PolyAfterTch,
    (DriverFunc)Controller,
    (DriverFunc)ProgramChange,
    (DriverFunc)ChnlAfterTch,
    (DriverFunc)PitchBend,
    (DriverFunc)SetReverb,
    (DriverFunc)MasterVol,
    (DriverFunc)SoundOn,
    (DriverFunc)SamplePlay,
    (DriverFunc)SampleEnd,
    (DriverFunc)SampleCheck,
    (DriverFunc)AskDriver
};

uint Driver(int function, uint16_t channel, uint8_t data1, uint8_t data2)
{
    return s_funcs[function](channel, data1, data2);
}

static void SendRawMsg(DWORD msg)
{
    midiOutShortMsg(s_hMidiOut, msg);
}

static void SendMsg(uint16_t channel,
                    uint8_t  command,
                    uint8_t  data1,
                    uint8_t  data2)
{
    DWORD msg;

    // al = channel, ah = command, ch = data1, cl = data2
    if (channel == 10) {
        channel = 15;
    }
    msg = (uint8_t)channel | command;
    msg |= (DWORD)data1 << 8;
    msg |= (DWORD)data2 << 16;
    SendRawMsg(msg);
}

static void TurnOffNote(uint16_t channel, uint8_t note)
{
    uint16_t mask = 1 << channel;
    s_noteMasks[note] &= ~mask;
}

static void TurnOnNote(uint16_t channel, uint8_t note)
{
    // channel = ax, note = ch
    uint16_t mask = 1 << channel;
    s_noteMasks[note] |= mask;
}

static void __cdecl NoteOff(uint16_t channel, uint8_t note, uint8_t velocity)
{
    TurnOffNote(channel, note);
    SendMsg(channel, NOTEOFF, note, velocity);
}

static void __cdecl NoteOn(uint16_t channel, uint8_t note, uint8_t velocity)
{
    TurnOnNote(channel, note);
    if (s_devID == 11) {
        velocity = s_mainVolScale;
    }
    SendMsg(channel, NOTEON, note, velocity);
}

static void SendAllNotesOff(uint16_t channel)
{
    // ax = channel
    if (s_devID == 7) {
        SendMsg(channel, CONTROLLER, 0x7B, 0);
    } else {
        uint16_t mask;
        uint8_t  note;

        mask = 1 << channel;

        for (note = 0; note < 128; ++note) {
            if ((s_noteMasks[note] & mask) != 0) {
                NoteOff(channel, note, 0);
            }
        }
    }
}

static uint __cdecl MasterVol(uint8_t bar)
{
    // cl = bar
    uint8_t channel;
    uint8_t res = s_volumeBar;

    if (bar != 255) {
        s_volumeBar = bar;

        if (s_soundOn) {
            s_mainVolScale = s_volBarToScale[s_volumeBar];

            channel = 16;
            while (channel-- != 0) {
                s_mainVolMsb[channel] =
                  (s_mainVolScale * byte_2612[channel]) >> 7;
                if (s_devID == 7) {
                    // Main Volume
                    SendMsg(channel, CONTROLLER, 7, s_mainVolMsb[channel]);
                }
            }
        }
    }

    return res;
}

static void __cdecl SoundOn(uint8_t bar)
{
    uint16_t channel;
    uint8_t  scale;

    // cl = bar
    if (bar != 255) {
        if (bar != 0) {
            s_soundOn = true;
            scale     = s_volBarToScale[s_volumeBar];
        } else {
            s_soundOn = false;
            scale     = 0;
        }

        channel = 16;
        while (channel-- != 0) {
            s_mainVolMsb[channel] = (scale * byte_2612[channel]) >> 7;
            if (s_devID == 7) {
                // Main Volume
                SendMsg(channel, CONTROLLER, 7, s_mainVolMsb[channel]);
            }
        }
    }
}

static uint __cdecl PatchReq(void)
{
    return 0xff | 1 << 8 | s_numVoices << 16 | s_devID << 24;
}

static uint __cdecl Init(void)
{
    midiOutOpen(&s_hMidiOut, s_uDeviceID, 0, 0, CALLBACK_NULL);
    return (((uint)s_loChnl | ((uint)s_hiChnl << 8)) << 16) | 0x2108;
    // ax = 2108h, cl = s_loChnl, ch = s_hiChnl
}

static void ClearChannels(void)
{
    uint16_t channel;
    for (channel = 0; channel < 16; ++channel) {
        SendAllNotesOff(channel);
    }
}

static void __cdecl Terminate(void)
{
    ClearChannels();
    midiOutClose(s_hMidiOut);
    s_hMidiOut = NULL;
}

static void __cdecl PitchBend(uint16_t channel, uint8_t lsb, uint8_t msb)
{
    SendMsg(channel, PBEND, lsb, msb);
}

static void __cdecl Controller(uint16_t channel,
                               uint8_t  controller,
                               uint8_t  value)
{
    // ax = channel, ch = controller, cl = value

    if (controller == 7) // Main Volume
    {
        byte_2612[channel] = value;
        if (s_soundOn) {
            s_mainVolMsb[channel] = (s_mainVolScale * value) >> 7;
            if (s_devID == 7) {
                SendMsg(channel, CONTROLLER, 7, s_mainVolMsb[channel]);
            }
        }
    } else if (controller == 0x7B) // All notes off
    {
        SendAllNotesOff(channel);
    } else {
        SendMsg(channel, CONTROLLER, controller, value);
    }
}

static void __cdecl ProgramChange(uint16_t channel, uint8_t program)
{
    DWORD msg;

    //    byte_2743[channel] = program;

    // ax = channel, cl = program
    if (channel == 10) {
        channel = 15;
    }
    msg = channel | PCHANGE;
    msg |= (DWORD)program << 8;
    SendRawMsg(msg);
}
