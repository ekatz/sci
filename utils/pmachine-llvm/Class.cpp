//===- Class.cpp ----------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "Class.hpp"
#include "Decl.hpp"
#include "Method.hpp"
#include "Object.hpp"
#include "Property.hpp"
#include "Resource.hpp"
#include "Script.hpp"
#include "World.hpp"

using namespace sci;
using namespace llvm;

static unsigned CalcNumInheritedElements(StructType *STy) {
  unsigned Num = 0;
  for (Type *ElemTy : STy->elements())
    if (ElemTy->isIntegerTy())
      Num++;
    else
      Num += CalcNumInheritedElements(cast<StructType>(ElemTy));
  return Num;
}

StructType *Class::GetAbstractType() {
  return GetWorld().getAbstractClassType();
}

Class *Class::get(unsigned ClassID) { return GetWorld().getClass(ClassID); }

Class::Class(const ObjRes &Res, Script &S)
    : TheScript(S), Species(nullptr), MethodOffs(nullptr), Props(nullptr),
      OverloadMethods(nullptr) {
  assert(Res.magic == OBJID);

  World &W = GetWorld();
  SelectorTable &Sels = W.getSelectorTable();
  IntegerType *SizeTy = W.getSizeType();
  IntegerType *Int16Ty = Type::getInt16Ty(W.getContext());
  unsigned I, N;

  ID = Res.speciesSel;
  Super = W.getClass(selector_cast<unsigned>(Res.superSel));
  Depth = (Super != nullptr) ? Super->Depth + 1 : 0;

  const int16_t *PropVals = Res.getPropertyValues();
  PropCount = Res.getPropertyCount();
  if (PropCount > 0) {
    size_t Size = PropCount * sizeof(Property);
    Props = reinterpret_cast<Property *>(malloc(Size));
    memset(Props, 0, Size);

    if (Res.isClass()) {
      const ObjID *propSels = Res.getPropertySelectors();
      for (I = 0, N = PropCount; I < N; ++I) {
        new (&Props[I]) Property(propSels[I], PropVals[I], *this);
        Sels.addProperty(Props[I], I);
      }

      if (Super != nullptr) {
        ArrayRef<Property> SuperProps = Super->getProperties();
        assert(SuperProps.size() <= PropCount);
        for (I = 0, N = SuperProps.size(); I < N; ++I) {
          assert(Props[I].getSelector() == SuperProps[I].getSelector());
        }
      }
    } else {
      assert(Super != nullptr && Super->getPropertyCount() <= PropCount);
      ArrayRef<Property> SuperProps = Super->getProperties();
      for (I = 0, N = SuperProps.size(); I < N; ++I) {
        new (&Props[I])
            Property(static_cast<ObjID>(SuperProps[I].getSelector()),
                     PropVals[I], *this);
        Sels.addProperty(Props[I], I);
      }
    }
  }

  std::vector<Type *> Args;
  N = PropCount;
  if (Super != nullptr) {
    Args.push_back(Super->getType());
    I = Super->getPropertyCount();
    assert(I != 0);
    assert((I + 1) == CalcNumInheritedElements(Super->getType()));
  } else {
    // Add the species pointer.
    Args.push_back(SizeTy);
    I = 0;
  }
  for (; I < N; ++I)
    Args.push_back(SizeTy);

  StringRef Name;
  std::string NameStr;
  const char *NameSel = TheScript.getDataAt(Res.nameSel);
  if (NameSel != nullptr && NameSel[0] != '\0') {
    if (Super != nullptr && Super->getName() == "MenuBar@255")
      NameStr = "TheMenuBar";
    else
      NameStr = NameSel;
  } else {
    if (S.getId() == 255) {
      if (ID == 9)
        NameStr = "MenuBar";
      else if (ID == 10)
        NameStr = "DItem";
    }
    if (NameStr.empty()) {
      NameStr = Res.isClass() ? "Class" : "obj";
      NameStr += '@' + utostr(getId());
    }
  }
  NameStr += '@' + utostr(TheScript.getId());
  Name = NameStr;

  Ty = StructType::create(W.getContext(), Args, Name);

  const uint16_t *MethodOffs = Res.getMethodOffsets();
  const ObjID *MethodSels = Res.getMethodSelectors();
  OverloadMethodCount = Res.getMethodCount();
  if (OverloadMethodCount > 0) {
    size_t Size = OverloadMethodCount * sizeof(Method);
    OverloadMethods = reinterpret_cast<Method *>(malloc(Size));
    memset(OverloadMethods, 0, Size);

    for (I = 0, N = OverloadMethodCount; I < N; ++I)
      new (&OverloadMethods[I]) Method(MethodSels[I], MethodOffs[I], *this);
  }

  createMethods();
  createSpecies();
}

