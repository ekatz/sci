//===- Method.cpp ---------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "Method.hpp"
#include "Class.hpp"
#include "PMachine.hpp"
#include "SelectorTable.hpp"
#include "World.hpp"

using namespace sci;
using namespace llvm;

Method::Method(ObjID Selector, uint16_t Offset, Class &Parent)
    : Procedure(Selector, Offset, Parent.getScript()), Parent(Parent) {}

StringRef Method::getName() const {
  return GetWorld().getSelectorTable().getSelectorName(getSelector());
}

Function *Method::load() {
  if (!Func) {
    std::string Name = getName();
    Name += '@';
    Name += Parent.getName();
    Procedure::load(Name, &Parent);
  }
  return Func;
}
