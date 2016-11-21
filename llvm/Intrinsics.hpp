#pragma once
#ifndef _Intrinsics_HPP_
#define _Intrinsics_HPP_

#include "Types.hpp"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Constants.h>


BEGIN_NAMESPACE_SCI

class Procedure;
class Method;

class Intrinsic
{
public:
    enum ID : unsigned
    {
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

    static llvm::Function* Get(ID iid);
    llvm::Function* get(ID iid) const;

    Intrinsic();
    ~Intrinsic();

private:
    std::unique_ptr<llvm::Function> m_funcs[num_intrinsics];
};


class IntrinsicInst : public llvm::CallInst
{
    IntrinsicInst() = delete;
    IntrinsicInst(const IntrinsicInst&) = delete;
    void operator=(const IntrinsicInst&) = delete;

public:
    Intrinsic::ID getIntrinsicID() const;

    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static bool classof(const llvm::CallInst *call);
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<llvm::CallInst>(val) && classof(llvm::cast<llvm::CallInst>(val));
    }

protected:
    using llvm::CallInst::Create;
};


class PushInst : public IntrinsicInst {
public:
    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntrinsicInst *inst) {
        return inst->getIntrinsicID() == Intrinsic::push;
    }
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<IntrinsicInst>(val) && classof(llvm::cast<IntrinsicInst>(val));
    }

    static PushInst *Create(llvm::Value *val, llvm::Instruction *insertBefore = nullptr) {
        return static_cast<PushInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::push), val, "", insertBefore));
    }
    static PushInst *Create(llvm::Value *val, llvm::BasicBlock *insertAtEnd) {
        return static_cast<PushInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::push), val, "", insertAtEnd));
    }

    llvm::Value *getValue() const {
        return getArgOperand(0);
    }
};

class PopInst : public IntrinsicInst
{
public:
    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntrinsicInst *inst) {
        return inst->getIntrinsicID() == Intrinsic::pop;
    }
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<IntrinsicInst>(val) && classof(llvm::cast<IntrinsicInst>(val));
    }

    static PopInst *Create(llvm::Instruction *insertBefore = nullptr) {
        return static_cast<PopInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::pop), "", insertBefore));
    }
    static PopInst *Create(llvm::BasicBlock *insertAtEnd) {
        return static_cast<PopInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::pop), "", insertAtEnd));
    }
};

class RestInst : public IntrinsicInst
{
public:
    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntrinsicInst *inst) {
        return inst->getIntrinsicID() == Intrinsic::rest;
    }
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<IntrinsicInst>(val) && classof(llvm::cast<IntrinsicInst>(val));
    }

    static RestInst *Create(llvm::ConstantInt *paramIndex, llvm::Instruction *insertBefore = nullptr) {
        return static_cast<RestInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::rest), paramIndex, "", insertBefore));
    }
    static RestInst *Create(llvm::ConstantInt *paramIndex, llvm::BasicBlock *insertAtEnd) {
        return static_cast<RestInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::rest), paramIndex, "", insertAtEnd));
    }
};

class ClassInst : public IntrinsicInst
{
public:
    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntrinsicInst *inst) {
        return inst->getIntrinsicID() == Intrinsic::clss;
    }
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<IntrinsicInst>(val) && classof(llvm::cast<IntrinsicInst>(val));
    }

    static ClassInst *Create(llvm::ConstantInt *classID, llvm::Instruction *insertBefore = nullptr) {
        return static_cast<ClassInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::clss), classID, "", insertBefore));
    }
    static ClassInst *Create(llvm::ConstantInt *classID, llvm::BasicBlock *insertAtEnd) {
        return static_cast<ClassInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::clss), classID, "", insertAtEnd));
    }

    llvm::ConstantInt* getClassID() const {
        return llvm::cast<llvm::ConstantInt>(getArgOperand(0));
    }
};

class PropertyInst : public IntrinsicInst
{
public:
    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntrinsicInst *inst) {
        return inst->getIntrinsicID() == Intrinsic::prop;
    }
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<IntrinsicInst>(val) && classof(llvm::cast<IntrinsicInst>(val));
    }

    static PropertyInst *Create(llvm::ConstantInt *index, llvm::Instruction *insertBefore = nullptr) {
        return static_cast<PropertyInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::prop), index, "", insertBefore));
    }
    static PropertyInst *Create(llvm::ConstantInt *index, llvm::BasicBlock *insertAtEnd) {
        return static_cast<PropertyInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::prop), index, "", insertAtEnd));
    }

    llvm::ConstantInt* getIndex() const {
        return llvm::cast<llvm::ConstantInt>(getArgOperand(0));
    }
};

class SendMessageInst : public IntrinsicInst
{
public:
    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntrinsicInst *inst) {
        return inst->getIntrinsicID() == Intrinsic::send;
    }
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<IntrinsicInst>(val) && classof(llvm::cast<IntrinsicInst>(val));
    }

    static SendMessageInst *Create(ArrayRef<llvm::Value *> args, llvm::Instruction *insertBefore = nullptr) {
        return static_cast<SendMessageInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::send), args, "", insertBefore));
    }
    static SendMessageInst *Create(ArrayRef<llvm::Value *> args, llvm::BasicBlock *insertAtEnd) {
        return static_cast<SendMessageInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::send), args, "", insertAtEnd));
    }

    llvm::Value* getObject()   const { return getArgOperand(0); }
    llvm::Value* getSelector() const { return getArgOperand(1); }
    llvm::Value* getArgCount() const { return getArgOperand(2); }

    static uint MatchArgOperandIndex(llvm::Argument *arg);
    static uint MatchArgOperandIndex(Method *method, llvm::Argument *arg);
};