Class::~Class() {
  if (OverloadMethods != nullptr) {
    for (unsigned I = 0, N = OverloadMethodCount; I < N; ++I)
      OverloadMethods[I].~Method();
    free(OverloadMethods);
  }

  if (Props != nullptr) {
    for (unsigned I = 0, N = PropCount; I < N; ++I)
      Props[I].~Property();
    free(Props);
  }
}

StringRef Class::getName() const { return Ty->getName(); }

ArrayRef<Method> Class::getOverloadedMethods() const {
  return makeArrayRef(OverloadMethods, OverloadMethodCount);
}

ArrayRef<Method *> Class::getMethods() const {
  return makeArrayRef(Methods.get(), MethodCount);
}

Method *Class::getMethod(unsigned MethodID) const {
  int Index;
  return getMethod(MethodID, Index);
}

Method *Class::getMethod(unsigned MethodID, int &Index) const {
  Index = findMethod(MethodID);
  return (Index >= 0) ? Methods[Index] : nullptr;
}

Property *Class::getProperty(unsigned PropID) const {
  int Index;
  return getProperty(PropID, Index);
}

Property *Class::getProperty(unsigned PropID, int &Index) const {
  Index = findProperty(PropID);
  return (Index >= 0) ? &Props[Index] : nullptr;
}

ArrayRef<Property> Class::getProperties() const {
  return makeArrayRef(Props, PropCount);
}

Module &Class::getModule() const { return *TheScript.getModule(); }

void Class::createSpecies() {
  World &W = GetWorld();
  LLVMContext &Ctx = W.getContext();
  Module &M = getModule();
  PointerType *Int8PtrTy = Type::getInt8PtrTy(Ctx);
  IntegerType *Int16Ty = Type::getInt16Ty(Ctx);

  MethodOffs =
      new GlobalVariable(M, ArrayType::get(Int8PtrTy, MethodCount), true,
                         GlobalValue::LinkOnceODRLinkage, nullptr,
                         std::string("?methodOffs@") + getName());
  MethodOffs->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  MethodOffs->setAlignment(W.getTypeAlignment(Int8PtrTy));

  GlobalVariable *MethodSels;
  MethodSels =
      new GlobalVariable(M, ArrayType::get(Int16Ty, MethodCount + 1), true,
                         GlobalValue::LinkOnceODRLinkage, nullptr,
                         std::string("?methodSels@") + getName());
  MethodSels->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  MethodSels->setAlignment(W.getTypeAlignment(Int16Ty));

  GlobalVariable *PropSels;
  PropSels = new GlobalVariable(M, ArrayType::get(Int16Ty, PropCount + 1), true,
                                GlobalValue::LinkOnceODRLinkage, nullptr,
                                std::string("?propSels@") + getName());
  PropSels->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  PropSels->setAlignment(W.getTypeAlignment(Int16Ty));

  Species = new GlobalVariable(M, ArrayType::get(Int8PtrTy, 4), true,
                               GlobalValue::LinkOnceODRLinkage, nullptr,
                               std::string("?species@") + getName());
  Species->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  Species->setAlignment(W.getTypeAlignment(Int8PtrTy));

  std::unique_ptr<Constant *[]> InitVals(
      new Constant *[std::max({MethodCount + 1, PropCount + 1, 4U})]);
  Constant *C;
  ArrayType *ArrTy;
  Type *ElemTy;

  ArrTy = cast<ArrayType>(MethodSels->getType()->getElementType());
  InitVals[0] = ConstantInt::get(Int16Ty, MethodCount);

  for (unsigned I = 0, N = MethodCount; I < N; ++I) {
    InitVals[I + 1] = ConstantInt::get(Int16Ty, Methods[I]->getSelector());
  }

  C = ConstantArray::get(ArrTy, makeArrayRef(InitVals.get(), MethodCount + 1));
  MethodSels->setInitializer(C);

  ArrTy = cast<ArrayType>(PropSels->getType()->getElementType());
  InitVals[0] = ConstantInt::get(Int16Ty, PropCount);

  for (unsigned I = 0, N = PropCount; I < N; ++I) {
    InitVals[I + 1] = ConstantInt::get(Int16Ty, Props[I].getSelector());
  }

  C = ConstantArray::get(ArrTy, makeArrayRef(InitVals.get(), PropCount + 1));
  PropSels->setInitializer(C);

  ArrTy = cast<ArrayType>(Species->getType()->getElementType());
  ElemTy = ArrTy->getElementType();

  if (Super != nullptr) {
    GlobalVariable *SuperSpecies =
        getGlobalVariableDecl(Super->getSpecies(), &M);
    InitVals[0] = ConstantExpr::getBitCast(SuperSpecies, ElemTy);
  } else {
    InitVals[0] = Constant::getNullValue(ElemTy);
  }
  InitVals[1] = ConstantExpr::getBitCast(PropSels, ElemTy);
  InitVals[2] = ConstantExpr::getBitCast(MethodSels, ElemTy);
  InitVals[3] = ConstantExpr::getBitCast(MethodOffs, ElemTy);

  C = ConstantArray::get(ArrTy, makeArrayRef(InitVals.get(), 4));
  Species->setInitializer(C);
}

