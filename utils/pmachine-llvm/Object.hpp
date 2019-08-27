//===- Object.hpp ---------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_OBJECT_HPP
#define SCI_UTILS_PMACHINE_LLVM_OBJECT_HPP

#include "Class.hpp"

namespace sci {

class Object : public Class {
public:
  unsigned getId() const;

  virtual StringRef getName() const;

  llvm::GlobalVariable *getGlobal() const { return Global; }

private:
  void setInitializer() const;
  llvm::ConstantStruct *createInitializer(llvm::StructType *STy,
                                          const Property *From,
                                          const Property *To,
                                          const Property *&Ptr) const;

  Object(const ObjRes &Res, Script &S);
  ~Object();

  llvm::GlobalVariable *Global;

  friend class World;
  friend class Script;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_OBJECT_HPP
