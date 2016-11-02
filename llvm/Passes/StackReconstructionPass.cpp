#include "StackReconstructionPass.hpp"
#include "../World.hpp"
#include <llvm/IR/CFG.h>
#include <llvm/IR/CallSite.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


static void AddWorkItem(BasicBlock *bb, int balance, SmallVector<std::pair<BasicBlock *, int>, 8> &workList)
{
    for (auto i = workList.begin(), e = workList.end(); i != e; ++i)
    {
        if (i->first == bb)
        {
            return;
        }
    }

    workList.push_back(std::make_pair(bb, balance));
}


static void AddPredecessors(BasicBlock *bb, int balance, SmallVector<std::pair<BasicBlock *, int>, 8> &workList)
{
    for (pred_iterator pi = pred_begin(bb), pe = pred_end(bb); pi != pe; ++pi)
    {
        AddWorkItem(*pi, balance, workList);
    }
}


static void AddSuccessors(BasicBlock *bb, int balance, SmallVector<std::pair<BasicBlock *, int>, 8> &workList)
{
    for (succ_iterator si = succ_begin(bb), se = succ_end(bb); si != se; ++si)
    {
        AddWorkItem(*si, balance, workList);
    }
}


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
    World &world = GetWorld();
    IntegerType *sizeTy = world.getSizeType();
    Type *params[] = { sizeTy, sizeTy->getPointerTo() };
    FunctionType *funcTy;

    funcTy = FunctionType::get(Type::getVoidTy(world.getContext()), params, false);
    m_fnStubStore.reset(Function::Create(funcTy, Function::ExternalLinkage, "stub_store@SCI"));

    funcTy = FunctionType::get(sizeTy, params[1], false);
    m_fnStubLoad.reset(Function::Create(funcTy, Function::ExternalLinkage, "stub_load@SCI"));
}


StackReconstructionPass::~StackReconstructionPass()
{
    assert(!m_fnStubStore || m_fnStubStore->getNumUses() == 0);
    assert(!m_fnStubLoad || m_fnStubLoad->getNumUses() == 0);
}


void StackReconstructionPass::run()
{
    Function *caller = nullptr;
    Function *popFunc = Intrinsic::Get(Intrinsic::pop);
    while (!popFunc->user_empty())
    {
        CallInst *call = cast<CallInst>(popFunc->user_back());
        Function *func = call->getParent()->getParent();
        if (caller != func)
        {
            caller = func;
            m_insertPt = GetFirstInsertionPoint(func->getEntryBlock());
        }
        runOnPop(call);
    }

    eliminateRedundantStores();
    mutateStubs();
}


Instruction* StackReconstructionPass::runOnPop(CallInst *callPop, AllocaInst *stackAddr)
{
    World &world = GetWorld();
    SmallVector<std::pair<BasicBlock *, int>, 8> workList;
    uint workIdx = 0;

    int balance = 0;
    Instruction *inst = callPop;
    while (true)
    {
        if (inst != nullptr)
        {
            Instruction *prev = inst->getPrevNode();
            if (prev == nullptr)
            {
                AddPredecessors(inst->getParent(), balance, workList);
            }
            inst = prev;
        }

        if (inst == nullptr)
        {
            if (workIdx >= workList.size())
            {
                break;
            }
            auto item = workList[workIdx++];
            inst = &item.first->back();
            balance = item.second;
        }

        if (isa<CallInst>(inst))
        {
            CallInst *call = cast<CallInst>(inst);
            Function *calledFunc = call->getCalledFunction();
            if (isa<PushInst>(call))
            {
                if (balance == 0)
                {
                    if (workList.empty())
                    {
                        Value *val = cast<PushInst>(call)->getValue();
                        call->eraseFromParent();

                        callPop->replaceAllUsesWith(val);
                        callPop->eraseFromParent();
                        return nullptr;
                    }
                    else
                    {
                        if (stackAddr == nullptr)
                        {
                            assert(m_insertPt != nullptr);
                            stackAddr = new AllocaInst(world.getSizeType(), "stack.addr", m_insertPt);
                        }

                        callPop = createStubLoad(callPop, stackAddr);
                        runOnPush(call, stackAddr);
                        inst = nullptr;
                    }
                }
                else
                {
                    balance++;
                }
            }
            else if (isa<PopInst>(call))
            {
                inst = call->getNextNode();
                runOnPop(call);
            }
            else if (m_fnStubStore.get() == calledFunc)
            {
                if (balance == 0)
                {
                    AllocaInst *allocaInst = cast<AllocaInst>(call->getArgOperand(1));
                    if (stackAddr == nullptr)
                    {
                        stackAddr = allocaInst;
                    }
                    else
                    {
                        assert(stackAddr == allocaInst);
                    }

                    callPop = createStubLoad(callPop, stackAddr);
                    inst = nullptr;
                }
                else
                {
                    assert(stackAddr != cast<AllocaInst>(call->getArgOperand(1)));
                    balance++;
                }
            }
            else if (m_fnStubLoad.get() == calledFunc)
            {
                assert(stackAddr != cast<AllocaInst>(call->getArgOperand(0)));
                balance--;
            }
        }
    }
    return callPop;
}