int Class::findProperty(unsigned PropID) const {
  for (unsigned I = 0, N = PropCount; I < N; ++I)
    if (Props[I].getSelector() == PropID)
      return static_cast<int>(I);
  return -1;
}

int Class::findMethod(unsigned MethodID) const {
  for (unsigned I = 0, N = MethodCount; I < N; ++I)
    if (Methods[I]->getSelector() == MethodID)
      return static_cast<int>(I);
  return -1;
}

unsigned Class::countMethods() const {
  unsigned Count;
  if (Super && Super->getMethodCount()) {
    Count = Super->getMethodCount();
    for (unsigned I = 0, N = OverloadMethodCount; I < N; ++I)
      if (Super->findMethod(OverloadMethods[I].getSelector()) < 0)
        Count++;
  } else
    Count = OverloadMethodCount;
  return Count;
}

void Class::createMethods() {
  MethodCount = countMethods();
  if (MethodCount) {
    Methods.reset(new Method *[MethodCount]);

    if (Super && Super->getMethodCount()) {
      unsigned Count = Super->getMethodCount();
      memcpy(Methods.get(), Super->Methods.get(), Count * sizeof(Method *));

      for (unsigned I = 0, N = OverloadMethodCount; I < N; ++I) {
        Method *method = &OverloadMethods[I];
        int Pos = Super->findMethod(method->getSelector());
        if (Pos >= 0)
          Methods[Pos] = method;
        else
          Methods[Count++] = method;
      }
    } else
      for (unsigned I = 0, N = OverloadMethodCount; I < N; ++I)
        Methods[I] = &OverloadMethods[I];
  }
}

bool Class::loadMethods() {
  if (MethodOffs->hasInitializer())
    return true;

  SelectorTable &Sels = GetWorld().getSelectorTable();
  ArrayType *ArrTy = cast<ArrayType>(MethodOffs->getType()->getElementType());
  Type *ElemTy = ArrTy->getElementType();

  std::unique_ptr<Constant *[]> OffsInit(new Constant *[MethodCount]);

  for (unsigned I = 0, N = MethodCount; I < N; ++I) {
    Method *method = Methods[I];
    Sels.addMethod(*method, I);

    Function *Func = method->load();
    if (!Func)
      return false;

    Func = getFunctionDecl(Func, &getModule());
    OffsInit[I] = ConstantExpr::getBitCast(Func, ElemTy);
  }

  Constant *C =
      ConstantArray::get(ArrTy, makeArrayRef(OffsInit.get(), MethodCount));
  MethodOffs->setInitializer(C);
  return true;
}

bool Class::isObject() const { return (GetWorld().getClass(ID) != this); }
