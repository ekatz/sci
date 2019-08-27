//===- Decl.hpp -----------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_DECL_HPP
#define SCI_UTILS_PMACHINE_LLVM_DECL_HPP

#include "Types.hpp"
#include "llvm/IR/Function.h"

namespace sci {

llvm::Function *getFunctionDecl(llvm::Function *Orig, llvm::Module *M);
llvm::GlobalVariable *getGlobalVariableDecl(llvm::GlobalVariable *Orig,
                                            llvm::Module *M);

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_DECL_HPP
