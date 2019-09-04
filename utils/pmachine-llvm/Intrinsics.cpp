//===- Intrinsics.cpp -----------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "Intrinsics.hpp"
#include "Method.hpp"
#include "World.hpp"

using namespace llvm;

namespace sci {

namespace {

enum TypeKind : uint8_t { V, N, Z, P };

static Type *parseType(TypeKind TK) {
  switch (TK) {
  case V:
    return Type::getVoidTy(GetWorld().getContext());

  case N:
    return Type::getInt32Ty(GetWorld().getContext());

  case Z:
    return GetWorld().getSizeType();

  case P:
    return Type::getInt8PtrTy(GetWorld().getContext());

  default:
    return nullptr;
  }
}

class IntrinsicFunction : public Function {
public:
  struct Desc {
    Intrinsic::ID ID;
    const char *Name;
    SmallVector<TypeKind, 4> Args;
    bool IsVarArg;
    TypeKind ReturnVal;
  };

  static IntrinsicFunction *Create(const Desc &D) {
    SmallVector<Type *, 4> Params;
    for (TypeKind TK : D.Args)
      Params.push_back(parseType(TK));

    Type *RetTy = parseType(D.ReturnVal);

    FunctionType *FTy = FunctionType::get(RetTy, Params, D.IsVarArg);

    Function *F = Function::Create(FTy, ExternalLinkage, D.Name, nullptr);
    IntrinsicFunction *Intrin = static_cast<IntrinsicFunction *>(F);
    Intrin->setID(D.ID);
    return Intrin;
  }

private:
  void setID(Intrinsic::ID IID) {
    unsigned llvmID = IID + llvm::Intrinsic::num_intrinsics + 1;
    IntID = static_cast<llvm::Intrinsic::ID>(llvmID);
  }
};

} // end anonymous namespace

static const IntrinsicFunction::Desc Intrinsics[] = {
    {Intrinsic::push,  "push@SCI",     {Z    }, false, V},
    {Intrinsic::pop,   "pop@SCI",      {     }, false, Z},
    {Intrinsic::rest,  "rest@SCI",     {Z    }, false, Z},

    {Intrinsic::clss,  "class@SCI",    {Z    }, false, Z},
    {Intrinsic::objc,  "obj_cast@SCI", {Z,Z,Z}, true,  Z},

    {Intrinsic::prop,  "prop@SCI",     {Z    }, false, P},
    {Intrinsic::send,  "send@SCI",     {Z,Z,Z}, true,  Z},
    {Intrinsic::call,  "call@SCI",     {Z,Z  }, true,  Z},
    {Intrinsic::calle, "calle@SCI",    {Z,Z,Z}, true,  Z},
    {Intrinsic::callk, "callk@SCI",    {Z,Z  }, true,  Z},

    {Intrinsic::callv, "callv@SCI",    {Z    }, true,  Z}};

static_assert(array_lengthof(Intrinsics) == Intrinsic::num_intrinsics,
              "Incorrect number of intrinsic descriptors.");

Intrinsic::Intrinsic() {
  for (unsigned I = 0, E = Intrinsic::num_intrinsics; I < E; ++I)
    Funcs[I].reset(IntrinsicFunction::Create(Intrinsics[I]));
}

Intrinsic::~Intrinsic() {
  for (unsigned I = 0, E = Intrinsic::num_intrinsics; I < E; ++I) {
    assert(!Funcs[I] || Funcs[I]->getNumUses() == 0);
  }
}

Function *Intrinsic::get(ID IID) const {
  assert(IID < Intrinsic::num_intrinsics && "Intrinsic ID not in range.");
  assert(Funcs[IID] && "Intrinsic not supported.");

  return Funcs[IID].get();
}

Function *Intrinsic::Get(ID IID) { return GetWorld().getIntrinsic(IID); }

Intrinsic::ID IntrinsicInst::getIntrinsicID() const {
  unsigned llvmID = getCalledFunction()->getIntrinsicID();
  assert(llvmID > llvm::Intrinsic::num_intrinsics && "Invalid intrinsic ID.");
  return static_cast<Intrinsic::ID>(llvmID -
                                    (llvm::Intrinsic::num_intrinsics + 1));
}

bool IntrinsicInst::classof(const CallInst *Call) {
  if (const Function *CalledFunc = Call->getCalledFunction()) {
    unsigned llvmID = CalledFunc->getIntrinsicID();
    return (llvmID > llvm::Intrinsic::num_intrinsics);
  }
  return false;
}

} // end namespace sci
