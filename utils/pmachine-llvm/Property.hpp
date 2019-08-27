//===- Property.hpp -------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_PROPERTY_HPP
#define SCI_UTILS_PMACHINE_LLVM_PROPERTY_HPP

#include "Types.hpp"

namespace sci {

class Class;

class Property {
public:
  Property(ObjID Selector, int16_t Val, Class &Cls);

  int16_t getDefaultValue() const { return DefaultValue; }
  unsigned getSelector() const { return selector_cast<unsigned>(Selector); }
  Class &getClass() const { return Parent; }

  StringRef getName() const;

private:
  const ObjID Selector;
  const int16_t DefaultValue;
  Class &Parent;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_PROPERTY_HPP
