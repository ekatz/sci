//===- Passes/FixCodePass.hpp ---------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_PASSES_FIXCODEPASS_HPP
#define SCI_UTILS_PMACHINE_LLVM_PASSES_FIXCODEPASS_HPP

#include "../Types.hpp"
#include "llvm/IR/Module.h"

namespace sci {

class FixCodePass {
public:
  FixCodePass();
  ~FixCodePass();

  void run();

private:
  llvm::Function *createIsKindOfFunctionPrototype(llvm::Module *M) const;
  llvm::Function *createIsMemberOfFunctionPrototype(llvm::Module *M) const;
  llvm::Function *createObjMethodFunction(llvm::Module *M,
                                          llvm::Function *ExternFunc) const;

  llvm::IntegerType *SizeTy;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_PASSES_FIXCODEPASS_HPP
