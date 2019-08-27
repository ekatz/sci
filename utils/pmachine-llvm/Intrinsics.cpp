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

class IntrinsicFunction : public Function {
public:
  struct Desc {
    Intrinsic::ID ID;
    const char *Name;
    unsigned Argc;
    bool IsVarArg;
    uint8_t ReturnVal;
  };

  static IntrinsicFunction *Create(const Desc &D) {
    IntegerType *SizeTy = GetWorld().getSizeType();

    SmallVector<Type *, 6> Params;
    Params.resize(D.Argc, SizeTy);

    Type *RetTy;
    switch (D.ReturnVal) {
    case Type::VoidTyID:
      RetTy = Type::getVoidTy(SizeTy->getContext());
      break;

    case Type::IntegerTyID:
      RetTy = SizeTy;
      break;

    case Type::PointerTyID:
      RetTy = SizeTy->getPointerTo();
      break;

    default:
      RetTy = nullptr;
    }

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

static const IntrinsicFunction::Desc Intrinsics[] = {
    {Intrinsic::push, "push@SCI", 1, false, Type::VoidTyID},
    {Intrinsic::pop, "pop@SCI", 0, false, Type::IntegerTyID},
    {Intrinsic::rest, "rest@SCI", 1, false, Type::IntegerTyID},

    {Intrinsic::clss, "class@SCI", 1, false, Type::IntegerTyID},
    {Intrinsic::objc, "obj_cast@SCI", 3, true, Type::IntegerTyID},

    {Intrinsic::prop, "prop@SCI", 1, false, Type::PointerTyID},
    {Intrinsic::send, "send@SCI", 3, true, Type::IntegerTyID},
    {Intrinsic::call, "call@SCI", 2, true, Type::IntegerTyID},
    {Intrinsic::calle, "calle@SCI", 3, true, Type::IntegerTyID},
    {Intrinsic::callk, "callk@SCI", 2, true, Type::IntegerTyID},

    {Intrinsic::callv, "callv@SCI", 1, true, Type::IntegerTyID}};

static_assert(array_lengthof(Intrinsics) == Intrinsic::num_intrinsics,
              "Incorrect number of intrinsic descriptors.");

Intrinsic::Intrinsic() {
  for (unsigned I = 0, N = Intrinsic::num_intrinsics; I < N; ++I)
    Funcs[I].reset(IntrinsicFunction::Create(Intrinsics[I]));
}

Intrinsic::~Intrinsic() {
  for (unsigned I = 0, N = Intrinsic::num_intrinsics; I < N; ++I) {
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
