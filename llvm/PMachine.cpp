#include "PMachine.hpp"
#include "World.hpp"
#include "Class.hpp"
#include "Resource.hpp"

using namespace llvm;


BEGIN_NAMESPACE_SCI


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


PMachine::PfnOp PMachine::s_opTable[] = {
    &PMachine::bnotOp,
    &PMachine::addOp,
    &PMachine::subOp,
    &PMachine::mulOp,
    &PMachine::divOp,
    &PMachine::modOp,
    &PMachine::shrOp,
    &PMachine::shlOp,
    &PMachine::xorOp,
    &PMachine::andOp,
    &PMachine::orOp,
    &PMachine::negOp,
    &PMachine::notOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::btOp,
    &PMachine::btOp,
    &PMachine::jmpOp,
    &PMachine::loadiOp,
    &PMachine::pushOp,
    &PMachine::pushiOp,
    &PMachine::tossOp,
    &PMachine::dupOp,
    &PMachine::linkOp,
    &PMachine::callOp,
    &PMachine::callkOp,
    &PMachine::callbOp,
    &PMachine::calleOp,
    &PMachine::retOp,
    &PMachine::sendOp,
    &PMachine::badOp,
    &PMachine::badOp,
    &PMachine::classOp,
    &PMachine::badOp,
    &PMachine::selfOp,
    &PMachine::superOp,
    &PMachine::restOp,
    &PMachine::leaOp,
    &PMachine::selfIdOp,
    &PMachine::badOp,
    &PMachine::pprevOp,
    &PMachine::pToaOp,
    &PMachine::aTopOp,
    &PMachine::pTosOp,
    &PMachine::sTopOp,
    &PMachine::ipToaOp,
    &PMachine::dpToaOp,
    &PMachine::ipTosOp,
    &PMachine::dpTosOp,
    &PMachine::lofsaOp,
    &PMachine::lofssOp,
    &PMachine::push0Op,
    &PMachine::push1Op,
    &PMachine::push2Op,
    &PMachine::pushSelfOp,
    &PMachine::badOp
};


