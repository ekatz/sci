#include "sci/PMachine/PMachine.h"
#include "sci/PMachine/Object.h"
#include "sci/Driver/Input/Input.h"
#include "sci/Kernel/Audio.h"
#include "sci/Kernel/Graphics.h"
#include "sci/Kernel/Kernel.h"
#include "sci/Kernel/Menu.h"
#include "sci/Kernel/Motion.h"
#include "sci/Kernel/Palette.h"
#include "sci/Kernel/Resource.h"
#include "sci/Kernel/Restart.h"
#include "sci/Kernel/SaveGame.h"
#include "sci/Kernel/Selector.h"
#include "sci/Kernel/Sound.h"
#include "sci/Kernel/Sync.h"

#define PSTACKSIZE (10 * 1024)

#define OP_LDST   0x80 // load/store if set
#define OP_BYTE   0x01 // byte operation if set, word otw

#define OP_TYPE   0x60 // mask for operation type
#define OP_LOAD   0x00 // load
#define OP_STORE  0x20 // store
#define OP_INC    0x40 // increment operation
#define OP_DEC    0x60 // decrement operation

#define OP_INDEX  0x10 // indexed op if set, non-indexed otw

#define OP_STACK  0x08 // load to stack if set

#define OP_VAR    0x06 // mask for var type
#define OP_GLOBAL 0x00 // global var
#define OP_LOCAL  0x02 // local var
#define OP_TMP    0x04 // temporary var (on the stack)
#define OP_PARM   0x06 // parameter (different stack frame than tmp)

#define OP_bnot        0x00
//      BadOp          0x01
#define OP_add         0x02
//      BadOp          0x03
#define OP_sub         0x04
//      BadOp          0x05
#define OP_mul         0x06
//      BadOp          0x07
#define OP_div         0x08
//      BadOp          0x09
#define OP_mod         0x0A
//      BadOp          0x0B
#define OP_shr         0x0C
//      BadOp          0x0D
#define OP_shl         0x0E
//      BadOp          0x0F
#define OP_xor         0x10
//      BadOp          0x11
#define OP_and         0x12
//      BadOp          0x13
#define OP_or          0x14
//      BadOp          0x15
#define OP_neg         0x16
//      BadOp          0x17
#define OP_not         0x18
//      BadOp          0x19
#define OP_eq          0x1A
//      BadOp          0x1B
#define OP_ne          0x1C
//      BadOp          0x1D
#define OP_gt          0x1E
//      BadOp          0x1F
#define OP_ge          0x20
//      BadOp          0x21
#define OP_lt          0x22
//      BadOp          0x23
#define OP_le          0x24
//      BadOp          0x25
#define OP_ugt         0x26
//      BadOp          0x27
#define OP_uge         0x28
//      BadOp          0x29
#define OP_ult         0x2A
//      BadOp          0x2B
#define OP_ule         0x2C
//      BadOp          0x2D
#define OP_bt_TWO      0x2E
#define OP_bt_ONE      0x2F
#define OP_bnt_TWO     0x30
#define OP_bnt_ONE     0x31
#define OP_jmp_TWO     0x32
#define OP_jmp_ONE     0x33
#define OP_loadi_TWO   0x34
#define OP_loadi_ONE   0x35
#define OP_push        0x36
//      BadOp          0x37
#define OP_pushi_TWO   0x38
#define OP_pushi_ONE   0x39
#define OP_toss        0x3A
//      BadOp          0x3B
#define OP_dup         0x3C
//      BadOp          0x3D
#define OP_link_TWO    0x3E
#define OP_link_ONE    0x3F
#define OP_call_THREE  0x40
#define OP_call_TWO    0x41
#define OP_callk_THREE 0x42
#define OP_callk_TWO   0x43
#define OP_callb_THREE 0x44
#define OP_callb_TWO   0x45
#define OP_calle_FOUR  0x46
#define OP_calle_TWO   0x47
#define OP_ret         0x48
//      BadOp          0x49
#define OP_send_ONE    0x4A
//      BadOp          0x4B
//      BadOp          0x4C
//      BadOp          0x4D
//      BadOp          0x4E
//      BadOp          0x4F
#define OP_class_TWO   0x50
#define OP_class_ONE   0x51
//      BadOp          0x52
//      BadOp          0x53
#define OP_self_TWO    0x54
#define OP_self_ONE    0x55
#define OP_super_THREE 0x56
#define OP_super_TWO   0x57
//      BadOp          0x58
#define OP_rest_ONE    0x59
#define OP_lea_FOUR    0x5A
#define OP_lea_TWO     0x5B
#define OP_selfID      0x5C
//      BadOp          0x5D
//      BadOp          0x5E
//      BadOp          0x5F
#define OP_pprev       0x60
//      BadOp          0x61
#define OP_pToa_TWO    0x62
#define OP_pToa_ONE    0x63
#define OP_aTop_TWO    0x64
#define OP_aTop_ONE    0x65
#define OP_pTos_TWO    0x66
#define OP_pTos_ONE    0x67
#define OP_sTop_TWO    0x68
#define OP_sTop_ONE    0x69
#define OP_ipToa_TWO   0x6A
#define OP_ipToa_ONE   0x6B
#define OP_dpToa_TWO   0x6C
#define OP_dpToa_ONE   0x6D
#define OP_ipTos_TWO   0x6E
#define OP_ipTos_ONE   0x6F
#define OP_dpTos_TWO   0x70
#define OP_dpTos_ONE   0x71
#define OP_lofsa_TWO   0x72
//      BadOp          0x73
#define OP_lofss_TWO   0x74
//      BadOp          0x75
#define OP_push0       0x76
//      BadOp          0x77
#define OP_push1       0x78
//      BadOp          0x79
#define OP_push2       0x7A
//      BadOp          0x7B
#define OP_pushSelf    0x7C
//      BadOp          0x7B
//      BadOp          0x7B
//      BadOp          0x7F
#define OP_lag_TWO     0x80
#define OP_lag_ONE     0x81
#define OP_lal_TWO     0x82
#define OP_lal_ONE     0x83
#define OP_lat_TWO     0x84
#define OP_lat_ONE     0x85
#define OP_lap_TWO     0x86
#define OP_lap_ONE     0x87
#define OP_lsg_TWO     0x88
#define OP_lsg_ONE     0x89
#define OP_lsl_TWO     0x8A
#define OP_lsl_ONE     0x8B
#define OP_lst_TWO     0x8C
#define OP_lst_ONE     0x8D
#define OP_lsp_TWO     0x8E
#define OP_lsp_ONE     0x8F
#define OP_lagi_TWO    0x90
#define OP_lagi_ONE    0x91
#define OP_lali_TWO    0x92
#define OP_lali_ONE    0x93
#define OP_lati_TWO    0x94
#define OP_lati_ONE    0x95
#define OP_lapi_TWO    0x96
#define OP_lapi_ONE    0x97
#define OP_lsgi_TWO    0x98
#define OP_lsgi_ONE    0x99
#define OP_lsli_TWO    0x9A
#define OP_lsli_ONE    0x9B
#define OP_lsti_TWO    0x9C
#define OP_lsti_ONE    0x9D
#define OP_lspi_TWO    0x9E
#define OP_lspi_ONE    0x9F
#define OP_sag_TWO     0xA0
#define OP_sag_ONE     0xA1
#define OP_sal_TWO     0xA2
#define OP_sal_ONE     0xA3
#define OP_sat_TWO     0xA4
#define OP_sat_ONE     0xA5
#define OP_sap_TWO     0xA6
#define OP_sap_ONE     0xA7
#define OP_ssg_TWO     0xA8
#define OP_ssg_ONE     0xA9
#define OP_ssl_TWO     0xAA
#define OP_ssl_ONE     0xAB
#define OP_sst_TWO     0xAC
#define OP_sst_ONE     0xAD
#define OP_ssp_TWO     0xAE
#define OP_ssp_ONE     0xAF
#define OP_sagi_TWO    0xB0
#define OP_sagi_ONE    0xB1
#define OP_sali_TWO    0xB2
#define OP_sali_ONE    0xB3
#define OP_sati_TWO    0xB4
#define OP_sati_ONE    0xB5
#define OP_sapi_TWO    0xB6
#define OP_sapi_ONE    0xB7
#define OP_ssgi_TWO    0xB8
#define OP_ssgi_ONE    0xB9
#define OP_ssli_TWO    0xBA
#define OP_ssli_ONE    0xBB
#define OP_ssti_TWO    0xBC
#define OP_ssti_ONE    0xBD
#define OP_sspi_TWO    0xBE
#define OP_sspi_ONE    0xBF
#define OP_iag_TWO     0xC0
#define OP_iag_ONE     0xC1
#define OP_ial_TWO     0xC2
#define OP_ial_ONE     0xC3
#define OP_iat_TWO     0xC4
#define OP_iat_ONE     0xC5
#define OP_iap_TWO     0xC6
#define OP_iap_ONE     0xC7
#define OP_isg_TWO     0xC8
#define OP_isg_ONE     0xC9
#define OP_isl_TWO     0xCA
#define OP_isl_ONE     0xCB
#define OP_ist_TWO     0xCC
#define OP_ist_ONE     0xCD
#define OP_isp_TWO     0xCE
#define OP_isp_ONE     0xCF
#define OP_iagi_TWO    0xD0
#define OP_iagi_ONE    0xD1
#define OP_iali_TWO    0xD2
#define OP_iali_ONE    0xD3
#define OP_iati_TWO    0xD4
#define OP_iati_ONE    0xD5
#define OP_iapi_TWO    0xD6
#define OP_iapi_ONE    0xD7
#define OP_isgi_TWO    0xD8
#define OP_isgi_ONE    0xD9
#define OP_isli_TWO    0xDA
#define OP_isli_ONE    0xDB
#define OP_isti_TWO    0xDC
#define OP_isti_ONE    0xDD
#define OP_ispi_TWO    0xDE
#define OP_ispi_ONE    0xDF
#define OP_dag_TWO     0xE0
#define OP_dag_ONE     0xE1
#define OP_dal_TWO     0xE2
#define OP_dal_ONE     0xE3
#define OP_dat_TWO     0xE4
#define OP_dat_ONE     0xE5
#define OP_dap_TWO     0xE6
#define OP_dap_ONE     0xE7
#define OP_dsg_TWO     0xE8
#define OP_dsg_ONE     0xE9
#define OP_dsl_TWO     0xEA
#define OP_dsl_ONE     0xEB
#define OP_dst_TWO     0xEC
#define OP_dst_ONE     0xED
#define OP_dsp_TWO     0xEE
#define OP_dsp_ONE     0xEF
#define OP_dagi_TWO    0xF0
#define OP_dagi_ONE    0xF1
#define OP_dali_TWO    0xF2
#define OP_dali_ONE    0xF3
#define OP_dati_TWO    0xF4
#define OP_dati_ONE    0xF5
#define OP_dapi_TWO    0xF6
#define OP_dapi_ONE    0xF7
#define OP_dsgi_TWO    0xF8
#define OP_dsgi_ONE    0xF9
#define OP_dsli_TWO    0xFA
#define OP_dsli_ONE    0xFB
#define OP_dsti_TWO    0xFC
#define OP_dsti_ONE    0xFD
#define OP_dspi_TWO    0xFE
#define OP_dspi_ONE    0xFF

