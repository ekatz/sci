//===- Passes/SplitSendPass.hpp -------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_PASSES_SPLITSENDPASS_HPP
#define SCI_UTILS_PMACHINE_LLVM_PASSES_SPLITSENDPASS_HPP

#include "../Types.hpp"
#include "llvm/IR/Instructions.h"

namespace sci {

class SplitSendPass {
public:
  SplitSendPass();
  ~SplitSendPass();

  void run();

private:
  bool splitSend(llvm::CallInst *CallSend);

  std::unique_ptr<llvm::Function> FnStubSend;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_PASSES_SPLITSENDPASS_HPP
