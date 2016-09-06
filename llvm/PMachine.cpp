#include <llvm/IR/Instructions.h>
#include "PMachine.hpp"

using namespace llvm;

extern IntegerType *g_sizeTy;


#define PSTACKSIZE (10 * 1024)


#define OP_LDST     0x80  // load/store if set
#define OP_BYTE     0x01  // byte operation if set, word otw

#define OP_TYPE     0x60  // mask for operation type
#define OP_LOAD     0x00  // load
#define OP_STORE    0x20  // store
#define OP_INC      0x40  // increment operation
#define OP_DEC      0x60  // decrement operation

#define OP_INDEX    0x10  // indexed op if set, non-indexed otw

#define OP_STACK    0x08  // load to stack if set

#define OP_VAR      0x06  // mask for var type
#define OP_GLOBAL   0x00  // global var
#define OP_LOCAL    0x02  // local var
#define OP_TMP      0x04  // temporary var (on the stack)
#define OP_PARM     0x06  // parameter (different stack frame than tmp)



#define OP_bnot         0x00
//      BadOp           0x01
#define OP_add          0x02
//      BadOp           0x03
#define OP_sub          0x04
//      BadOp           0x05
#define OP_mul          0x06
//      BadOp           0x07
#define OP_div          0x08
//      BadOp           0x09
#define OP_mod          0x0A
//      BadOp           0x0B
#define OP_shr          0x0C
//      BadOp           0x0D
#define OP_shl          0x0E
//      BadOp           0x0F
#define OP_xor          0x10
//      BadOp           0x11
#define OP_and          0x12
//      BadOp           0x13
#define OP_or           0x14
//      BadOp           0x15
#define OP_neg          0x16
//      BadOp           0x17
#define OP_not          0x18
//      BadOp           0x19
#define OP_eq           0x1A
//      BadOp           0x1B
#define OP_ne           0x1C
//      BadOp           0x1D
#define OP_gt           0x1E
//      BadOp           0x1F
#define OP_ge           0x20
//      BadOp           0x21
#define OP_lt           0x22
//      BadOp           0x23
#define OP_le           0x24
//      BadOp           0x25
#define OP_ugt          0x26
//      BadOp           0x27
#define OP_uge          0x28
//      BadOp           0x29
#define OP_ult          0x2A
//      BadOp           0x2B
#define OP_ule          0x2C
//      BadOp           0x2D
#define OP_bt_TWO       0x2E
#define OP_bt_ONE       0x2F
#define OP_bnt_TWO      0x30
#define OP_bnt_ONE      0x31
#define OP_jmp_TWO      0x32
#define OP_jmp_ONE      0x33
#define OP_loadi_TWO    0x34
#define OP_loadi_ONE    0x35
#define OP_push         0x36
//      BadOp           0x37
#define OP_pushi_TWO    0x38
#define OP_pushi_ONE    0x39
#define OP_toss         0x3A
//      BadOp           0x3B
#define OP_dup          0x3C
//      BadOp           0x3D
#define OP_link_TWO     0x3E
#define OP_link_ONE     0x3F
#define OP_call_THREE   0x40
#define OP_call_TWO     0x41
#define OP_callk_THREE  0x42
#define OP_callk_TWO    0x43
#define OP_callb_THREE  0x44
#define OP_callb_TWO    0x45
#define OP_calle_FOUR   0x46
#define OP_calle_TWO    0x47
#define OP_ret          0x48
//      BadOp           0x49
#define OP_send_ONE     0x4A
//      BadOp           0x4B
//      BadOp           0x4C
//      BadOp           0x4D
//      BadOp           0x4E
//      BadOp           0x4F
#define OP_class_TWO    0x50
#define OP_class_ONE    0x51
//      BadOp           0x52
//      BadOp           0x53
#define OP_self_TWO     0x54
#define OP_self_ONE     0x55
#define OP_super_THREE  0x56
#define OP_super_TWO    0x57
//      BadOp           0x58
#define OP_rest_ONE     0x59
#define OP_lea_FOUR     0x5A
#define OP_lea_TWO      0x5B
#define OP_selfID       0x5C
//      BadOp           0x5D
//      BadOp           0x5E
//      BadOp           0x5F
#define OP_pprev        0x60
//      BadOp           0x61
#define OP_pToa_TWO     0x62
#define OP_pToa_ONE     0x63
#define OP_aTop_TWO     0x64
#define OP_aTop_ONE     0x65
#define OP_pTos_TWO     0x66
#define OP_pTos_ONE     0x67
#define OP_sTop_TWO     0x68
#define OP_sTop_ONE     0x69
#define OP_ipToa_TWO    0x6A
#define OP_ipToa_ONE    0x6B
#define OP_dpToa_TWO    0x6C
#define OP_dpToa_ONE    0x6D
#define OP_ipTos_TWO    0x6E
#define OP_ipTos_ONE    0x6F
#define OP_dpTos_TWO    0x70
#define OP_dpTos_ONE    0x71
#define OP_lofsa_TWO    0x72
//      BadOp           0x73
#define OP_lofss_TWO    0x74
//      BadOp           0x75
#define OP_push0        0x76
//      BadOp           0x77
#define OP_push1        0x78
//      BadOp           0x79
#define OP_push2        0x7A
//      BadOp           0x7B
#define OP_pushSelf     0x7C
//      BadOp           0x7B
//      BadOp           0x7B
//      BadOp           0x7F
#define OP_lag_TWO      0x80
#define OP_lag_ONE      0x81
#define OP_lal_TWO      0x82
#define OP_lal_ONE      0x83
#define OP_lat_TWO      0x84
#define OP_lat_ONE      0x85
#define OP_lap_TWO      0x86
#define OP_lap_ONE      0x87
#define OP_lsg_TWO      0x88
#define OP_lsg_ONE      0x89
#define OP_lsl_TWO      0x8A
#define OP_lsl_ONE      0x8B
#define OP_lst_TWO      0x8C
#define OP_lst_ONE      0x8D
#define OP_lsp_TWO      0x8E
#define OP_lsp_ONE      0x8F
#define OP_lagi_TWO     0x90
#define OP_lagi_ONE     0x91
#define OP_lali_TWO     0x92
#define OP_lali_ONE     0x93
#define OP_lati_TWO     0x94
#define OP_lati_ONE     0x95
#define OP_lapi_TWO     0x96
#define OP_lapi_ONE     0x97
#define OP_lsgi_TWO     0x98
#define OP_lsgi_ONE     0x99
#define OP_lsli_TWO     0x9A
#define OP_lsli_ONE     0x9B
#define OP_lsti_TWO     0x9C
#define OP_lsti_ONE     0x9D
#define OP_lspi_TWO     0x9E
#define OP_lspi_ONE     0x9F
#define OP_sag_TWO      0xA0
#define OP_sag_ONE      0xA1
#define OP_sal_TWO      0xA2
#define OP_sal_ONE      0xA3
#define OP_sat_TWO      0xA4
#define OP_sat_ONE      0xA5
#define OP_sap_TWO      0xA6
#define OP_sap_ONE      0xA7
#define OP_ssg_TWO      0xA8
#define OP_ssg_ONE      0xA9
#define OP_ssl_TWO      0xAA
#define OP_ssl_ONE      0xAB
#define OP_sst_TWO      0xAC
#define OP_sst_ONE      0xAD
#define OP_ssp_TWO      0xAE
#define OP_ssp_ONE      0xAF
#define OP_sagi_TWO     0xB0
#define OP_sagi_ONE     0xB1
#define OP_sali_TWO     0xB2
#define OP_sali_ONE     0xB3
#define OP_sati_TWO     0xB4
#define OP_sati_ONE     0xB5
#define OP_sapi_TWO     0xB6
#define OP_sapi_ONE     0xB7
#define OP_ssgi_TWO     0xB8
#define OP_ssgi_ONE     0xB9
#define OP_ssli_TWO     0xBA
#define OP_ssli_ONE     0xBB
#define OP_ssti_TWO     0xBC
#define OP_ssti_ONE     0xBD
#define OP_sspi_TWO     0xBE
#define OP_sspi_ONE     0xBF
#define OP_iag_TWO      0xC0
#define OP_iag_ONE      0xC1
#define OP_ial_TWO      0xC2
#define OP_ial_ONE      0xC3
#define OP_iat_TWO      0xC4
#define OP_iat_ONE      0xC5
#define OP_iap_TWO      0xC6
#define OP_iap_ONE      0xC7
#define OP_isg_TWO      0xC8
#define OP_isg_ONE      0xC9
#define OP_isl_TWO      0xCA
#define OP_isl_ONE      0xCB
#define OP_ist_TWO      0xCC
#define OP_ist_ONE      0xCD
#define OP_isp_TWO      0xCE
#define OP_isp_ONE      0xCF
#define OP_iagi_TWO     0xD0
#define OP_iagi_ONE     0xD1
#define OP_iali_TWO     0xD2
#define OP_iali_ONE     0xD3
#define OP_iati_TWO     0xD4
#define OP_iati_ONE     0xD5
#define OP_iapi_TWO     0xD6
#define OP_iapi_ONE     0xD7
#define OP_isgi_TWO     0xD8
#define OP_isgi_ONE     0xD9
#define OP_isli_TWO     0xDA
#define OP_isli_ONE     0xDB
#define OP_isti_TWO     0xDC
#define OP_isti_ONE     0xDD
#define OP_ispi_TWO     0xDE
#define OP_ispi_ONE     0xDF
#define OP_dag_TWO      0xE0
#define OP_dag_ONE      0xE1
#define OP_dal_TWO      0xE2
#define OP_dal_ONE      0xE3
#define OP_dat_TWO      0xE4
#define OP_dat_ONE      0xE5
#define OP_dap_TWO      0xE6
#define OP_dap_ONE      0xE7
#define OP_dsg_TWO      0xE8
#define OP_dsg_ONE      0xE9
#define OP_dsl_TWO      0xEA
#define OP_dsl_ONE      0xEB
#define OP_dst_TWO      0xEC
#define OP_dst_ONE      0xED
#define OP_dsp_TWO      0xEE
#define OP_dsp_ONE      0xEF
#define OP_dagi_TWO     0xF0
#define OP_dagi_ONE     0xF1
#define OP_dali_TWO     0xF2
#define OP_dali_ONE     0xF3
#define OP_dati_TWO     0xF4
#define OP_dati_ONE     0xF5
#define OP_dapi_TWO     0xF6
#define OP_dapi_ONE     0xF7
#define OP_dsgi_TWO     0xF8
#define OP_dsgi_ONE     0xF9
#define OP_dsli_TWO     0xFA
#define OP_dsli_ONE     0xFB
#define OP_dsti_TWO     0xFC
#define OP_dsti_ONE     0xFD
#define OP_dspi_TWO     0xFE
#define OP_dspi_ONE     0xFF


