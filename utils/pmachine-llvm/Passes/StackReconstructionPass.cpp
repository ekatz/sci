//===- Passes/StackReconstructionPass.cpp ---------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "StackReconstructionPass.hpp"
#include "../World.hpp"
#include "llvm/IR/CFG.h"
#include "llvm/IR/CallSite.h"

using namespace sci;
using namespace llvm;

static void
addWorkItem(BasicBlock *BB, int Balance,
            SmallVectorImpl<std::pair<BasicBlock *, int>> &WorkList) {
  for (auto &P : WorkList)
    if (P.first == BB)
      return;

  WorkList.push_back(std::make_pair(BB, Balance));
}

static void
addPredecessors(BasicBlock *BB, int Balance,
                SmallVectorImpl<std::pair<BasicBlock *, int>> &WorkList) {
  for (BasicBlock *Pred : predecessors(BB))
    addWorkItem(Pred, Balance, WorkList);
}

static void
addSuccessors(BasicBlock *BB, int Balance,
              SmallVectorImpl<std::pair<BasicBlock *, int>> &WorkList) {
  for (BasicBlock *Succ : successors(BB))
    addWorkItem(Succ, Balance, WorkList);
}

static Instruction *getFirstInsertionPoint(BasicBlock &BB) {
  for (Instruction &I : BB)
    if (!isa<AllocaInst>(I))
      return &I;

  return nullptr;
}

StackReconstructionPass::StackReconstructionPass() : InsertPt(nullptr) {
  World &W = GetWorld();
  IntegerType *SizeTy = W.getSizeType();
  Type *Params[] = {SizeTy, SizeTy->getPointerTo()};
  FunctionType *FTy;

  FTy = FunctionType::get(Type::getVoidTy(W.getContext()), Params, false);
  FnStubStore.reset(
      Function::Create(FTy, GlobalValue::ExternalLinkage, "stub_store@SCI"));

  FTy = FunctionType::get(SizeTy, Params[1], false);
  FnStubLoad.reset(
      Function::Create(FTy, GlobalValue::ExternalLinkage, "stub_load@SCI"));
}

StackReconstructionPass::~StackReconstructionPass() {
  assert(!FnStubStore || FnStubStore->getNumUses() == 0);
  assert(!FnStubLoad || FnStubLoad->getNumUses() == 0);
}

void StackReconstructionPass::run() {
  Function *Caller = nullptr;
  Function *PopFunc = Intrinsic::Get(Intrinsic::pop);
  while (!PopFunc->user_empty()) {
    CallInst *Call = cast<CallInst>(PopFunc->user_back());
    Function *Func = Call->getParent()->getParent();
    if (Caller != Func) {
      Caller = Func;
      InsertPt = getFirstInsertionPoint(Func->getEntryBlock());
    }
    runOnPop(Call);
  }

  eliminateRedundantStores();
  mutateStubs();
}

Instruction *StackReconstructionPass::runOnPop(CallInst *CallPop,
                                               AllocaInst *StackAddr) {
  World &W = GetWorld();
  SmallVector<std::pair<BasicBlock *, int>, 8> WorkList;
  unsigned WorkIdx = 0;

  int Balance = 0;
  Instruction *Inst = CallPop;
  while (true) {
    if (Inst) {
      Instruction *Prev = Inst->getPrevNode();
      if (!Prev)
        addPredecessors(Inst->getParent(), Balance, WorkList);
      Inst = Prev;
    }

    if (!Inst) {
      if (WorkIdx >= WorkList.size())
        break;

      auto &Item = WorkList[WorkIdx++];
      Inst = &Item.first->back();
      Balance = Item.second;
    }

    if (isa<CallInst>(Inst)) {
      CallInst *Call = cast<CallInst>(Inst);
      Function *CalledFunc = Call->getCalledFunction();
      if (isa<PushInst>(Call)) {
        if (Balance == 0) {
          if (WorkList.empty()) {
            Value *val = cast<PushInst>(Call)->getValue();
            Call->eraseFromParent();

            CallPop->replaceAllUsesWith(val);
            CallPop->eraseFromParent();
            return nullptr;
          } else {
            if (!StackAddr) {
              assert(InsertPt);
              StackAddr =
                  new AllocaInst(W.getSizeType(), 0, "stack.addr", InsertPt);
              StackAddr->setAlignment(W.getSizeTypeAlignment());
            }

            CallPop = createStubLoad(CallPop, StackAddr);
            runOnPush(Call, StackAddr);
            Inst = nullptr;
          }
        } else
          Balance++;
      } else if (isa<PopInst>(Call)) {
        Inst = Call->getNextNode();
        runOnPop(Call);
      } else if (FnStubStore.get() == CalledFunc) {
        if (Balance == 0) {
          AllocaInst *allocaInst = cast<AllocaInst>(Call->getArgOperand(1));
          if (!StackAddr)
            StackAddr = allocaInst;
          else {
            assert(StackAddr == allocaInst);
          }

          CallPop = createStubLoad(CallPop, StackAddr);
          Inst = nullptr;
        } else {
          assert(StackAddr != cast<AllocaInst>(Call->getArgOperand(1)));
          Balance++;
        }
      } else if (FnStubLoad.get() == CalledFunc) {
        assert(StackAddr != cast<AllocaInst>(Call->getArgOperand(0)));
        Balance--;
      }
    }
  }
  return CallPop;
}

