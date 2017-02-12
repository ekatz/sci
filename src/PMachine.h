#ifndef PMACHINE_H
#define PMACHINE_H

#include "Object.h"

#pragma warning(push)
#pragma warning(disable : 4201) // nameless struct/union

#define PE_BAD_DISPATCH       0
#define PE_BAD_OPCODE         1
#define PE_BAD_KERNAL         2
#define PE_LOAD_CLASS         3
#define PE_NOT_OBJECT         4
#define PE_BAD_SELECTOR       5
#define PE_CANT_FIXUP         6
#define PE_ZERO_DIVIDE        7
#define PE_STACK_BLOWN        8
#define PE_ZERO_MODULO        9
#define PE_LEFT_CLONE         10
#define PE_VER_STAMP_MISMATCH 11
#define PE_PACKHANDLE_HEAP    12
#define PE_PACKHANDLE_HUNK    13
#define PE_PACKHANDLE_FAILURE 14
#define PE_ODD_HEAP_RETURNED  15

extern bool g_gameStarted;
extern Obj *g_theGameObj;
extern uintptr_t g_globalVars[];

// Load the class table, allocate the p-machine stack.
void PMachine(void);

// Return a pointer to an entry in a script.
extern Obj *GetDispatchAddr(size_t scriptNum);

_Noreturn void PError(int perrCode, uintptr_t arg1, uintptr_t arg2);

#pragma warning(pop)

#endif // PMACHINE_H
