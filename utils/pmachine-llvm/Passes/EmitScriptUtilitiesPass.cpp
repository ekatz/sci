//===- Passes/EmitScriptUtilitiesPass.cpp ---------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "EmitScriptUtilitiesPass.hpp"
#include "../Decl.hpp"
#include "../Object.hpp"
#include "../Script.hpp"
#include "../World.hpp"
#include "llvm/IR/IRBuilder.h"

using namespace sci;
using namespace llvm;

EmitScriptUtilitiesPass::EmitScriptUtilitiesPass() {}

EmitScriptUtilitiesPass::~EmitScriptUtilitiesPass() {}

void EmitScriptUtilitiesPass::run() {
  World &W = GetWorld();

  Function *F = W.getIntrinsic(Intrinsic::callk);
  auto UI = F->user_begin();
  while (UI != F->user_end()) {
    CallKernelInst *Call = cast<CallKernelInst>(*UI);
    ++UI;

    if (static_cast<unsigned>(Call->getKernelOrdinal()->getZExtValue()) ==
        KORDINAL_ScriptID)
      expandScriptID(Call);
  }

  createDisposeScriptFunctions();
}

void EmitScriptUtilitiesPass::expandScriptID(CallKernelInst *Call) {
  unsigned Argc = static_cast<unsigned>(
      cast<ConstantInt>(Call->getArgCount())->getZExtValue());
  assert((Argc == 1 || Argc == 2) && "Invalid KScriptID call!");

  unsigned EntryIndex =
      (Argc == 2)
          ? static_cast<unsigned>(
                cast<ConstantInt>(Call->getArgOperand(1))->getZExtValue())
          : 0;

  Value *V;
  World &W = GetWorld();
  Value *ScriptIDVal = Call->getArgOperand(0);
  if (isa<ConstantInt>(ScriptIDVal)) {
    unsigned ScriptID =
        static_cast<unsigned>(cast<ConstantInt>(ScriptIDVal)->getZExtValue());

    Script *S = W.getScript(ScriptID);
    assert(S != nullptr && "Invalid script ID.");

    GlobalVariable *Var = cast<GlobalVariable>(S->getExportedValue(EntryIndex));
    Var = getGlobalVariableDecl(Var, Call->getModule());
    V = new PtrToIntInst(Var, W.getSizeType(), "", Call);
  } else {
    assert(EntryIndex == 0 &&
           "Non-constant Script ID is not supported without entry index 0!");
    Function *F = getOrCreateGetDispatchAddrFunction();
    F = getFunctionDecl(F, Call->getModule());

    IntegerType *Int32Ty = Type::getInt32Ty(W.getContext());
    if (ScriptIDVal->getType() != Int32Ty)
      ScriptIDVal = new TruncInst(ScriptIDVal, Int32Ty, "", Call);
    V = CallInst::Create(F, {ScriptIDVal, Constant::getNullValue(Int32Ty)}, "",
                         Call);
  }

  Call->replaceAllUsesWith(V);
  Call->eraseFromParent();
}

void EmitScriptUtilitiesPass::createDisposeScriptFunctions() {
  World &W = GetWorld();
  LLVMContext &Ctx = W.getContext();
  IntegerType *SizeTy = W.getSizeType();
  Module *MainModule = W.getScript(0)->getModule();

  FunctionType *FTy;

  FTy = FunctionType::get(Type::getVoidTy(Ctx), false);
  Function *DisposeAllFunc = Function::Create(FTy, GlobalValue::ExternalLinkage,
                                              "DisposeAllScripts", MainModule);
  BasicBlock *AllBB = BasicBlock::Create(Ctx, "entry", DisposeAllFunc);

  FTy = FunctionType::get(Type::getVoidTy(Ctx), SizeTy, false);
  Function *DisposeFunc = Function::Create(FTy, GlobalValue::ExternalLinkage,
                                           "DisposeScript", MainModule);
  BasicBlock *EntryBB = BasicBlock::Create(Ctx, "entry", DisposeFunc);

  BasicBlock *UnknownBB = BasicBlock::Create(Ctx, "case.unknown", DisposeFunc);
  ReturnInst::Create(Ctx, UnknownBB);

  SwitchInst *SwitchScript = SwitchInst::Create(
      &*DisposeFunc->arg_begin(), UnknownBB, W.getScriptCount(), EntryBB);

  for (Script &S : W.scripts()) {
    Function *F = getOrCreateDisposeScriptNumFunction(&S);
    F = getFunctionDecl(F, MainModule);

    CallInst::Create(F, "", AllBB);

    BasicBlock *CaseBB = BasicBlock::Create(
        Ctx, "case." + S.getModule()->getName(), DisposeFunc, UnknownBB);
    CallInst::Create(F, "", CaseBB);
    ReturnInst::Create(Ctx, CaseBB);

    SwitchScript->addCase(ConstantInt::get(SizeTy, S.getId()), CaseBB);
  }

  ReturnInst::Create(Ctx, AllBB);
}