PMachineInterpreter::PfnOp PMachineInterpreter::s_opTable[] = {
    &PMachineInterpreter::bnotOp,
    &PMachineInterpreter::addOp,
    &PMachineInterpreter::subOp,
    &PMachineInterpreter::mulOp,
    &PMachineInterpreter::divOp,
    &PMachineInterpreter::modOp,
    &PMachineInterpreter::shrOp,
    &PMachineInterpreter::shlOp,
    &PMachineInterpreter::xorOp,
    &PMachineInterpreter::andOp,
    &PMachineInterpreter::orOp,
    &PMachineInterpreter::negOp,
    &PMachineInterpreter::notOp,
    &PMachineInterpreter::cmpOp,
    &PMachineInterpreter::cmpOp,
    &PMachineInterpreter::cmpOp,
    &PMachineInterpreter::cmpOp,
    &PMachineInterpreter::cmpOp,
    &PMachineInterpreter::cmpOp,
    &PMachineInterpreter::cmpOp,
    &PMachineInterpreter::cmpOp,
    &PMachineInterpreter::cmpOp,
    &PMachineInterpreter::cmpOp,
    &PMachineInterpreter::btOp,
    &PMachineInterpreter::btOp,
    &PMachineInterpreter::jmpOp,
    &PMachineInterpreter::loadiOp,
    &PMachineInterpreter::pushOp,
    &PMachineInterpreter::pushiOp,
    &PMachineInterpreter::tossOp,
    &PMachineInterpreter::dupOp,
    &PMachineInterpreter::linkOp,
    &PMachineInterpreter::callOp,
    &PMachineInterpreter::callkOp,
    &PMachineInterpreter::callbOp,
    &PMachineInterpreter::calleOp,
    &PMachineInterpreter::retOp,
    &PMachineInterpreter::sendOp,
    &PMachineInterpreter::badOp,
    &PMachineInterpreter::badOp,
    &PMachineInterpreter::classOp,
    &PMachineInterpreter::badOp,
    &PMachineInterpreter::selfOp,
    &PMachineInterpreter::superOp,
    &PMachineInterpreter::restOp,
    &PMachineInterpreter::leaOp,
    &PMachineInterpreter::selfIdOp,
    &PMachineInterpreter::badOp,
    &PMachineInterpreter::pprevOp,
    &PMachineInterpreter::pToaOp,
    &PMachineInterpreter::aTopOp,
    &PMachineInterpreter::pTosOp,
    &PMachineInterpreter::sTopOp,
    &PMachineInterpreter::ipToaOp,
    &PMachineInterpreter::dpToaOp,
    &PMachineInterpreter::ipTosOp,
    &PMachineInterpreter::dpTosOp,
    &PMachineInterpreter::lofsaOp,
    &PMachineInterpreter::lofssOp,
    &PMachineInterpreter::push0Op,
    &PMachineInterpreter::push1Op,
    &PMachineInterpreter::push2Op,
    &PMachineInterpreter::pushSelfOp,
    &PMachineInterpreter::badOp
};


