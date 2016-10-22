#include "SplitSendPass.hpp"
#include "../World.hpp"
#include <llvm/IR/Instructions.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


static std::string CreateCallInstName(Value *sel)
{
    std::string name;
    if (isa<ConstantInt>(sel))
    {
        name = SelectorTable::Get().getSelectorName(static_cast<uint>(cast<ConstantInt>(sel)->getSExtValue()));
        if (!name.empty())
        {
            name = "res@" + name;
        }
    }
    return name;
}


static inline bool IsRestCall(const Value *val)
{
    const CallInst *call = dyn_cast<CallInst>(val);
    return (call != nullptr && call->getCalledFunction() == GetWorld().getStub(Stub::rest));
}


SplitSendPass::SplitSendPass()
{
    World &world = GetWorld();
    IntegerType *sizeTy = world.getSizeType();
    Type *params[] = { sizeTy, sizeTy, sizeTy };

    FunctionType *funcTy = FunctionType::get(sizeTy, params, true);
    m_fnStubSend.reset(Function::Create(funcTy, Function::ExternalLinkage, "stub_send@SCI"));
}


SplitSendPass::~SplitSendPass()
{
    assert(!m_fnStubSend || m_fnStubSend->getNumUses() == 0);
}


void SplitSendPass::run()
{
    Function *sendFunc = GetWorld().getStub(Stub::send);
    for (auto u = sendFunc->user_begin(), e = sendFunc->user_end(); u != e;)
    {
        CallInst *call = cast<CallInst>(*u);
        ++u;

        splitSend(call);
    }

    m_fnStubSend->replaceAllUsesWith(sendFunc);
}


bool SplitSendPass::splitSend(CallInst *callSend)
{
    ConstantInt *argc = cast<ConstantInt>(callSend->getArgOperand(2));
    uint n = static_cast<uint>(argc->getZExtValue());

    auto iLast = callSend->arg_end() - 1;
    Value *rest = nullptr;
    if (IsRestCall(*iLast))
    {
        rest = *iLast;
        --iLast;
        ++n;
    }

    if (callSend->getNumArgOperands() == (n + 3))
    {
        if (!callSend->hasName())
        {
            Value *sel = callSend->getArgOperand(1);
            std::string name = CreateCallInstName(sel);
            if (!name.empty())
            {
                callSend->setName(name);
            }
        }
        return false;
    }

    SmallVector<Value *, 16> args;
    auto iArg = callSend->arg_begin();

    Value *obj = *iArg;
    Value *acc;

    while (iArg != iLast)
    {
        args.clear();

        Value *sel = *++iArg;
        argc = cast<ConstantInt>(*++iArg);

        args.push_back(obj);
        args.push_back(sel);
        args.push_back(argc);

        n = static_cast<uint>(argc->getZExtValue());
        for (uint i = 0; i < n; ++i)
        {
            args.push_back(*++iArg);
        }

        if (rest != nullptr)
        {
            args.push_back(rest);
            rest = nullptr;
        }

        std::string name = CreateCallInstName(sel);
        acc = CallInst::Create(m_fnStubSend.get(), args, name, callSend);
    }

    callSend->replaceAllUsesWith(acc);
    callSend->eraseFromParent();
    return true;
}


END_NAMESPACE_SCI
