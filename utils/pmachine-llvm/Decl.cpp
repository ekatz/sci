//===- Decl.cpp -----------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "Decl.hpp"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace sci {

Function *getFunctionDecl(Function *Orig, Module *M) {
  assert(Orig->hasName() && "Declaration must have a name!");
  Function *F = M->getFunction(Orig->getName());
  if (F == nullptr) {
    F = Function::Create(Orig->getFunctionType(), GlobalValue::ExternalLinkage,
                         Orig->getName(), M);
    F->copyAttributesFrom(Orig);

    if (!Orig->hasExternalLinkage())
      Orig->setLinkage(GlobalValue::ExternalLinkage);
  }
  return F;
}

GlobalVariable *getGlobalVariableDecl(GlobalVariable *Orig, Module *M) {
  assert(Orig->hasName() && "Declaration must have a name!");
  GlobalVariable *GV = M->getGlobalVariable(Orig->getName(), true);
  if (GV == nullptr) {
    GV = new GlobalVariable(
        *M, Orig->getType()->getElementType(), Orig->isConstant(),
        GlobalValue::ExternalLinkage, nullptr, Orig->getName(), nullptr,
        Orig->getThreadLocalMode(), Orig->getType()->getAddressSpace());
    GV->copyAttributesFrom(Orig);
    GV->setAlignment(Orig->getAlignment());

    if (!Orig->hasExternalLinkage())
      Orig->setLinkage(GlobalValue::ExternalLinkage);
  }
  return GV;
}

} // end namespace sci