const char* PMachineInterpreter::s_kernelNames[] = {
    "KLoad",
    "KUnLoad",
    "KScriptID",
    "KDisposeScript",
    "KClone",
    "KDisposeClone",
    "KIsObject",
    "KRespondsTo",
    "KDrawPic",
    "KShow",
    "KPicNotValid",
    "KAnimate",
    "KSetNowSeen",
    "KNumLoops",
    "KNumCels",
    "KCelWide",
    "KCelHigh",
    "KDrawCel",
    "KAddToPic",
    "KNewWindow",
    "KGetPort",
    "KSetPort",
    "KDisposeWindow",
    "KDrawControl",
    "KHiliteControl",
    "KEditControl",
    "KTextSize",
    "KDisplay",
    "KGetEvent",
    "KGlobalToLocal",
    "KLocalToGlobal",
    "KMapKeyToDir",
    "KDrawMenuBar",
    "KMenuSelect",
    "KAddMenu",
    "KDrawStatus",
    "KParse",
    "KSaid",
    "KSetSynonyms",
    "KHaveMouse",
    "KSetCursor",
    "KSaveGame",
    "KRestoreGame",
    "KRestartGame",
    "KGameIsRestarting",
    "KDoSound",
    "KNewList",
    "KDisposeList",
    "KNewNode",
    "KFirstNode",
    "KLastNode",
    "KEmptyList",
    "KNextNode",
    "KPrevNode",
    "KNodeValue",
    "KAddAfter",
    "KAddToFront",
    "KAddToEnd",
    "KFindKey",
    "KDeleteKey",
    "KRandom",
    "KAbs",
    "KSqrt",
    "KGetAngle",
    "KGetDistance",
    "KWait",
    "KGetTime",
    "KStrEnd",
    "KStrCat",
    "KStrCmp",
    "KStrLen",
    "KStrCpy",
    "KFormat",
    "KGetFarText",
    "KReadNumber",
    "KBaseSetter",
    "KDirLoop",
    "KCantBeHere",
    "KOnControl",
    "KInitBresen",
    "KDoBresen",
    "KDoAvoider",
    "KSetJump",
    "KSetDebug",
    "KInspectObj",
    "KShowSends",
    "KShowObjs",
    "KShowFree",
    "KMemoryInfo",
    "KStackUsage",
    "KProfiler",
    "KGetMenu",
    "KSetMenu",
    "KGetSaveFiles",
    "KGetCWD",
    "KCheckFreeSpace",
    "KValidPath",
    "KCoordPri",
    "KStrAt",
    "KDeviceInfo",
    "KGetSaveDir",
    "KCheckSaveGame",
    "KShakeScreen",
    "KFlushResources",
    "KSinMult",
    "KCosMult",
    "KSinDiv",
    "KCosDiv",
    "KGraph",
    "KJoystick",
    "KShiftScreen",
    "KPalette",
    "KMemorySegment",
    "KIntersections",
    "KMemory",
    "KListOps",
    "KFileIO",
    "KDoAudio",
    "KDoSync",
    "KAvoidPath",
    "KSort",
    "KATan",
    "KLock",
    "KStrSplit",
    "KMessage",
    "KIsItSkip"
};


