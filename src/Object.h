#ifndef OBJECT_H
#define OBJECT_H

#include "List.h"

#ifdef SendMessage
#undef SendMessage
#endif

#pragma warning(push)
#pragma warning(disable : 4200) // zero-sized array in struct/union
#pragma warning(disable : 4201) // nameless struct/union

struct Obj;

typedef uintptr_t (*PfnMethod)(struct Obj *obj, uintptr_t args[]);

typedef struct SelList {
    unsigned short count;
    unsigned short sels[0];
} SelList;

typedef struct Species {
    const struct Species *super;
    const SelList        *propSels;
    const SelList        *methodSels;
    const PfnMethod      *methods;
} Species;

typedef struct Obj {
    const Species *species;
    uintptr_t      props[0];
} Obj;

enum { PROP_INFO_OFFSET, PROP_NAME_OFFSET, PROP_VALUES_OFFSET };

#define RES_PROP_INFO_OFFSET 2

#define IndexedPropAddr(object, prop)                                          \
    ((object)->props + g_objOfs[prop] - RES_PROP_INFO_OFFSET)
#define IndexedProp(object, prop) (*IndexedPropAddr(object, prop))

extern uint g_objOfs[];

// Load the offsets to indexed object properties from a file.
void LoadPropOffsets(void);

bool IsObject(Obj *obj);

// Return pointer to copy of an object or class.
Obj *Clone(Obj *obj);

// Dispose of a clone of a class.
void DisposeClone(Obj *obj);

// Invoke an object method from the kernel.
// 'obj' is the object to send the 'selector' message.
// 'sel' is the number of arguments to the method,
// 'argc' is the start of the argument list.
uintptr_t InvokeMethod(Obj *obj, ObjID sel, uint argc, ...);
uintptr_t CallMethod(Obj *obj, uintptr_t sel, uintptr_t args[]);

// Send messages to the given object.
uintptr_t SendMessage(Obj *obj, uintptr_t sel, uintptr_t args[]);

// Return whether 'selector' is a property or method of 'obj' or its
// superclasses
bool RespondsTo(Obj *obj, uint selector);

bool IsKindOf(const Obj *obj1, const Obj *obj2);

// Gets the address of an object's property.
uintptr_t *GetPropAddr(Obj *obj, uint prop);

// Return the value of a property for an object.
uintptr_t GetProperty(Obj *obj, uintptr_t prop);

// Set the value of a property for an object.
void SetProperty(Obj *obj, uintptr_t prop, uintptr_t value);

const char *GetObjName(Obj *obj);

char *GetSelectorName(uint id, char *str);

int GetSelectorNum(char *str);

#pragma warning(pop)

#endif // OBJECT_H
