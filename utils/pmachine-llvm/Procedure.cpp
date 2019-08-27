//===- Procedure.cpp ------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "Procedure.hpp"
#include "Method.hpp"
#include "PMachine.hpp"
#include "Script.hpp"

using namespace sci;
using namespace llvm;

Procedure::Procedure(uint16_t Offset, Script &Script)
    : Procedure((ObjID)-1, Offset, Script) {}

Procedure::Procedure(ObjID Selector, uint16_t Offset, Script &S)
    : Selector(Selector), Offset(Offset), Func(nullptr), TheScript(S) {}

Function *Procedure::load() {
  if (!Func) {
    std::string Name = "proc@";
    Name += utohexstr(Offset, true);
    Name += '@';
    Name += utostr(TheScript.getId());
    load(Name);
  }
  return Func;
}

Function *Procedure::load(StringRef Name, Class *Cls) {
  PMachine Machine(TheScript);
  const uint8_t *Code =
      reinterpret_cast<const uint8_t *>(TheScript.getDataAt(Offset));
  Func = Machine.interpretFunction(Code, Name,
                                   selector_cast<unsigned>(Selector), Cls);

  if (Func)
    GetWorld().registerProcedure(*this);
  return Func;
}

bool Procedure::isMethod() const { return (Selector != -1U); }

const Method *Procedure::asMethod() const {
  return isMethod() ? static_cast<const Method *>(this) : nullptr;
}

Method *Procedure::asMethod() {
  return isMethod() ? static_cast<Method *>(this) : nullptr;
}