PMachineInterpreter::PMachineInterpreter(Script &script) :
    m_script(script),
    m_ctx(script.getModule()->getContext()),
    m_pc(NULL),
    m_acc(NULL),
    m_accAddr(NULL),
    m_paccAddr(NULL),
    m_bb(NULL),
    m_temp(NULL),
    m_self(NULL),
    m_argc(NULL),
    m_numArgs(0)
{
    m_funcPush = script.getModule()->getFunction("@push");
    m_funcPop = script.getModule()->getFunction("@pop");
}


Function* PMachineInterpreter::createFunction(const uint8_t *code, uint id)
{
    m_pc = code;
    m_acc = NULL;
    m_accAddr = NULL;
    m_paccAddr = NULL;
    m_temp = NULL;

    if (id != (uint)-1)
    {
        m_self = new Argument(g_sizeTy, "self");
        m_args.push_back(m_self);
        m_numArgs++;
    }

    m_argc = new Argument(g_sizeTy, "argc");
    m_args.push_back(m_argc);
    m_numArgs++;

    getBasicBlock(code, "entry");
    processBasicBlocks();

    Type **argTypes = (Type **)alloca(sizeof(Type*) * m_numArgs);
    Type **argTy = argTypes;
    for (auto i = m_args.begin(), e = m_args.end(); i != e; ++i, ++argTy)
    {
        *argTy = i->getType();
    }

    FunctionType *funcTy = FunctionType::get(g_sizeTy, makeArrayRef(argTypes, m_numArgs), true);
    Function *func = Function::Create(funcTy, Function::LinkOnceODRLinkage, "", getModule());

    auto i2 = func->arg_begin();
    for (auto i = m_args.begin(), e = m_args.end(); i != e; ++i)
    {
        i->replaceAllUsesWith(&*i2);
        i2->takeName(&*i);
    }

    m_args.clear();
    m_numArgs = 0;
    m_self = NULL;
    m_argc = NULL;

    for (auto i = m_labels.begin(), e = m_labels.end(); i != e; ++i)
    {
        i->second->insertInto(func);
    }
    return func;
}


