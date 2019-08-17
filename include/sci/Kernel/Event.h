#ifndef SCI_KERNEL_EVENT_H
#define SCI_KERNEL_EVENT_H

#include "sci/Kernel/GrTypes.h"
#include "sci/PMachine/Object.h"

typedef struct REventRecord {
    ushort type;      // type of event
    ushort message;   // variable data
    ushort modifiers; // misc extra stuff
    uint   when;      // 60 second of ticks
    RPoint where;     // global mouse coords
} REventRecord;

// Event types.
#define nullEvt   0x0000
#define mouseDown 0x0001
#define mouseUp   0x0002
#define keyDown   0x0004
#define keyUp     0x0008
#define menuStart 0x0010
#define menuHit   0x0020
#define direction 0x0040
#define saidEvent 0x0080
#define joyDown   0x0100
#define joyUp     0x0200

#define leaveEvt  0x8000

#define allEvents 0x7fff // mask to get all event types

// Event modifies.
#define shft 3 // either shift key
#define ctrl 4 // control key
#define alt  8 // alt key down

// Direction events.
#define dirStop 0
#define dirN    1
#define dirNE   2
#define dirE    3
#define dirSE   4
#define dirS    5
#define dirSW   6
#define dirW    7
#define dirNW   8

// Key definitions by name (based on IBM modified extended codes)
#define ESC        27
#define CR         0x0d
#define TAB        0x09
#define LF         0x0a
#define BS         0x08
#define SP         0x20
#define CLEARKEY   0x03 // clear line in TextEdit

// Numeric key code in scan code order with missing codes added.
#define HOMEKEY    0x4700
#define UPARROW    0x4800
#define PAGEUP     0x4900
#define LEFTARROW  0x4b00
#define CENTERKEY  0x4c00
#define RIGHTARROW 0x4d00
#define ENDKEY     0x4f00
#define DOWNARROW  0x5000
#define PAGEDOWN   0x5100
#define DELETE     0x5300
#define PAUSEKEY   0x7000
#define CTRLLEFT   0x7300
#define CTRLRIGHT  0x7400
#define CTRLEND    0x7500
#define CTRLPGDN   0x7600
#define CTRLHOME   0x7700
#define CTRLPGUP   0x8400

#define F1         0x3b00
#define F2         0x3c00
#define F3         0x3d00
#define F4         0x3e00
#define F5         0x3f00
#define F6         0x4000
#define F7         0x4100
#define F8         0x4200
#define F9         0x4300
#define F10        0x4400

// Init manager for num events.
void InitEvent(uint num);

void EndEvent(void);

// Return next event to user.
bool RGetNextEvent(ushort mask, REventRecord *event);

// Flush all events specified by mask from buffer.
void RFlushEvents(ushort mask);

// Return but don't remove.
bool REventAvail(ushort mask, REventRecord *event);

// Look for any mouse ups
bool RStillDown(void);

// Add event to Queue.
void RPostEvent(REventRecord *event);

void EventToObj(const REventRecord *evt, Obj *evtObj);

void ObjToEvent(Obj *evtObj, REventRecord *evt);

REventRecord *MapKeyToDir(REventRecord *evt);

#endif // SCI_KERNEL_EVENT_H
