//===- Passes/TranslateClassIntrinsicPass.hpp -----------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_PASSES_TRANSLATECLASSINTRINSICPASS_HPP
#define SCI_UTILS_PMACHINE_LLVM_PASSES_TRANSLATECLASSINTRINSICPASS_HPP

#include "../Intrinsics.hpp"
#include "../Types.hpp"

namespace sci {

class TranslateClassIntrinsicPass {
public:
  TranslateClassIntrinsicPass();
  ~TranslateClassIntrinsicPass();

  void run();

private:
  void translate(ClassInst *Call);

  llvm::IntegerType *SizeTy;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_PASSES_TRANSLATECLASSINTRINSICPASS_HPP