Instruction* StackReconstructionPass::runOnPush(CallInst *callPush, AllocaInst *stackAddr)
{
    assert(stackAddr != nullptr);

    World &world = GetWorld();
    SmallVector<std::pair<BasicBlock *, int>, 8> workList;
    uint workIdx = 0;

    int balance = 0;
    Instruction *inst = callPush;
    while (true)
    {
        if (inst != nullptr)
        {
            Instruction *next = inst->getNextNode();
            if (next == nullptr)
            {
                AddSuccessors(inst->getParent(), balance, workList);
            }
            inst = next;
        }

        if (inst == nullptr)
        {
            if (workIdx >= workList.size())
            {
                break;
            }
            auto item = workList[workIdx++];
            inst = &item.first->front();
            balance = item.second;
        }

        if (isa<CallInst>(inst))
        {
            CallInst *call = cast<CallInst>(inst);
            Function *calledFunc = call->getCalledFunction();
            if (isa<PushInst>(call) || m_fnStubStore.get() == calledFunc)
            {
                assert(m_fnStubStore.get() != calledFunc || stackAddr != cast<AllocaInst>(call->getArgOperand(1)));
                balance++;
            }
            else if (isa<PopInst>(call) || m_fnStubLoad.get() == calledFunc)
            {
                if (balance == 0)
                {
                    callPush = createStubStore(callPush, stackAddr);

                    if (isa<PopInst>(call))
                    {
                        runOnPop(call, stackAddr);
                    }
                    else
                    {
                        assert(stackAddr == cast<AllocaInst>(call->getArgOperand(0)));
                    }

                    inst = nullptr;
                }
                else
                {
                    assert(m_fnStubLoad.get() != calledFunc || stackAddr != cast<AllocaInst>(call->getArgOperand(0)));
                    balance--;
                }
            }
        }
    }
    return callPush;
}


CallInst* StackReconstructionPass::createStubStore(CallInst *callPush, AllocaInst *stackAddr)
{
    if (callPush->getCalledFunction() == m_fnStubStore.get())
    {
        return callPush;
    }

    Value *val = callPush->getArgOperand(0);
    Value *args[] = { val, stackAddr };
    CallInst *stub = CallInst::Create(m_fnStubStore.get(), args, "", callPush);

    callPush->eraseFromParent();
    return stub;
}


CallInst* StackReconstructionPass::createStubLoad(CallInst *callPop, AllocaInst *stackAddr)
{
    if (callPop->getCalledFunction() == m_fnStubLoad.get())
    {
        return callPop;
    }

    CallInst *stub = CallInst::Create(m_fnStubLoad.get(), stackAddr, "", callPop);
    callPop->replaceAllUsesWith(stub);

    callPop->eraseFromParent();
    return stub;
}


uint StackReconstructionPass::countStores(AllocaInst &inst, Constant *&c)
{
    uint storeCount = 0;
    c = nullptr;
    for (User *user : inst.users())
    {
        CallInst *call = cast<CallInst>(user);
        if (call->getCalledFunction() == m_fnStubStore.get())
        {
            if (++storeCount > 1)
            {
                if (c == nullptr || c != dyn_cast<Constant>(call->getArgOperand(0)))
                {
                    return (uint)-1;
                }
            }
            else
            {
                c = dyn_cast<Constant>(call->getArgOperand(0));
            }
        }
    }
    return storeCount;
}


