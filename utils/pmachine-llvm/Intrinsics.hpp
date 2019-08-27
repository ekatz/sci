//===- Intrinsics.hpp -----------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_INTRINSICS_HPP
#define SCI_UTILS_PMACHINE_LLVM_INTRINSICS_HPP

#include "Types.hpp"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"

namespace sci {

class Procedure;
class Method;

class Intrinsic {
public:
  enum ID : unsigned {
    push,
    pop,
    rest,

    clss,
    objc,

    prop,
    send,
    call,
    calle,
    callk,

    callv,

    num_intrinsics
  };

  static llvm::Function *Get(ID IID);
  llvm::Function *get(ID IID) const;

  Intrinsic();
  ~Intrinsic();

private:
  std::unique_ptr<llvm::Function> Funcs[num_intrinsics];
};

class IntrinsicInst : public llvm::CallInst {
  IntrinsicInst() = delete;
  IntrinsicInst(const IntrinsicInst &) = delete;
  void operator=(const IntrinsicInst &) = delete;

public:
  Intrinsic::ID getIntrinsicID() const;

  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static bool classof(const llvm::CallInst *Call);
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<llvm::CallInst>(V) &&
           classof(llvm::cast<llvm::CallInst>(V));
  }

protected:
  using llvm::CallInst::Create;
};

class PushInst : public IntrinsicInst {
public:
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const IntrinsicInst *Inst) {
    return Inst->getIntrinsicID() == Intrinsic::push;
  }
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<IntrinsicInst>(V) && classof(llvm::cast<IntrinsicInst>(V));
  }

  static PushInst *Create(llvm::Value *V,
                          llvm::Instruction *InsertBefore = nullptr) {
    return static_cast<PushInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::push), V, "", InsertBefore));
  }
  static PushInst *Create(llvm::Value *V, llvm::BasicBlock *InsertAtEnd) {
    return static_cast<PushInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::push), V, "", InsertAtEnd));
  }

  llvm::Value *getValue() const { return getArgOperand(0); }
};

class PopInst : public IntrinsicInst {
public:
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const IntrinsicInst *Inst) {
    return Inst->getIntrinsicID() == Intrinsic::pop;
  }
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<IntrinsicInst>(V) && classof(llvm::cast<IntrinsicInst>(V));
  }

  static PopInst *Create(llvm::Instruction *InsertBefore = nullptr) {
    return static_cast<PopInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::pop), "", InsertBefore));
  }
  static PopInst *Create(llvm::BasicBlock *InsertAtEnd) {
    return static_cast<PopInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::pop), "", InsertAtEnd));
  }
};

class RestInst : public IntrinsicInst {
public:
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const IntrinsicInst *Inst) {
    return Inst->getIntrinsicID() == Intrinsic::rest;
  }
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<IntrinsicInst>(V) && classof(llvm::cast<IntrinsicInst>(V));
  }

  static RestInst *Create(llvm::ConstantInt *ParamIndex,
                          llvm::Instruction *InsertBefore = nullptr) {
    return static_cast<RestInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::rest), ParamIndex, "", InsertBefore));
  }
  static RestInst *Create(llvm::ConstantInt *ParamIndex,
                          llvm::BasicBlock *InsertAtEnd) {
    return static_cast<RestInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::rest), ParamIndex, "", InsertAtEnd));
  }

  llvm::ConstantInt *getParamIndex() const {
    return llvm::cast<llvm::ConstantInt>(getArgOperand(0));
  }
};

class ClassInst : public IntrinsicInst {
public:
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const IntrinsicInst *Inst) {
    return Inst->getIntrinsicID() == Intrinsic::clss;
  }
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<IntrinsicInst>(V) && classof(llvm::cast<IntrinsicInst>(V));
  }

  static ClassInst *Create(llvm::ConstantInt *ClassID,
                           llvm::Instruction *InsertBefore = nullptr) {
    return static_cast<ClassInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::clss), ClassID, "", InsertBefore));
  }
  static ClassInst *Create(llvm::ConstantInt *ClassID,
                           llvm::BasicBlock *InsertAtEnd) {
    return static_cast<ClassInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::clss), ClassID, "", InsertAtEnd));
  }

  llvm::ConstantInt *getClassID() const {
    return llvm::cast<llvm::ConstantInt>(getArgOperand(0));
  }
};