const char* PMachine::s_kernelNames[] = {
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


PMachine::PMachine(Script &script) :
    m_script(script),
    m_ctx(script.getModule()->getContext()),
    m_sizeTy(GetWorld().getSizeType()),
    m_funcCallIntrin(nullptr),
    m_pc(nullptr),
    m_acc(nullptr),
    m_accAddr(nullptr),
    m_paccAddr(nullptr),
    m_bb(nullptr),
    m_entry(nullptr),
    m_temp(nullptr),
    m_tempCount(0),
    m_rest(nullptr),
    m_retTy(nullptr),
    m_argc(nullptr),
    m_vaListIndex(0),
    m_param1(nullptr),
    m_paramCount(0)
{
    Intrinsic::Get(Intrinsic::push);
    Intrinsic::Get(Intrinsic::pop);
    Intrinsic::Get(Intrinsic::rest);
    Intrinsic::Get(Intrinsic::clss);
    Intrinsic::Get(Intrinsic::objc);
    Intrinsic::Get(Intrinsic::prop);
    Intrinsic::Get(Intrinsic::send);
    Intrinsic::Get(Intrinsic::call);
    Intrinsic::Get(Intrinsic::calle);
    Intrinsic::Get(Intrinsic::callk);
}


Function* PMachine::interpretFunction(const uint8_t *code, StringRef name, uint id)
{
    m_acc = nullptr;
    m_accAddr = nullptr;
    m_paccAddr = nullptr;
    m_temp = nullptr;
    m_tempCount = 0;
    m_retTy = nullptr;
    m_self.release();
    m_vaList.release();
    m_argc = nullptr;
    m_param1 = nullptr;

    if (id != (uint)-1)
    {
        m_self.reset(new Argument(m_sizeTy, "self"));
    }

    m_entry = getBasicBlock(code, "entry");
    processBasicBlocks();

    if (m_retTy == nullptr)
    {
        m_retTy = Type::getVoidTy(m_ctx);
    }

    if (m_accAddr != nullptr && m_accAddr->use_empty())
    {
        m_accAddr->eraseFromParent();
    }
    if (m_paccAddr != nullptr && m_paccAddr->use_empty())
    {
        m_paccAddr->eraseFromParent();
    }

    uint paramCount = getParamCount();
    uint totalArgCount = paramCount;
    if (m_self)
    {
        totalArgCount++;
    }
    if (m_argc != nullptr)
    {
        totalArgCount++;
    }
    if (m_vaList)
    {
        totalArgCount++;
    }

    Type **argTypes = reinterpret_cast<Type **>(alloca(sizeof(Type *) * totalArgCount));
    Type **argTy = argTypes;
    if (m_self)
    {
        *argTy++ = m_self->getType();
    }
    if (m_argc != nullptr)
    {
        *argTy++ = m_sizeTy;
    }
    for (uint i = 0, n = paramCount; i < n; ++i)
    {
        *argTy++ = m_sizeTy;
    }
    if (m_vaList)
    {
        *argTy++ = m_vaList->getType();
    }

    FunctionType *funcTy = FunctionType::get(m_retTy, makeArrayRef(argTypes, totalArgCount), false);
    Function *func = Function::Create(funcTy, Function::LinkOnceODRLinkage, name, getModule());

    Instruction *instAfterAlloca = nullptr;
    if (m_param1 != nullptr || m_argc != nullptr)
    {
        instAfterAlloca = (m_param1 != nullptr) ? m_param1 : m_argc;
        do 
        {
            instAfterAlloca = instAfterAlloca->getNextNode();
        }
        while (isa<AllocaInst>(instAfterAlloca));
    }

    auto iArg = func->arg_begin();
    if (m_self)
    {
        m_self->replaceAllUsesWith(&*iArg);
        iArg->takeName(m_self.get());
        ++iArg;
    }
    if (m_argc != nullptr)
    {
        new StoreInst(&*iArg, m_argc, instAfterAlloca);
        iArg->setName("argc");
        ++iArg;
    }
    if (m_param1 != nullptr)
    {
        Instruction *param = m_param1;
        uint i = 1, n;
        for (n = paramCount; i <= n; ++i)
        {
            new StoreInst(&*iArg, param, instAfterAlloca);
            iArg->setName(std::string("param") + utostr_32(i));
            ++iArg;
            param = param->getNextNode();
        }

        if (i <= m_paramCount)
        {
            for (n = m_paramCount; i <= n; ++i)
            {
                Value *val = GetElementPtrInst::CreateInBounds(&*iArg, ConstantInt::get(m_sizeTy, i - m_paramCount), "", instAfterAlloca);
                val = new LoadInst(val, std::string("param") + utostr_32(i), instAfterAlloca);
                new StoreInst(val, param, instAfterAlloca);
                param = param->getNextNode();
            }
        }
    }
    if (m_vaList)
    {
        m_vaList->replaceAllUsesWith(&*iArg);
        iArg->takeName(m_vaList.get());
        ++iArg;
    }

    for (auto i = m_labels.begin(), e = m_labels.end(); i != e; ++i)
    {
        i->second->insertInto(func);
    }
    return func;
}


void PMachine::processBasicBlocks()
{
    assert(m_worklist.size() == 1);

    auto item = m_worklist.back();
    m_worklist.pop_back();
    m_pc = item.first;
    m_bb = item.second;

    m_rest = nullptr;
    m_accAddr = new AllocaInst(m_sizeTy, "acc.addr", m_bb);
    m_paccAddr = new AllocaInst(m_sizeTy, "prev.addr", m_bb);
    while (processNextInstruction())
        ;
    assert(m_rest == nullptr);

    while (!m_worklist.empty())
    {
        item = m_worklist.back();
        m_worklist.pop_back();
        m_pc = item.first;
        m_bb = item.second;

        m_rest = nullptr;
        m_acc = loadAcc();
        while (processNextInstruction())
            ;
        assert(m_rest == nullptr);
    }
}


BasicBlock* PMachine::getBasicBlock(const uint8_t *label, StringRef name)
{
    uint offset = m_script.getOffsetOf(label);
    BasicBlock *&bb = m_labels[offset];
    if (bb == nullptr)
    {
        std::string fullName = name;
        fullName += '@';
        fullName += utohexstr(offset, true);
        bb = BasicBlock::Create(m_ctx, fullName);
        m_worklist.push_back(std::make_pair(label, bb));
    }
    return bb;
}


uint PMachine::getParamCount() const
{
    return m_vaList ? m_vaListIndex - 1 : m_paramCount;
}


Function* PMachine::getCallIntrinsic()
{
    if (m_funcCallIntrin == nullptr)
    {
        Function *funcCallIntrin = Intrinsic::Get(Intrinsic::call);
        m_funcCallIntrin = cast<Function>(m_script.getModule()->getOrInsertFunction(funcCallIntrin->getName(), funcCallIntrin->getFunctionType()));
    }
    return m_funcCallIntrin;
}


Function* PMachine::getKernelFunction(uint id)
{
    if (id >= ARRAYSIZE(s_kernelNames))
    {
        return nullptr;
    }

    Function* func = getModule()->getFunction(s_kernelNames[id]);
    if (func == nullptr)
    {
        FunctionType *funcTy = FunctionType::get(m_sizeTy, m_sizeTy, true);
        func = Function::Create(funcTy, Function::ExternalLinkage, s_kernelNames[id], getModule());
        func->arg_begin()->setName("argc");
    }
    return func;
}


void PMachine::emitSend(Value *obj)
{
    uint paramCount = getByte() / sizeof(uint16_t);
    if (paramCount == 0)
    {
        return;
    }

    SmallVector<Value *, 16> args;

    uint restParam = (m_rest != nullptr) ? 1 : 0;
    args.resize(paramCount + 1 + restParam);
    args[0] = obj;
    for (; paramCount != 0; --paramCount)
    {
        args[paramCount] = PopInst::Create(m_bb);
    }

    if (m_rest != nullptr)
    {
        args.back() = m_rest;
        m_rest = nullptr;
    }

    m_acc = SendMessageInst::Create(args, m_bb);
}


void PMachine::emitCall(Function *func, ArrayRef<Constant*> constants)
{
    uint paramCount = getByte() / sizeof(uint16_t);

    SmallVector<Value *, 16> args;

    uint restParam = (m_rest != nullptr) ? 1 : 0;
    uint constCount = constants.size();
    args.resize(paramCount + 1 + constCount + restParam);

    for (uint i = 0; i < constCount; ++i)
    {
        args[i] = constants[i];
    }
    for (paramCount += constCount; paramCount >= constCount; --paramCount)
    {
        args[paramCount] = PopInst::Create(m_bb);
    }

    if (m_rest != nullptr)
    {
        args.back() = m_rest;
        m_rest = nullptr;
    }

    m_acc = CallInst::Create(func, args, "", m_bb);
}


Value* PMachine::getIndexedPropPtr(uint8_t opcode)
{
    uint idx = getUInt(opcode) / sizeof(uint16_t);

#if 0
    Type *i32Ty = Type::getInt32Ty(m_ctx);

    // Create indices into the Class Abstract Type.
    uint idxCount = 0;
    Value *indices[3];
    indices[idxCount++] = ConstantInt::get(i32Ty, 0);
    if (idx < ObjRes::VALUES_OFFSET)
    {
        indices[idxCount++] = ConstantInt::get(i32Ty, idx + 1);
    }
    else
    {
        indices[idxCount++] = ConstantInt::get(i32Ty, ObjRes::VALUES_OFFSET + 1);
        indices[idxCount++] = ConstantInt::get(m_sizeTy, idx - ObjRes::VALUES_OFFSET);
    }

    Value *ptr = new IntToPtrInst(m_self.get(), Class::GetAbstractType()->getPointerTo(), "", m_bb);
    return GetElementPtrInst::CreateInBounds(ptr, makeArrayRef(indices, idxCount), GetWorld().getSelectorName(idx), m_bb);
#endif

    ConstantInt *c = ConstantInt::get(m_sizeTy, idx);
    return PropertyInst::Create(c, m_bb);
}


Value* PMachine::getValueByOffset(uint8_t opcode)
{
#if defined __WINDOWS__ || 1
    uint offset = getUInt(opcode);
#else
#error Not implemented
#endif

    Value *val = m_script.getRelocatedValue(offset);
    if (val != nullptr)
    {
        Instruction *inst = dyn_cast<Instruction>(val);
        if (inst != nullptr)
        {
            inst = inst->clone();
            m_bb->getInstList().push_back(inst);
            val = inst;
        }
    }
    return val;
}


Value* PMachine::getIndexedVariablePtr(uint8_t opcode, uint idx)
{
    if ((opcode & OP_INDEX) != 0)
    {
        castAccToSizeType();
    }

    Value *var;
    switch (opcode & OP_VAR)
    {
    default:
    case OP_GLOBAL:
        var = m_script.getGlobalVariable(idx);
        break;

    case OP_LOCAL:
        var = m_script.getLocalVariable(idx);
        break;

    case OP_TMP: {
        assert(idx < m_tempCount);
        Value *indices[] = {
            ConstantInt::get(m_sizeTy, 0),
            ConstantInt::get(m_sizeTy, idx)
        };
        var = GetElementPtrInst::CreateInBounds(m_temp, indices);
        break;
    }

    case OP_PARM:
        assert(0);
    }

    m_bb->getInstList().push_back(static_cast<Instruction*>(var));

    // If the variable is indexed, add the index in acc to the offset.
    if ((opcode & OP_INDEX) != 0)
    {
        var = GetElementPtrInst::CreateInBounds(var, m_acc, "", m_bb);
    }
    return var;
}


Instruction* PMachine::getParameter(uint idx)
{
    Instruction *param = nullptr;

    // If idx is 0, then this is the argc (meta-param).
    if (idx == 0)
    {
        if (m_argc == nullptr)
        {
            m_argc = new AllocaInst(m_sizeTy, "argc.addr");
            m_entry->getInstList().push_front(m_argc);
        }
        param = m_argc;
    }
    else if (idx <= m_paramCount)
    {
        param = m_param1;
        while (idx != 1)
        {
            idx--;
            param = param->getNextNode();
        }
    }
    // If the param is not created yes, then create it (and the preceding params).
    else
    {
        auto instAnchor = m_entry->begin();
        if (m_param1 == nullptr)
        {
            if (m_argc != nullptr)
            {
                instAnchor = m_argc->getIterator();
                ++instAnchor;
            }

            m_param1 = new AllocaInst(m_sizeTy, "param1.addr");
            m_entry->getInstList().insert(instAnchor, m_param1);
            m_paramCount++;
        }
        else
        {
            instAnchor = m_param1->getIterator();
            for (uint i = 0, n = m_paramCount; i < n; ++i)
            {
                ++instAnchor;
            }
        }

        param = m_param1;
        while (m_paramCount < idx)
        {
            m_paramCount++;
            param = new AllocaInst(m_sizeTy, std::string("param") + utostr_32(m_paramCount) + ".addr");
            m_entry->getInstList().insert(instAnchor, param);
        }
    }
    return param;
}


Argument* PMachine::getVaList(uint idx)
{
    // idx cannot be 0, as this is the argc (meta-param)!
    assert(idx != 0);

    // We currently do not support moving the va_list.
    assert(m_vaListIndex <= idx || !m_vaList);

    // There can be only one va_list!
    if (!m_vaList)
    {
        if (idx > 1)
        {
            // If idx is not the last one then make it (by adding the preceding params).
            getParameter(idx - 1);
        }

        m_vaList.reset(new Argument(m_sizeTy->getPointerTo(), "vaList"));
        m_vaListIndex = idx;
    }
    return m_vaList.get();
}


bool PMachine::processNextInstruction()
{
    assert(m_pc != nullptr);
    assert(m_bb != nullptr);

    uint8_t opcode = getByte();
    if ((opcode & 0x80) != 0)
    {
        return ldstOp(opcode);
    }
    return (this->*s_opTable[opcode >> 1])(opcode);
}


Value* PMachine::castValueToSizeType(Value *val, BasicBlock *bb)
{
    if (val != nullptr)
    {
        if (val->getType()->isIntegerTy())
        {
            unsigned srcBits = val->getType()->getScalarSizeInBits();
            unsigned dstBits = m_sizeTy->getScalarSizeInBits();
            if (srcBits != dstBits)
            {
                val = new ZExtInst(val, m_sizeTy, "", bb);
            }
        }
        else if (val->getType()->isPointerTy())
        {
            val = new PtrToIntInst(val, m_sizeTy, "", bb);
        }
    }
    return val;
}


Value* PMachine::castValueToSizeType(Value *val)
{
    return castValueToSizeType(val, m_bb);
}


void PMachine::castAccToSizeType()
{
    m_acc = castValueToSizeType(m_acc);
}


void PMachine::storeAcc()
{
    Value *val = castValueToSizeType(m_acc);
    new StoreInst(val, m_accAddr, m_bb);
}


Value* PMachine::loadAcc()
{
    return new LoadInst(m_accAddr, "", m_bb);
}


void PMachine::storePrevAcc()
{
    Value *val = castValueToSizeType(m_acc);
    new StoreInst(val, m_paccAddr, m_bb);
}


Value* PMachine::loadPrevAcc()
{
    return new LoadInst(m_paccAddr, "", m_bb);
}


Value* PMachine::callPop()
{
    return PopInst::Create(m_bb);
}


void PMachine::callPush(llvm::Value *val)
{
    PushInst::Create(val, m_bb);
}


bool PMachine::bnotOp(uint8_t opcode)
{
    castAccToSizeType();
    m_acc = BinaryOperator::CreateNot(m_acc, "", m_bb);
    return true;
}


bool PMachine::addOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *tos = callPop();
    m_acc = BinaryOperator::CreateAdd(tos, m_acc, "", m_bb);
    return true;
}


