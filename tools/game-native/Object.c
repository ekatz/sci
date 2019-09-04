#include "sci/Kernel/FarData.h"
#include "sci/Kernel/Resource.h"
#include "sci/Kernel/Selector.h"
#include <stdarg.h>

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

uintptr_t *GetPropAddr(Obj *obj, uint prop);

#define SCI_PMACHINE_OBJECT_H
#include "sci/PMachine/PMachine.h"

// Bits in the -info- property.
#define CLASSBIT  0x8000
#define CLONEBIT  0x0001
#define NODISPOSE 0x0002
#define NODISPLAY 0x0004 // Don't display in ShowObj()

uint g_objOfs[OBJOFSSIZE] = { 0 };

static void CheckObject(Obj *obj);
static bool LookupMethod(const Obj *obj, uintptr_t sel, PfnMethod *method);

uintptr_t *IndexedPropAddr(Obj *obj, size_t prop)
{
    return obj->props + g_objOfs[prop] - RES_PROP_INFO_OFFSET;
}

void LoadPropOffsets(void)
{
    int       i;
    uint16_t *op;

    op = (uint16_t *)ResLoad(RES_VOCAB, PROPOFS_VOCAB);

    // Read and store each offset.
    for (i = 0; i < OBJOFSSIZE; ++i) {
        g_objOfs[i] = *op++;
    }
}

bool IsObject(Obj *obj)
{
    // A pointer points to an object if it's non-NULL and aligned to pointer
    // size.
    return obj != NULL && ((uintptr_t)obj & (sizeof(uintptr_t) - 1)) == 0;
}

void SaveObjectState(Obj *obj, uintptr_t *buf)
{
    memcpy(buf,
           obj->props,
           sizeof(uintptr_t) * (size_t)obj->species->propSels->count);
}

void LoadObjectState(uintptr_t *buf, Obj *obj)
{
    memcpy(obj->props,
           buf,
           sizeof(uintptr_t) * (size_t)obj->species->propSels->count);
}

Obj *Clone(Obj *obj)
{
    size_t size;
    Obj   *newObj;

    // Is 'obj' an object?
    CheckObject(obj);

    // Get memory and copy into it.
    size            = (size_t)obj->species->propSels->count * sizeof(uintptr_t);
    newObj          = (Obj *)malloc(sizeof(Obj) + size);
    newObj->species = obj->species;
    memcpy(newObj->props, obj->props, size);

    // If we're copying a class, turn off its class bit.
    newObj->props[PROP_INFO_OFFSET] &= ~CLASSBIT;

    // Mark the object as cloned.
    newObj->props[PROP_INFO_OFFSET] |= CLONEBIT;
    return newObj;
}

void DisposeClone(Obj *obj)
{
    // Is 'obj' an object?
    CheckObject(obj);

    // Free the object's memory.
    if ((obj->props[PROP_INFO_OFFSET] & (CLONEBIT | NODISPOSE)) == CLONEBIT) {
        free(obj);
    }
}

uintptr_t CallMethod(Obj *obj, uintptr_t sel, uintptr_t args[])
{
    PfnMethod method;
    if (!LookupMethod(obj, sel, &method)) {
        PError(PE_BAD_SELECTOR, (uintptr_t)obj, sel);
    }
    return (*method)(obj, args);
}

uintptr_t InvokeMethod(Obj *obj, ObjID sel, uint argc, ...)
{
    va_list    argv;
    uintptr_t *args = (uintptr_t *)alloca((argc + 1) * sizeof(uintptr_t *));
    uint       i;

    va_start(argv, argc);
    args[0] = argc;
    for (i = 1; i <= argc; ++i) {
        args[i] = va_arg(argv, uint16_t);
    }
    va_end(argv);

    return CallMethod(obj, sel, args);
}

uintptr_t SendMessageEx(Obj *obj, uintptr_t sel, uintptr_t args[])
{
    uintptr_t *prop = GetPropAddr(obj, sel);
    if (prop != NULL) {
        if (args[0] != 0) {
            *prop = args[1];
        }
        return *prop;
    }
    return CallMethod(obj, sel, args);
}

bool RespondsTo(Obj *obj, uint selector)
{
    // Is 'obj' an object?
    CheckObject(obj);

    if (GetPropAddr(obj, selector) == NULL) {
        PfnMethod method;
        if (!LookupMethod(obj, selector, &method)) {
            return false;
        }
    }
    return true;
}

bool IsKindOf(const Obj *obj1, const Obj *obj2)
{
    const Species *s1, *s2 = obj2->species;
    for (s1 = obj1->species; s1 != NULL; s1 = s1->super) {
        if (s1 == s2) {
            return true;
        }
    }
    return false;
}

bool IsMemberOf(const Obj *obj1, const Obj *obj2)
{
    if ((obj2->props[PROP_INFO_OFFSET] & CLASSBIT) != 0 &&
        (obj1->props[PROP_INFO_OFFSET] & CLASSBIT) == 0) {
        return (obj1->species == obj2->species ||
                obj1->species->super == obj2->species);
    }
    return false;
}

uintptr_t *GetPropAddr(Obj *obj, uint prop)
{
    const Species *species  = obj->species;
    const SelList *propSels = species->propSels;
    unsigned int   i, n;
    assert(prop != 0);
#if 0
    if (prop == 0)
    {
        return (uintptr_t *)&obj->species;
    }
#endif
    for (i = 0, n = propSels->count; i < n; ++i) {
        if (propSels->sels[i] == (uint16_t)prop) {
            return &obj->props[i];
        }
    }
    return NULL;
}

uintptr_t GetProperty(Obj *obj, uint prop)
{
    if (!IsObject(obj))
        return 0;
    uintptr_t *var = GetPropAddr(obj, prop);
    if (var == NULL) {
        return 0;
    }
    return *var;
}

void SetProperty(Obj *obj, uint prop, uintptr_t value)
{
    if (!IsObject(obj))
        return;
    uintptr_t *var = GetPropAddr(obj, prop);
    if (var != NULL) {
        *var = value;
    }
}

const char *GetObjName(Obj *obj)
{
    return (const char *)GetProperty(obj, s_name);
}

char *GetSelectorName(uint id, char *str)
{
    if (GetFarStr(SELECTOR_VOCAB, id, str) == NULL) {
        sprintf(str, "%x", id);
    }
    return str;
}

int GetSelectorNum(char *str)
{
    int  i;
    char selector[60];

    // Keep trying new selector numbers until we get a NULL return
    // for the string address -- this indicates that we've exceeded
    // the maximum selector number.
    for (i = 0; GetFarStr(SELECTOR_VOCAB, (uint)i, selector) != NULL; ++i) {
        if (strcmp(str, selector) == 0) {
            return i;
        }
    }
    return -1;
}

static void CheckObject(Obj *obj)
{
    if (!IsObject(obj)) {
        PError(PE_NOT_OBJECT, (uintptr_t)obj, 0);
    }
}

static bool LookupMethod(const Obj *obj, uintptr_t sel, PfnMethod *method)
{
    const Species *species    = obj->species;
    const SelList *methodSels = species->methodSels;
    unsigned int   i, n;
    for (i = 0, n = methodSels->count; i < n; ++i) {
        if (methodSels->sels[i] == (uint16_t)sel) {
            *method = species->methods[i];
            return true;
        }
    }
    return false;
}
