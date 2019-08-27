//===- Class.hpp ----------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_CLASS_HPP
#define SCI_UTILS_PMACHINE_LLVM_CLASS_HPP

#include "Resource.hpp"
#include "llvm/IR/Constants.h"

namespace sci {

class Script;
class Method;
class Property;

class Class {
public:
  static llvm::StructType *GetAbstractType();
  static Class *get(unsigned ClassID);

  bool loadMethods();

  unsigned getId() const { return ID; }

  virtual StringRef getName() const;
  llvm::StructType *getType() const { return Ty; }

  Class *getSuper() const { return Super; }
  ArrayRef<Method> getOverloadedMethods() const;

  Method *getMethod(unsigned MethodID) const;
  Method *getMethod(unsigned MethodID, int &Index) const;
  ArrayRef<Method *> getMethods() const;
  unsigned getMethodCount() const { return MethodCount; }

  Property *getProperty(unsigned PropID) const;
  Property *getProperty(unsigned PropID, int &Index) const;
  ArrayRef<Property> getProperties() const;
  unsigned getPropertyCount() const { return PropCount; }

  Script &getScript() const { return TheScript; }
  llvm::Module &getModule() const;

  llvm::GlobalVariable *getSpecies() const { return Species; }

  bool isObject() const;

  unsigned getDepth() const { return Depth; }

private:
  unsigned countMethods() const;
  void createMethods();

protected:
  void createSpecies();
  int findMethod(unsigned MethodID) const;
  int findProperty(unsigned PropID) const;

  Class(const ObjRes &Res, Script &S);
  ~Class();

  unsigned ID;
  unsigned Depth;
  llvm::StructType *Ty;
  llvm::GlobalVariable *Species;
  llvm::GlobalVariable *MethodOffs;

  Property *Props;
  unsigned PropCount;

  Method *OverloadMethods;
  unsigned OverloadMethodCount;

  std::unique_ptr<Method *[]> Methods;
  unsigned MethodCount;

  Class *Super;
  Script &TheScript;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_CLASS_HPP
