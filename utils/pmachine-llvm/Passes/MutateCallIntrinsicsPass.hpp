#pragma once
#ifndef _MutateCallIntrinsicsPass_HPP_
#define _MutateCallIntrinsicsPass_HPP_

#include "../Types.hpp"
#include "../Intrinsics.hpp"

BEGIN_NAMESPACE_SCI

class MutateCallIntrinsicsPass
{
public:
    MutateCallIntrinsicsPass();
    ~MutateCallIntrinsicsPass();

    void run();

private:
    void mutateCallKernel(CallKernelInst *callk);
    void mutateCallInternal(CallInternalInst *calli);
    void mutateCallExternal(CallExternalInst *calle);
    void mutateSendMessage(SendMessageInst *sendMsg);

    void extractRest(llvm::CallInst *call, llvm::Value *&restc, llvm::Value *&restv) const;
    llvm::CallInst* createPropertyCall(SendMessageInst *sendMsg, bool getter);
    llvm::CallInst* createMethodCall(SendMessageInst *sendMsg, bool method, llvm::Value *restc, llvm::Value *restv);
    llvm::CallInst* createSuperMethodCall(SendMessageInst *sendMsg, llvm::Value *restc, llvm::Value *restv);

    llvm::CallInst* delegateCall(llvm::CallInst *stubCall,
                                 llvm::Function *func, uint prefixc,
                                 llvm::Value *self, llvm::Value *sel,
                                 llvm::Value *restc, llvm::Value *restv,
                                 llvm::ConstantInt *argc, llvm::CallInst::op_iterator argi);

    llvm::Function* getOrCreateDelegator(llvm::ConstantInt *argc, llvm::Module *module);

    llvm::Function* getSendMessageFunction(llvm::Module *module);
    llvm::Function* getGetPropertyFunction(llvm::Module *module);
    llvm::Function* getSetPropertyFunction(llvm::Module *module);
    llvm::Function* getCallMethodFunction(llvm::Module *module);

    llvm::Function* getOrCreateKernelFunction(uint id, llvm::Module *module);

    llvm::IntegerType *m_sizeTy;
    llvm::ConstantInt *m_sizeBytesVal;
    uint m_sizeAlign;

    llvm::PointerType *m_int8PtrTy;

    llvm::ConstantInt *m_zero;
    llvm::ConstantInt *m_one;
    llvm::ConstantInt *m_allOnes;
    llvm::ConstantPointerNull *m_nullPtr;

    llvm::Function *m_sendMsgFunc;
    llvm::Function *m_getPropFunc;
    llvm::Function *m_setPropFunc;
    llvm::Function *m_callMethodFunc;

    static const char* s_kernelNames[];
};

END_NAMESPACE_SCI

#endif // !_MutateCallIntrinsicsPass_HPP_
