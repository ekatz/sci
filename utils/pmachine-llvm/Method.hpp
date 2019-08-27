//===- Method.hpp ---------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_METHOD_HPP
#define SCI_UTILS_PMACHINE_LLVM_METHOD_HPP

#include "Procedure.hpp"

namespace sci {

class Class;

class Method : public Procedure {
public:
  Method(ObjID Selector, uint16_t Offset, Class &Parent);

  llvm::Function *load();

  unsigned getSelector() const { return selector_cast<unsigned>(Selector); }
  Class &getClass() const { return Parent; }

  StringRef getName() const;

private:
  Class &Parent;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_METHOD_HPP
