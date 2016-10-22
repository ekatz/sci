#include "ValueOriginTracer.hpp"
#include "../World.hpp"
#include "../Script.hpp"
#include "../Class.hpp"
#include "../Method.hpp"

using namespace llvm;


BEGIN_NAMESPACE_SCI


static bool EqualGEPs(GetElementPtrInst &gep1, GetElementPtrInst &gep2)
{
    auto i1 = gep1.idx_begin(), e1 = gep1.idx_end();
    auto i2 = gep2.idx_begin(), e2 = gep2.idx_end();
    while (true)
    {
        // If one reached the end but the other not, then exit.
        if ((i1 == e1) ^ (i2 == e2))
        {
            return false;
        }
        if (i1 == e1)
        {
            break;
        }

        ConstantInt *int1 = dyn_cast<ConstantInt>(i1->get());
        ConstantInt *int2 = dyn_cast<ConstantInt>(i2->get());
        if ((int1 != nullptr) ^ (int2 != nullptr))
        {
            return false;
        }
        if (int1 != nullptr)
        {
            if (int1->getZExtValue() != int2->getZExtValue())
            {
                return false;
            }
        }
    }
    return true;
}


ValueOriginTracer::ValueOriginTracer()
{
}


ValueOriginTracer::~ValueOriginTracer()
{
}


void ValueOriginTracer::run()
{
    Function *sendFunc = GetWorld().getStub(Stub::send);
    for (auto user : sendFunc->users())
    {
        CallInst *call = cast<CallInst>(user);
        addReference(call);
    }

    while (!sendFunc->user_empty())
    {
        CallInst *call = cast<CallInst>(sendFunc->user_back());

        if (isa<ConstantInt>(call->getArgOperand(0)))
        {
            mutateSuper(call);
        }
    }
}


void ValueOriginTracer::addReference(CallInst *call)
{GetWorld().getClass(id)->getGlobal()
    //TODO: not always ConstantInt!
    uint methodId = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue());
  //  m_refs.insert(std::make_pair(methodId, call));
}


Value* ValueOriginTracer::backtraceValue(Value *val)
{
    SmallVector<BasicBlock *, 8> blocks;

    while (true)
    {
        switch (val->getValueID())
        {
        case Value::GlobalVariableVal:
            if (!val->getName().empty() && val->getName().front() != '?')
            {
                assert(m_gep == nullptr);
                // We have found the object!
                return val;
            }
            return traceStoredGlobal(cast<GlobalVariable>(val));

        case Value::ConstantIntVal:
            return val;

        case Value::ArgumentVal:
            return backtraceArgument(cast<Argument>(val));

        case Value::InstructionVal + Instruction::Alloca:
            return traceStoredLocal(val);

        case Value::InstructionVal + Instruction::Call:
            return backtraceReturnValue(cast<CallInst>(val));

        case Value::InstructionVal + Instruction::PtrToInt:
            val = cast<PtrToIntInst>(val)->getPointerOperand();
            break;

        case Value::InstructionVal + Instruction::Load:
            val = cast<LoadInst>(val)->getPointerOperand();
            break;

       // case Value::InstructionVal + Instruction::Store:
        case Value::InstructionVal + Instruction::GetElementPtr:
            assert(m_gep == nullptr);
            m_gep = cast<GetElementPtrInst>(val);
            val = m_gep->getPointerOperand();
            break;

        default:
            assert(0);
            return nullptr;
        }
    }
    return val;
}


Value* ValueOriginTracer::backtraceReturnValue(CallInst *call)
{
    World &world = GetWorld();
    Function *calledFunc = call->getCalledFunction();
    if (calledFunc == world.getStub(Stub::send))
    {
        Value *sel = call->getArgOperand(1);
        if (isa<ConstantInt>(sel))
        {
            uint methodId = static_cast<uint>(cast<ConstantInt>(sel)->getSExtValue());
            Procedure *proc = world.getSelectorTable().getMethods(methodId);
            for (User *user : calledFunc->users())
            {
                sel = cast<CallInst>(user)->getArgOperand(1);
                if (isa<ConstantInt>(sel))
                {
                    if (static_cast<uint>(cast<ConstantInt>(sel)->getSExtValue()) == methodId)
                    {
                        v = backtraceValue(v);
                        if (v != nullptr)
                        {
                            return v;
                        }
                    }
                }
                else
                {
                    //TODO: handle method pointer as a candidate.
                    //backtraceValue - for method selector (stop on constant int)
                }
            }
        }
        else
        {
            //TODO: handle method pointer as a candidate.
        }
    }
}


