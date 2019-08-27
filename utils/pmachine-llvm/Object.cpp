//===- Object.cpp ---------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "Object.hpp"
#include "Property.hpp"
#include "Resource.hpp"
#include "Script.hpp"
#include "World.hpp"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

using namespace sci;
using namespace llvm;

Object::Object(const ObjRes &Res, Script &S) : Class(Res, S), Global(nullptr) {
  assert(Res.isClass() || getMethodCount() == getSuper()->getMethodCount());

  Global =
      new GlobalVariable(getModule(), Ty, false, GlobalValue::InternalLinkage,
                         nullptr, Ty->getName());
  Global->setAlignment(16);
  setInitializer();
}

Object::~Object() {}

unsigned Object::getId() const { return TheScript.getObjectId(*this); }

StringRef Object::getName() const { return Global->getName(); }

void Object::setInitializer() const {
  // Make one room for the species.
  const Property *Ptr = Props - 1;
  ConstantStruct *InitStruct = createInitializer(
      Ty, Props + (ObjRes::VALUES_OFFSET - ObjRes::INFO_OFFSET),
      Props + PropCount, Ptr);

  Global->setInitializer(InitStruct);
}

ConstantStruct *Object::createInitializer(StructType *STy, const Property *From,
                                          const Property *To,
                                          const Property *&Ptr) const {
  std::vector<Constant *> Args;

  for (Type *ElemTy : STy->elements()) {
    Constant *C;
    if (ElemTy->isIntegerTy()) {
      assert(Ptr < To);
      if (Ptr < From) {
        int idx = (ObjRes::VALUES_OFFSET - ObjRes::INFO_OFFSET) - (From - Ptr);
        switch (idx) {
        case -1:
          C = ConstantExpr::getPtrToInt(Species, ElemTy);
          break;

        case (ObjRes::NAME_OFFSET - ObjRes::INFO_OFFSET): {
          const char *str = TheScript.getDataAt(
              static_cast<uint16_t>(Ptr->getDefaultValue()));
          C = ConstantExpr::getPtrToInt(TheScript.getString(str), ElemTy);
          break;
        }

        default:
          C = ConstantInt::get(ElemTy,
                               static_cast<uint16_t>(Ptr->getDefaultValue()));
          break;
        }
      } else {
        int16_t val = Ptr->getDefaultValue();
        C = TheScript.getLocalString(static_cast<uint16_t>(val));
        if (C)
          C = ConstantExpr::getPtrToInt(C, ElemTy);
        else
          C = GetWorld().getConstantValue(val);
      }
      Ptr++;
    } else
      C = createInitializer(cast<StructType>(ElemTy), From, To, Ptr);
    Args.push_back(C);
  }

  return static_cast<ConstantStruct *>(ConstantStruct::get(STy, Args));
}
