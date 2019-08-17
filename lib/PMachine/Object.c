#include "sci/PMachine/Object.h"
#include "sci/PMachine/PMachine.h"
#include "sci/Kernel/FarData.h"
#include "sci/Kernel/Resource.h"
#include "sci/Kernel/Selector.h"
#include "sci/Logger/Log.h"
#include <stdarg.h>

#define MINOBJECTADDR 0x2000

// Bits in the -info- property.
#define CLASSBIT  0x8000
#define CLONEBIT  0x0001
#define NODISPOSE 0x0002
#define NODISPLAY 0x0004 // Don't display in ShowObj()

typedef struct ResClassEntry {
    uint16_t obj;       // pointer to Obj
    uint16_t scriptNum; // script number
} ResClassEntry;

ClassEntry *g_classTbl           = NULL;
uint        g_numClasses         = 0;
uint        g_objOfs[OBJOFSSIZE] = { 0 };

static void CheckObject(Obj *obj);
static int  FindSelector(ObjID *list, uint n, ObjID sel);
static void QuickMessage(Obj *obj, uint argc);

uintptr_t *IndexedPropAddr(Obj *obj, size_t prop)
{
    return obj->vars + g_objOfs[prop];
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

void LoadClassTbl(void)
{
    ResClassEntry *res;
    uint           i, n;

    res          = (ResClassEntry *)ResLoad(RES_VOCAB, CLASSTBL_VOCAB);
    g_numClasses = ResHandleSize(res) / sizeof(ResClassEntry);

    g_classTbl = (ClassEntry *)malloc(g_numClasses * sizeof(ClassEntry));
    for (i = 0, n = g_numClasses; i < n; ++i) {
        g_classTbl[i].obj       = NULL;
        g_classTbl[i].scriptNum = res[i].scriptNum;
    }
}

Obj *GetClass(ObjID n)
{
    Obj *obj;

    if (n == (ObjID)-1) {
        return NULL;
    }

    obj = g_classTbl[n].obj;
    if (obj == NULL) {
        LoadScript(g_classTbl[n].scriptNum);
        obj = g_classTbl[n].obj;
    }
    return obj;
}

bool IsObject(Obj *obj)
{
    // A pointer points to an object if it's non-NULL, not odd, and its magic
    // field is OBJID.
    return obj != NULL && ((uintptr_t)obj & 1) == 0 &&
           OBJHEADER(obj)->magic == OBJID;
}

void SaveObjectState(Obj *obj, uintptr_t *buf)
{
    memcpy(buf, obj->vars, sizeof(uintptr_t) * OBJHEADER(obj)->varSelNum);
}

void LoadObjectState(uintptr_t *buf, Obj *obj)
{
    memcpy(obj->vars, buf, sizeof(uintptr_t) * OBJHEADER(obj)->varSelNum);
}

Obj *Clone(Obj *obj)
{
    uint       size;
    ObjHeader *newObjHeader;
    Obj       *newObj;
    Script    *sp;

    // Is 'obj' an object?
    CheckObject(obj);

    // Get memory and copy into it.
    size         = OBJSIZE(OBJHEADER(obj)->varSelNum);
    newObjHeader = (ObjHeader *)malloc(size);
    memcpy(newObjHeader, OBJHEADER(obj), size);

    newObj = (Obj *)(newObjHeader + 1);

    // If we're copying a class, set the new object's super to the class and
    // turn off its class bit.
    if ((newObj->info & CLASSBIT) != 0) {
        newObj->super = obj;
        newObj->info &= ~CLASSBIT;
    }

    // Mark the object as cloned.
    newObj->info |= CLONEBIT;

    // Increment script's reference count.
    sp = OBJHEADER(newObj)->script;
    sp->clones++;

    return newObj;
}

void DisposeClone(Obj *obj)
{
    // Is 'obj' an object?
    CheckObject(obj);

    // Clear the object ID, decrement the script's reference count,
    // and free the object's memory.
    if ((obj->info & (CLONEBIT | NODISPOSE)) == CLONEBIT) {
        Script *sp            = OBJHEADER(obj)->script;
        OBJHEADER(obj)->magic = 0;
        sp->clones--;
        free(OBJHEADER(obj));
    }
}

void SendMessage(Obj *obj)
{
    Obj *prevObj = g_object;
    g_object     = obj;
    Messager(obj);
    g_object = prevObj;
}

void Messager(Obj *obj)
{
    uint argc = GetByte();
    argc /= sizeof(uint16_t);
    argc += g_restArgsCount;
    QuickMessage(obj, argc);
}

uintptr_t InvokeMethod(Obj *obj, ObjID sel, uint argc, ...)
{
    va_list    args;
    Obj       *prevObj;
    uintptr_t *prevBase;
    uint       i;

    va_start(args, argc);

    // Set this as the current object.
    prevObj  = g_object;
    g_object = obj;

    prevBase = g_bp;
    g_bp     = g_sp;

    Push(sel);
    Push(argc);
    for (i = 0; i < argc; ++i) {
        Push(va_arg(args, uint16_t));
    }
    va_end(args);

    QuickMessage(obj, argc + 2);

    g_sp     = g_bp;
    g_bp     = prevBase;
    g_object = prevObj;
    return g_acc;
}

bool RespondsTo(Obj *obj, uint selector)
{
    ObjID *funcs;

    // Is 'obj' an object?
    CheckObject(obj);

    // Is 'selector' a property?
    if (GetPropAddr(obj, selector)) {
        return true;
    }

    // Search the method dictionary hierarchy.
    do {
        funcs = OBJHEADER(obj)->funcSelList;
        if (FindSelector(funcs, funcs[-1], (ObjID)selector) >= 0) {
            return true;
        }

        obj = obj->super;
    } while (obj != NULL);

    return false;
}

uintptr_t *GetPropAddr(Obj *obj, uint prop)
{
    int idx;

#if 0
    if (obj < MINOBJECTADDR)
    {
        return NULL;
    }
#endif

    // Not an object -- odd address.
    if ((((uintptr_t)obj) & 1) != 0) {
        return NULL;
    }

    idx = FindSelector(obj->props, OBJHEADER(obj)->varSelNum, (ObjID)prop);
    if (idx < 0) {
        return NULL;
    }

    return &obj->vars[idx];
}

uintptr_t GetProperty(Obj *obj, uint prop)
{
    uintptr_t *var = GetPropAddr(obj, prop);
    if (var == NULL) {
        return 0;
    }
    return *var;
}

void SetProperty(Obj *obj, uint prop, uintptr_t value)
{
    uintptr_t *var = GetPropAddr(obj, prop);
    if (var != NULL) {
        *var = value;
    }
}

const char *GetObjName(Obj *obj)
{
    uintptr_t *name = (uintptr_t *)GetPropAddr(obj, s_name);
    return (name != 0) ? (const char *)GetScriptHeapPtr(*name) : NULL;
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

static void QuickMessage(Obj *obj, uint argc)
{
    uint       prevScriptNum;
    Handle     prevScriptHandle;
    Obj       *prevSuper;
    Obj       *origObj;
    ObjID     *funcs;
    uintptr_t *prevLocalVar;
    uintptr_t *prevParmVar;
    uintptr_t *frame;
    uintptr_t *parm;
    uintptr_t *localVar;
    uintptr_t *prevTempVar;
    uint8_t   *retAddr;
    uint16_t  *funcOffsets;
    uint       selector;
    uint       frameSize;
    int        idx;

    prevScriptNum    = g_thisScript;
    prevScriptHandle = g_scriptHandle;

    CheckObject(obj);

    retAddr = g_pc;

    prevSuper = g_super;
    g_super   = obj->super;

    prevLocalVar = g_vars.local;
    g_vars.local = OBJHEADER(obj)->script->vars;

    prevParmVar = g_vars.parm;

    // Pointer to top of parameters.
    frame = g_bp - argc;

    parm = frame + 1;

    // Check for completion of the send (no more messages).
    while (argc != 0) {
        // Get the message selector and move the parameter pointer past it.
        selector = (uint)*parm++;

        // Get the number of bytes of arguments for this message and
        // adjust the number of bytes of parameters remaining accordingly.
        frameSize = (uint)*parm; // Number of parameters
        frameSize += g_restArgsCount;
        argc -= frameSize; // Update byte count of parameters on stack
        argc -= 2;         // Account for selector and number of parms

        // Set up the new parameter variables and then point past the parameters
        // to this send.
        g_vars.parm = parm;
        *parm++     = frameSize;
        parm += frameSize;

        idx =
          FindSelector(obj->props, OBJHEADER(obj)->varSelNum, (ObjID)selector);
        if (idx >= 0) {
            // A query -- load value into accumulator.
            if (frameSize == 0) {
                SetAcc(obj->vars[idx]);
            }
            // Not a query -- set the property.
            else {
                obj->vars[idx] = g_vars.parm[1];
            }
            g_restArgsCount = 0;
        }
        // Not a property.
        // Search up through the method dictionary hierarchy.
        else {
            origObj  = obj;
            localVar = g_vars.local;

            while (true) {
                CheckObject(obj);

                // Update current script number.
                g_thisScript = OBJHEADER(obj)->script->num;

                // Update current code handle.
                g_scriptHandle = OBJHEADER(obj)->script->hunk;

                // Get offset in hunk resource of method dictionary.
                funcs = OBJHEADER(obj)->funcSelList;
                idx   = FindSelector(funcs, funcs[-1], (ObjID)selector);
                if (idx >= 0) {
                    break;
                }

                // The selector is not a method for this class/object.
                // Search its superclass.
                obj = obj->super;
                if (obj == NULL) {
                    PError(PE_BAD_SELECTOR,
                           (uintptr_t)g_object,
                           (uintptr_t)selector);
                }

                // Get pointer to variables in class' scope.
                g_vars.local = OBJHEADER(obj)->script->vars;
            }

            // Found the selector -- next list is method offsets.
            funcOffsets = (uint16_t *)(OBJHEADER(obj)->funcSelList +
                                       OBJHEADER(obj)->funcSelList[-1]);
            // Skip the zero (barrier).
            funcOffsets++;

            prevTempVar     = g_vars.temp;
            g_restArgsCount = 0;
            g_pc            = (uint8_t *)g_scriptHandle + funcOffsets[idx];

            DebugFunctionEntry(obj, selector);
            ExecuteCode();
            DebugFunctionExit();

            g_vars.temp  = prevTempVar;
            g_vars.local = localVar;
            obj          = origObj;
        }
    }

    g_bp           = frame;
    g_vars.parm    = prevParmVar;
    g_vars.local   = prevLocalVar;
    g_pc           = retAddr;
    g_super        = prevSuper;
    g_scriptHandle = prevScriptHandle;
    g_thisScript   = prevScriptNum;
}

static int FindSelector(ObjID *list, uint n, ObjID sel)
{
    uint i;
    for (i = 0; i < n; ++i) {
        if (list[i] == sel) {
            return i;
        }
    }
    return -1;
}