class ObjCastInst : public IntrinsicInst
{
public:
    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntrinsicInst *inst) {
        return inst->getIntrinsicID() == Intrinsic::objc;
    }
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<IntrinsicInst>(val) && classof(llvm::cast<IntrinsicInst>(val));
    }

    static ObjCastInst *Create(llvm::ConstantInt *classID, llvm::Instruction *insertBefore = nullptr) {
        return static_cast<ObjCastInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::objc), classID, "", insertBefore));
    }
    static ObjCastInst *Create(llvm::ConstantInt *classID, llvm::BasicBlock *insertAtEnd) {
        return static_cast<ObjCastInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::objc), classID, "", insertAtEnd));
    }

    llvm::Value* getObject() const {
        return getArgOperand(0);
    }
    llvm::ConstantInt* getClassID() const {
        return llvm::cast<llvm::ConstantInt>(getArgOperand(1));
    }
    void setClassID(llvm::ConstantInt *classId) {
        setArgOperand(1, classId);
    }
    llvm::Value* getAnchor() const {
        return getArgOperand(2);
    }
    SendMessageInst* getSendMessage() {
        return llvm::cast<SendMessageInst>(user_back());
    }


    op_iterator class_begin() { return arg_begin() + 1; }
    op_iterator class_end() { return arg_end(); }
    iterator_range<op_iterator> classes() {
        return make_range(class_begin(), class_end());
    }

    const_op_iterator class_begin() const { return arg_begin() + 1; }
    const_op_iterator class_end() const { return arg_end(); }
    iterator_range<const_op_iterator> classes() const {
        return make_range(class_begin(), arg_end());
    }
};

class CallInternalInst : public IntrinsicInst
{
public:
    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntrinsicInst *inst) {
        return inst->getIntrinsicID() == Intrinsic::call;
    }
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<IntrinsicInst>(val) && classof(llvm::cast<IntrinsicInst>(val));
    }

    static CallInternalInst *Create(ArrayRef<llvm::Value *> args, llvm::Instruction *insertBefore = nullptr) {
        return static_cast<CallInternalInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::call), args, "", insertBefore));
    }
    static CallInternalInst *Create(ArrayRef<llvm::Value *> args, llvm::BasicBlock *insertAtEnd) {
        return static_cast<CallInternalInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::call), args, "", insertAtEnd));
    }

    llvm::ConstantInt* getOffset() const {
        return llvm::cast<llvm::ConstantInt>(getArgOperand(0));
    }
    llvm::Value* getArgCount() const {
        return getArgOperand(1);
    }

    static uint MatchArgOperandIndex(llvm::Argument *arg);
    static uint MatchArgOperandIndex(Procedure *proc, llvm::Argument *arg);
};

class CallExternalInst: public IntrinsicInst
{
public:
    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntrinsicInst *inst) {
        return inst->getIntrinsicID() == Intrinsic::calle;
    }
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<IntrinsicInst>(val) && classof(llvm::cast<IntrinsicInst>(val));
    }

    static CallExternalInst *Create(ArrayRef<llvm::Value *> args, llvm::Instruction *insertBefore = nullptr) {
        return static_cast<CallExternalInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::calle), args, "", insertBefore));
    }
    static CallExternalInst *Create(ArrayRef<llvm::Value *> args, llvm::BasicBlock *insertAtEnd) {
        return static_cast<CallExternalInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::calle), args, "", insertAtEnd));
    }

    llvm::ConstantInt* getScriptID() const {
        return llvm::cast<llvm::ConstantInt>(getArgOperand(0));
    }
    llvm::ConstantInt* getEntryIndex() const {
        return llvm::cast<llvm::ConstantInt>(getArgOperand(1));
    }
    llvm::Value* getArgCount() const {
        return getArgOperand(2);
    }

    static uint MatchArgOperandIndex(llvm::Argument *arg);
    static uint MatchArgOperandIndex(Procedure *proc, llvm::Argument *arg);
};

class CallKernelInst : public IntrinsicInst
{
public:
    // Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntrinsicInst *inst) {
        return inst->getIntrinsicID() == Intrinsic::callk;
    }
    static inline bool classof(const llvm::Value *val) {
        return llvm::isa<IntrinsicInst>(val) && classof(llvm::cast<IntrinsicInst>(val));
    }

    static CallKernelInst *Create(ArrayRef<llvm::Value *> args, llvm::Instruction *insertBefore = nullptr) {
        return static_cast<CallKernelInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::callk), args, "", insertBefore));
    }
    static CallKernelInst *Create(ArrayRef<llvm::Value *> args, llvm::BasicBlock *insertAtEnd) {
        return static_cast<CallKernelInst *>(
            llvm::CallInst::Create(Intrinsic::Get(Intrinsic::callk), args, "", insertAtEnd));
    }

    llvm::ConstantInt* getKernelOrdinal() const {
        return llvm::cast<llvm::ConstantInt>(getArgOperand(0));
    }
    llvm::Value* getArgCount() const {
        return getArgOperand(1);
    }
};

END_NAMESPACE_SCI

#endif // !_Intrinsics_HPP_
