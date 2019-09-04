//===- World.hpp ----------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_WORLD_HPP
#define SCI_UTILS_PMACHINE_LLVM_WORLD_HPP

#include "Intrinsics.hpp"
#include "Resource.hpp"
#include "ScriptIterator.hpp"
#include "SelectorTable.hpp"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/DataLayout.h"

namespace sci {

class Class;
class Object;
class Procedure;

class World {
public:
  World();
  ~World();

  void setDataLayout(const llvm::DataLayout &DL);
  const llvm::DataLayout &getDataLayout() const { return DL; }
  unsigned getTypeAlignment(llvm::Value *V) const {
    return getTypeAlignment(V->getType());
  }
  unsigned getTypeAlignment(llvm::Type *Ty) const {
    return DL.getPrefTypeAlignment(Ty);
  }
  unsigned getElementTypeAlignment(llvm::Value *V) const {
    return getElementTypeAlignment(V->getType());
  }
  unsigned getElementTypeAlignment(llvm::Type *Ty) const {
    return getTypeAlignment(Ty->getPointerElementType());
  }
  unsigned getSizeTypeAlignment() const { return getTypeAlignment(SizeTy); }

  std::unique_ptr<llvm::Module> load();

  llvm::LLVMContext &getContext() { return Context; }
  llvm::IntegerType *getSizeType() const { return SizeTy; }
  llvm::ConstantInt *getConstantValue(int16_t Val) const;

  Object *addClass(const ObjRes &Res, Script &S);
  Object *getClass(unsigned ClassID);
  ArrayRef<Object> getClasses() const {
    return llvm::makeArrayRef(Classes, ClassCount);
  }
  llvm::StructType *getAbstractClassType() const { return AbsClassTy; }

  SelectorTable &getSelectorTable() { return Sels; }
  StringRef getSelectorName(unsigned SelID);

  unsigned getGlobalVariablesCount() const;

  llvm::Function *getIntrinsic(Intrinsic::ID IID) const {
    return Intrinsics->get(IID);
  }

  Script *getScript(llvm::Module &M) const;
  Script *getScript(unsigned ScriptID) const;
  unsigned getScriptCount() const { return ScriptCount; }

  Object *lookupObject(llvm::GlobalVariable &Var) const;

  bool registerProcedure(Procedure &Proc);
  Procedure *getProcedure(const llvm::Function &Func) const;

  const_script_iterator begin() const {
    return const_script_iterator(&Scripts[0], &Scripts[1000]);
  }
  script_iterator begin() {
    return script_iterator(&Scripts[0], &Scripts[1000]);
  }

  const_script_iterator end() const {
    return const_script_iterator(&Scripts[1000], &Scripts[1000]);
  }
  script_iterator end() {
    return script_iterator(&Scripts[1000], &Scripts[1000]);
  }

  llvm::iterator_range<script_iterator> scripts() {
    return llvm::make_range(begin(), end());
  }

  llvm::iterator_range<const_script_iterator> scripts() const {
    return llvm::make_range(begin(), end());
  }

private:
  Script *acquireScript(unsigned ScriptID);
  std::unique_ptr<llvm::Module> link();

  llvm::DataLayout DL;
  llvm::LLVMContext Context;
  llvm::IntegerType *SizeTy;
  llvm::StructType *AbsClassTy;
  std::unique_ptr<Script> Scripts[1000];
  unsigned ScriptCount;
  Object *Classes;
  unsigned ClassCount;

  llvm::DenseMap<const llvm::Function *, Procedure *> FuncMap;
  SelectorTable Sels;

  std::unique_ptr<Intrinsic> Intrinsics;
};

World &GetWorld();

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_WORLD_HPP
