#ifndef EVENT_H
#define EVENT_H

#include "GrTypes.h"
#include "Object.h"

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

void ObjToEvent(const Obj *evtObj, REventRecord *evt);

#endif // EVENT_H
