#include "MidiDriver.h"
#include "Midi.h"
#if defined(__WINDOWS__)
#elif defined(__IOS__)
#include "Resource.h"
#include <AudioToolbox/AUGraph.h>
#else
#include <CoreMIDI/CoreMIDI.h>
#endif

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

#if defined(__WINDOWS__)
static uint     s_uDeviceID = MIDI_MAPPER;
static HMIDIOUT s_hMidiOut  = NULL;
#elif defined(__IOS__)
static AUGraph   s_auGraph   = NULL;
static AudioUnit s_synthUnit;
static AudioUnit s_ioUnit;
#else
MIDIClientRef   s_midiClient = 0;
MIDIPortRef     s_midiPort   = 0;
MIDIEndpointRef s_midiDest   = 0;
#endif

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

static void SendMsg(uint16_t channel,
                    uint8_t  command,
                    uint8_t  data1,
                    uint8_t  data2)
{
#if defined(__WINDOWS__)
    DWORD msg;

    // al = channel, ah = command, ch = data1, cl = data2
    if (channel == 10) {
        channel = 15;
    }
    msg = (uint8_t)channel | command;
    msg |= (DWORD)data1 << 8;
    msg |= (DWORD)data2 << 16;
    midiOutShortMsg(s_hMidiOut, msg);
#elif defined(__IOS__)
    MusicDeviceMIDIEvent(
      s_synthUnit, (uint8_t)channel | command, data1, data2, 0);
#else
    Byte msg[3] = { (uint8_t)channel | command, data1, data2 };

    Byte            buffer[sizeof(MIDIPacketList) + sizeof(msg)];
    MIDIPacketList *packetList = (MIDIPacketList *)buffer;
    MIDITimeStamp   timeStamp  = 0; // AudioGetCurrentHostTime();

    MIDIPacket *packet = MIDIPacketListInit(packetList);
    MIDIPacketListAdd(
      packetList, sizeof(buffer), packet, timeStamp, sizeof(msg), msg);
    MIDISend(s_midiPort, s_midiDest, packetList);
#endif
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
#if defined(__WINDOWS__)
    midiOutOpen(&s_hMidiOut, s_uDeviceID, 0, 0, CALLBACK_NULL);
#elif defined(__IOS__)
    OSStatus result;

    // Create 2 audio units one sampler and one IO
    AUNode   synthNode, ioNode;
    UInt32   usesReverb;
    char     fontPath[512];
    CFURLRef bankURL;

    // Specify the common portion of an audio unit's identify,
    // used for both audio units in the graph.
    // Setup the manufacturer - in this case Apple.
    AudioComponentDescription cd = {};
    cd.componentManufacturer     = kAudioUnitManufacturer_Apple;

    // Instantiate an audio processing graph
    result = NewAUGraph(&s_auGraph);
    assert(result == noErr && "Unable to create an AUGraph object.");

    // Specify the Sampler unit, to be used as the first node of the graph.
    cd.componentType    = kAudioUnitType_MusicDevice; // type - music device
    cd.componentSubType = kAudioUnitSubType_MIDISynth;

    // Add the Sampler unit node to the graph
    result = AUGraphAddNode(s_auGraph, &cd, &synthNode);
    assert(result == noErr &&
           "Unable to add the Synthesizer unit to the audio processing graph.");

    // Specify the Output unit, to be used as the second and final node of the
    // graph.
    cd.componentType    = kAudioUnitType_Output;      // Output
    cd.componentSubType = kAudioUnitSubType_RemoteIO; // Output to speakers

    // Add the Output unit node to the graph
    result = AUGraphAddNode(s_auGraph, &cd, &ioNode);
    assert(result == noErr &&
           "Unable to add the Output unit to the audio processing graph.");

    // Open the graph
    result = AUGraphOpen(s_auGraph);
    assert(result == noErr && "Unable to open the audio processing graph.");

    // Connect the Sampler unit to the output unit
    result = AUGraphConnectNodeInput(s_auGraph, synthNode, 0, ioNode, 0);
    assert(result == noErr &&
           "Unable to interconnect the nodes in the audio processing graph.");

    // Obtain a reference to the Sampler unit from its node
    result = AUGraphNodeInfo(s_auGraph, synthNode, 0, &s_synthUnit);
    assert(result == noErr &&
           "Unable to obtain a reference to the Sampler unit.");

    // Obtain a reference to the I/O unit from its node
    result = AUGraphNodeInfo(s_auGraph, ioNode, 0, &s_ioUnit);
    assert(result == noErr && "Unable to obtain a reference to the I/O unit.");

    // Initialize the audio processing graph.
    result = AUGraphInitialize(s_auGraph);
    assert(result == noErr && "Unable to initialize AUGraph object.");

    // Disable reverb mode, as that sucks up a lot of CPU power, which can
    // be painful on low end machines.
    // TODO: Make this customizable via a config key?
    usesReverb = 0;
    AudioUnitSetProperty(s_synthUnit,
                         kAudioUnitProperty_UsesInternalReverb,
                         kAudioUnitScope_Global,
                         0,
                         &usesReverb,
                         sizeof(usesReverb));

    strcpy(fontPath, g_resDir);
    strcat(fontPath, "FreeFont.sf2");
    bankURL = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, (const UInt8 *)fontPath, strlen(fontPath), false);
    result = AudioUnitSetProperty(s_synthUnit,
                                  kMusicDeviceProperty_SoundBankURL,
                                  kAudioUnitScope_Global,
                                  0,
                                  &bankURL,
                                  sizeof(bankURL));
    CFRelease(bankURL);

    // Start the graph
    result = AUGraphStart(s_auGraph);
    assert(result == noErr && "Unable to start audio processing graph.");