void PMachineInterpreter::processBasicBlocks()
{
    assert(m_worklist.size() == 1);

    m_bb = m_worklist.back();
    m_worklist.pop_back();

    m_accAddr = new AllocaInst(g_sizeTy, "acc.addr", m_bb);
    m_paccAddr = new AllocaInst(g_sizeTy, "prev.addr", m_bb);
    while (processNextInstruction())
        ;

    while (!m_worklist.empty())
    {
        m_bb = m_worklist.back();
        m_worklist.pop_back();

        m_acc = loadAcc();
        while (processNextInstruction())
            ;
    }
}


void AddArgToFunction(Function *func, ArrayRef<Type*> params)
{
//     assert(!func->isVarArg());
// 
//     FunctionType *funcTy = cast<FunctionType>(func->getType());
// 
//     uint num = funcTy->getNumParams();
//     uint newNum = num + params.size();
//     std::unique_ptr<Type*[]> newParams(new Type*[newNum]);
//     memcpy(newParams.get(), funcTy->param_begin(), num * sizeof(Type*));
//     memcpy(newParams.get() + num, params.data(), params.size() * sizeof(Type*));
// 
//     FunctionType *newFuncTy = FunctionType::get(func->getReturnType(), makeArrayRef(newParams.get(), newNum), false);
//     Function *newFunc = Function::Create(newFuncTy, func->getLinkage(),);

    // Since we have now created the new function, splice the body of the old
    // function right into the new function, leaving the old rotting hulk of the
    // function empty.
//     NF->getBasicBlockList().splice(NF->begin(), F->getBasicBlockList());
// 
//     // Loop over the argument list, transferring uses of the old arguments over to
//     // the new arguments, also transferring over the names as well.
//     //
//     for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(),
//         I2 = NF->arg_begin(); I != E; ++I) {
//         if (!ArgsToPromote.count(&*I) && !ByValArgsToTransform.count(&*I)) {
//             // If this is an unmodified argument, move the name and users over to the
//             // new version.
//             I->replaceAllUsesWith(&*I2);
//             I2->takeName(&*I);
//             ++I2;
//             continue;
//         }

}


