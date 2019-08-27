//===- Passes/StackReconstructionPass.hpp ---------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_PASSES_STACKRECONSTRUCTIONPASS_HPP
#define SCI_UTILS_PMACHINE_LLVM_PASSES_STACKRECONSTRUCTIONPASS_HPP

#include "../Types.hpp"
#include "llvm/IR/Instructions.h"
#include <set>

namespace sci {

class StackReconstructionPass {
public:
  StackReconstructionPass();
  ~StackReconstructionPass();

  void run();

  bool verifyFunction(llvm::Function &Func);
  int calcBasicBlockBalance(llvm::BasicBlock &BB);

private:
  llvm::Instruction *runOnPop(llvm::CallInst *CallPop,
                              llvm::AllocaInst *StackAddr = nullptr);
  llvm::Instruction *runOnPush(llvm::CallInst *CallPush,
                               llvm::AllocaInst *StackAddr = nullptr);

  llvm::CallInst *createStubStore(llvm::CallInst *CallPush,
                                  llvm::AllocaInst *StackAddr);
  llvm::CallInst *createStubLoad(llvm::CallInst *CallPop,
                                 llvm::AllocaInst *StackAddr);

  unsigned countStores(llvm::AllocaInst &iInst, llvm::Constant *&C);
  void eliminateRedundantStores();
  void mutateStubs();

  bool verifyPath(llvm::BasicBlock &BB, int Balance,
                  const std::map<llvm::BasicBlock *, int> &Balances,
                  std::set<llvm::BasicBlock *> &Visits);

  llvm::Instruction *InsertPt;
  std::unique_ptr<llvm::Function> FnStubStore;
  std::unique_ptr<llvm::Function> FnStubLoad;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_PASSES_STACKRECONSTRUCTIONPASS_HPP
