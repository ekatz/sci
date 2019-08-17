#include "ValueRetracer.hpp"
#include "../World.hpp"
#include "../Script.hpp"
#include "../Procedure.hpp"
#include <llvm/IR/CFG.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


static bool EqualGEPs(GetElementPtrInst *gep1, GetElementPtrInst *gep2)
{
    auto i1 = gep1->idx_begin(), e1 = gep1->idx_end();
    auto i2 = gep2->idx_begin(), e2 = gep2->idx_end();
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

        ++i1;
        ++i2;
    }
    return true;
}


static StoreInst* FindLastStoreInst(Value *ptrVal, BasicBlock *bb)
{
    for (auto i = bb->rbegin(), e = bb->rend(); i != e; ++i)
    {
        StoreInst *store = dyn_cast<StoreInst>(&*i);
        if (store != nullptr)
        {
            if (store->getPointerOperand() == ptrVal)
            {
                return store;
            }
        }
    }
    return nullptr;
}



void ValueRetracer::clear()
{
    m_values.clear();
    m_blocks.clear();
    m_gep = nullptr;
}


bool ValueRetracer::addBasicBlock(BasicBlock *bb)
{
    bool ret = (find(m_blocks, bb) == m_blocks.end());
    if (ret)
    {
        m_blocks.push_back(bb);
    }
    return ret;
}


bool ValueRetracer::addValue(Value *val)
{
    bool ret = (find(m_values, val) == m_values.end());
    if (ret)
    {
        m_values.push_back(val);
    }
    return ret;
}


void ValueRetracer::retraceBasicBlockTransition(AllocaInst *accAddr, BasicBlock *bb)
{
    for (BasicBlock *pred : predecessors(bb))
    {
        if (!addBasicBlock(pred))
        {
            continue;
        }

        StoreInst *store = FindLastStoreInst(accAddr, pred);
        if (store == nullptr)
        {
            continue;
        }

        retraceValue(store->getValueOperand());
    }
}


void ValueRetracer::retraceValue(Value *val)
{
    while (true)
    {
        switch (val->getValueID())
        {
        case Value::ArgumentVal:
        case Value::GlobalVariableVal:
        case Value::ConstantIntVal:
            addValue(val);
            return;

        case Value::InstructionVal + Instruction::Call:
            retraceReturnValue(cast<CallInst>(val));
            return;

        case Value::InstructionVal + Instruction::PtrToInt:
            val = cast<PtrToIntInst>(val)->getPointerOperand();
            break;

        case Value::InstructionVal + Instruction::Load:
            m_load = cast<LoadInst>(val);
            val = m_load->getPointerOperand();
            if (isa<AllocaInst>(val) && val->getName().equals("acc.addr"))
            {
                BasicBlock *bb = m_load->getParent();
                m_load = nullptr;
                retraceBasicBlockTransition(cast<AllocaInst>(val), bb);
                return;
            }
            break;

        case Value::InstructionVal + Instruction::Alloca:
            retraceStoredValue(val);
            return;

        case Value::InstructionVal + Instruction::GetElementPtr:
            assert(m_gep == nullptr && "Cannot retrace more than one GEP instruction.");
            do
            {
                m_gep = cast<GetElementPtrInst>(val);
                val = m_gep->getPointerOperand();
            } while (isa<GetElementPtrInst>(val));
            break;

        default:
            assert(false && "Cannot retrace from unsupported instruction");
            return;
        }
    }
}


Value* ValueRetracer::retraceStoredValue(Value *ptrVal)
{
    assert(ptrVal->getType()->isPointerTy() && "Destination value is not a pointer.");

    Value *res = nullptr;
    if (m_gep != nullptr)
    {
        assert(m_gep->getPointerOperand() == ptrVal && "Mismatch GEP instruction.");
        GetElementPtrInst *gep = m_gep;
        m_gep = nullptr;

        for (User *user : ptrVal->users())
        {
            if (isa<GetElementPtrInst>(user))
            {
                GetElementPtrInst *otherGep = cast<GetElementPtrInst>(user);
                if (gep != otherGep && EqualGEPs(gep, otherGep))
                {
                    Value *val = retraceStoredValue(otherGep);
                    if (val != nullptr)
                    {
                        res = val;
                        if (!(isa<Constant>(val) && cast<Constant>(val)->isNullValue()))
                        {
                            break;
                        }
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
                Value *val = retraceValue(store->getValueOperand());
                if (val != nullptr)
                {
                    res = val;
                    if (!(isa<Constant>(val) && cast<Constant>(val)->isNullValue()))
                    {
                        break;
                    }
                }
            }
        }
    }
    return res;
}


void ValueRetracer::retraceReturnValue(CallInst *call)
{
    switch (cast<IntrinsicInst>(call)->getIntrinsicID())
    {
    case Intrinsic::objc: {
        Value *anchor = cast<ObjCastInst>(call)->getAnchor();
        addValue(anchor);
        break;
    }

    case Intrinsic::call: {
        uint offset = cast<CallInternalInst>(call)->getOffset()->getZExtValue();
        Script *script = GetWorld().getScript(*call->getModule());
        assert(script != nullptr && "Module is not owned by a script!");
        Procedure *proc = script->getProcedure(offset);
        assert(proc != nullptr && !proc->isMethod() && "Procedure not found!");
        retraceFunctionReturnValue(proc->getFunction());
        break;
    }

    case Intrinsic::calle: {
        CallExternalInst *calle = cast<CallExternalInst>(call);
        uint scriptId = static_cast<uint>(calle->getScriptID()->getZExtValue());
        uint entryIndex = static_cast<uint>(calle->getEntryIndex()->getZExtValue());
        Script *script = GetWorld().getScript(scriptId);
        GlobalObject *func = script->getExportedValue(entryIndex);
        retraceFunctionReturnValue(cast<Function>(func));
        break;
    }
// 
//     case Intrinsic::prop: {
//         PropertyInst *prop = cast<PropertyInst>(call);
//         prop->getIndex();
//         break;
//     }

    case Intrinsic::send: {
        SendMessageInst *sendMsg = cast<SendMessageInst>(call);
        Value *selVal = sendMsg->getSelector();
        if (isa<ConstantInt>(selVal))
        {
            uint sel = static_cast<uint>(cast<ConstantInt>(selVal)->getSExtValue());
            if (sel == GetWorld().getSelectorTable().getNewMethodSelector())
            {
                Value *obj = sendMsg->getObject();
                retraceValue(obj);//TODO: Can we replace the "New Method" with KClone?
                break;
            }
        }
    }
    default:
        addValue(call);
        break;
    }
}


void ValueRetracer::retraceFunctionReturnValue(Function *func)
{
    assert(func->getReturnType() == GetWorld().getSizeType() &&
        "Function does not return an object size value!");

    for (BasicBlock &bb : *func)
    {
        ReturnInst *ret = dyn_cast<ReturnInst>(bb.getTerminator());
        if (ret == nullptr)
        {
            continue;
        }

        Value *val = ret->getReturnValue();
        retraceValue(val);
    }
}


uint ValueRetracer::retrace(Value *val)
{
    clear();
    val = retraceValue(val);
    assert(m_gep == nullptr || m_gep->getPointerOperand() == val);
    return (m_gep == nullptr) ? val : m_gep;
}


END_NAMESPACE_SCI