llvm::BasicBlock* PMachineInterpreter::getBasicBlock(const uint8_t *label, const llvm::StringRef &name)
{
    BasicBlock *&bb = m_labels[label];
    if (bb == NULL)
    {
        bb = BasicBlock::Create(m_ctx, name);
        m_worklist.push_back(bb);
    }
    return bb;
}


Function* PMachineInterpreter::getKernelFunction(uint id)
{
    if (id >= ARRAYSIZE(s_kernelNames))
    {
        return NULL;
    }

    Function* func = getModule()->getFunction(s_kernelNames[id]);
    if (func == NULL)
    {
        FunctionType *funcTy = FunctionType::get(g_sizeTy, g_sizeTy, true);
        func = Function::Create(funcTy, Function::ExternalLinkage, s_kernelNames[id], getModule());
        func->arg_begin()->setName("argc");
    }
    return func;
}


bool PMachineInterpreter::processNextInstruction()
{
    assert(m_pc != NULL);
    assert(m_bb != NULL);

    uint8_t opcode = getByte();
    if ((opcode & 0x80) != 0)
    {
        return ldstOp(opcode);
    }
    return (this->*s_opTable[opcode >> 1])(opcode);
}


void PMachineInterpreter::fixAcc()
{
    if (m_acc != NULL && m_acc->getType()->isIntegerTy(1))
    {
        m_acc = new ZExtInst(m_acc, g_sizeTy, "", m_bb);
    }
}


void PMachineInterpreter::storeAcc()
{
    Value *prev = m_acc;
    fixAcc();
    new StoreInst(m_acc, m_accAddr, m_bb);
    m_acc = prev;
}


Value* PMachineInterpreter::loadAcc()
{
    return new LoadInst(m_accAddr, "", m_bb);
}


void PMachineInterpreter::storePrevAcc()
{
    Value *prev = m_acc;
    fixAcc();
    new StoreInst(m_acc, m_paccAddr, m_bb);
    m_acc = prev;
}


