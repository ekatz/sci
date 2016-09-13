#include "StackReconstructionPass.hpp"
#include "../World.hpp"
#include <llvm/IR/CFG.h>
#include <llvm/IR/CallSite.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


static Instruction* GetFirstInsertionPoint(BasicBlock &bb)
{
    for (Instruction &i : bb)
    {
        if (!isa<AllocaInst>(i))
        {
            return &i;
        }
    }
    return nullptr;
}


StackReconstructionPass::StackReconstructionPass() :
    m_insertPt(nullptr)
{
}


void StackReconstructionPass::run()
{
    Function *caller = nullptr;
    Function *popFunc = GetWorld().getStub(Stub::pop);
    while (!popFunc->user_empty())
    {
        CallInst *call = cast<CallInst>(*popFunc->user_begin());
        Function *func = call->getParent()->getParent();
        if (caller != func)
        {
            caller = func;
            m_insertPt = GetFirstInsertionPoint(func->getEntryBlock());
        }
        runOnPop(call);
        call->eraseFromParent();
    }
}


static void AddPredecessors(BasicBlock *bb, SmallVector<BasicBlock *, 8> &workList)
{
    for (pred_iterator pi = pred_begin(bb), e = pred_end(bb); pi != e; ++pi)
    {
        if (std::find(workList.begin(), workList.end(), *pi) == workList.end())
        {
            workList.push_back(*pi);
        }
    }
}


void StackReconstructionPass::runOnPop(CallInst *callPop)
{
    World &world = GetWorld();
    SmallVector<BasicBlock *, 8> workList;
    uint workIdx = 0;

    int balance = 0;
    AllocaInst *crossBB = nullptr;
    Instruction *inst = callPop;
    while (true)
    {
        if (inst == nullptr)
        {
            if (workIdx >= workList.size())
            {
                break;
            }
            inst = &workList[workIdx++]->back();
        }
        else
        {
            Instruction *prev = inst->getPrevNode();
            if (prev == nullptr)
            {
                if (crossBB == nullptr)
                {
                    assert(m_insertPt != nullptr);
                    crossBB = new AllocaInst(world.getSizeType(), "stack.addr", m_insertPt);

                    Value *val = new LoadInst(crossBB, "", callPop);
                    callPop->replaceAllUsesWith(val);
                }

                AddPredecessors(inst->getParent(), workList);
                if (workIdx >= workList.size())
                {
                    break;
                }
                prev = &workList[workIdx++]->back();
            }
            inst = prev;
        }

        if (isa<CallInst>(inst))
        {
            CallSite cs(inst);
            Function *calledFunc = cs.getCalledFunction();
            if (world.getStub(Stub::push) == calledFunc)
            {
                if (balance == 0)
                {
                    Value *val = cs.getArgument(0);
                    if (workList.empty())
                    {
                        callPop->replaceAllUsesWith(val);
                        inst->eraseFromParent();
                        break;
                    }
                    else
                    {
                        assert(crossBB != nullptr);
                        new StoreInst(val, crossBB, inst);
                        inst->eraseFromParent();
                        inst = nullptr;
                    }
                }
                else
                {
                    balance++;
                }
            }
            else if (world.getStub(Stub::send) == calledFunc || world.getStub(Stub::super) == calledFunc)
            {
                uint64_t paramCount = cast<ConstantInt>(cs.getArgument(1))->getZExtValue();
                balance -= static_cast<int>(paramCount);
            }
            else if (world.getStub(Stub::call) == calledFunc || world.getStub(Stub::callb) == calledFunc || world.getStub(Stub::callk) == calledFunc)
            {
                uint64_t paramCount = cast<ConstantInt>(cs.getArgument(1))->getZExtValue();
                balance -= static_cast<int>(paramCount) + 1;
            }
            else if (world.getStub(Stub::calle) == calledFunc)
            {
                uint64_t paramCount = cast<ConstantInt>(cs.getArgument(2))->getZExtValue();
                balance -= static_cast<int>(paramCount) + 1;
            }
            else if (world.getStub(Stub::pop) == calledFunc)
            {
                runOnPop(cast<CallInst>(inst));
                Instruction *next = inst->getNextNode();
                inst->eraseFromParent();
                inst = next;
            }
        }
    }
}

/*
* AllocaInst *crossBB = nullptr;
* int balance = 0;
* scan backwards
** if no more instruction in BB -> if (crossBB == nullptr) crossBB = AllocaInst(entry);    replace "pop" with LoadInst(crossBB);
** if ("send") balance -= [#];
** if ("call*") balance -= [#] + 1;
** if ("pop") recursive scan this pop!! balance--;
** if ("push") if(balance == 0) remove "push", replace "pop" with pushed val break; else balance++;
*/
END_NAMESPACE_SCI
