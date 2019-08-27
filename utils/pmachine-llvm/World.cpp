//===- World.cpp ----------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "World.hpp"
#include "Object.hpp"
#include "Procedure.hpp"
#include "Resource.hpp"
#include "Script.hpp"
#include "Passes/EmitScriptUtilitiesPass.hpp"
#include "Passes/FixCodePass.hpp"
#include "Passes/MutateCallIntrinsicsPass.hpp"
#include "Passes/SplitSendPass.hpp"
#include "Passes/StackReconstructionPass.hpp"
#include "Passes/TranslateClassIntrinsicPass.hpp"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

using namespace sci;
using namespace llvm;

static World TheWorld;

static void DumpScriptModule(unsigned ScriptID) {
  std::error_code EC;
  Module *M = GetWorld().getScript(ScriptID)->getModule();
  M->print(raw_fd_ostream(M->getName().str() + ".ll", EC, sys::fs::F_Text),
           nullptr);
}

World &sci::GetWorld() { return TheWorld; }

World::World()
    : DL(""), SizeTy(nullptr), ScriptCount(0), Classes(nullptr), ClassCount(0) {
  setDataLayout(DL);
}

World::~World() {
  if (Classes) {
    for (unsigned I = 0, N = ClassCount; I < N; ++I)
      if (Classes[I].getType())
        Classes[I].~Object();
    free(Classes);
  }
}

ConstantInt *World::getConstantValue(int16_t Val) const {
  ConstantInt *C;
  if (isUnsignedValue(Val)) {
    if (Val < 0 && (uint16_t)Val != 0x8000)
      printf("unsigned = %X\n", (uint16_t)Val);
    C = ConstantInt::get(getSizeType(),
                         static_cast<uint64_t>(static_cast<uint16_t>(Val)),
                         false);
  } else {
    if (Val < 0 && Val != -1)
      printf("__signed = %X\n", (uint16_t)Val);
    C = ConstantInt::get(
        getSizeType(), static_cast<uint64_t>(static_cast<int16_t>(Val)), true);
  }
  return C;
}

void World::setDataLayout(const llvm::DataLayout &DL) {
  this->DL = DL;
  SizeTy = Type::getIntNTy(Context, DL.getPointerSizeInBits());
  Type *Elems[] = {
      SizeTy,                   // species
      ArrayType::get(SizeTy, 0) // vars
  };
  AbsClassTy = StructType::create(Context, Elems);
}

bool World::load() {
  ResClassEntry *Res = (ResClassEntry *)ResLoad(RES_VOCAB, CLASSTBL_VOCAB);
  if (!Res)
    return false;

  Intrinsics.reset(new Intrinsic());
  ClassCount = ResHandleSize(Res) / sizeof(ResClassEntry);
  size_t Size = ClassCount * sizeof(Object);
  Classes = reinterpret_cast<Object *>(malloc(Size));
  memset(Classes, 0, Size);
  for (unsigned I = 0, N = ClassCount; I < N; ++I) {
    Script **ClassScript = reinterpret_cast<Script **>(&Classes[I].Super + 1);
    *ClassScript = acquireScript(Res[I].scriptNum);
  }

  ResUnLoad(RES_VOCAB, CLASSTBL_VOCAB);

  for (unsigned I = 0; I < 1000; ++I)
    if (Script *S = acquireScript(I))
      S->load();

  StackReconstructionPass().run();
  printf("Finished stack reconstruction!\n");

  SplitSendPass().run();
  printf("Finished splitting Send Message calls!\n");

  FixCodePass().run();
  printf("Finished fixing code issues!\n");

  TranslateClassIntrinsicPass().run();
  printf("Finished translating class intrinsic!\n");

  EmitScriptUtilitiesPass().run();
  printf("Finished expanding KScriptID calls!\n");

  MutateCallIntrinsicsPass().run();
  printf("Finished mutating Call intrinsics!\n");

  for (Script &S : scripts()) {
    DumpScriptModule(S.getId());
  }
  return true;
}

Script *World::acquireScript(unsigned ScriptID) {
  Script *S = Scripts[ScriptID].get();
  if (!S)
    if (Handle Hunk = ResLoad(RES_SCRIPT, ScriptID)) {
      S = new Script(ScriptID, Hunk);
      Scripts[ScriptID].reset(S);
      ScriptCount++;
    }
  return S;
}

Script *World::getScript(unsigned ScriptID) const {
  return Scripts[ScriptID].get();
}

Script *World::getScript(Module &M) const {
  Script *S = nullptr;
  StringRef Name = M.getName();
  if (Name.size() == 9 && Name.startswith("Script"))
    if (isdigit(Name[8]) && isdigit(Name[7]) && isdigit(Name[6])) {
      unsigned ScriptID =
          (Name[6] - '0') * 100 + (Name[7] - '0') * 10 + (Name[8] - '0') * 1;

      S = Scripts[ScriptID].get();
      assert(!S || S->getModule() == &M);
    }
  return S;
}

Object *World::lookupObject(GlobalVariable &Var) const {
  Script *S = getScript(*Var.getParent());
  return S ? S->lookupObject(Var) : nullptr;
}

StringRef World::getSelectorName(unsigned SelID) {
  return Sels.getSelectorName(SelID);
}

Object *World::getClass(unsigned ClassID) {
  if (ClassID >= ClassCount) {
    return nullptr;
  }

  Object *Cls = &Classes[ClassID];
  if (!Cls->getType())
    if (!Cls->TheScript.load() || !Cls->getType())
      Cls = nullptr;
  return Cls;
}

Object *World::addClass(const ObjRes &Res, Script &S) {
  assert(selector_cast<unsigned>(Res.speciesSel) < ClassCount);
  assert(&S == &Classes[Res.speciesSel].TheScript);

  Object *Cls = &Classes[Res.speciesSel];
  if (!Cls->getType())
    new (Cls) Object(Res, S);
  return Cls;
}

bool World::registerProcedure(Procedure &Proc) {
  bool Ret = false;
  Function *Func = Proc.getFunction();
  if (Func) {
    Procedure *&Slot = FuncMap[Func];
    Ret = (Slot == nullptr);
    Slot = &Proc;
  }
  return Ret;
}

Procedure *World::getProcedure(const Function &Func) const {
  return FuncMap.lookup(&Func);
}

unsigned World::getGlobalVariablesCount() const {
  return Scripts[0]->getLocalVariablesCount();
}