Value* PMachineInterpreter::loadPrevAcc()
{
    return new LoadInst(m_paccAddr, "", m_bb);
}


bool PMachineInterpreter::bnotOp(uint8_t opcode)
{
    fixAcc();
    m_acc = BinaryOperator::CreateNot(m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::addOp(uint8_t opcode)
{
    fixAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = BinaryOperator::CreateAdd(tos, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::subOp(uint8_t opcode)
{
    fixAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = BinaryOperator::CreateSub(tos, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::mulOp(uint8_t opcode)
{
    fixAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = BinaryOperator::CreateMul(tos, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::divOp(uint8_t opcode)
{
    fixAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = BinaryOperator::CreateSDiv(tos, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::modOp(uint8_t opcode)
{
    fixAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = BinaryOperator::CreateSRem(tos, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::shrOp(uint8_t opcode)
{
    fixAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = BinaryOperator::CreateLShr(tos, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::shlOp(uint8_t opcode)
{
    fixAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = BinaryOperator::CreateShl(tos, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::xorOp(uint8_t opcode)
{
    fixAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = BinaryOperator::CreateXor(tos, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::andOp(uint8_t opcode)
{
    fixAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = BinaryOperator::CreateAnd(tos, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::orOp(uint8_t opcode)
{
    fixAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = BinaryOperator::CreateOr(tos, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::negOp(uint8_t opcode)
{
    fixAcc();
    m_acc = BinaryOperator::CreateNeg(m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::notOp(uint8_t opcode)
{
    if (m_acc->getType()->isIntegerTy(1))
    {
        m_acc = BinaryOperator::CreateNot(m_acc, "not", m_bb);
    }
    else
    {
        m_acc = new ICmpInst(*m_bb, CmpInst::ICMP_EQ, m_acc, ConstantInt::get(g_sizeTy, 0));
    }
    return true;
}


bool PMachineInterpreter::cmpOp(uint8_t opcode)
{
    static CmpInst::Predicate s_cmpTable[] = {
        CmpInst::ICMP_EQ,
        CmpInst::ICMP_NE,
        CmpInst::ICMP_SGT,
        CmpInst::ICMP_SGE,
        CmpInst::ICMP_SLT,
        CmpInst::ICMP_SLE,
        CmpInst::ICMP_UGT,
        CmpInst::ICMP_UGE,
        CmpInst::ICMP_ULT,
        CmpInst::ICMP_ULE
    };

    CmpInst::Predicate pred = s_cmpTable[(opcode >> 1) - (OP_eq >> 1)];

    fixAcc();
    storePrevAcc();
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    m_acc = new ICmpInst(*m_bb, pred, tos, m_acc);
    return true;
}


bool PMachineInterpreter::btOp(uint8_t opcode)
{
    storeAcc();
    if (!m_acc->getType()->isIntegerTy(1))
    {
        m_acc = new ICmpInst(*m_bb, CmpInst::ICMP_NE, m_acc, ConstantInt::get(g_sizeTy, 0));
    }

    const uint8_t *pc = m_pc;
    BasicBlock *ifTrue = getBasicBlock(pc + getSInt(opcode), "if.then");
    BasicBlock *ifFalse = getBasicBlock(m_pc, "if.else");

    if (opcode <= OP_bt_ONE)
    {
        BranchInst::Create(ifTrue, ifFalse, m_acc, m_bb);
    }
    else
    {
        BranchInst::Create(ifFalse, ifTrue, m_acc, m_bb);
    }
    return false;
}


bool PMachineInterpreter::jmpOp(uint8_t opcode)
{
    storeAcc();
    const uint8_t *pc = m_pc;
    BasicBlock *dst = getBasicBlock(pc + getSInt(opcode), "if.end");
    BranchInst::Create(dst, m_bb);
    return false;
}


bool PMachineInterpreter::loadiOp(uint8_t opcode)
{
    m_acc = ConstantInt::get(g_sizeTy, getSInt(opcode), true);
    return true;
}


bool PMachineInterpreter::pushOp(uint8_t opcode)
{
    fixAcc();
    CallInst::Create(m_funcPush, m_acc, "", m_bb);
    return true;
}


bool PMachineInterpreter::pushiOp(uint8_t opcode)
{
    Value *imm = ConstantInt::get(g_sizeTy, getSInt(opcode), true);
    CallInst::Create(m_funcPush, imm, "", m_bb);
    return true;
}


bool PMachineInterpreter::tossOp(uint8_t opcode)
{
    m_acc = CallInst::Create(m_funcPop, "", m_bb);
    return true;
}


bool PMachineInterpreter::dupOp(uint8_t opcode)
{
    //TODO: Maybe we can do this more efficient?
    Value *tos = CallInst::Create(m_funcPop, "", m_bb);
    CallInst::Create(m_funcPush, tos, "", m_bb);
    CallInst::Create(m_funcPush, tos, "", m_bb);
    return true;
}


bool PMachineInterpreter::linkOp(uint8_t opcode)
{
    assert(m_temp == NULL);
    uint count = getUInt(opcode);
    m_temp = new AllocaInst(ArrayType::get(g_sizeTy, count), NULL, g_sizeTy->getBitWidth() / 8, "temp", m_bb);
    return true;
}


bool PMachineInterpreter::callOp(uint8_t opcode)
{
    uintptr_t offset = getSInt(opcode);
    uint paramsByteCount = getByte();
    uint8_t *retAttr = g_pc;
    g_pc += offset;
    DoCall(paramsByteCount / sizeof(uint16_t));
    g_pc = retAttr;
    return true;
}


bool PMachineInterpreter::callkOp(uint8_t opcode)
{
    // We currently do not support "rest" arguments for kernel calls;
    assert(!m_restArgs);

    uint kernelNum = getUInt(opcode);
    uint numArgs = getByte() / sizeof(uint16_t);
    Value *restArgc; // Load  RestArgc
    // store 0 into RestArgc
    CallInst::Create(m_funcPush, tos, "", m_bb);
    return true;
}


bool PMachineInterpreter::callbOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::calleOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::retOp(uint8_t opcode)
{
    Value *val = m_acc;
    if (val == NULL)
    {
        val = ConstantInt::get(g_sizeTy, 0);
    }
    ReturnInst::Create(m_ctx, val, m_bb);
    return false;
}


bool PMachineInterpreter::sendOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::classOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::selfOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::superOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::restOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::leaOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::selfIdOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::pprevOp(uint8_t opcode)
{
    Value *prev = loadPrevAcc();
    CallInst::Create(m_funcPush, prev, "", m_bb);
    return true;
}


bool PMachineInterpreter::pToaOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::aTopOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::pTosOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::sTopOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::ipToaOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::dpToaOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::ipTosOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::dpTosOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::lofsaOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::lofssOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::push0Op(uint8_t opcode)
{
    Value *val = ConstantInt::get(g_sizeTy, 0);
    CallInst::Create(m_funcPush, val, "", m_bb);
    return true;
}


bool PMachineInterpreter::push1Op(uint8_t opcode)
{
    Value *val = ConstantInt::get(g_sizeTy, 1);
    CallInst::Create(m_funcPush, val, "", m_bb);
    return true;
}


bool PMachineInterpreter::push2Op(uint8_t opcode)
{
    Value *val = ConstantInt::get(g_sizeTy, 2);
    CallInst::Create(m_funcPush, val, "", m_bb);
    return true;
}


bool PMachineInterpreter::pushSelfOp(uint8_t opcode)
{
    CallInst::Create(m_funcPush, m_self, "", m_bb);
    return true;
}


bool PMachineInterpreter::ldstOp(uint8_t opcode)
{
    return true;
}


bool PMachineInterpreter::badOp(uint8_t opcode)
{
    return false;
}