#define GetIndexByte() (g_acc + (uintptr_t)GetByte())
#define GetIndexWord() (g_acc + (uintptr_t)GetWord())

kFunc s_kernelDispTbl[] = { KLoad,
                            KUnLoad,
                            KScriptID,
                            KDisposeScript,
                            KClone,
                            KDisposeClone,
                            KIsObject,
                            KRespondsTo,
                            KDrawPic,
                            KShow,
                            KPicNotValid,
                            KAnimate,
                            KSetNowSeen,
                            KNumLoops,
                            KNumCels,
                            KCelWide,
                            KCelHigh,
                            KDrawCel,
                            KAddToPic,
                            KNewWindow,
                            KGetPort,
                            KSetPort,
                            KDisposeWindow,
                            KDrawControl,
                            KHiliteControl,
                            KEditControl,
                            KTextSize,
                            KDisplay,
                            KGetEvent,
                            KGlobalToLocal,
                            KLocalToGlobal,
                            KMapKeyToDir,
                            KDrawMenuBar,
                            KMenuSelect,
                            KAddMenu,
                            KDrawStatus,
                            KParse,
                            KSaid,
                            KSetSynonyms,
                            KHaveMouse,
                            KSetCursor,
                            KSaveGame,
                            KRestoreGame,
                            KRestartGame,
                            KGameIsRestarting,
                            KDoSound,
                            KNewList,
                            KDisposeList,
                            KNewNode,
                            KFirstNode,
                            KLastNode,
                            KEmptyList,
                            KNextNode,
                            KPrevNode,
                            KNodeValue,
                            KAddAfter,
                            KAddToFront,
                            KAddToEnd,
                            KFindKey,
                            KDeleteKey,
                            KRandom,
                            KAbs,
                            KSqrt,
                            KGetAngle,
                            KGetDistance,
                            KWait,
                            KGetTime,
                            KStrEnd,
                            KStrCat,
                            KStrCmp,
                            KStrLen,
                            KStrCpy,
                            KFormat,
                            KGetFarText,
                            KReadNumber,
                            KBaseSetter,
                            KDirLoop,
                            KCantBeHere,
                            KOnControl,
                            KInitBresen,
                            KDoBresen,
                            KDoAvoider,
                            KSetJump,
                            KSetDebug,
                            KInspectObj,
                            KShowSends,
                            KShowObjs,
                            KShowFree,
                            KMemoryInfo,
                            KStackUsage,
                            KProfiler,
                            KGetMenu,
                            KSetMenu,
                            KGetSaveFiles,
                            KGetCWD,
                            KCheckFreeSpace,
                            KValidPath,
                            KCoordPri,
                            KStrAt,
                            KDeviceInfo,
                            KGetSaveDir,
                            KCheckSaveGame,
                            KShakeScreen,
                            KFlushResources,
                            KSinMult,
                            KCosMult,
                            KSinDiv,
                            KCosDiv,
                            KGraph,
                            KJoystick,
                            KShiftScreen,
                            KPalette,
                            KMemorySegment,
                            KIntersections,
                            KMemory,
                            KListOps,
                            KFileIO,
                            KDoAudio,
                            KDoSync,
                            KAvoidPath,
                            KSort,
                            KATan,
                            KLock,
                            KStrSplit,
                            KMessage,
                            KIsItSkip };

#define KERNELMAX ARRAYSIZE(s_kernelDispTbl)

bool g_gameStarted = false;
Obj *g_theGameObj  = NULL;
Obj *g_object      = NULL;
Obj *g_super       = NULL;

uint g_restArgsCount = 0;

uintptr_t  g_prevAcc = 0;
uintptr_t  g_acc     = 0;
uintptr_t *g_sp      = NULL;
uintptr_t *g_bp      = NULL;
uint8_t   *g_pc      = NULL;
uint8_t   *g_thisIP  = NULL;

uintptr_t *g_pStack    = NULL;
uintptr_t *g_pStackEnd = NULL;

