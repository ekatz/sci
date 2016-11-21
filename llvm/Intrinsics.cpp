#include "Intrinsics.hpp"
#include "World.hpp"
#include "Method.hpp"

using namespace llvm;


BEGIN_NAMESPACE_SCI


class IntrinsicFunction : public Function
{
public:
    struct Desc
    {
        Intrinsic::ID id;
        const char *name;
        uint argc;
        bool isVarArg;
        uint8_t returnVal;
    };

    static IntrinsicFunction *Create(const Desc &desc)
    {
        IntegerType *sizeTy = GetWorld().getSizeType();

        SmallVector<Type *, 6> params;
        params.resize(desc.argc, sizeTy);

        Type *retTy;
        switch (desc.returnVal)
        {
        case Type::VoidTyID:
            retTy = Type::getVoidTy(sizeTy->getContext());
            break;

        case Type::IntegerTyID:
            retTy = sizeTy;
            break;

        case Type::PointerTyID:
            retTy = sizeTy->getPointerTo();
            break;

        default:
            retTy = nullptr;
        }

        FunctionType *funcTy = FunctionType::get(retTy, params, desc.isVarArg);

        Function *func = Function::Create(funcTy, ExternalLinkage, desc.name, nullptr);
        IntrinsicFunction *intrin = static_cast<IntrinsicFunction *>(func);
        intrin->setID(desc.id);
        return intrin;
    }

private:
    void setID(Intrinsic::ID iid)
    {
        uint llvmID = iid + llvm::Intrinsic::num_intrinsics + 1;
        IntID = static_cast<llvm::Intrinsic::ID>(llvmID);
    }
};


static const IntrinsicFunction::Desc s_intrinsics[] = {
    { Intrinsic::push,  "push@SCI",     1, false, Type::VoidTyID    },
    { Intrinsic::pop,   "pop@SCI",      0, false, Type::IntegerTyID },
    { Intrinsic::rest,  "rest@SCI",     1, false, Type::IntegerTyID },

    { Intrinsic::clss,  "class@SCI",    1, false, Type::IntegerTyID },
    { Intrinsic::objc,  "obj_cast@SCI", 3, true,  Type::IntegerTyID },

    { Intrinsic::prop,  "prop@SCI",     1, false, Type::PointerTyID },
    { Intrinsic::send,  "send@SCI",     3, true,  Type::IntegerTyID },
    { Intrinsic::call,  "call@SCI",     2, true,  Type::IntegerTyID },
    { Intrinsic::calle, "calle@SCI",    3, true,  Type::IntegerTyID },
    { Intrinsic::callk, "callk@SCI",    2, true,  Type::IntegerTyID },

    { Intrinsic::callv, "callv@SCI",    1, true,  Type::IntegerTyID }
};


static_assert(ARRAYSIZE(s_intrinsics) == Intrinsic::num_intrinsics,
              "Incorrect number of intrinsic descriptors.");

Intrinsic::Intrinsic()
{
    for (uint i = 0, n = Intrinsic::num_intrinsics; i < n; ++i)
    {
        m_funcs[i].reset(IntrinsicFunction::Create(s_intrinsics[i]));
    }
}


Intrinsic::~Intrinsic()
{
    for (uint i = 0, n = Intrinsic::num_intrinsics; i < n; ++i)
    {
        assert(!m_funcs[i] || m_funcs[i]->getNumUses() == 0);
    }
}


Function* Intrinsic::get(ID iid) const
{
    assert(iid < Intrinsic::num_intrinsics && "Intrinsic ID not in range.");
    assert(m_funcs[iid] && "Intrinsic not supported.");

    return m_funcs[iid].get();
}


Function* Intrinsic::Get(ID iid)
{
    return GetWorld().getIntrinsic(iid);
}


Intrinsic::ID IntrinsicInst::getIntrinsicID() const
{
    uint llvmID = getCalledFunction()->getIntrinsicID();
    assert(llvmID > llvm::Intrinsic::num_intrinsics && "Invalid intrinsic ID.");
    return static_cast<Intrinsic::ID>(llvmID - (llvm::Intrinsic::num_intrinsics + 1));
}


bool IntrinsicInst::classof(const CallInst *call)
{
    if (const Function *calledFunc = call->getCalledFunction())
    {
        uint llvmID = calledFunc->getIntrinsicID();
        return (llvmID > llvm::Intrinsic::num_intrinsics);
    }
    return false;
}


uint SendMessageInst::MatchArgOperandIndex(Argument *arg)
{
    Procedure *proc = GetWorld().getProcedure(*arg->getParent());
    assert(proc != nullptr && "Argument does not belong to a known procedure!");
    Method *method = proc->asMethod();
    assert(method != nullptr && "Cannot send a message to a procedure!");
    return MatchArgOperandIndex(method, arg);
}


uint SendMessageInst::MatchArgOperandIndex(Method *method, Argument *arg)
{
    assert(method->getFunction() == arg->getParent() && "Argument does not belong to the procedure!");

    uint argIndex = arg->getArgNo();
    if (argIndex == 0)
    {
        return 0;
    }
    if (method->hasArgc())
    {
        argIndex--;
    }
    return argIndex + 2; // Skip the first 2 arguments (the Object and the Selector).
}


uint CallInternalInst::MatchArgOperandIndex(Argument *arg)
{
    Procedure *proc = GetWorld().getProcedure(*arg->getParent());
    assert(proc != nullptr && "Argument does not belong to a known procedure!");
    return MatchArgOperandIndex(proc, arg);
}


uint CallInternalInst::MatchArgOperandIndex(Procedure *proc, Argument *arg)
{
    assert(!proc->isMethod() && "Cannot call a method directly!");
    assert(proc->getFunction() == arg->getParent() && "Argument does not belong to the procedure!");

    uint argIndex = arg->getArgNo();
    if (!proc->hasArgc())
    {
        argIndex++;
    }
    return argIndex + 1; // Skip the first argument (the procedure offset).
}


uint CallExternalInst::MatchArgOperandIndex(Argument *arg)
{
    return MatchArgOperandIndex(GetWorld().getProcedure(*arg->getParent()), arg);
}


uint CallExternalInst::MatchArgOperandIndex(Procedure *proc, Argument *arg)
{
    assert(!proc->isMethod() && "Cannot call a method directly!");
    assert(proc->getFunction() == arg->getParent() && "Argument does not belong to the procedure!");

    uint argIndex = arg->getArgNo();
    if (!proc->hasArgc())
    {
        argIndex++;
    }
    return argIndex + 2; // Skip the first 2 arguments (the Script ID and the Entry Index).
}


END_NAMESPACE_SCI
