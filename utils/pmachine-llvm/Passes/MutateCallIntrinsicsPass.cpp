#include "MutateCallIntrinsicsPass.hpp"
#include "../World.hpp"
#include "../Script.hpp"
#include "../Method.hpp"
#include "../Object.hpp"
#include "../Decl.hpp"
#include <llvm/IR/IRBuilder.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


#define MSG_TYPE_METHOD     (1U << 0)
#define MSG_TYPE_PROP_GET   (1U << 1)
#define MSG_TYPE_PROP_SET   (1U << 2)
#define MSG_TYPE_PROP       (MSG_TYPE_PROP_GET | MSG_TYPE_PROP_SET)


const char* MutateCallIntrinsicsPass::s_kernelNames[] = {
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


static uint GetPotentialMessageType(const SendMessageInst *sendMsg)
{
    uint msgType = MSG_TYPE_METHOD;
    ConstantInt *c;

    c = dyn_cast<ConstantInt>(sendMsg->getSelector());
    if (c != nullptr)
    {
        uint sel = static_cast<uint>(c->getSExtValue());
        if (!(sel == 0 || sel == 1) && GetWorld().getSelectorTable().getProperties(sel).empty())
        {
            return MSG_TYPE_METHOD;
        }

        if (GetWorld().getSelectorTable().getMethods(sel).empty())
        {
            msgType = 0;
        }
    }

    c = cast<ConstantInt>(sendMsg->getArgCount());
    uint argc = static_cast<uint>(c->getZExtValue());
    switch (argc)
    {
    case 0:
        msgType |= MSG_TYPE_PROP_GET;
        if (sendMsg->user_empty())
        {
            assert((msgType & MSG_TYPE_METHOD) != 0 && "Unknown message destination!");
            return MSG_TYPE_METHOD;
        }
        break;

    case 1:
        msgType |= MSG_TYPE_PROP_SET;
        if (!sendMsg->user_empty())
        {
            if (sendMsg->hasOneUse())
            {
                const Instruction *user = sendMsg->user_back();
                const StoreInst *store = dyn_cast<StoreInst>(user);
                if (store != nullptr)
                {
                    const AllocaInst *alloc = dyn_cast<AllocaInst>(store->getPointerOperand());
                    if (alloc != nullptr)
                    {
                        if (alloc->getName().equals("acc.addr"))
                        {
                            break;
                        }
                    }
                }
                else if (isa<ReturnInst>(user))
                {
                    break;
                }
            }
            assert((msgType & MSG_TYPE_METHOD) != 0 && "Unknown message destination!");
            return MSG_TYPE_METHOD;
        }
        break;

    default:
        assert((msgType & MSG_TYPE_METHOD) != 0 && "Unknown message destination!");
        return MSG_TYPE_METHOD;
    }
    return msgType;
}


MutateCallIntrinsicsPass::MutateCallIntrinsicsPass() :
    m_sizeTy(GetWorld().getSizeType()),
    m_sizeBytesVal(ConstantInt::get(m_sizeTy, m_sizeTy->getBitWidth() / 8)),
    m_sizeAlign(GetWorld().getSizeTypeAlignment()),
    m_int8PtrTy(Type::getInt8PtrTy(GetWorld().getContext())),
    m_zero(ConstantInt::get(m_sizeTy, 0)),
    m_one(ConstantInt::get(m_sizeTy, 1)),
    m_allOnes(cast<ConstantInt>(Constant::getAllOnesValue(m_sizeTy))),
    m_nullPtr(ConstantPointerNull::get(m_sizeTy->getPointerTo()))
{
}


MutateCallIntrinsicsPass::~MutateCallIntrinsicsPass()
{
}


void MutateCallIntrinsicsPass::run()
{
    World &world = GetWorld();
    Function *func;

    func = world.getIntrinsic(Intrinsic::send);
    while (!func->user_empty())
    {
        SendMessageInst *call = cast<SendMessageInst>(func->user_back());
        mutateSendMessage(call);
    }

    func = world.getIntrinsic(Intrinsic::call);
    while (!func->user_empty())
    {
        CallInternalInst *call = cast<CallInternalInst>(func->user_back());
        mutateCallInternal(call);
    }

    func = world.getIntrinsic(Intrinsic::calle);
    while (!func->user_empty())
    {
        CallExternalInst *call = cast<CallExternalInst>(func->user_back());
        mutateCallExternal(call);
    }

    func = world.getIntrinsic(Intrinsic::callk);
    while (!func->user_empty())
    {
        CallKernelInst *call = cast<CallKernelInst>(func->user_back());
        mutateCallKernel(call);
    }
}


void MutateCallIntrinsicsPass::mutateCallKernel(CallKernelInst *callk)
{
    uint kernelOrdinal = static_cast<uint>(callk->getKernelOrdinal()->getZExtValue());
    Function *func = getOrCreateKernelFunction(kernelOrdinal, callk->getModule());

    Value *restc, *restv;
    extractRest(callk, restc, restv);

    ConstantInt *argcVal = cast<ConstantInt>(callk->getArgCount());
    CallInst *call = delegateCall(callk, func, 0, m_zero, m_allOnes, restc, restv, argcVal, callk->arg_begin());

    callk->replaceAllUsesWith(call);
    callk->eraseFromParent();
}


void MutateCallIntrinsicsPass::mutateCallInternal(CallInternalInst *calli)
{
    uint offset = static_cast<uint>(calli->getOffset()->getZExtValue());
    Script *script = GetWorld().getScript(*calli->getModule());
    assert(script != nullptr && "Not a script module.");
    Procedure *proc = script->getProcedure(offset);
    assert(proc != nullptr && "No procedure in the offset.");
    Function *func = GetFunctionDecl(proc->getFunction(), calli->getModule());

    Value *restc, *restv;
    extractRest(calli, restc, restv);

    ConstantInt *argcVal = cast<ConstantInt>(calli->getArgCount());
    CallInst *call = delegateCall(calli, func, 0, m_zero, m_allOnes, restc, restv, argcVal, calli->arg_begin());

    calli->replaceAllUsesWith(call);
    calli->eraseFromParent();
}


void MutateCallIntrinsicsPass::mutateCallExternal(CallExternalInst *calle)
{
    uint scriptId = static_cast<uint>(calle->getScriptID()->getZExtValue());
    uint entryIndex = static_cast<uint>(calle->getEntryIndex()->getZExtValue());
    Script *script = GetWorld().getScript(scriptId);
    assert(script != nullptr && "Invalid script ID.");
    Function *func = GetFunctionDecl(cast<Function>(script->getExportedValue(entryIndex)), calle->getModule());

    Value *restc, *restv;
    extractRest(calle, restc, restv);

    ConstantInt *argcVal = cast<ConstantInt>(calle->getArgCount());
    CallInst *call = delegateCall(calle, func, 0, m_zero, m_allOnes, restc, restv, argcVal, calle->arg_begin());

    calle->replaceAllUsesWith(call);
    calle->eraseFromParent();
}


void MutateCallIntrinsicsPass::mutateSendMessage(SendMessageInst *sendMsg)
{
    Value *restc, *restv;
    extractRest(sendMsg, restc, restv);

    CallInst *call;
    uint msgType = GetPotentialMessageType(sendMsg);
    if ((msgType & MSG_TYPE_METHOD) != 0)
    {
        call = createMethodCall(sendMsg, (msgType == MSG_TYPE_METHOD), restc, restv);
    }
    else
    {
        assert(restc == m_zero && restv == m_nullPtr && "Property cannot have a 'rest' param!");
        call = createPropertyCall(sendMsg, (msgType == MSG_TYPE_PROP_GET));
    }

    sendMsg->replaceAllUsesWith(call);
    sendMsg->eraseFromParent();
}


void MutateCallIntrinsicsPass::extractRest(CallInst *call, Value *&restc, Value *&restv) const
{
    auto iLast = call->arg_end() - 1;
    RestInst *rest = dyn_cast<RestInst>(*iLast);
    if (rest != nullptr)
    {
        Function *parentFunc = call->getFunction();
        auto a = parentFunc->arg_begin();
        if (parentFunc->arg_size() != 1)
        {
            assert(parentFunc->arg_begin()->getName().equals("self") && "Function with more than one parameter that is not 'self'.");
            ++a;
        }

        Value *parentArgs = &*a;
        Value *parentArgc = GetElementPtrInst::CreateInBounds(parentArgs, m_zero, "", rest);
        parentArgc = new LoadInst(parentArgc, "argc", false, GetWorld().getElementTypeAlignment(parentArgc), rest);

        ConstantInt *idx = ConstantInt::get(m_sizeTy, rest->getParamIndex()->getZExtValue() - (uint64_t)1);
        restc = BinaryOperator::CreateSub(parentArgc, idx, "rest.count", rest);

        restv = GetElementPtrInst::CreateInBounds(parentArgs, rest->getParamIndex(), "rest.args", rest);

        rest->replaceAllUsesWith(restc);
        rest->eraseFromParent();
    }
    else
    {
        restc = m_zero;
        restv = m_nullPtr;
    }
}


CallInst* MutateCallIntrinsicsPass::createPropertyCall(SendMessageInst *sendMsg, bool getter)
{
    assert(!isa<ConstantInt>(sendMsg->getObject()) && "Property cannot be called for super!");

    Module *module = sendMsg->getModule();
    StringRef oldName = sendMsg->getName();
    std::string name;
    if (oldName.startswith("res@"))
    {
        name = "prop";
        name += oldName.substr(3);
    }
    else
    {
        name = oldName;
    }

    Function *func;
    uint argc = 2;
    Value *args[3] = {
        sendMsg->getObject(),
        sendMsg->getSelector()
    };

    if (getter)
    {
        func = getGetPropertyFunction(module);
    }
    else
    {
        args[argc++] = sendMsg->getArgOperand(0);
        func = getSetPropertyFunction(module);
    }

    return CallInst::Create(func, makeArrayRef(args, argc), name, sendMsg);
}


CallInst* MutateCallIntrinsicsPass::createMethodCall(SendMessageInst *sendMsg, bool method, Value *restc, Value *restv)
{
    if (isa<ConstantInt>(sendMsg->getObject()))
    {
        return createSuperMethodCall(sendMsg, restc, restv);
    }

    Module *module = sendMsg->getModule();
    ConstantInt *argcVal = cast<ConstantInt>(sendMsg->getArgCount());
    Function *func = method ? getCallMethodFunction(module) : getSendMessageFunction(module);
    return delegateCall(sendMsg, func, 2, sendMsg->getObject(), sendMsg->getSelector(), restc, restv, argcVal, sendMsg->arg_begin());
}


CallInst* MutateCallIntrinsicsPass::createSuperMethodCall(SendMessageInst *sendMsg, Value *restc, Value *restv)
{
    Module *module = sendMsg->getModule();
    ConstantInt *superIdVal = cast<ConstantInt>(sendMsg->getObject());
    ConstantInt *selVal = cast<ConstantInt>(sendMsg->getSelector());
    uint superId = static_cast<uint>(superIdVal->getZExtValue());
    uint sel = static_cast<uint>(selVal->getSExtValue());

    Class *super = GetWorld().getClass(superId);
    assert(super != nullptr && "Invalid class ID.");
    Method *method = super->getMethod(sel);
    assert(method != nullptr && "Method selector not found!");
    Function *func = GetFunctionDecl(method->getFunction(), module);

    Argument *self = &*sendMsg->getFunction()->arg_begin();
    assert(self->getType() == m_sizeTy && "Call to 'super' is not within a method!");
    ConstantInt *argcVal = cast<ConstantInt>(sendMsg->getArgCount());
    return delegateCall(sendMsg, func, 1, self, selVal, restc, restv, argcVal, sendMsg->arg_begin());
}


CallInst* MutateCallIntrinsicsPass::delegateCall(CallInst *stubCall,
                                                 Function *func, uint prefixc,
                                                 Value *self, Value *sel,
                                                 Value *restc, Value *restv,
                                                 ConstantInt *argcVal, CallInst::op_iterator argi)
{
    Module *module = stubCall->getModule();
    Function *delegFunc = getOrCreateDelegator(argcVal, module);
    uint argc = static_cast<uint>(argcVal->getZExtValue());
    uint paramsCount = 1/*func*/ + 1/*prefixc*/ + 1/*self*/ + 1/*selector*/ + 1/*restc*/ + 1/*restv*/ + argc;

    uint i = 0;
    Value **args = static_cast<Value **>(alloca(paramsCount * sizeof(Value *)));

    Value *funcCast = CastInst::Create(Instruction::BitCast, func, m_int8PtrTy, "", stubCall);
    args[i++] = funcCast;
    args[i++] = ConstantInt::get(m_sizeTy, prefixc);
    args[i++] = self;
    args[i++] = sel;
    args[i++] = restc;
    args[i++] = restv;

    for (; i < paramsCount; ++i, ++argi)
    {
        args[i] = *argi;
    }

    CallInst *call = CallInst::Create(delegFunc, makeArrayRef(args, paramsCount), "", stubCall);
    call->takeName(stubCall);
    return call;
}


Function* MutateCallIntrinsicsPass::getOrCreateDelegator(ConstantInt *argcVal, Module *module)
{
    //
    // uintptr_t delegate@SCI@argc(uint8_t *func, uintptr_t prefixc, uintptr_t self, uintptr_t sel, uintptr_t restc, uintptr_t *restv, ...)
    //

    uint argc = static_cast<uint>(argcVal->getZExtValue());

    std::string delegFuncName = "delegate@SCI@" + utostr(argc);
    Function *delegFunc = module->getFunction(delegFuncName);
    if (delegFunc != nullptr)
    {
        return delegFunc;
    }

    World &world = GetWorld();
    LLVMContext &ctx = world.getContext();
    uint paramsCount = 1/*func*/ + 1/*prefixc*/ + 1/*self*/ + 1/*selector*/ + 1/*restc*/ + 1/*restv*/ + argc;
    uint i = 0;
    Type **paramTypes = static_cast<Type **>(alloca(paramsCount * sizeof(Type *)));

    paramTypes[i++] = m_int8PtrTy;              // func
    paramTypes[i++] = m_sizeTy;                 // prefixc
    paramTypes[i++] = m_sizeTy;                 // self
    paramTypes[i++] = m_sizeTy;                 // selector
    paramTypes[i++] = m_sizeTy;                 // restc
    paramTypes[i++] = m_sizeTy->getPointerTo(); // restv
    for (; i < paramsCount; ++i)
    {
        paramTypes[i] = m_sizeTy;
    }

    FunctionType *funcTy = FunctionType::get(m_sizeTy, makeArrayRef(paramTypes, paramsCount), false);
    delegFunc = Function::Create(funcTy, GlobalValue::PrivateLinkage, delegFuncName, module);
    delegFunc->addFnAttr(Attribute::AlwaysInline);

    auto params = delegFunc->arg_begin();
    Argument *func = &*params;
    ++params;
    Argument *prefixc = &*params;
    ++params;
    Argument *self = &*params;
    ++params;
    Argument *sel = &*params;
    ++params;
    Argument *restc = &*params;
    ++params;
    Argument *restv = &*params;
    ++params;

    BasicBlock *bb = BasicBlock::Create(ctx, "entry", delegFunc);

    // Add code to check if 'restc' is less than 0, then set to 0.
    ICmpInst *cmp = new ICmpInst(*bb, CmpInst::ICMP_SGT, restc, m_zero);
    SelectInst::Create(cmp, restc, m_zero, "", bb);

    ConstantInt *argcTotal = ConstantInt::get(m_sizeTy, argc + 1);
    Value *argsLen = BinaryOperator::CreateAdd(restc, argcTotal, "argsLen", bb);

    Value *args = new AllocaInst(m_sizeTy, argsLen, m_sizeAlign, "args", bb);

    Value *total = BinaryOperator::CreateAdd(restc, argcVal, "argc", bb);
    new StoreInst(total, args, false, m_sizeAlign, bb);

    for (i = 1; i <= argc; ++i)
    {
        Value *param = &*params;
        ++params;

        ConstantInt *c = ConstantInt::get(m_sizeTy, i);
        Value *argSlot = GetElementPtrInst::CreateInBounds(args, c, "", bb);
        new StoreInst(param, argSlot, false, m_sizeAlign, bb);
    }

    Value *argRest = GetElementPtrInst::CreateInBounds(args, argcTotal, "", bb);
    Value *restcBytes = BinaryOperator::CreateMul(restc, m_sizeBytesVal, "restcBytes", bb);

    IRBuilder<>(bb).CreateMemCpy(argRest, restv, restcBytes, m_sizeAlign);

    BasicBlock *switchBBs[3];
    switchBBs[2] = BasicBlock::Create(ctx, "case.method", delegFunc);
    switchBBs[1] = BasicBlock::Create(ctx, "case.super", delegFunc);
    switchBBs[0] = BasicBlock::Create(ctx, "case.proc", delegFunc);

    SwitchInst *switchPrefix = SwitchInst::Create(prefixc, switchBBs[2], 2, bb);
    switchPrefix->addCase(m_one, switchBBs[1]);
    switchPrefix->addCase(m_zero, switchBBs[0]);

    Value *callArgs[3];
    for (i = 0; i < 3; ++i)
    {
        if (i >= 1)
        {
            paramTypes[0] = m_sizeTy;
            callArgs[0] = self;

            if (i >= 2)
            {
                paramTypes[1] = m_sizeTy;
                callArgs[1] = sel;
            }
        }

        paramTypes[i] = m_sizeTy->getPointerTo();
        callArgs[i] = args;

        BasicBlock *switchBB = switchBBs[i];
        funcTy = FunctionType::get(m_sizeTy, makeArrayRef(paramTypes, i + 1), false);
        Value *funcCast = CastInst::Create(Instruction::BitCast, func, funcTy->getPointerTo(), "", switchBB);

        CallInst *call = CallInst::Create(funcCast, makeArrayRef(callArgs, i + 1), "", switchBB);
        ReturnInst::Create(ctx, call, switchBB);
    }

    return delegFunc;
}


Function* MutateCallIntrinsicsPass::getSendMessageFunction(Module *module)
{
    //
    // uintptr_t SendMessage(uintptr_t self, uintptr_t sel, uintptr_t *args)
    //

    StringRef name = "SendMessage";
    Function *func = module->getFunction(name);
    if (func == nullptr)
    {
        Type *params[] = {
            m_sizeTy,                   // self
            m_sizeTy,                   // sel
            m_sizeTy->getPointerTo()    // args
        };
        FunctionType *funcTy = FunctionType::get(m_sizeTy, params, false);
        func = Function::Create(funcTy, GlobalValue::ExternalLinkage, name, module);
    }
    return func;
}


Function* MutateCallIntrinsicsPass::getGetPropertyFunction(Module *module)
{
    //
    // uintptr_t GetProperty(uintptr_t self, uintptr_t sel)
    //

    StringRef name = "GetProperty";
    Function *func = module->getFunction(name);
    if (func == nullptr)
    {
        Type *params[] = {
            m_sizeTy,                   // self
            m_sizeTy                    // sel
        };
        FunctionType *funcTy = FunctionType::get(m_sizeTy, params, false);
        func = Function::Create(funcTy, GlobalValue::ExternalLinkage, name, module);
    }
    return func;
}


Function* MutateCallIntrinsicsPass::getSetPropertyFunction(Module *module)
{
    //
    // uintptr_t SetProperty(uintptr_t self, uintptr_t sel, uintptr_t val)
    //

    StringRef name = "SetProperty";
    Function *func = module->getFunction(name);
    if (func == nullptr)
    {
        Type *params[] = {
            m_sizeTy,                   // self
            m_sizeTy,                   // sel
            m_sizeTy                    // val
        };
        FunctionType *funcTy = FunctionType::get(m_sizeTy, params, false);
        func = Function::Create(funcTy, GlobalValue::ExternalLinkage, name, module);
    }
    return func;
}


Function* MutateCallIntrinsicsPass::getCallMethodFunction(Module *module)
{
    //
    // uintptr_t CallMethod(uintptr_t self, uintptr_t sel, uintptr_t *args)
    //

    StringRef name = "CallMethod";
    Function *func = module->getFunction(name);
    if (func == nullptr)
    {
        Type *params[] = {
            m_sizeTy,                   // self
            m_sizeTy,                   // sel
            m_sizeTy->getPointerTo()    // args
        };
        FunctionType *funcTy = FunctionType::get(m_sizeTy, params, false);
        func = Function::Create(funcTy, GlobalValue::ExternalLinkage, name, module);
    }
    return func;
}


Function* MutateCallIntrinsicsPass::getOrCreateKernelFunction(uint id, Module *module)
{
    if (id >= ARRAYSIZE(s_kernelNames))
    {
        return nullptr;
    }

    Function* func = module->getFunction(s_kernelNames[id]);
    if (func == nullptr)
    {
        FunctionType *funcTy = FunctionType::get(m_sizeTy, m_sizeTy->getPointerTo(), false);
        func = Function::Create(funcTy, GlobalValue::ExternalLinkage, s_kernelNames[id], module);
        func->arg_begin()->setName("args");
    }
    return func;
}


END_NAMESPACE_SCI