#else
    OSStatus    result;
    CFStringRef name;
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
    name =
      CFStringCreateWithCString(NULL, "SciMidiClient", kCFStringEncodingASCII);
    result = MIDIClientCreate(name, NULL, NULL, &s_midiClient);
    CFRelease(name);
    if (result != noErr) {
        LogError("Failed to create MIDI client!");
    }

    name =
      CFStringCreateWithCString(NULL, "SciMidiOutput", kCFStringEncodingASCII);
    result = MIDIOutputPortCreate(s_midiClient, name, &s_midiPort);
    CFRelease(name);
    if (result != noErr) {
        LogError("Failed to create MIDI output port!");
    }

    s_midiDest = MIDIGetDestination(0);
    if (s_midiDest == 0) {
        LogError("Failed to get MIDI output destination (%u)!",
                 MIDIGetNumberOfDestinations());
    }
#endif
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

#if defined(__WINDOWS__)
    midiOutClose(s_hMidiOut);
    s_hMidiOut = NULL;
#elif defined(__IOS__)
    if (s_auGraph != NULL) {
        AUGraphStop(s_auGraph);
        DisposeAUGraph(s_auGraph);
        s_auGraph = NULL;
    }
#else
    if (s_midiPort != 0) {
        MIDIPortDispose(s_midiPort);
        s_midiPort = 0;
    }

    if (s_midiClient != 0) {
        MIDIClientDispose(s_midiClient);
        s_midiClient = 0;
    }
#endif
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
    //    byte_2743[channel] = program;

#if defined(__IOS__)
    UInt32 enabled = 1;
    AudioUnitSetProperty(s_synthUnit,
                         kAUMIDISynthProperty_EnablePreload,
                         kAudioUnitScope_Global,
                         0,
                         &enabled,
                         sizeof(enabled));

    SendMsg(channel, PCHANGE, program, 0);

    enabled = 0;
    AudioUnitSetProperty(s_synthUnit,
                         kAUMIDISynthProperty_EnablePreload,
                         kAudioUnitScope_Global,
                         0,
                         &enabled,
                         sizeof(enabled));
#endif

    // ax = channel, cl = program
    SendMsg(channel, PCHANGE, program, 0);
}