Instruction *StackReconstructionPass::runOnPush(CallInst *CallPush,
                                                AllocaInst *StackAddr) {
  assert(StackAddr);

  World &W = GetWorld();
  SmallVector<std::pair<BasicBlock *, int>, 8> WorkList;
  unsigned WorkIdx = 0;

  int Balance = 0;
  Instruction *Inst = CallPush;
  while (true) {
    if (Inst) {
      Instruction *Next = Inst->getNextNode();
      if (!Next)
        addSuccessors(Inst->getParent(), Balance, WorkList);
      Inst = Next;
    }

    if (!Inst) {
      if (WorkIdx >= WorkList.size())
        break;

      auto &Item = WorkList[WorkIdx++];
      Inst = &Item.first->front();
      Balance = Item.second;
    }

    if (isa<CallInst>(Inst)) {
      CallInst *Call = cast<CallInst>(Inst);
      Function *CalledFunc = Call->getCalledFunction();
      if (isa<PushInst>(Call) || FnStubStore.get() == CalledFunc) {
        assert(FnStubStore.get() != CalledFunc ||
               StackAddr != cast<AllocaInst>(Call->getArgOperand(1)));
        Balance++;
      } else if (isa<PopInst>(Call) || FnStubLoad.get() == CalledFunc) {
        if (Balance == 0) {
          CallPush = createStubStore(CallPush, StackAddr);

          if (isa<PopInst>(Call))
            runOnPop(Call, StackAddr);
          else {
            assert(StackAddr == cast<AllocaInst>(Call->getArgOperand(0)));
          }

          Inst = nullptr;
        } else {
          assert(FnStubLoad.get() != CalledFunc ||
                 StackAddr != cast<AllocaInst>(Call->getArgOperand(0)));
          Balance--;
        }
      }
    }
  }
  return CallPush;
}

CallInst *StackReconstructionPass::createStubStore(CallInst *CallPush,
                                                   AllocaInst *StackAddr) {
  if (CallPush->getCalledFunction() == FnStubStore.get())
    return CallPush;

  Value *Val = CallPush->getArgOperand(0);
  Value *Args[] = {Val, StackAddr};
  CallInst *Stub = CallInst::Create(FnStubStore.get(), Args, "", CallPush);

  CallPush->eraseFromParent();
  return Stub;
}

CallInst *StackReconstructionPass::createStubLoad(CallInst *CallPop,
                                                  AllocaInst *StackAddr) {
  if (CallPop->getCalledFunction() == FnStubLoad.get()) {
    return CallPop;
  }

  CallInst *Stub = CallInst::Create(FnStubLoad.get(), StackAddr, "", CallPop);
  CallPop->replaceAllUsesWith(Stub);

  CallPop->eraseFromParent();
  return Stub;
}

unsigned StackReconstructionPass::countStores(AllocaInst &Inst, Constant *&C) {
  unsigned StoreCount = 0;
  C = nullptr;
  for (User *U : Inst.users()) {
    CallInst *Call = cast<CallInst>(U);
    if (Call->getCalledFunction() == FnStubStore.get()) {
      if (++StoreCount > 1) {
        if (!C || C != dyn_cast<Constant>(Call->getArgOperand(0)))
          return -1U;
      } else
        C = dyn_cast<Constant>(Call->getArgOperand(0));
    }
  }
  return StoreCount;
}

