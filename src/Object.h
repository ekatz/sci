#ifndef OBJECT_H
#define OBJECT_H

#include "Script.h"

#ifdef SendMessage
#undef SendMessage
#endif

#pragma warning(push)
#pragma warning(disable : 4200) // zero-sized array in struct/union
#pragma warning(disable : 4201) // nameless struct/union

#define OBJID ((uint16_t)0x1234)

typedef struct ObjHeader // sizeof = 10
{
    uint16_t magic;       // -10
    Script  *script;      // -8
    ObjID   *funcSelList; // -6
    Script  *scriptSuper; // -4 // This is the class script for "OBJECT"

    // Packing is important as the size of a selectors list is always at pos -1.
    uint16_t _packing;

    uint16_t varSelNum; // -2
} ObjHeader;

typedef struct Obj {
    union {
        uintptr_t vars[0];

        struct {
            ObjID      *props; // ? species
            struct Obj *super;
            uintptr_t   info;
        };
    };
} Obj;

// Format of classTbl resource
typedef struct ClassEntry {
    Obj     *obj;       // pointer to Obj
    uint16_t scriptNum; // script number
} ClassEntry;

#define OBJHEADER(obj) ((ObjHeader *)((uint8_t *)(obj) - sizeof(ObjHeader)))

#define OBJSIZE(varSelNum)                                                     \
    (sizeof(ObjHeader) + offsetof(Obj, vars[(varSelNum)]))

#define IndexedPropAddr(object, prop) ((object)->vars + g_objOfs[prop])
#define IndexedProp(object, prop)     (*IndexedPropAddr(object, prop))

extern ClassEntry *g_classTbl;
extern uint        g_numClasses;
extern uint        g_objOfs[];

// Load the offsets to indexed object properties from a file.
void LoadPropOffsets(void);

void LoadClassTbl(void);

Obj *GetClass(ObjID n);

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

// Send messages to the given object.
void SendMessage(Obj *obj);

// Send messages to the current object.
void Messager(Obj *obj);

// Return whether 'selector' is a property or method of 'obj' or its
// superclasses
bool RespondsTo(Obj *obj, uint selector);

// Gets the address of an object's property.
uintptr_t *GetPropAddr(Obj *obj, uint prop);

// Return the value of a property for an object.
uintptr_t GetProperty(Obj *obj, uint prop);

// Set the value of a property for an object.
void SetProperty(Obj *obj, uint prop, uintptr_t value);

const char *GetObjName(Obj *obj);

char *GetSelectorName(uint id, char *str);

int GetSelectorNum(char *str);

#pragma warning(pop)

#endif // OBJECT_H