bool PMachine::subOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *tos = callPop();
    m_acc = BinaryOperator::CreateSub(tos, m_acc, "", m_bb);
    return true;
}


bool PMachine::mulOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *tos = callPop();
    m_acc = BinaryOperator::CreateMul(tos, m_acc, "", m_bb);
    return true;
}


bool PMachine::divOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *tos = callPop();
    m_acc = BinaryOperator::CreateSDiv(tos, m_acc, "", m_bb);
    return true;
}


bool PMachine::modOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *tos = callPop();
    m_acc = BinaryOperator::CreateSRem(tos, m_acc, "", m_bb);
    return true;
}


bool PMachine::shrOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *tos = callPop();
    m_acc = BinaryOperator::CreateLShr(tos, m_acc, "", m_bb);
    return true;
}


bool PMachine::shlOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *tos = callPop();
    m_acc = BinaryOperator::CreateShl(tos, m_acc, "", m_bb);
    return true;
}


bool PMachine::xorOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *tos = callPop();
    m_acc = BinaryOperator::CreateXor(tos, m_acc, "", m_bb);
    return true;
}


bool PMachine::andOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *tos = callPop();
    m_acc = BinaryOperator::CreateAnd(tos, m_acc, "", m_bb);
    return true;
}


bool PMachine::orOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *tos = callPop();
    m_acc = BinaryOperator::CreateOr(tos, m_acc, "", m_bb);
    return true;
}