Value* ValueOriginTracer::traceStoredLocal(Value *ptrVal)
{
    assert(ptrVal->getType()->isPointerTy());

    if (m_gep != nullptr)
    {
        GetElementPtrInst *gep = m_gep;
        m_gep = nullptr;

        for (User *user : ptrVal->users())
        {
            if (isa<GetElementPtrInst>(user))
            {
                GetElementPtrInst *otherGep = cast<GetElementPtrInst>(user);
                if (EqualGEPs(*gep, *otherGep))
                {
                    Value *v = traceStoredLocal(otherGep->getPointerOperand());
                    if (v != nullptr)
                    {
                        return v;
                    }
                }
            }
        }
    }
    else
    {
        for (User *user : ptrVal->users())
        {
            if (isa<StoreInst>(user))
            {
                StoreInst *store = cast<StoreInst>(user);
                Value *v = backtraceValue(store->getValueOperand());
                if (v != nullptr)
                {
                    return v;
                }
            }
        }
    }
    return nullptr;
}


Value* ValueOriginTracer::backtraceArgument(Argument *arg)
{
    uint argIndex = arg->getArgNo();
    Procedure *proc = GetWorld().getProcedure(*arg->getParent());
    if (proc->isMethod())
    {
        if (argIndex == 0)
        {
            // If this is the "self" param.
            static_cast<Method *>(proc)->getClass()
        }

        if (proc->hasArgc())
        {
            argIndex--;
        }
        argIndex--; // "self" is always the first param.

        uint methodId = static_cast<Method *>(proc)->getSelector();
        Function *sendFunc = GetWorld().getStub(Stub::send);
        for (User *user : sendFunc->users())
        {
            CallInst *call = cast<CallInst>(user);
            Value *sel = call->getArgOperand(1);
            if (isa<ConstantInt>(sel))
            {
                if (static_cast<uint>(cast<ConstantInt>(sel)->getSExtValue()) == methodId)
                {
                    Value *v = call->getArgOperand(argIndex + 3);
                    v = backtraceValue(v);
                    if (v != nullptr)
                    {
                        return v;
                    }
                }
            }
            else
            {
                //TODO: handle method pointer as a candidate.
                //backtraceValue - for method selector (stop on constant int)
            }
        }
    }
    else if (proc->getFunction()->hasInternalLinkage())
    {
        ;
    }
    else
    {
        for (Script &script : GetWorld().scripts())
        {
            ;
        }
    }
    return nullptr;
}


Value* ValueOriginTracer::traceStoredGlobal(GlobalVariable *var)
{
    assert(m_gep != nullptr);
    GetElementPtrInst *gep = m_gep;
    m_gep = nullptr;

    if (var->getName().equals("?globals"))
    {
        for (Script &script : GetWorld().scripts())
        {
            var = script.getModule()->getGlobalVariable("?globals");
            if (var != nullptr)
            {
                Value *v = traceStoredScriptVariable(var, *gep);
                if (v != nullptr)
                {
                    return v;
                }
            }
        }
        return nullptr;
    }
    else
    {
        return traceStoredScriptVariable(var, *gep);
    }
}


Value* ValueOriginTracer::traceStoredScriptVariable(GlobalVariable *var, GetElementPtrInst &gep)
{
    for (User *user : var->users())
    {
        if (isa<GetElementPtrInst>(user))
        {
            GetElementPtrInst *otherGep = cast<GetElementPtrInst>(user);
            if (EqualGEPs(gep, *otherGep))
            {
                Value *v = traceStoredLocal(otherGep->getPointerOperand());
                if (v != nullptr)
                {
                    return v;
                }
            }
        }
    }
    return nullptr;
}


END_NAMESPACE_SCI