class PropertyInst : public IntrinsicInst {
public:
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const IntrinsicInst *Inst) {
    return Inst->getIntrinsicID() == Intrinsic::prop;
  }
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<IntrinsicInst>(V) && classof(llvm::cast<IntrinsicInst>(V));
  }

  static PropertyInst *Create(llvm::ConstantInt *Index,
                              llvm::Instruction *InsertBefore = nullptr) {
    return static_cast<PropertyInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::prop), Index, "", InsertBefore));
  }
  static PropertyInst *Create(llvm::ConstantInt *Index,
                              llvm::BasicBlock *InsertAtEnd) {
    return static_cast<PropertyInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::prop), Index, "", InsertAtEnd));
  }

  llvm::ConstantInt *getIndex() const {
    return llvm::cast<llvm::ConstantInt>(getArgOperand(0));
  }
};

class SendMessageInst : public IntrinsicInst {
public:
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const IntrinsicInst *Inst) {
    return Inst->getIntrinsicID() == Intrinsic::send;
  }
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<IntrinsicInst>(V) && classof(llvm::cast<IntrinsicInst>(V));
  }

  static SendMessageInst *Create(ArrayRef<llvm::Value *> Args,
                                 llvm::Instruction *InsertBefore = nullptr) {
    return static_cast<SendMessageInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::send), Args, "", InsertBefore));
  }
  static SendMessageInst *Create(ArrayRef<llvm::Value *> Args,
                                 llvm::BasicBlock *InsertAtEnd) {
    return static_cast<SendMessageInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::send), Args, "", InsertAtEnd));
  }

  llvm::Value *getObject() const { return llvm::CallInst::getArgOperand(0); }
  llvm::Value *getSelector() const { return llvm::CallInst::getArgOperand(1); }
  llvm::Value *getArgCount() const { return llvm::CallInst::getArgOperand(2); }
  llvm::Value *getArgOperand(unsigned i) const {
    return llvm::CallInst::getArgOperand(i + 3);
  }

  op_iterator arg_begin() { return llvm::CallInst::arg_begin() + 3; }
  op_iterator arg_end() { return llvm::CallInst::arg_end(); }
  llvm::iterator_range<op_iterator> arg_operands() {
    return make_range(arg_begin(), arg_end());
  }

  const_op_iterator arg_begin() const {
    return llvm::CallInst::arg_begin() + 3;
  }
  const_op_iterator arg_end() const { return llvm::CallInst::arg_end(); }
  llvm::iterator_range<const_op_iterator> arg_operands() const {
    return make_range(arg_begin(), arg_end());
  }
};

class ObjCastInst : public IntrinsicInst {
public:
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const IntrinsicInst *Inst) {
    return Inst->getIntrinsicID() == Intrinsic::objc;
  }
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<IntrinsicInst>(V) && classof(llvm::cast<IntrinsicInst>(V));
  }

  static ObjCastInst *Create(llvm::ConstantInt *ClassID,
                             llvm::Instruction *InsertBefore = nullptr) {
    return static_cast<ObjCastInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::objc), ClassID, "", InsertBefore));
  }
  static ObjCastInst *Create(llvm::ConstantInt *ClassID,
                             llvm::BasicBlock *InsertAtEnd) {
    return static_cast<ObjCastInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::objc), ClassID, "", InsertAtEnd));
  }

  llvm::Value *getObject() const { return getArgOperand(0); }
  llvm::ConstantInt *getClassID() const {
    return llvm::cast<llvm::ConstantInt>(getArgOperand(1));
  }
  void setClassID(llvm::ConstantInt *ClassID) { setArgOperand(1, ClassID); }
  llvm::Value *getAnchor() const { return getArgOperand(2); }
  SendMessageInst *getSendMessage() {
    return llvm::cast<SendMessageInst>(user_back());
  }

  op_iterator class_begin() { return arg_begin() + 1; }
  op_iterator class_end() { return arg_end(); }
  llvm::iterator_range<op_iterator> classes() {
    return make_range(class_begin(), class_end());
  }

  const_op_iterator class_begin() const { return arg_begin() + 1; }
  const_op_iterator class_end() const { return arg_end(); }
  llvm::iterator_range<const_op_iterator> classes() const {
    return make_range(class_begin(), arg_end());
  }
};

