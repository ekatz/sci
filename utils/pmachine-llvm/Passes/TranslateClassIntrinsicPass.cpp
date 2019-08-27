//===- Passes/TranslateClassIntrinsicPass.cpp -----------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "TranslateClassIntrinsicPass.hpp"
#include "../Decl.hpp"
#include "../Object.hpp"
#include "../World.hpp"
#include "llvm/IR/GlobalVariable.h"

using namespace sci;
using namespace llvm;

TranslateClassIntrinsicPass::TranslateClassIntrinsicPass()
    : SizeTy(GetWorld().getSizeType()) {}

TranslateClassIntrinsicPass::~TranslateClassIntrinsicPass() {}

void TranslateClassIntrinsicPass::run() {
  Function *Func = GetWorld().getIntrinsic(Intrinsic::clss);
  while (!Func->user_empty()) {
    ClassInst *Call = cast<ClassInst>(Func->user_back());
    translate(Call);
  }
}

void TranslateClassIntrinsicPass::translate(ClassInst *Call) {
  unsigned ClassID = static_cast<unsigned>(Call->getClassID()->getZExtValue());
  Object *Cls = GetWorld().getClass(ClassID);
  assert(Cls != nullptr && "Invalid class ID.");

  GlobalVariable *Var =
      getGlobalVariableDecl(Cls->getGlobal(), Call->getModule());
  Value *Val = new PtrToIntInst(Var, SizeTy, "", Call);

  Call->replaceAllUsesWith(Val);
  Call->eraseFromParent();
}
