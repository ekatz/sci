//===- Passes/FixCodePass.cpp ---------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "FixCodePass.hpp"
#include "../Method.hpp"
#include "../Object.hpp"
#include "../Resource.hpp"
#include "../Script.hpp"
#include "../World.hpp"
#include "sci/Kernel/Selector.h"

using namespace sci;
using namespace llvm;

FixCodePass::FixCodePass() : SizeTy(GetWorld().getSizeType()) {}

FixCodePass::~FixCodePass() {}

void FixCodePass::run() {
  World &W = GetWorld();

  Object *Obj = W.getClass(0);
  Method *Mtd;
  Function *Func, *NewFunc;
  Module *Mod;

  Mtd = Obj->getMethod(s_isKindOf);
  Func = Mtd->getFunction();
  Mod = Func->getParent();

  NewFunc = createObjMethodFunction(Mod, createIsKindOfFunctionPrototype(Mod));

  NewFunc->takeName(Func);
  Func->replaceAllUsesWith(NewFunc);
  Func->eraseFromParent();

  Mtd = Obj->getMethod(s_isMemberOf);
  Func = Mtd->getFunction();
  Mod = Func->getParent();

  NewFunc =
      createObjMethodFunction(Mod, createIsMemberOfFunctionPrototype(Mod));

  NewFunc->takeName(Func);
  Func->replaceAllUsesWith(NewFunc);
  Func->eraseFromParent();
}

Function *FixCodePass::createIsKindOfFunctionPrototype(Module *M) const {
  Type *Params[] = {SizeTy, SizeTy};
  FunctionType *FTy =
      FunctionType::get(Type::getInt1Ty(SizeTy->getContext()), Params, false);

  return Function::Create(FTy, GlobalValue::ExternalLinkage, "IsKindOf", M);
}

Function *FixCodePass::createIsMemberOfFunctionPrototype(Module *M) const {
  Type *Params[] = {SizeTy, SizeTy};
  FunctionType *funcTy =
      FunctionType::get(Type::getInt1Ty(SizeTy->getContext()), Params, false);

  return Function::Create(funcTy, GlobalValue::ExternalLinkage, "IsMemberOf",
                          M);
}

Function *FixCodePass::createObjMethodFunction(Module *M,
                                               Function *ExternFunc) const {
  World &W = GetWorld();
  LLVMContext &Ctx = W.getContext();

  Type *Params[] = {SizeTy, SizeTy->getPointerTo()};
  FunctionType *FTy = FunctionType::get(SizeTy, Params, false);
  Function *F = Function::Create(FTy, GlobalValue::LinkOnceODRLinkage, "", M);

  BasicBlock *BB = BasicBlock::Create(Ctx, "entry", F);

  auto AI = F->arg_begin();
  Value *Obj1Var = &*AI;
  ConstantInt *C = ConstantInt::get(SizeTy, 1);
  Value *Obj2Var = GetElementPtrInst::CreateInBounds(&*++AI, C, "", BB);
  Obj2Var = new LoadInst(Obj2Var, "", false, W.getSizeTypeAlignment(), BB);

  Value *Args[] = {Obj1Var, Obj2Var};
  Value *V = CallInst::Create(ExternFunc, Args, "", BB);
  V = new ZExtInst(V, SizeTy, "", BB);
  ReturnInst::Create(Ctx, V, BB);
  return F;
}
