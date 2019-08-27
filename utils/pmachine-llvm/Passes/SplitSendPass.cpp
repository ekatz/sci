//===- Passes/SplitSendPass.cpp -------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "SplitSendPass.hpp"
#include "../World.hpp"
#include "llvm/IR/Instructions.h"

using namespace sci;
using namespace llvm;

static std::string createCallInstName(Value *Sel) {
  std::string Name;
  if (isa<ConstantInt>(Sel)) {
    Name = GetWorld().getSelectorTable().getSelectorName(
        static_cast<unsigned>(cast<ConstantInt>(Sel)->getSExtValue()));
    if (!Name.empty())
      Name = "res@" + Name;
  }
  return Name;
}

SplitSendPass::SplitSendPass() {
  World &W = GetWorld();
  IntegerType *SizeTy = W.getSizeType();
  Type *Params[] = {SizeTy, SizeTy, SizeTy};

  FunctionType *FTy = FunctionType::get(SizeTy, Params, true);
  FnStubSend.reset(
      Function::Create(FTy, GlobalValue::ExternalLinkage, "stub_send@SCI"));
}

SplitSendPass::~SplitSendPass() {
  assert(!FnStubSend || FnStubSend->getNumUses() == 0);
}

void SplitSendPass::run() {
  Function *SendFunc = Intrinsic::Get(Intrinsic::send);
  for (auto U = SendFunc->user_begin(), E = SendFunc->user_end(); U != E;) {
    CallInst *Call = cast<CallInst>(*U);
    ++U;

    splitSend(Call);
  }

  FnStubSend->replaceAllUsesWith(SendFunc);
}

bool SplitSendPass::splitSend(CallInst *CallSend) {
  ConstantInt *Argc = cast<ConstantInt>(CallSend->getArgOperand(2));
  unsigned N = static_cast<unsigned>(Argc->getZExtValue());

  auto AE = CallSend->arg_end() - 1;
  Value *Rest = nullptr;
  if (isa<RestInst>(*AE)) {
    Rest = *AE;
    --AE;
    ++N;
  }

  if (CallSend->getNumArgOperands() == (N + 3)) {
    if (!CallSend->hasName()) {
      Value *Sel = CallSend->getArgOperand(1);
      std::string Name = createCallInstName(Sel);
      if (!Name.empty())
        CallSend->setName(Name);
    }
    return false;
  }

  SmallVector<Value *, 16> Args;
  auto AI = CallSend->arg_begin();

  Value *Obj = *AI;
  Value *Acc = nullptr;

  while (AI != AE) {
    Args.clear();

    Value *Sel = *++AI;
    Argc = cast<ConstantInt>(*++AI);

    Args.push_back(Obj);
    Args.push_back(Sel);
    Args.push_back(Argc);

    N = static_cast<unsigned>(Argc->getZExtValue());
    for (unsigned I = 0; I < N; ++I)
      Args.push_back(*++AI);

    if (Rest) {
      Args.push_back(Rest);
      Rest = nullptr;
    }

    std::string Name = createCallInstName(Sel);
    Acc = CallInst::Create(FnStubSend.get(), Args, Name, CallSend);
  }

  CallSend->replaceAllUsesWith(Acc);
  CallSend->eraseFromParent();
  return true;
}