uint   g_thisScript;
Handle g_scriptHandle = NULL;

PVars g_vars = { 0 };

static void KernelCall(uint kernelNum);
static void DoCall(uint parmCount);
static void Dispatch(uint scriptNum, uint entryNum, uint parmCount);
static void LoadEffectiveAddress(uint varType, uint varNum);
static Script *GetDispatchAddrInHeap(uint scriptNum, uint entryNum, Obj **obj);
static Script *GetDispatchAddrInHunk(uint      scriptNum,
                                     uint      entryNum,
                                     uint8_t **code);
static char *GetKernelName(uint num, char *buffer);

void PMachine(void)
{
    Script *script;
    ObjID   startMethod;

    g_theGameObj = NULL;
    if (!g_gameStarted) {
        LoadClassTbl();
        g_restArgsCount = 0;
        void *stack     = malloc(PSTACKSIZE);
        memset(stack, 'S', PSTACKSIZE);
        g_pStack    = (uintptr_t *)stack;
        g_pStackEnd = (uintptr_t *)((byte *)stack + PSTACKSIZE);
    }

    g_scriptHandle = NULL;
    script         = GetDispatchAddrInHeap(0, 0, &g_object);
    g_theGameObj   = g_object;
    g_scriptHandle = script->hunk;
    g_vars.global  = script->vars;

    g_sp = g_pStack;

    if (!g_gameStarted) {
        g_gameStarted = true;
        startMethod   = s_play;
    } else {
        startMethod = s_replay;
    }

    InvokeMethod(g_object, startMethod, 0, 0);
}

Obj *GetDispatchAddr(uint scriptNum, uint entryNum)
{
    Obj *obj = NULL;
    GetDispatchAddrInHeap(scriptNum, entryNum, &obj);
    return obj;
}