void StackReconstructionPass::eliminateRedundantStores()
{
    for (auto u = m_fnStubStore->user_begin(), e = m_fnStubStore->user_end(); u != e;)
    {
        CallInst *callStore = cast<CallInst>(*u);
        ++u;

        Instruction *prev = callStore->getPrevNode();
        if (prev != nullptr && isa<CallInst>(prev))
        {
            CallInst *prevCall = cast<CallInst>(prev);
            if (prevCall->getCalledFunction() == m_fnStubLoad.get())
            {
                Value *val = callStore->getArgOperand(0);
                if (val == prevCall)
                {
                    AllocaInst *storeAddr = cast<AllocaInst>(callStore->getArgOperand(1));
                    AllocaInst *loadAddr = cast<AllocaInst>(prevCall->getArgOperand(0));
                    if (storeAddr == loadAddr)
                    {
                        callStore->eraseFromParent();
                    }
                }
            }
        }
    }
}


void StackReconstructionPass::mutateStubs()
{
    while (!m_fnStubStore->user_empty())
    {
        CallInst *callStub = cast<CallInst>(m_fnStubStore->user_back());

        Value *val = callStub->getArgOperand(0);
        AllocaInst *stackAddr = cast<AllocaInst>(callStub->getArgOperand(1));

        Constant *c;
        uint stores = countStores(*stackAddr, c);
        if (stores == 1)
        {
            callStub->eraseFromParent();
            while (!stackAddr->user_empty())
            {
                callStub = cast<CallInst>(stackAddr->user_back());
                callStub->replaceAllUsesWith(val);
                callStub->eraseFromParent();
            }
            stackAddr->eraseFromParent();
        }
        else if (c != nullptr)
        {
            while (!stackAddr->user_empty())
            {
                callStub = cast<CallInst>(stackAddr->user_back());
                if (callStub->getCalledFunction() == m_fnStubLoad.get())
                {
                    callStub->replaceAllUsesWith(val);
                }
                callStub->eraseFromParent();
            }
            stackAddr->eraseFromParent();
        }
        else
        {
            for (auto u = stackAddr->user_begin(), e = stackAddr->user_end(); u != e;)
            {
                callStub = cast<CallInst>(*u);
                ++u;

                if (callStub->getCalledFunction() == m_fnStubStore.get())
                {
                    new StoreInst(val, stackAddr, callStub);
                }
                else
                {
                    LoadInst *load = new LoadInst(stackAddr, "", callStub);
                    callStub->replaceAllUsesWith(load);
                }
                callStub->eraseFromParent();
            }
        }
    }
    assert(m_fnStubLoad->user_empty());
}


bool StackReconstructionPass::verifyFunction(llvm::Function &func)
{
    std::map<BasicBlock *, int> balances;
    for (BasicBlock &bb : func)
    {
        int balance = calcBasicBlockBalance(bb);
        balances[&bb] = balance;
    }

    std::set<llvm::BasicBlock *> visits;
    visits.insert(&func.getEntryBlock());
    return verifyPath(func.getEntryBlock(), 0, balances, visits);
}


int StackReconstructionPass::calcBasicBlockBalance(llvm::BasicBlock &bb)
{
    int balance = 0;
    World &world = GetWorld();
    for (Instruction &inst : bb)
    {
        if (isa<PushInst>(&inst))
        {
            balance++;
        }
        else if (isa<PopInst>(&inst))
        {
            balance--;
        }
    }
    return balance;
}


bool StackReconstructionPass::verifyPath(llvm::BasicBlock &bb, int balance, const std::map<llvm::BasicBlock *, int> &balances, std::set<llvm::BasicBlock *> &visits)
{
    balance += balances.find(&bb)->second;
    succ_iterator si = succ_begin(&bb), se = succ_end(&bb);
    if (si == se)
    {
        return (balance >= 0);
    }

    for (; si != se; ++si)
    {
        BasicBlock *succ = *si;
        auto i = visits.insert(succ); // To prevent loops!
        if (i.second)
        {
            if (!verifyPath(*succ, balance, balances, visits))
            {
                return false;
            }
            visits.erase(i.first);
        }
    }
    return true;
}


END_NAMESPACE_SCI
