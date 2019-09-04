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
#include "llvm/Support/Debug.h"

using namespace sci;
using namespace llvm;

static World TheWorld;

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
    C = ConstantInt::get(getSizeType(),
                         static_cast<uint64_t>(static_cast<uint16_t>(Val)),
                         false);
  } else {
    C = ConstantInt::get(
        getSizeType(), static_cast<uint64_t>(static_cast<int16_t>(Val)), true);
  }
  return C;
}

void World::setDataLayout(const DataLayout &DL) {
  this->DL = DL;
  SizeTy = Type::getIntNTy(Context, DL.getPointerSizeInBits());
  Type *Elems[] = {
      SizeTy,                   // species
      ArrayType::get(SizeTy, 0) // vars
  };
  AbsClassTy = StructType::create(Context, Elems);
}

std::unique_ptr<Module> World::load() {
  ResClassEntry *Res = (ResClassEntry *)ResLoad(RES_VOCAB, CLASSTBL_VOCAB);
  if (!Res)
    return nullptr;

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

  dbgs() << "Reconstructing stack\n";
  StackReconstructionPass().run();

  dbgs() << "Splitting Send Message calls\n";
  SplitSendPass().run();

  dbgs() << "Fixing code issues\n";
  FixCodePass().run();

  dbgs() << "Translating class intrinsic\n";
  TranslateClassIntrinsicPass().run();

  dbgs() << "Expanding KScriptID calls\n";
  EmitScriptUtilitiesPass().run();

  dbgs() << "Mutating Call intrinsics\n";
  MutateCallIntrinsicsPass().run();

  dbgs() << "Linking SCI module\n";
  return link();
}

static void migrateGlobals(Module &Src, Module &Dst) {
  while (!Src.global_empty()) {
    GlobalVariable *SrcGV = &*Src.global_begin();
    GlobalVariable *DstGV = Dst.getNamedGlobal(SrcGV->getName());
    if (!DstGV) {
      SrcGV->removeFromParent();
      Dst.getGlobalList().push_back(SrcGV);
    } else if (SrcGV->hasInitializer() && !DstGV->hasInitializer()) {
      DstGV->replaceAllUsesWith(SrcGV);
      DstGV->eraseFromParent();
      SrcGV->removeFromParent();
      Dst.getGlobalList().push_back(SrcGV);
    } else {
      assert(!(DstGV->hasInitializer() && SrcGV->hasInitializer() &&
               DstGV->getInitializer() != SrcGV->getInitializer()) &&
             "Cannot merge two globals with different initializers");
      SrcGV->replaceAllUsesWith(DstGV);
      SrcGV->eraseFromParent();
    }
  }
}

static void migrateFunctions(Module &Src, Module &Dst) {
  while (!Src.empty()) {
    Function *SrcFunc = &*Src.begin();
    Function *DstFunc = Dst.getFunction(SrcFunc->getName());
    if (!DstFunc) {
      SrcFunc->removeFromParent();
      Dst.getFunctionList().push_back(SrcFunc);
    } else if (!SrcFunc->empty() && DstFunc->empty()) {
      DstFunc->replaceAllUsesWith(SrcFunc);
      DstFunc->eraseFromParent();
      SrcFunc->removeFromParent();
      Dst.getFunctionList().push_back(SrcFunc);
    } else {
      SrcFunc->replaceAllUsesWith(DstFunc);
      SrcFunc->eraseFromParent();
    }
  }
}

std::unique_ptr<Module> World::link() {
  auto Composite = std::make_unique<Module>("sci", Context);
  for (Script &S : scripts()) {
    std::unique_ptr<Module> M = getScript(S.getId())->takeModule();
    migrateGlobals(*M, *Composite);
    migrateFunctions(*M, *Composite);
  }
  return std::move(Composite);
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
