#include "sci/PMachine/PMachine.h"
#include "sci/Kernel/Selector.h"

bool g_gameStarted = false;
Obj *g_theGameObj  = NULL;

extern uintptr_t g_globalVars[];

void PMachine(void)
{
    ObjID startMethod;

    g_theGameObj = GetDispatchAddr(0, 0);

    if (!g_gameStarted) {
        g_gameStarted = true;
        startMethod   = s_play;
    } else {
        startMethod = s_replay;
    }

    InvokeMethod(g_theGameObj, startMethod, 0);
}

uintptr_t GetGlobalVariable(size_t index)
{
    return g_globalVars[index];
}

_Noreturn void PError(int perrCode, uintptr_t arg1, uintptr_t arg2)
{
    char        str[300];
    char        select[40];
    const char *object;

    switch (perrCode) {
        case PE_BAD_DISPATCH:
            sprintf(str, "Dispatch number too large: %d", (int)arg1);
            break;

        case PE_BAD_OPCODE:
            sprintf(str, "Bad opcode: $%x", (uint)arg1);
            break;

        case PE_BAD_KERNAL:
            sprintf(str, "Kernel entry # too large: %d", (int)arg1);
            break;

        case PE_LOAD_CLASS:
            sprintf(str, "Can't load class %d", (int)arg1);
            break;

        case PE_NOT_OBJECT:
            sprintf(str, "Not an object: $%x", (uint)arg1);
            break;

        case PE_ZERO_DIVIDE:
            sprintf(str, "Attempt to divide by zero.");
            break;

        case PE_BAD_SELECTOR:
            GetSelectorName((uint)arg2, select);
            object = GetObjName((Obj *)arg1);
            if (object == NULL) {
                object = "object";
            }
            sprintf(str, "'%s' is not a selector for %s.", select, object);
            break;

        case PE_STACK_BLOWN:
            strcpy(str, "Stack overflow.");
            break;

        case PE_ZERO_MODULO:
            strcpy(str, "Zero modulo.");
            break;

        case PE_LEFT_CLONE:
            sprintf(str, "Clone without script--> %d", (int)arg1);
            break;

        case PE_VER_STAMP_MISMATCH:
            sprintf(str,
                    "The interpreter and game version stamps are mismatched.");
            break;

        case PE_PACKHANDLE_HEAP:
            sprintf(str,
                    "PackHandle failure, duplicate table error at $%x in heap",
                    (uint)arg1);
            break;

        case PE_PACKHANDLE_HUNK:
            sprintf(
              str,
              "PackHandle failure, checksum error in loadlink at segment $%x",
              (uint)arg1);
            break;

        case PE_PACKHANDLE_FAILURE:
            sprintf(str,
                    "PackHandle failure, missing handle is for $%x segment",
                    (uint)arg1);
            break;

        case PE_ODD_HEAP_RETURNED:
            sprintf(str,
                    "Heap failure, attempt to return heap at odd address. "
                    "Address given is $%x",
                    (uint)arg1);
            break;

            //     case E_INVALID_PROPERTY:
            //         sprintf(str, "Invalid property %d", arg1);
            //         break;
    }
        //  errorWin = SizedWindow(str, "PMachine", TRUE);
#ifdef __WINDOWS__
    MessageBoxA(NULL, str, "PError", MB_OK | MB_ICONERROR);
#else
    MessageBox("PError", str);
#endif
    exit(1);
}