llvm::Function *
EmitScriptUtilitiesPass::getOrCreateDisposeScriptNumFunction(Script *S) {
  World &W = GetWorld();
  LLVMContext &Ctx = W.getContext();
  IntegerType *SizeTy = W.getSizeType();
  Module *M = S->getModule();

  char Name[32];
  sprintf(Name, "DisposeScript%03u", S->getId());

  Function *F = M->getFunction(Name);
  if (F)
    return F;

  FunctionType *FTy = FunctionType::get(Type::getVoidTy(Ctx), false);
  F = Function::Create(FTy, GlobalValue::ExternalLinkage, Name, M);
  BasicBlock *BB = BasicBlock::Create(Ctx, "entry", F);

  for (GlobalVariable &Var : M->globals()) {
    if (Var.isConstant() || !Var.hasInitializer())
      continue;

    Constant *C = Var.getInitializer();
    Type *Ty = Var.getType()->getElementType();
    GlobalVariable *Init =
        new GlobalVariable(*M, Ty, true, GlobalValue::PrivateLinkage, C);
    Init->setAlignment(Var.getAlignment());

    IRBuilder<>(BB).CreateMemCpy(&Var, Var.getAlignment(), Init,
                                 Init->getAlignment(),
                                 W.getDataLayout().getTypeAllocSize(Ty));
  }

  ReturnInst::Create(Ctx, BB);
  return F;
}

Function *EmitScriptUtilitiesPass::getOrCreateGetDispatchAddrFunction() const {
  StringRef Name = "GetDispatchAddr";
  World &W = GetWorld();
  Module *MainModule = W.getScript(0)->getModule();
  Function *F = MainModule->getFunction(Name);
  if (F)
    return F;

  LLVMContext &Ctx = W.getContext();
  IntegerType *SizeTy = W.getSizeType();
  IntegerType *Int32Ty = Type::getInt32Ty(Ctx);

  FunctionType *FTy = FunctionType::get(SizeTy, {Int32Ty, Int32Ty}, false);
  F = Function::Create(FTy, GlobalValue::InternalLinkage, Name, MainModule);

  BasicBlock *EntryBB = BasicBlock::Create(Ctx, "entry", F);

  BasicBlock *UnknownBB = BasicBlock::Create(Ctx, "case.unknown", F);
  ReturnInst::Create(Ctx, Constant::getNullValue(SizeTy), UnknownBB);

  SwitchInst *SwitchScript = SwitchInst::Create(&*F->arg_begin(), UnknownBB,
                                                W.getScriptCount(), EntryBB);

  for (Script &S : W.scripts()) {
    GlobalVariable *Var =
        dyn_cast_or_null<GlobalVariable>(S.getExportedValue(0));
    if (Var) {
      Var = getGlobalVariableDecl(Var, MainModule);
      BasicBlock *CaseBB = BasicBlock::Create(
          Ctx, "case." + S.getModule()->getName(), F, UnknownBB);
      Value *V = new PtrToIntInst(Var, SizeTy, "", CaseBB);
      ReturnInst::Create(Ctx, V, CaseBB);

      SwitchScript->addCase(ConstantInt::get(Int32Ty, S.getId()), CaseBB);
    }
  }
  return F;
}