class CallInternalInst : public IntrinsicInst {
public:
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const IntrinsicInst *Inst) {
    return Inst->getIntrinsicID() == Intrinsic::call;
  }
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<IntrinsicInst>(V) && classof(llvm::cast<IntrinsicInst>(V));
  }

  static CallInternalInst *Create(ArrayRef<llvm::Value *> Args,
                                  llvm::Instruction *InsertBefore = nullptr) {
    return static_cast<CallInternalInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::call), Args, "", InsertBefore));
  }
  static CallInternalInst *Create(ArrayRef<llvm::Value *> Args,
                                  llvm::BasicBlock *InsertAtEnd) {
    return static_cast<CallInternalInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::call), Args, "", InsertAtEnd));
  }

  llvm::ConstantInt *getOffset() const {
    return llvm::cast<llvm::ConstantInt>(getArgOperand(0));
  }
  llvm::Value *getArgCount() const { return getArgOperand(1); }

  op_iterator arg_begin() { return llvm::CallInst::arg_begin() + 2; }
  op_iterator arg_end() { return llvm::CallInst::arg_end(); }
  llvm::iterator_range<op_iterator> arg_operands() {
    return make_range(arg_begin(), arg_end());
  }

  const_op_iterator arg_begin() const {
    return llvm::CallInst::arg_begin() + 2;
  }
  const_op_iterator arg_end() const { return llvm::CallInst::arg_end(); }
  llvm::iterator_range<const_op_iterator> arg_operands() const {
    return make_range(arg_begin(), arg_end());
  }
};

class CallExternalInst : public IntrinsicInst {
public:
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const IntrinsicInst *Inst) {
    return Inst->getIntrinsicID() == Intrinsic::calle;
  }
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<IntrinsicInst>(V) && classof(llvm::cast<IntrinsicInst>(V));
  }

  static CallExternalInst *Create(ArrayRef<llvm::Value *> Args,
                                  llvm::Instruction *InsertBefore = nullptr) {
    return static_cast<CallExternalInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::calle), Args, "", InsertBefore));
  }
  static CallExternalInst *Create(ArrayRef<llvm::Value *> Args,
                                  llvm::BasicBlock *InsertAtEnd) {
    return static_cast<CallExternalInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::calle), Args, "", InsertAtEnd));
  }

  llvm::ConstantInt *getScriptID() const {
    return llvm::cast<llvm::ConstantInt>(getArgOperand(0));
  }
  llvm::ConstantInt *getEntryIndex() const {
    return llvm::cast<llvm::ConstantInt>(getArgOperand(1));
  }
  llvm::Value *getArgCount() const { return getArgOperand(2); }

  op_iterator arg_begin() { return llvm::CallInst::arg_begin() + 3; }
  op_iterator arg_end() { return llvm::CallInst::arg_end(); }
  llvm::iterator_range<op_iterator> arg_operands() {
    return make_range(arg_begin(), arg_end());
  }

  const_op_iterator arg_begin() const {
    return llvm::CallInst::arg_begin() + 3;
  }
  const_op_iterator arg_end() const { return llvm::CallInst::arg_end(); }
  llvm::iterator_range<const_op_iterator> arg_operands() const {
    return make_range(arg_begin(), arg_end());
  }
};

class CallKernelInst : public IntrinsicInst {
public:
  // Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const IntrinsicInst *Inst) {
    return Inst->getIntrinsicID() == Intrinsic::callk;
  }
  static inline bool classof(const llvm::Value *V) {
    return llvm::isa<IntrinsicInst>(V) && classof(llvm::cast<IntrinsicInst>(V));
  }

  static CallKernelInst *Create(ArrayRef<llvm::Value *> Args,
                                llvm::Instruction *InsertBefore = nullptr) {
    return static_cast<CallKernelInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::callk), Args, "", InsertBefore));
  }
  static CallKernelInst *Create(ArrayRef<llvm::Value *> Args,
                                llvm::BasicBlock *InsertAtEnd) {
    return static_cast<CallKernelInst *>(llvm::CallInst::Create(
        Intrinsic::Get(Intrinsic::callk), Args, "", InsertAtEnd));
  }

  llvm::ConstantInt *getKernelOrdinal() const {
    return llvm::cast<llvm::ConstantInt>(llvm::CallInst::getArgOperand(0));
  }
  llvm::Value *getArgCount() const { return llvm::CallInst::getArgOperand(1); }

  llvm::Value *getArgOperand(unsigned I) const {
    return llvm::CallInst::getArgOperand(I + 2);
  }

  op_iterator arg_begin() { return llvm::CallInst::arg_begin() + 2; }
  op_iterator arg_end() { return llvm::CallInst::arg_end(); }
  llvm::iterator_range<op_iterator> arg_operands() {
    return make_range(arg_begin(), arg_end());
  }

  const_op_iterator arg_begin() const {
    return llvm::CallInst::arg_begin() + 2;
  }
  const_op_iterator arg_end() const { return llvm::CallInst::arg_end(); }
  llvm::iterator_range<const_op_iterator> arg_operands() const {
    return make_range(arg_begin(), arg_end());
  }
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_INTRINSICS_HPP
