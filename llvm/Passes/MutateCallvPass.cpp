#include "MutateCallvPass.hpp"
#include "../World.hpp"
#include "../Script.hpp"
#include "../Class.hpp"
#include "../Method.hpp"

using namespace llvm;


BEGIN_NAMESPACE_SCI


MutateCallvPass::MutateCallvPass()
{
}


MutateCallvPass::~MutateCallvPass()
{
}


void MutateCallvPass::run()
{
    Function *sendFunc = GetWorld().getStub(Stub::send);
    for (auto i : sendFunc->users())
  //  while (!sendFunc->user_empty())
    {
        CallInst *call = cast<CallInst>(/*sendFunc->user_back()*/i);

        if (isa<ConstantInt>(call->getArgOperand(0)))
        {
            mutateSuper(call);
        }
    }
}


void MutateCallvPass::mutateSuper(CallInst *call)
{
    World &world = GetWorld();
    Function *callvFunc = world.getStub(Stub::callv);

    uint classId = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(0))->getZExtValue());
    Class *super = Class::Get(classId);
    assert(super != nullptr);

    uint methodId = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue());
    Method *superMethod = super->getMethod(methodId);
    assert(superMethod->getFunction() != nullptr);

    Method *selfMethod = static_cast<Method *>(world.getProcedure(*call->getFunction()));
    assert(selfMethod->isMethod());

    Script &selfScript = selfMethod->getScript();
    Script &superScript = superMethod->getScript();

    Function *superFunc = superMethod->getFunction();
    if (selfScript.getId() != superScript.getId())
    {
        superFunc = cast<Function>(selfScript.getModule()->getOrInsertFunction(superFunc->getName(), superFunc->getFunctionType()));
    }

    SmallVector<Value *, 16> args;
    args.push_back(&*selfMethod->getFunction()->arg_begin()); // self

    if (superMethod->hasArgc())
    {
        args.push_back(call->getArgOperand(2));
    }
    for (auto i = call->arg_begin() + 3, e = call->arg_end(); i != e; ++i)
    {
        args.push_back(*i);
    }

    CallInst *newCall = CallInst::Create(superFunc, args, "", call);

    

    //TODO: handle "rest"
//     uint paramCount = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue());
//     args.resize(paramCount + 1);
//     args[0] = call->getArgOperand(0);
//     for (; paramCount != 0; --paramCount)
//     {
//         args[paramCount] = CallInst::Create(popFunc, "", call);
//     }
// 
//     CallInst *newCall = CallInst::Create(world.getStub(Stub::callv), args, "", call);
    call->replaceAllUsesWith(newCall);
    call->eraseFromParent();
}


END_NAMESPACE_SCI
