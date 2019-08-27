//===- Passes/EmitScriptUtilitiesPass.hpp ---------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_PASSES_EXPANDSCRIPTIDPASS_HPP
#define SCI_UTILS_PMACHINE_LLVM_PASSES_EXPANDSCRIPTIDPASS_HPP

#include "../Intrinsics.hpp"

namespace sci {

class Script;

class EmitScriptUtilitiesPass {
public:
  EmitScriptUtilitiesPass();
  ~EmitScriptUtilitiesPass();

  void run();

private:
  void expandScriptID(CallKernelInst *Call);
  void createDisposeScriptFunctions();

  llvm::Function *getOrCreateDisposeScriptNumFunction(Script *S);
  llvm::Function *getOrCreateGetDispatchAddrFunction() const;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_PASSES_EXPANDSCRIPTIDPASS_HPP