void StackReconstructionPass::eliminateRedundantStores() {
  for (auto U = FnStubStore->user_begin(), E = FnStubStore->user_end();
       U != E;) {
    CallInst *CallStore = cast<CallInst>(*U);
    ++U;

    Instruction *Prev = CallStore->getPrevNode();
    if (Prev && isa<CallInst>(Prev)) {
      CallInst *PrevCall = cast<CallInst>(Prev);
      if (PrevCall->getCalledFunction() == FnStubLoad.get()) {
        Value *Val = CallStore->getArgOperand(0);
        if (Val == PrevCall) {
          AllocaInst *StoreAddr = cast<AllocaInst>(CallStore->getArgOperand(1));
          AllocaInst *LoadAddr = cast<AllocaInst>(PrevCall->getArgOperand(0));
          if (StoreAddr == LoadAddr)
            CallStore->eraseFromParent();
        }
      }
    }
  }
}

void StackReconstructionPass::mutateStubs() {
  while (!FnStubStore->user_empty()) {
    CallInst *CallStub = cast<CallInst>(FnStubStore->user_back());

    Value *Val = CallStub->getArgOperand(0);
    AllocaInst *StackAddr = cast<AllocaInst>(CallStub->getArgOperand(1));

    Constant *C;
    unsigned Stores = countStores(*StackAddr, C);

    // If there is only 1 store.
    if (Stores == 1) {
      CallStub->eraseFromParent();
      while (!StackAddr->user_empty()) {
        CallStub = cast<CallInst>(StackAddr->user_back());
        CallStub->replaceAllUsesWith(Val);
        CallStub->eraseFromParent();
      }
      StackAddr->eraseFromParent();
    }
    // If there is a constant value common to all stores.
    else if (C) {
      while (!StackAddr->user_empty()) {
        CallStub = cast<CallInst>(StackAddr->user_back());
        if (CallStub->getCalledFunction() == FnStubLoad.get())
          CallStub->replaceAllUsesWith(Val);
        CallStub->eraseFromParent();
      }
      StackAddr->eraseFromParent();
    } else {
      for (auto U = StackAddr->user_begin(), E = StackAddr->user_end();
           U != E;) {
        CallStub = cast<CallInst>(*U);
        ++U;

        if (CallStub->getCalledFunction() == FnStubStore.get()) {
          Val = CallStub->getArgOperand(0);
          new StoreInst(Val, StackAddr, false, GetWorld().getTypeAlignment(Val),
                        CallStub);
        } else { // (callStub->getCalledFunction() == m_fnStubLoad.get())
          LoadInst *Load = new LoadInst(
              StackAddr, "", false,
              GetWorld().getTypeAlignment(StackAddr->getAllocatedType()),
              CallStub);
          CallStub->replaceAllUsesWith(Load);
        }
        CallStub->eraseFromParent();
      }
    }
  }
  assert(FnStubLoad->user_empty());
}

bool StackReconstructionPass::verifyFunction(llvm::Function &Func) {
  std::map<BasicBlock *, int> Balances;
  for (BasicBlock &BB : Func) {
    int Balance = calcBasicBlockBalance(BB);
    Balances[&BB] = Balance;
  }

  std::set<llvm::BasicBlock *> Visits;
  Visits.insert(&Func.getEntryBlock());
  return verifyPath(Func.getEntryBlock(), 0, Balances, Visits);
}

int StackReconstructionPass::calcBasicBlockBalance(llvm::BasicBlock &BB) {
  int Balance = 0;
  World &W = GetWorld();
  for (Instruction &Inst : BB) {
    if (isa<PushInst>(&Inst))
      Balance++;
    else if (isa<PopInst>(&Inst))
      Balance--;
  }
  return Balance;
}

bool StackReconstructionPass::verifyPath(
    llvm::BasicBlock &BB, int Balance,
    const std::map<llvm::BasicBlock *, int> &Balances,
    std::set<llvm::BasicBlock *> &Visits) {
  Balance += Balances.find(&BB)->second;
  succ_iterator SI = succ_begin(&BB), SE = succ_end(&BB);
  if (SI == SE)
    return (Balance >= 0);

  for (; SI != SE; ++SI) {
    BasicBlock *succ = *SI;
    auto I = Visits.insert(succ); // To prevent loops!
    if (I.second) {
      if (!verifyPath(*succ, Balance, Balances, Visits))
        return false;
      Visits.erase(I.first);
    }
  }
  return true;
}
