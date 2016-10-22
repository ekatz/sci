#include "AnalyzeObjectsPass.hpp"
#include "../World.hpp"
#include "../Script.hpp"
#include "../Object.hpp"
#include <llvm/IR/Metadata.h>

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


AnalyzeObjectsPass::AnalyzeObjectsPass()
{
}


AnalyzeObjectsPass::~AnalyzeObjectsPass()
{
}


void AnalyzeObjectsPass::run()
{
    analyzeObjects();
}


void AnalyzeObjectsPass::analyzeObjects()
{
    for (const Script &script : GetWorld())
    {
        for (const Object &obj : script.getObjects())
        {
            analyzeObject(obj.getGlobal(), obj.getSuper());
        }
    }
}


void AnalyzeObjectsPass::analyzeClassObjects()
{
    Function *classFunc = GetWorld().getStub(Stub::clss);
    for (User *user : classFunc->users())
    {
        CallInst *callClass = cast<CallInst>(user);
       // if ();
    }
}


void AnalyzeObjectsPass::analyzeObject(Value *obj, Class *cls)
{
    ObjectAnalyzer(cls).evolve(obj);
}


void AnalyzeObjectsPass::analyzeStoredObject(Value *ptrVal, GetElementPtrInst *gep, Class *cls)
{
    for (User *user : ptrVal->users())
    {
        GetElementPtrInst *ugep = dyn_cast<GetElementPtrInst>(user);
        if (ugep != nullptr && EqualGEPs(gep, ugep))
        {
            analyzeObject(ugep, cls);
        }
    }
}


ObjectAnalyzer::ObjectAnalyzer(Class *cls) : m_class(cls)
{
}


ObjectAnalyzer::~ObjectAnalyzer()
{
}


void ObjectAnalyzer::evolve(Value *val)
{
    for (User *user : val->users())
    {
        switch (user->getValueID())
        {
        case Value::InstructionVal + Instruction::Call:
            analyzeAsArgument(val, cast<CallInst>(user));
            return;

        case Value::InstructionVal + Instruction::PtrToInt:
            evolve(user);
            return;

//         case Value::InstructionVal + Instruction::Load:
//             break;

        case Value::InstructionVal + Instruction::Store:
            assert(val == cast<StoreInst>(user)->getValueOperand());
            return;

        default:
            assert(0);
            return;
        }
    }
}

// retractValue


void ObjectAnalyzer::analyzeAsArgument(Value *arg, CallInst *call)
{
    World &world = GetWorld();
    if (call->getCalledFunction() == world.getStub(Stub::send))
    {
//         IntegerType *sizeTy = world.getSizeType();
//         ConstantAsMetadata *c = ConstantAsMetadata::get(ConstantInt::get(sizeTy, m_class->getId()));
//         call->setMetadata("sci.class", MDNode::get(world.getContext(), c));
        if (call->getArgOperand(0) == arg)
        {
            ;
        }
    }
}


END_NAMESPACE_SCI