void ExecuteCode(void)
{
    while (true) {
        switch (GetByte()) {
            // Do a bitwise not of the acc.
            case OP_bnot: {
                LogDebug("bnot");
                SetAcc(~g_acc);
            } break;

            // Add the top value of the stack to the acc.
            case OP_add: {
                LogDebug("add");
                SetAcc(Pop() + g_acc);
            } break;

            // Subtract the acc from the top value on the stack.
            case OP_sub: {
                LogDebug("sub");
                SetAcc(Pop() - g_acc);
            } break;

            // Multiply the acc and the top value on the stack.
            case OP_mul: {
                LogDebug("mul");
                SetAcc(Pop() * g_acc);
            } break;

            // Divide the top value on the stack by the acc.
            case OP_div: {
                LogDebug("div");
                if (g_acc == 0) {
                    PError(PE_ZERO_MODULO, 0, 0);
                }
                SetAcc(Pop() / g_acc);
            } break;

            // Put S (mod acc) in the acc.
            case OP_mod: {
                LogDebug("mod");
                if (g_acc == 0) {
                    PError(PE_ZERO_MODULO, 0, 0);
                }
                SetAcc(Pop() % g_acc);
            } break;

            // Shift the value on the stack right by the amount in the acc.
            case OP_shr: {
                LogDebug("shr");
                SetAcc(Pop() >> g_acc);
            } break;

            // Shift the value on the stack left by the amount in the acc.
            case OP_shl: {
                LogDebug("shl");
                SetAcc(Pop() << g_acc);
            } break;

            // Xor the value on the stack with that in the acc.
            case OP_xor: {
                LogDebug("xor");
                SetAcc(Pop() ^ g_acc);
            } break;

            // And the value on the stack with that in the acc.
            case OP_and: {
                LogDebug("and");
                SetAcc(Pop() & g_acc);
            } break;

            // Or the value on the stack with that in the acc.
            case OP_or: {
                LogDebug("or");
                SetAcc(Pop() | g_acc);
            } break;

            // Negate the value in the acc.
            case OP_neg: {
                LogDebug("neg");
                SetAcc((uintptr_t)(-(intptr_t)g_acc));
            } break;

            // Do a logical not on the value in the acc.
            case OP_not: {
                LogDebug("not");
                SetAcc((uintptr_t)!g_acc);
            } break;

            // Test for equality.
            case OP_eq: {
                SetAcc((uintptr_t)(Pop() == g_acc));
                LogDebug("eq? (%u)", g_acc);
            } break;

            // Test for inequality.
            case OP_ne: {
                SetAcc((uintptr_t)(Pop() != g_acc));
                LogDebug("ne? (%u)", g_acc);
            } break;

            // Is the stack value > acc?   (Signed)
            case OP_gt: {
                SetAcc((uintptr_t)((intptr_t)Pop() > (intptr_t)g_acc));
                LogDebug("gt? (%u)", g_acc);
            } break;

            // Is the stack value >= acc?   (Signed)
            case OP_ge: {
                SetAcc((uintptr_t)((intptr_t)Pop() >= (intptr_t)g_acc));
                LogDebug("ge? (%u)", g_acc);
            } break;

            // Is the stack value < acc?   (Signed)
            case OP_lt: {
                SetAcc((uintptr_t)((intptr_t)Pop() < (intptr_t)g_acc));
                LogDebug("lt? (%u)", g_acc);
            } break;

            // Is the stack value <= acc?   (Signed)
            case OP_le: {
                SetAcc((uintptr_t)((intptr_t)Pop() <= (intptr_t)g_acc));
                LogDebug("le? (%u)", g_acc);
            } break;

            // Is the stack value > acc?   (Unsigned)
            case OP_ugt: {
                SetAcc((uintptr_t)(Pop() > g_acc));
                LogDebug("ugt? (%u)", g_acc);
            } break;

            // Is the stack value >= acc?   (Unsigned)
            case OP_uge: {
                SetAcc((uintptr_t)(Pop() >= g_acc));
                LogDebug("uge? (%u)", g_acc);
            } break;

            // Is the stack value < acc?   (Unsigned)
            case OP_ult: {
                SetAcc((uintptr_t)(Pop() < g_acc));
                LogDebug("ult? (%u)", g_acc);
            } break;

            // Is the stack value <= acc?   (Unsigned)
            case OP_ule: {
                SetAcc((uintptr_t)(Pop() <= g_acc));
                LogDebug("ule? (%u)", g_acc);
            } break;

            // Add the following byte to the current scan pointer if acc is
            // true.
            case OP_bt_ONE: {
                LogDebug("bt %+d", *((int8_t *)g_pc));
                if (g_acc != 0) {
                    g_pc += GetSByte();
                } else {
                    // Skip
                    g_pc += 1;
                }
            } break;

            // Add the following word to the current scan pointer if acc is
            // true.
            case OP_bt_TWO: {
                LogDebug("bt %+d", *((int16_t *)g_pc));
                if (g_acc != 0) {
                    g_pc += GetSWord();
                } else {
                    // Skip
                    g_pc += 2;
                }
            } break;

            // Add the following byte to the current scan pointer if acc is
            // false.
            case OP_bnt_ONE: {
                LogDebug("bnt %+d", *((int8_t *)g_pc));
                if (g_acc == 0) {
                    g_pc += GetSByte();
                } else {
                    // Skip
                    g_pc += 1;
                }
            } break;

            // Add the following word to the current scan pointer if acc is
            // false.
            case OP_bnt_TWO: {
                LogDebug("bnt %+d", *((int16_t *)g_pc));
                if (g_acc == 0) {
                    g_pc += GetSWord();
                } else {
                    // Skip
                    g_pc += 2;
                }
            } break;

            // Unconditional branch.
            case OP_jmp_ONE: {
                LogDebug("jmp %+d", *((int8_t *)g_pc));
                g_pc += GetSByte();
            } break;

            // Unconditional branch.
            case OP_jmp_TWO: {
                LogDebug("jmp %+d", *((int16_t *)g_pc));
                g_pc += GetSWord();
            } break;

            // Load an immediate value into acc.
            case OP_loadi_ONE: {
                LogDebug("loadi %d", *((int8_t *)g_pc));
                SetAcc(GetSByte());
            } break;

            // Load an immediate value into acc.
            case OP_loadi_TWO: {
                LogDebug("loadi %d", *((int16_t *)g_pc));
                SetAcc(GetSWord());
            } break;

            // Push the value in the acc on the stack.
            case OP_push: {
                LogDebug("push");
                Push(g_acc);
            } break;

            // Push an immediate value on the stack.
            case OP_pushi_ONE: {
                LogDebug("pushi %d", *((int8_t *)g_pc));
                Push(GetSByte());
            } break;

            // Push an immediate value on the stack.
            case OP_pushi_TWO: {
                LogDebug("pushi %d", *((int16_t *)g_pc));
                Push(GetSWord());
            } break;

            // Pop the stack and discard the value.
            case OP_toss: {
                LogDebug("toss");
                Pop();
            } break;

            // Duplicate the current top value on the stack.
            case OP_dup: {
                LogDebug("dup");
                uintptr_t tos = Peek();
                Push(tos);
            } break;

            // Link to a procedure by creating a temporary variable space.
            case OP_link_ONE: {
                LogDebug("link %u", *g_pc);
                uintptr_t frame = GetByte();
                g_vars.temp     = g_bp + 1;
                g_bp += frame;
                if (g_bp >= g_pStackEnd) {
                    PError(PE_STACK_BLOWN, 0, 0);
                }
            } break;

            // Link to a procedure by creating a temporary variable space.
            case OP_link_TWO: {
                LogDebug("link %u", *((uint16_t *)g_pc));
                uintptr_t frame = GetWord();
                g_vars.temp     = g_bp + 1;
                g_bp += frame;
                if (g_bp >= g_pStackEnd) {
                    PError(PE_STACK_BLOWN, 0, 0);
                }
            } break;

            // Call a procedure in the current module.
            case OP_call_THREE: {
                LogDebug("call %+d, %u", *((int16_t *)g_pc), g_pc[2]);
                uintptr_t offset          = GetSWord();
                uint      paramsByteCount = GetByte();
                uint8_t  *retAttr         = g_pc;
                g_pc += offset;
                DoCall(paramsByteCount / sizeof(uint16_t));
                g_pc = retAttr;
            } break;

            case OP_call_TWO: {
                LogDebug("call %+d, %u", *((int8_t *)g_pc), g_pc[1]);
                uintptr_t offset          = GetSByte();
                uint      paramsByteCount = GetByte();
                uint8_t  *retAttr         = g_pc;
                g_pc += offset;
                DoCall(paramsByteCount / sizeof(uint16_t));
                g_pc = retAttr;
            } break;

            // Call a kernel routine.
            case OP_callk_THREE: {
                {
                    char buf[64] = { 0 };
                    LogDebug("callk %u (%s), %u",
                             *((uint16_t *)g_pc),
                             GetKernelName(*((uint16_t *)g_pc), buf),
                             g_pc[2]);
                }
                g_thisIP       = g_pc;
                uint kernelNum = GetWord();
                KernelCall(kernelNum);
            } break;

            case OP_callk_TWO: {
                {
                    char buf[64] = { 0 };
                    LogDebug("callk %u (%s), %u",
                             *g_pc,
                             GetKernelName(*g_pc, buf),
                             g_pc[1]);
                }
                g_thisIP       = g_pc;
                uint kernelNum = GetByte();
                KernelCall(kernelNum);
            } break;

            // Call a procedure in the base script.
            case OP_callb_THREE: {
                LogDebug("callb %u, %u", *((uint16_t *)g_pc), g_pc[2]);
                uint entryNum        = GetWord();
                uint paramsByteCount = GetByte();
                Dispatch(0, entryNum, paramsByteCount / sizeof(uint16_t));
            } break;

            case OP_callb_TWO: {
                LogDebug("callb %u, %u", *g_pc, g_pc[1]);
                uint entryNum        = GetByte();
                uint paramsByteCount = GetByte();
                Dispatch(0, entryNum, paramsByteCount / sizeof(uint16_t));
            } break;

            // Call a procedure in an external script.
            case OP_calle_FOUR: {
                LogDebug("calle %u, %u, %u",
                         ((uint16_t *)g_pc)[0],
                         ((uint16_t *)g_pc)[1],
                         g_pc[4]);
                uint scriptNum       = GetWord();
                uint entryNum        = GetWord();
                uint paramsByteCount = GetByte();
                Dispatch(
                  scriptNum, entryNum, paramsByteCount / sizeof(uint16_t));
            } break;

            case OP_calle_TWO: {
                LogDebug("calle %u, %u, %u", *g_pc, g_pc[1], g_pc[2]);
                uint scriptNum       = GetByte();
                uint entryNum        = GetByte();
                uint paramsByteCount = GetByte();
                Dispatch(
                  scriptNum, entryNum, paramsByteCount / sizeof(uint16_t));
            } break;

            case OP_ret:
                LogDebug("ret ------------");
                return;

            // Send messages to an object whose ID is in the acc.
            case OP_send_ONE: {
                LogDebug("send %u", *g_pc);
                SendMessage((Obj *)g_acc);
            } break;

            // Get a class address based on the class number.
            case OP_class_TWO: {
                LogDebug("class %u", *((uint16_t *)g_pc));
                ObjID sel = GetWord();
                Obj  *obj = GetClass(sel);
                SetAcc((uintptr_t)obj);
            } break;

            case OP_class_ONE: {
                LogDebug("class %u", *g_pc);
                ObjID sel = GetByte();
                Obj  *obj = GetClass(sel);
                SetAcc((uintptr_t)obj);
            } break;

            // Return the address of the current object in the acc.
            case OP_selfID: {
                LogDebug("selfID");
                SetAcc((uintptr_t)g_object);
            } break;

            // Send to current object.
            case OP_self_TWO:
            case OP_self_ONE: {
                LogDebug("self %u", *g_pc);
                Messager(g_object);
            } break;

            // Send to a class address based on the class number.
            case OP_super_THREE: {
                LogDebug("super %u, %u", *((uint16_t *)g_pc), g_pc[2]);
                ObjID sel = GetWord();
                Obj  *obj = GetClass(sel);
                Messager(obj);
            } break;

            case OP_super_TWO: {
                LogDebug("super %u, %u", *g_pc, g_pc[1]);
                ObjID sel = GetByte();
                Obj  *obj = GetClass(sel);
                Messager(obj);
            } break;

            // Add the 'rest' of the current stack frame to the parameters which
            // are already on the stack.
            case OP_rest_ONE: {
                LogDebug("rest %u", *g_pc);
                // Get a pointer to the parameters.
                uintptr_t *parmVar = g_vars.parm;
                // Get number of parameters in current frame.
                uint parmCount = *parmVar;
                // Get number of parameter to start with.
                uint startCount = GetByte();
                parmCount       = parmCount - startCount + 1;
                if ((short)parmCount < 0) {
                    parmCount = 0;
                }

                // Tell others how to adjust stack frame.
                g_restArgsCount += parmCount;
                parmVar += startCount;

                for (; parmCount != 0; --parmCount) {
                    // Get parameter and put it on the stack.
                    Push(*parmVar++);
                }
            } break;

            // Load the effective address of a variable into the acc.
            case OP_lea_FOUR: {
                LogDebug(
                  "lea %u, %u", ((uint16_t *)g_pc)[0], ((uint16_t *)g_pc)[1]);
                // Get the type of the variable.
                uint varType = GetWord();
                // Get the number of the variable.
                uint varNum = GetWord();
                LoadEffectiveAddress(varType, varNum);
            } break;

            case OP_lea_TWO: {
                LogDebug("lea %u, %u", g_pc[0], g_pc[1]);
                // Get the type of the variable.
                uint varType = GetByte();
                // Get the number of the variable.
                uint varNum = GetByte();
                LoadEffectiveAddress(varType, varNum);
            } break;

            // Push previous value of acc on the stack.
            case OP_pprev: {
                LogDebug("pprev");
                Push(g_prevAcc);
            } break;

            // Load prop to acc
            case OP_pToa_TWO: {
                LogDebug("pToa %u", *((uint16_t *)g_pc));
                uint idx = GetWord();
                SetAcc(*GetIndexedPropPtr(idx));
            } break;

            // Load prop to acc
            case OP_pToa_ONE: {
                LogDebug("pToa %u", *g_pc);
                uint idx = GetByte();
                SetAcc(*GetIndexedPropPtr(idx));
            } break;

            // Load prop to stack
            case OP_pTos_TWO: {
                LogDebug("pTos %u", *((uint16_t *)g_pc));
                uint idx = GetWord();
                Push(*GetIndexedPropPtr(idx));
            } break;

            // Load prop to stack
            case OP_pTos_ONE: {
                LogDebug("pTos %u", *g_pc);
                uint idx = GetByte();
                Push(*GetIndexedPropPtr(idx));
            } break;

            // Store acc to prop
            case OP_aTop_TWO: {
                LogDebug("aTop %u", *((uint16_t *)g_pc));
                uint idx                = GetWord();
                *GetIndexedPropPtr(idx) = g_acc;
            } break;

            // Store acc to prop
            case OP_aTop_ONE: {
                LogDebug("aTop %u", *g_pc);
                uint idx                = GetByte();
                *GetIndexedPropPtr(idx) = g_acc;
            } break;

            // Store stack to prop
            case OP_sTop_TWO: {
                LogDebug("sTop %u", *((uint16_t *)g_pc));
                uint idx                = GetWord();
                *GetIndexedPropPtr(idx) = Pop();
            } break;

            // Store stack to prop
            case OP_sTop_ONE: {
                LogDebug("sTop %u", *g_pc);
                uint idx                = GetByte();
                *GetIndexedPropPtr(idx) = Pop();
            } break;

            // Inc prop
            case OP_ipToa_TWO: {
                LogDebug("ipToa %u", *((uint16_t *)g_pc));
                uint idx = GetWord();
                SetAcc(++(*GetIndexedPropPtr(idx)));
            } break;

            // Inc prop
            case OP_ipToa_ONE: {
                LogDebug("ipToa %u", *g_pc);
                uint idx = GetByte();
                SetAcc(++(*GetIndexedPropPtr(idx)));
            } break;

            // Inc prop to stack
            case OP_ipTos_TWO: {
                LogDebug("ipTos %u", *((uint16_t *)g_pc));
                uint idx = GetWord();
                Push(++(*GetIndexedPropPtr(idx)));
            } break;

            // Inc prop to stack
            case OP_ipTos_ONE: {
                LogDebug("ipTos %u", *g_pc);
                uint idx = GetByte();
                Push(++(*GetIndexedPropPtr(idx)));
            } break;

            // Dec prop
            case OP_dpToa_TWO: {
                LogDebug("dpToa %u", *((uint16_t *)g_pc));
                uint idx = GetWord();
                SetAcc(--(*GetIndexedPropPtr(idx)));
            } break;

            // Dec prop
            case OP_dpToa_ONE: {
                LogDebug("dpToa %u", *g_pc);
                uint idx = GetByte();
                SetAcc(--(*GetIndexedPropPtr(idx)));
            } break;

            // Dec prop to stack
            case OP_dpTos_TWO: {
                LogDebug("dpTos %u", *((uint16_t *)g_pc));
                uint idx = GetWord();
                Push(--(*GetIndexedPropPtr(idx)));
            } break;

            // Dec prop to stack
            case OP_dpTos_ONE: {
                LogDebug("dpTos %u", *g_pc);
                uint idx = GetByte();
                Push(--(*GetIndexedPropPtr(idx)));
            } break;

            // Load offset
            case OP_lofsa_TWO: {
                LogDebug("lofsa %+d", *((int16_t *)g_pc));
#if defined __WINDOWS__ || 1
                uintptr_t offset = GetWord();
                SetAcc((uintptr_t)g_scriptHeap + offset);
#else
#error Not implemented
#endif
            } break;

            // Load offset to stack
            case OP_lofss_TWO: {
                LogDebug("lofss %+d", *((int16_t *)g_pc));
#if defined __WINDOWS__ || 1
                uintptr_t offset = GetWord();
                Push((uintptr_t)g_scriptHeap + offset);
#else
#error Not implemented
#endif
            } break;

            case OP_push0: {
                LogDebug("push0");
                Push(0);
            } break;

            case OP_push1: {
                LogDebug("push1");
                Push(1);
            } break;

            case OP_push2: {
                LogDebug("push2");
                Push(2);
            } break;

            case OP_pushSelf: {
                LogDebug("pushSelf");
                Push((uintptr_t)g_object);
            } break;

// The following macros encapsulates load operations:
#define LoadByte(type) SetAcc(g_vars.type[GetByte()])
#define LoadWord(type) SetAcc(g_vars.type[GetWord()])

            case OP_lag_TWO: {
                LogDebug("lag %u", *((int16_t *)g_pc));
                LoadWord(global);
            } break;

            case OP_lag_ONE: {
                LogDebug("lag %u", *g_pc);
                LoadByte(global);
            } break;

            case OP_lal_TWO: {
                LogDebug("lal %u", *((int16_t *)g_pc));
                LoadWord(local);
            } break;

            case OP_lal_ONE: {
                LogDebug("lal %u", *g_pc);
                LoadByte(local);
            } break;

            case OP_lat_TWO: {
                LogDebug("lat %u", *((int16_t *)g_pc));
                LoadWord(temp);
            } break;

            case OP_lat_ONE: {
                LogDebug("lat %u", *g_pc);
                LoadByte(temp);
            } break;

            case OP_lap_TWO: {
                LogDebug("lap %u", *((int16_t *)g_pc));
                LoadWord(parm);
            } break;

            case OP_lap_ONE: {
                LogDebug("lap %u", *g_pc);
                LoadByte(parm);
            } break;

#undef LoadByte
#undef LoadWord

#define LoadByte(type) Push(g_vars.type[GetByte()])
#define LoadWord(type) Push(g_vars.type[GetWord()])

            case OP_lsg_TWO: {
                LogDebug("lsg %u", *((int16_t *)g_pc));
                LoadWord(global);
            } break;

            case OP_lsg_ONE: {
                LogDebug("lsg %u", *g_pc);
                LoadByte(global);
            } break;

            case OP_lsl_TWO: {
                LogDebug("lsl %u", *((int16_t *)g_pc));
                LoadWord(local);
            } break;

            case OP_lsl_ONE: {
                LogDebug("lsl %u", *g_pc);
                LoadByte(local);
            } break;

            case OP_lst_TWO: {
                LogDebug("lst %u", *((int16_t *)g_pc));
                LoadWord(temp);
            } break;

            case OP_lst_ONE: {
                LogDebug("lst %u", *g_pc);
                LoadByte(temp);
            } break;

            case OP_lsp_TWO: {
                LogDebug("lsp %u", *((int16_t *)g_pc));
                LoadWord(parm);
            } break;

            case OP_lsp_ONE: {
                LogDebug("lsp %u", *g_pc);
                LoadByte(parm);
            } break;

#undef LoadByte
#undef LoadWord

#define LoadByte(type) SetAcc(g_vars.type[GetIndexByte()])
#define LoadWord(type) SetAcc(g_vars.type[GetIndexWord()])

            case OP_lagi_TWO: {
                LogDebug("lagi %u", *((int16_t *)g_pc));
                LoadWord(global);
            } break;

            case OP_lagi_ONE: {
                LogDebug("lagi %u", *g_pc);
                LoadByte(global);
            } break;

            case OP_lali_TWO: {
                LogDebug("lali %u", *((int16_t *)g_pc));
                LoadWord(local);
            } break;

            case OP_lali_ONE: {
                LogDebug("lali %u", *g_pc);
                LoadByte(local);
            } break;

            case OP_lati_TWO: {
                LogDebug("lati %u", *((int16_t *)g_pc));
                LoadWord(temp);
            } break;

            case OP_lati_ONE: {
                LogDebug("lati %u", *g_pc);
                LoadByte(temp);
            } break;

            case OP_lapi_TWO: {
                LogDebug("lapi %u", *((int16_t *)g_pc));
                LoadWord(parm);
            } break;

            case OP_lapi_ONE: {
                LogDebug("lapi %u", *g_pc);
                LoadByte(parm);
            } break;

#undef LoadByte
#undef LoadWord

#define LoadByte(type) Push(g_vars.type[GetIndexByte()])
#define LoadWord(type) Push(g_vars.type[GetIndexWord()])

            case OP_lsgi_TWO: {
                LogDebug("lsgi %u", *((int16_t *)g_pc));
                LoadWord(global);
            } break;

            case OP_lsgi_ONE: {
                LogDebug("lsgi %u", *g_pc);
                LoadByte(global);
            } break;

            case OP_lsli_TWO: {
                LogDebug("lsli %u", *((int16_t *)g_pc));
                LoadWord(local);
            } break;

            case OP_lsli_ONE: {
                LogDebug("lsli %u", *g_pc);
                LoadByte(local);
            } break;

            case OP_lsti_TWO: {
                LogDebug("lsti %u", *((int16_t *)g_pc));
                LoadWord(temp);
            } break;

            case OP_lsti_ONE: {
                LogDebug("lsti %u", *g_pc);
                LoadByte(temp);
            } break;

            case OP_lspi_TWO: {
                LogDebug("lspi %u", *((int16_t *)g_pc));
                LoadWord(parm);
            } break;

            case OP_lspi_ONE: {
                LogDebug("lspi %u", *g_pc);
                LoadByte(parm);
            } break;

#undef LoadByte
#undef LoadWord

// The following macros encapsulates the store operations:
#define StoreByte(type) g_vars.type[GetByte()] = g_acc
#define StoreWord(type) g_vars.type[GetWord()] = g_acc

            case OP_sag_TWO: {
                LogDebug("sag %u", *((int16_t *)g_pc));
                StoreWord(global);
            } break;

            case OP_sag_ONE: {
                LogDebug("sag %u", *g_pc);
                StoreByte(global);
            } break;

            case OP_sal_TWO: {
                LogDebug("sal %u", *((int16_t *)g_pc));
                StoreWord(local);
            } break;

            case OP_sal_ONE: {
                LogDebug("sal %u", *g_pc);
                StoreByte(local);
            } break;

            case OP_sat_TWO: {
                LogDebug("sat %u", *((int16_t *)g_pc));
                StoreWord(temp);
            } break;

            case OP_sat_ONE: {
                LogDebug("sat %u", *g_pc);
                StoreByte(temp);
            } break;

            case OP_sap_TWO: {
                LogDebug("sap %u", *((int16_t *)g_pc));
                StoreWord(parm);
            } break;

            case OP_sap_ONE: {
                LogDebug("sap %u", *g_pc);
                StoreByte(parm);
            } break;

#undef StoreByte
#undef StoreWord

#define StoreByte(type) g_vars.type[GetByte()] = Pop()
#define StoreWord(type) g_vars.type[GetWord()] = Pop()

            case OP_ssg_TWO: {
                LogDebug("ssg %u", *((int16_t *)g_pc));
                StoreWord(global);
            } break;

            case OP_ssg_ONE: {
                LogDebug("ssg %u", *g_pc);
                StoreByte(global);
            } break;

            case OP_ssl_TWO: {
                LogDebug("ssl %u", *((int16_t *)g_pc));
                StoreWord(local);
            } break;

            case OP_ssl_ONE: {
                LogDebug("ssl %u", *g_pc);
                StoreByte(local);
            } break;

            case OP_sst_TWO: {
                LogDebug("sst %u", *((int16_t *)g_pc));
                StoreWord(temp);
            } break;

            case OP_sst_ONE: {
                LogDebug("sst %u", *g_pc);
                StoreByte(temp);
            } break;

            case OP_ssp_TWO: {
                LogDebug("ssp %u", *((int16_t *)g_pc));
                StoreWord(parm);
            } break;

            case OP_ssp_ONE: {
                LogDebug("ssp %u", *g_pc);
                StoreByte(parm);
            } break;

#undef StoreByte
#undef StoreWord

#define StoreByte(type) g_acc = g_vars.type[GetIndexByte()] = Pop()
#define StoreWord(type) g_acc = g_vars.type[GetIndexWord()] = Pop()

            case OP_sagi_TWO: {
                LogDebug("sagi %u", *((int16_t *)g_pc));
                StoreWord(global);
            } break;

            case OP_sagi_ONE: {
                LogDebug("sagi %u", *g_pc);
                StoreByte(global);
            } break;

            case OP_sali_TWO: {
                LogDebug("sali %u", *((int16_t *)g_pc));
                StoreWord(local);
            } break;

            case OP_sali_ONE: {
                LogDebug("sali %u", *g_pc);
                StoreByte(local);
            } break;

            case OP_sati_TWO: {
                LogDebug("sati %u", *((int16_t *)g_pc));
                StoreWord(temp);
            } break;

            case OP_sati_ONE: {
                LogDebug("sati %u", *g_pc);
                StoreByte(temp);
            } break;

            case OP_sapi_TWO: {
                LogDebug("sapi %u", *((int16_t *)g_pc));
                StoreWord(parm);
            } break;

            case OP_sapi_ONE: {
                LogDebug("sapi %u", *g_pc);
                StoreByte(parm);
            } break;

#undef StoreByte
#undef StoreWord

#define StoreByte(type) g_vars.type[GetIndexByte()] = Pop()
#define StoreWord(type) g_vars.type[GetIndexWord()] = Pop()

            case OP_ssgi_TWO: {
                LogDebug("ssgi %u", *((int16_t *)g_pc));
                StoreWord(global);
            } break;

            case OP_ssgi_ONE: {
                LogDebug("ssgi %u", *g_pc);
                StoreByte(global);
            } break;

            case OP_ssli_TWO: {
                LogDebug("ssli %u", *((int16_t *)g_pc));
                StoreWord(local);
            } break;

            case OP_ssli_ONE: {
                LogDebug("ssli %u", *g_pc);
                StoreByte(local);
            } break;

            case OP_ssti_TWO: {
                LogDebug("ssti %u", *((int16_t *)g_pc));
                StoreWord(temp);
            } break;

            case OP_ssti_ONE: {
                LogDebug("ssti %u", *g_pc);
                StoreByte(temp);
            } break;

            case OP_sspi_TWO: {
                LogDebug("sspi %u", *((int16_t *)g_pc));
                StoreWord(parm);
            } break;

            case OP_sspi_ONE: {
                LogDebug("sspi %u", *g_pc);
                StoreByte(parm);
            } break;

#undef StoreByte
#undef StoreWord

// The following macro encapsulates the increment operations:
#define IncrementByte(type) SetAcc(++(g_vars.type[GetByte()]))
#define IncrementWord(type) SetAcc(++(g_vars.type[GetWord()]))

            case OP_iag_TWO: {
                LogDebug("iag %u", *((int16_t *)g_pc));
                IncrementWord(global);
            } break;

            case OP_iag_ONE: {
                LogDebug("iag %u", *g_pc);
                IncrementByte(global);
            } break;

            case OP_ial_TWO: {
                LogDebug("ial %u", *((int16_t *)g_pc));
                IncrementWord(local);
            } break;

            case OP_ial_ONE: {
                LogDebug("ial %u", *g_pc);
                IncrementByte(local);
            } break;

            case OP_iat_TWO: {
                LogDebug("iat %u", *((int16_t *)g_pc));
                IncrementWord(temp);
            } break;

            case OP_iat_ONE: {
                LogDebug("iat %u", *g_pc);
                IncrementByte(temp);
            } break;

            case OP_iap_TWO: {
                LogDebug("iap %u", *((int16_t *)g_pc));
                IncrementWord(parm);
            } break;

            case OP_iap_ONE: {
                LogDebug("iap %u", *g_pc);
                IncrementByte(parm);
            } break;

#undef IncrementByte
#undef IncrementWord

#define IncrementByte(type) Push(++(g_vars.type[GetByte()]))
#define IncrementWord(type) Push(++(g_vars.type[GetWord()]))

            case OP_isg_TWO: {
                LogDebug("isg %u", *((int16_t *)g_pc));
                IncrementWord(global);
            } break;

            case OP_isg_ONE: {
                LogDebug("isg %u", *g_pc);
                IncrementByte(global);
            } break;

            case OP_isl_TWO: {
                LogDebug("isl %u", *((int16_t *)g_pc));
                IncrementWord(local);
            } break;

            case OP_isl_ONE: {
                LogDebug("isl %u", *g_pc);
                IncrementByte(local);
            } break;

            case OP_ist_TWO: {
                LogDebug("ist %u", *((int16_t *)g_pc));
                IncrementWord(temp);
            } break;

            case OP_ist_ONE: {
                LogDebug("ist %u", *g_pc);
                IncrementByte(temp);
            } break;

            case OP_isp_TWO: {
                LogDebug("isp %u", *((int16_t *)g_pc));
                IncrementWord(parm);
            } break;

            case OP_isp_ONE: {
                LogDebug("isp %u", *g_pc);
                IncrementByte(parm);
            } break;

#undef IncrementByte
#undef IncrementWord

#define IncrementByte(type) SetAcc(++(g_vars.type[GetIndexByte()]))
#define IncrementWord(type) SetAcc(++(g_vars.type[GetIndexWord()]))

            case OP_iagi_TWO: {
                LogDebug("iagi %u", *((int16_t *)g_pc));
                IncrementWord(global);
            } break;

            case OP_iagi_ONE: {
                LogDebug("iagi %u", *g_pc);
                IncrementByte(global);
            } break;

            case OP_iali_TWO: {
                LogDebug("iali %u", *((int16_t *)g_pc));
                IncrementWord(local);
            } break;

            case OP_iali_ONE: {
                LogDebug("iali %u", *g_pc);
                IncrementByte(local);
            } break;

            case OP_iati_TWO: {
                LogDebug("iati %u", *((int16_t *)g_pc));
                IncrementWord(temp);
            } break;

            case OP_iati_ONE: {
                LogDebug("iati %u", *g_pc);
                IncrementByte(temp);
            } break;

            case OP_iapi_TWO: {
                LogDebug("iapi %u", *((int16_t *)g_pc));
                IncrementWord(parm);
            } break;

            case OP_iapi_ONE: {
                LogDebug("iapi %u", *g_pc);
                IncrementByte(parm);
            } break;

#undef IncrementByte
#undef IncrementWord

#define IncrementByte(type) Push(++(g_vars.type[GetIndexByte()]))
#define IncrementWord(type) Push(++(g_vars.type[GetIndexWord()]))

            case OP_isgi_TWO: {
                LogDebug("isgi %u", *((int16_t *)g_pc));
                IncrementWord(global);
            } break;

            case OP_isgi_ONE: {
                LogDebug("isgi %u", *g_pc);
                IncrementByte(global);
            } break;

            case OP_isli_TWO: {
                LogDebug("isli %u", *((int16_t *)g_pc));
                IncrementWord(local);
            } break;

            case OP_isli_ONE: {
                LogDebug("isli %u", *g_pc);
                IncrementByte(local);
            } break;

            case OP_isti_TWO: {
                LogDebug("isti %u", *((int16_t *)g_pc));
                IncrementWord(temp);
            } break;

            case OP_isti_ONE: {
                LogDebug("isti %u", *g_pc);
                IncrementByte(temp);
            } break;

            case OP_ispi_TWO: {
                LogDebug("ispi %u", *((int16_t *)g_pc));
                IncrementWord(parm);
            } break;

            case OP_ispi_ONE: {
                LogDebug("ispi %u", *g_pc);
                IncrementByte(parm);
            } break;

#undef IncrementByte
#undef IncrementWord

// The following macro encapsulates the decrement operations:
#define DecrementByte(type) SetAcc(--(g_vars.type[GetByte()]))
#define DecrementWord(type) SetAcc(--(g_vars.type[GetWord()]))

            case OP_dag_TWO: {
                LogDebug("dag %u", *((int16_t *)g_pc));
                DecrementWord(global);
            } break;

            case OP_dag_ONE: {
                LogDebug("dag %u", *g_pc);
                DecrementByte(global);
            } break;

            case OP_dal_TWO: {
                LogDebug("dal %u", *((int16_t *)g_pc));
                DecrementWord(local);
            } break;

            case OP_dal_ONE: {
                LogDebug("dal %u", *g_pc);
                DecrementByte(local);
            } break;

            case OP_dat_TWO: {
                LogDebug("dat %u", *((int16_t *)g_pc));
                DecrementWord(temp);
            } break;

            case OP_dat_ONE: {
                LogDebug("dat %u", *g_pc);
                DecrementByte(temp);
            } break;

            case OP_dap_TWO: {
                LogDebug("dap %u", *((int16_t *)g_pc));
                DecrementWord(parm);
            } break;

            case OP_dap_ONE: {
                LogDebug("dap %u", *g_pc);
                DecrementByte(parm);
            } break;

#undef DecrementByte
#undef DecrementWord

#define DecrementByte(type) Push(--(g_vars.type[GetByte()]))
#define DecrementWord(type) Push(--(g_vars.type[GetWord()]))

            case OP_dsg_TWO: {
                LogDebug("dsg %u", *((int16_t *)g_pc));
                DecrementWord(global);
            } break;

            case OP_dsg_ONE: {
                LogDebug("dsg %u", *g_pc);
                DecrementByte(global);
            } break;

            case OP_dsl_TWO: {
                LogDebug("dsl %u", *((int16_t *)g_pc));
                DecrementWord(local);
            } break;

            case OP_dsl_ONE: {
                LogDebug("dsl %u", *g_pc);
                DecrementByte(local);
            } break;

            case OP_dst_TWO: {
                LogDebug("dst %u", *((int16_t *)g_pc));
                DecrementWord(temp);
            } break;

            case OP_dst_ONE: {
                LogDebug("dst %u", *g_pc);
                DecrementByte(temp);
            } break;

            case OP_dsp_TWO: {
                LogDebug("dsp %u", *((int16_t *)g_pc));
                DecrementWord(parm);
            } break;

            case OP_dsp_ONE: {
                LogDebug("dsp %u", *g_pc);
                DecrementByte(parm);
            } break;

#undef DecrementByte
#undef DecrementWord

#define DecrementByte(type) SetAcc(--(g_vars.type[GetIndexByte()]))
#define DecrementWord(type) SetAcc(--(g_vars.type[GetIndexWord()]))

            case OP_dagi_TWO: {
                LogDebug("dagi %u", *((int16_t *)g_pc));
                DecrementWord(global);
            } break;

            case OP_dagi_ONE: {
                LogDebug("dagi %u", *g_pc);
                DecrementByte(global);
            } break;

            case OP_dali_TWO: {
                LogDebug("dali %u", *((int16_t *)g_pc));
                DecrementWord(local);
            } break;

            case OP_dali_ONE: {
                LogDebug("dali %u", *g_pc);
                DecrementByte(local);
            } break;

            case OP_dati_TWO: {
                LogDebug("dati %u", *((int16_t *)g_pc));
                DecrementWord(temp);
            } break;

            case OP_dati_ONE: {
                LogDebug("dati %u", *g_pc);
                DecrementByte(temp);
            } break;

            case OP_dapi_TWO: {
                LogDebug("dapi %u", *((int16_t *)g_pc));
                DecrementWord(parm);
            } break;

            case OP_dapi_ONE: {
                LogDebug("dapi %u", *g_pc);
                DecrementByte(parm);
            } break;

#undef DecrementByte
#undef DecrementWord

#define DecrementByte(type) Push(--(g_vars.type[GetIndexByte()]))
#define DecrementWord(type) Push(--(g_vars.type[GetIndexWord()]))

            case OP_dsgi_TWO: {
                LogDebug("dsgi %u", *((int16_t *)g_pc));
                DecrementWord(global);
            } break;

            case OP_dsgi_ONE: {
                LogDebug("dsgi %u", *g_pc);
                DecrementByte(global);
            } break;

            case OP_dsli_TWO: {
                LogDebug("dsli %u", *((int16_t *)g_pc));
                DecrementWord(local);
            } break;

            case OP_dsli_ONE: {
                LogDebug("dsli %u", *g_pc);
                DecrementByte(local);
            } break;

            case OP_dsti_TWO: {
                LogDebug("dsti %u", *((int16_t *)g_pc));
                DecrementWord(temp);
            } break;

            case OP_dsti_ONE: {
                LogDebug("dsti %u", *g_pc);
                DecrementByte(temp);
            } break;

            case OP_dspi_TWO: {
                LogDebug("dspi %u", *((int16_t *)g_pc));
                DecrementWord(parm);
            } break;

            case OP_dspi_ONE: {
                LogDebug("dspi %u", *g_pc);
                DecrementByte(parm);
            } break;

#undef DecrementByte
#undef DecrementWord
        }
    }
}

