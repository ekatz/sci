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

typedef union PVars {
    uintptr_t *all[4];
    struct {
        uintptr_t *global;
        uintptr_t *local;
        uintptr_t *temp;
        uintptr_t *parm;
    };
} PVars;

extern bool g_gameStarted;
extern Obj *g_theGameObj;
extern Obj *g_object;
extern Obj *g_super;

extern uint g_restArgsCount;

extern uintptr_t g_prevAcc;
extern uintptr_t g_acc; // accumulator
extern uintptr_t *g_sp; // The current stack pointer.
                        // Note that the stack in the original SCI interpreter
                        // is used bottom-up instead of the more usual top-down.
extern uintptr_t *g_bp;
extern uint8_t   *g_pc; // The Program Counter (instruction pointer). Points to
                        // the currently executing instruction.
extern uint8_t *g_thisIP;

extern uintptr_t *g_pStack;
extern uintptr_t *g_pStackEnd;

extern uint   g_thisScript;
extern Handle g_scriptHandle;

extern PVars g_vars;

// uintptr_t *GetIndexedPropPtr(uint idx)
//
// Get a pointer to a property in the current object which is specified as
// an index into the property storage of the object.
#define GetIndexedPropPtr(idx) &g_object->vars[(idx) / sizeof(uint16_t)]

// uintptr_t Peek()
//
// Read the value on the top of the pmachine stack.
#define Peek() (*g_bp)

// uintptr_t Pop()
//
// Pop the top of the pmachine stack into a register.
#define Pop() (*g_bp--)

// uintptr_t Push(uintptr_t val)
//
// Push a register on the pmachine stack.
#define Push(val) (*(++g_bp) = (uintptr_t)(val))

// uint8_t GetByte()
#define GetByte() (*g_pc++)

// int8_t GetSByte()
#define GetSByte() ((int8_t)*g_pc++)

// uint16_t GetWord()
#define GetWord() (*((uint16_t *)(g_pc += 2) - 1))

// int16_t GetSWord()
#define GetSWord() (*((int16_t *)(g_pc += 2) - 1))

// void SetAcc(uintptr_t acc)
#define SetAcc(acc)                                                            \
    g_prevAcc = g_acc;                                                         \
    g_acc     = acc

// Load the class table, allocate the p-machine stack.
void PMachine(void);

void ExecuteCode(void);

// Return a pointer to an entry in a script.
Obj *GetDispatchAddr(uint scriptNum, uint entryNum);

_Noreturn void PError(int perrCode, uintptr_t arg1, uintptr_t arg2);

#pragma warning(pop)

#endif // PMACHINE_H