bool PMachine::negOp(uint8_t opcode)
{
    castAccToSizeType();
    m_acc = BinaryOperator::CreateNeg(m_acc, "", m_bb);
    return true;
}


bool PMachine::notOp(uint8_t opcode)
{
    if (m_acc->getType()->isIntegerTy(1))
    {
        m_acc = BinaryOperator::CreateNot(m_acc, "not", m_bb);
    }
    else
    {
        m_acc = new ICmpInst(*m_bb, CmpInst::ICMP_EQ, m_acc, ConstantInt::get(m_acc->getType(), 0));
    }
    return true;
}


bool PMachine::cmpOp(uint8_t opcode)
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

    castAccToSizeType();
    storePrevAcc();
    Value *tos = callPop();
    m_acc = new ICmpInst(*m_bb, pred, tos, m_acc);
    return true;
}


bool PMachine::btOp(uint8_t opcode)
{
    storeAcc();
    if (!m_acc->getType()->isIntegerTy(1))
    {
        m_acc = new ICmpInst(*m_bb, CmpInst::ICMP_NE, m_acc, Constant::getNullValue(m_acc->getType()));
    }

    int offset = getSInt(opcode);
    BasicBlock *ifTrue = getBasicBlock(m_pc + offset);
    BasicBlock *ifFalse = getBasicBlock(m_pc);

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


bool PMachine::jmpOp(uint8_t opcode)
{
    storeAcc();
    int offset = getSInt(opcode);
    BasicBlock *dst = getBasicBlock(m_pc + offset);
    BranchInst::Create(dst, m_bb);
    return false;
}


bool PMachine::loadiOp(uint8_t opcode)
{
    m_acc = GetWorld().getConstantValue(static_cast<int16_t>(getSInt(opcode)));
    return true;
}


bool PMachine::pushOp(uint8_t opcode)
{
    castAccToSizeType();
    callPush(m_acc);
    return true;
}


bool PMachine::pushiOp(uint8_t opcode)
{
    Value *imm = GetWorld().getConstantValue(static_cast<int16_t>(getSInt(opcode)));
    callPush(imm);
    return true;
}


bool PMachine::tossOp(uint8_t opcode)
{
    callPop();
    return true;
}


bool PMachine::dupOp(uint8_t opcode)
{
    //TODO: Maybe we can do this more efficient?
    Value *tos = callPop();
    callPush(tos);
    callPush(tos);
    return true;
}


bool PMachine::linkOp(uint8_t opcode)
{
    assert(m_temp == nullptr && m_tempCount == 0);
    m_tempCount = getUInt(opcode);
    m_temp = new AllocaInst(ArrayType::get(m_sizeTy, m_tempCount), "temp", m_bb);
    m_temp->setAlignment(m_sizeTy->getBitWidth() / 8);
    return true;
}


bool PMachine::callOp(uint8_t opcode)
{
    int offset = getSInt(opcode);
    uint absOffset = m_script.getOffsetOf((m_pc + 1) + offset);
    Constant *constants[] = {
        ConstantInt::get(m_sizeTy, absOffset)
    };
    emitCall(getCallIntrinsic(), constants);
    return true;
}


bool PMachine::callkOp(uint8_t opcode)
{
    uint kernelOrdinal = getUInt(opcode);

    Constant *constants[] = {
        ConstantInt::get(m_sizeTy, kernelOrdinal)
    };
    emitCall(Intrinsic::Get(Intrinsic::callk), constants);
    return true;
}


bool PMachine::callbOp(uint8_t opcode)
{
    uint entryIndex = getUInt(opcode);

    Constant *constants[] = {
        ConstantInt::get(m_sizeTy, 0),
        ConstantInt::get(m_sizeTy, entryIndex)
    };
    emitCall(Intrinsic::Get(Intrinsic::calle), constants);
    return true;
}


bool PMachine::calleOp(uint8_t opcode)
{
    uint scriptId = getUInt(opcode);
    uint entryIndex = getUInt(opcode);

    Constant *constants[] = {
        ConstantInt::get(m_sizeTy, scriptId),
        ConstantInt::get(m_sizeTy, entryIndex)
    };
    emitCall(Intrinsic::Get(Intrinsic::calle), constants);
    return true;
}


bool PMachine::retOp(uint8_t opcode)
{
    if (m_acc == nullptr)
    {
        assert(m_retTy == nullptr || m_retTy->isVoidTy());
        if (m_retTy == nullptr)
        {
            m_retTy = Type::getVoidTy(m_ctx);
        }
        ReturnInst::Create(m_ctx, m_bb);
    }
    else
    {
        if (m_retTy == nullptr)
        {
            m_retTy = m_acc->getType();
        }
        else if (m_retTy != m_acc->getType())
        {
            assert(!m_retTy->isVoidTy());
            castAccToSizeType();
            m_retTy = m_sizeTy;

            // Cast all other return values to the same type.
            for (auto i = m_labels.begin(), e = m_labels.end(); i != e; ++i)
            {
                BasicBlock *bb = i->second;
                ReturnInst *retInst = dyn_cast_or_null<ReturnInst>(bb->getTerminator());
                if (retInst != nullptr)
                {
                    Value *val = retInst->getReturnValue();
                    retInst->eraseFromParent();
                    val = castValueToSizeType(val, bb);

                    ReturnInst::Create(m_ctx, val, bb);
                }
            }
        }
        ReturnInst::Create(m_ctx, m_acc, m_bb);
    }
    return false;
}


bool PMachine::sendOp(uint8_t opcode)
{
    assert(m_acc != nullptr);
    castAccToSizeType();
    emitSend(m_acc);
    return true;
}


bool PMachine::classOp(uint8_t opcode)
{
    uint classId = getUInt(opcode);
    ConstantInt *c = ConstantInt::get(m_sizeTy, classId);
    m_acc = ClassInst::Create(c, m_bb);
    return true;
}


bool PMachine::selfOp(uint8_t opcode)
{
    assert(m_self);
    emitSend(m_self.get());
    return true;
}


bool PMachine::superOp(uint8_t opcode)
{
    uint classId = getUInt(opcode);
    ConstantInt *cls = ConstantInt::get(m_sizeTy, classId);
    emitSend(cls);
    return true;
}


bool PMachine::restOp(uint8_t opcode)
{
    assert(m_rest == nullptr);
    uint paramIndex = getUInt(opcode);
    ConstantInt *c = ConstantInt::get(m_sizeTy, paramIndex);
    m_rest = RestInst::Create(c, m_bb);
    return true;
}


bool PMachine::leaOp(uint8_t opcode)
{
    // Get the type of the variable.
    uint varType = getUInt(opcode);
    // Get the number of the variable.
    uint varIdx = getUInt(opcode);

    m_acc = getIndexedVariablePtr(static_cast<uint8_t>(varType), varIdx);
    return true;
}


bool PMachine::selfIdOp(uint8_t opcode)
{
    assert(m_self);
    m_acc = m_self.get();
    return true;
}


bool PMachine::pprevOp(uint8_t opcode)
{
    Value *prev = loadPrevAcc();
    callPush(prev);
    return true;
}


bool PMachine::pToaOp(uint8_t opcode)
{
    Value *prop = getIndexedPropPtr(opcode);
    m_acc = new LoadInst(prop, "", m_bb);
    castAccToSizeType();
    return true;
}


bool PMachine::aTopOp(uint8_t opcode)
{
    castAccToSizeType();
    Value *prop = getIndexedPropPtr(opcode);
    new StoreInst(m_acc, prop, m_bb);
    return true;
}


bool PMachine::pTosOp(uint8_t opcode)
{
    Value *prop = getIndexedPropPtr(opcode);
    prop = new LoadInst(prop, "", m_bb);
    prop = castValueToSizeType(prop);
    callPush(prop);
    return true;
}


bool PMachine::sTopOp(uint8_t opcode)
{
    Value *val = callPop();
    Value *prop = getIndexedPropPtr(opcode);
    new StoreInst(val, prop, m_bb);
    return true;
}


bool PMachine::ipToaOp(uint8_t opcode)
{
    Value *prop = getIndexedPropPtr(opcode);
    Value *val = new LoadInst(prop, "", m_bb);
    m_acc = BinaryOperator::CreateAdd(val, ConstantInt::get(val->getType(), 1), "", m_bb);
    new StoreInst(m_acc, prop, m_bb);
    castAccToSizeType();
    return true;
}


bool PMachine::dpToaOp(uint8_t opcode)
{
    Value *prop = getIndexedPropPtr(opcode);
    Value *val = new LoadInst(prop, "", m_bb);
    m_acc = BinaryOperator::CreateSub(val, ConstantInt::get(val->getType(), 1), "", m_bb);
    new StoreInst(m_acc, prop, m_bb);
    castAccToSizeType();
    return true;
}


bool PMachine::ipTosOp(uint8_t opcode)
{
    Value *prop = getIndexedPropPtr(opcode);
    Value *val = new LoadInst(prop, "", m_bb);
    val = BinaryOperator::CreateAdd(val, ConstantInt::get(val->getType(), 1), "", m_bb);
    new StoreInst(val, prop, m_bb);
    val = castValueToSizeType(val);
    callPush(val);
    return true;
}


bool PMachine::dpTosOp(uint8_t opcode)
{
    Value *prop = getIndexedPropPtr(opcode);
    Value *val = new LoadInst(prop, "", m_bb);
    val = BinaryOperator::CreateSub(val, ConstantInt::get(m_sizeTy, 1), "", m_bb);
    new StoreInst(val, prop, m_bb);
    val = castValueToSizeType(val);
    callPush(val);
    return true;
}


bool PMachine::lofsaOp(uint8_t opcode)
{
    Value *val = getValueByOffset(opcode);
    assert(val != nullptr);

    m_acc = val;
    return true;
}


bool PMachine::lofssOp(uint8_t opcode)
{
    Value *val = getValueByOffset(opcode);
    assert(val != nullptr);

    callPush(val);
    return true;
}


bool PMachine::push0Op(uint8_t opcode)
{
    Value *val = ConstantInt::get(m_sizeTy, 0);
    callPush(val);
    return true;
}


bool PMachine::push1Op(uint8_t opcode)
{
    Value *val = ConstantInt::get(m_sizeTy, 1);
    callPush(val);
    return true;
}


bool PMachine::push2Op(uint8_t opcode)
{
    Value *val = ConstantInt::get(m_sizeTy, 2);
    callPush(val);
    return true;
}


bool PMachine::pushSelfOp(uint8_t opcode)
{
    callPush(m_self.get());
    return true;
}


bool PMachine::ldstOp(uint8_t opcode)
{
    uint varIdx = getUInt(opcode);

    Value *var, *val;
    if ((opcode & OP_VAR) == OP_PARM)
    {
        if ((opcode & OP_INDEX) != 0)
        {
            castAccToSizeType();
            var = getVaList(varIdx);
            var = GetElementPtrInst::CreateInBounds(var, m_acc, "", m_bb);
        }
        else
        {
            var = getParameter(varIdx);
        }
    }
    else
    {
        var = getIndexedVariablePtr(opcode, varIdx);
    }
    
    switch (opcode & OP_TYPE)
    {
    default:
    case OP_LOAD:
        val = new LoadInst(var, "", m_bb);
        break;

    case OP_INC:
        val = new LoadInst(var, "", m_bb);
        val = BinaryOperator::CreateAdd(val, ConstantInt::get(m_sizeTy, 1), "", m_bb);
        new StoreInst(val, var, m_bb);
        break;

    case OP_DEC:
        val = new LoadInst(var, "", m_bb);
        val = BinaryOperator::CreateSub(val, ConstantInt::get(m_sizeTy, 1), "", m_bb);
        new StoreInst(val, var, m_bb);
        break;

    case OP_STORE:
        if ((opcode & OP_INDEX) != 0)
        {
            val = callPop();
            if ((opcode & OP_STACK) == 0)
            {
                m_acc = val;
            }
        }
        else
        {
            if ((opcode & OP_STACK) == 0)
            {
                castAccToSizeType();
                val = m_acc;
            }
            else
            {
                val = callPop();
            }
        }
        new StoreInst(val, var, m_bb);
        return true;
    }

    if ((opcode & OP_STACK) == 0)
    {
        m_acc = val;
    }
    else
    {
        callPush(val);
    }
    return true;
}


bool PMachine::badOp(uint8_t opcode)
{
    assert(0);
    return false;
}


END_NAMESPACE_SCI