static void KernelCall(uint kernelNum)
{
    uintptr_t *prevSP = g_sp;

    // Point to top of parameter space.
    uint paramsByteCount = GetByte();
    g_bp -= paramsByteCount / sizeof(uint16_t);
    g_bp -= g_restArgsCount;
    *g_bp += g_restArgsCount;
    g_restArgsCount = 0;

    if (kernelNum < KERNELMAX) {
        s_kernelDispTbl[kernelNum](g_bp);
    } else {
        PError(PE_BAD_KERNAL, kernelNum, 0);
    }

    PollInputEvent();

    g_bp--;
    g_sp = prevSP;
}

static void DoCall(uint parmCount)
{
    uintptr_t *prevParmVar = g_vars.parm;
    uintptr_t *prevTempVar = g_vars.temp;

    g_vars.parm = g_bp - (parmCount + g_restArgsCount);
    *g_vars.parm += g_restArgsCount;
    g_restArgsCount = 0;

    DebugFunctionEntry(NULL, (uint)-1);
    ExecuteCode();
    DebugFunctionExit();

    g_bp        = g_vars.parm - 1; // Toss the params count.
    g_sp        = g_bp;
    g_vars.temp = prevTempVar;
    g_vars.parm = prevParmVar;
}

static void Dispatch(uint scriptNum, uint entryNum, uint parmCount)
{
    Handle     prevScriptHandle;
    uint       prevScriptNum;
    uint8_t   *prevIP;
    uintptr_t *prevLocalVar;

    prevScriptHandle = g_scriptHandle;
    prevScriptNum    = g_thisScript;
    g_thisScript     = scriptNum;
    prevIP           = g_pc;
    prevLocalVar     = g_vars.local;

    Script *script = GetDispatchAddrInHunk(scriptNum, entryNum, &g_pc);
    g_scriptHandle = script->hunk;
    g_vars.local   = script->vars;

    DoCall(parmCount);

    g_vars.local   = prevLocalVar;
    g_pc           = prevIP;
    g_thisScript   = prevScriptNum;
    g_scriptHandle = prevScriptHandle;
}

