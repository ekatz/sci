#include "Event.h"
#include "Input.h"
#include "Mouse.h"
#include "Selector.h"
#include "Timer.h"

static REventRecord *s_evHead     = NULL;
static REventRecord *s_evTail     = NULL;
static REventRecord *s_evQueue    = NULL;
static REventRecord *s_evQueueEnd = NULL;

static void MakeNullEvent(REventRecord *event);

// Move s_evQueue pointer to next slot.
static REventRecord *bump(REventRecord *ptr);

void InitEvent(uint num)
{
    // Setup the event queue.
    s_evQueue = (REventRecord *)malloc(num * sizeof(REventRecord));
    s_evHead = s_evTail = s_evQueue;
    s_evQueueEnd        = (s_evQueue + num);
}

void EndEvent(void) {}

bool RGetNextEvent(ushort mask, REventRecord *event)
{
    bool          ret = false;
    REventRecord *found;

    PollInputEvent();

    // Scan all events in s_evQueue.
    found = s_evHead;
    while (found != s_evTail) {
        if ((found->type & mask) != 0) {
            ret = true;
            break;
        }
        found = bump(found);
    }

    // Give it to him and blank it out.
    if (ret) {
        memcpy(event, found, sizeof(REventRecord));
        found->type = nullEvt;
        s_evHead    = bump(s_evHead);
    } else {
        // Use his storage.
        MakeNullEvent(event);
    }
    return ret;
}

void RFlushEvents(ushort mask)
{
    REventRecord event;

    while (RGetNextEvent(mask, &event))
        ;
}

bool REventAvail(ushort mask, REventRecord *event)
{
    bool          ret = false;
    REventRecord *found;

    // Scan all events in s_evQueue.
    found = s_evHead;
    while (found != s_evTail) {
        if ((found->type & mask) != 0) {
            ret = true;
            break;
        }
        found = bump(found);
    }

    // A null REventRecord pointer says just return result.
    if (event != NULL) {
        if (ret) {
            memcpy(event, found, sizeof(REventRecord));
        } else {
            MakeNullEvent(event);
        }
    }

    return ret;
}

bool RStillDown(void)
{
    return !REventAvail(mouseUp, NULL);
}

// Add event to s_evQueue at s_evTail, bump s_evHead if == s_evTail
void RPostEvent(REventRecord *event)
{
    event->when = RTickCount();
    memcpy(s_evTail, event, sizeof(REventRecord));
    s_evTail = bump(s_evTail);

    // Throw away oldest.
    if (s_evTail == s_evHead) {
        s_evHead = bump(s_evHead);
    }
}

void EventToObj(const REventRecord *evt, Obj *evtObj)
{
    IndexedProp(evtObj, evType) = (uintptr_t)evt->type;
    IndexedProp(evtObj, evMod)  = (uintptr_t)evt->modifiers;
    IndexedProp(evtObj, evMsg)  = (uintptr_t)evt->message;
    IndexedProp(evtObj, evX)    = (uintptr_t)evt->where.h;
    IndexedProp(evtObj, evY)    = (uintptr_t)evt->where.v;
}

void ObjToEvent(const Obj *evtObj, REventRecord *evt)
{
    evt->type      = (ushort)IndexedProp(evtObj, evType);
    evt->modifiers = (ushort)IndexedProp(evtObj, evMod);
    evt->message   = (ushort)IndexedProp(evtObj, evMsg);
    evt->where.h   = (int16_t)IndexedProp(evtObj, evX);
    evt->where.v   = (int16_t)IndexedProp(evtObj, evY);
}

static void MakeNullEvent(REventRecord *event)
{
    event->type = 0;
    CurMouse(&(event->where));
    event->modifiers = GetModifiers();
}

static REventRecord *bump(REventRecord *ptr)
{
    if (++ptr == s_evQueueEnd) {
        ptr = s_evQueue;
    }
    return ptr;
}
