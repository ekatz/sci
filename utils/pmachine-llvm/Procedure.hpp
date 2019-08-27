//===- Procedure.hpp ------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_PROCEDURE_HPP
#define SCI_UTILS_PMACHINE_LLVM_PROCEDURE_HPP

#include "Types.hpp"
#include "llvm/IR/Function.h"

namespace sci {

class Script;
class Class;
class Method;

class Procedure {
public:
  Procedure(uint16_t Offset, Script &S);

  llvm::Function *load();

  llvm::Function *getFunction() const { return Func; }
  unsigned getOffset() const { return Offset; }

  bool isMethod() const;
  const Method *asMethod() const;
  Method *asMethod();

  Script &getScript() const { return TheScript; }

protected:
  Procedure(ObjID Selector, uint16_t Offset, Script &S);
  llvm::Function *load(StringRef Name, Class *Cls = nullptr);

  const ObjID Selector;
  const uint16_t Offset;

  llvm::Function *Func;
  Script &TheScript;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_PROCEDURE_HPP