static void LoadEffectiveAddress(uint varType, uint varNum)
{
    uint       index;
    uintptr_t *var;

    // If the variable is indexed, add the index to the offset.
    if ((varType & OP_INDEX) != 0) {
        // The index is in the acc.
        varNum += g_acc;
    }

    // Get index into var base table.
    index = (varType & OP_VAR) / sizeof(uint16_t);

    // Get the base of the var space.
    var = g_vars.all[index];
    SetAcc((uintptr_t)(var + varNum));
}

static Script *GetDispatchAddrInHeap(uint scriptNum, uint entryNum, Obj **obj)
{
    Script *script = ScriptPtr(scriptNum);
    // TODO: should this be "<=" ???
    if (script != NULL && entryNum < (uint)script->exports->numEntries) {
        assert(script->exports->entries[entryNum].ptrSeg == (uint16_t)-1);
        *obj =
          (Obj *)(g_scriptHeap + script->exports->entries[entryNum].ptrOff);
    }
    return script;
}

static Script *GetDispatchAddrInHunk(uint      scriptNum,
                                     uint      entryNum,
                                     uint8_t **code)
{
    Script *script = ScriptPtr(scriptNum);
    if (script != NULL && entryNum < (uint)script->exports->numEntries) {
        assert(script->exports->entries[entryNum].ptrSeg == 0);
        *code =
          (uint8_t *)script->hunk + script->exports->entries[entryNum].ptrOff;
    }
    return script;
}

static char *GetKernelName(uint num, char *buffer)
{
    char *text;
    char *textEnd;

    // Find the script, get the handle to its far text, find the
    // string in the script, then copy the string to the buffer.
    text = (char *)ResLoad(RES_VOCAB, KERNEL_VOCAB);
    if (text == NULL) {
        // shouldn't get back here...but just in case
        *buffer = '\0';
    } else {
        textEnd = text + ResHandleSize(text);

        // Scan for the nth string.
        while (text < textEnd && num-- != 0) {
            text += strlen(text) + 1;
        }

        // Then copy the string.
        if (text < textEnd) {
            strcpy(buffer, text);
        } else {
            *buffer = '\0';
#ifdef DEBUG
            static char const *const s_knamesExt[] = {
                "DoAudio", "DoSync",   "AvoidPath", "Sort",    "ATan",
                "Lock",    "StrSplit", "Message",   "IsItSkip"
            };
            if (num < ARRAYSIZE(s_knamesExt)) {
                strcpy(buffer, s_knamesExt[num]);
            }
#endif
        }
    }

    return buffer;
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
