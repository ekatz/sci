//===- Property.cpp -------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "Property.hpp"
#include "SelectorTable.hpp"
#include "World.hpp"

using namespace sci;
using namespace llvm;

Property::Property(ObjID Selector, int16_t Val, Class &Cls)
    : Selector(Selector), DefaultValue(Val), Parent(Cls) {}

StringRef Property::getName() const {
  return GetWorld().getSelectorTable().getSelectorName(getSelector());
}
