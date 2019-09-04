//===- Passes/MutateCallIntrinsicsPass.hpp --------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_PASSES_MUTATECALLINTRINSICSPASS_HPP
#define SCI_UTILS_PMACHINE_LLVM_PASSES_MUTATECALLINTRINSICSPASS_HPP

#include "../Intrinsics.hpp"
#include "../Types.hpp"

namespace sci {

class MutateCallIntrinsicsPass {
public:
  MutateCallIntrinsicsPass();
  ~MutateCallIntrinsicsPass();

  void run();

private:
  void mutateCallKernel(CallKernelInst *Callk);
  void mutateCallInternal(CallInternalInst *Calli);
  void mutateCallExternal(CallExternalInst *Calle);
  void mutateSendMessage(SendMessageInst *SendMsg);

  void extractRest(llvm::CallInst *Call, llvm::Value *&Restc,
                   llvm::Value *&Restv) const;
  llvm::CallInst *createPropertyCall(SendMessageInst *SendMsg, bool IsGetter);
  llvm::CallInst *createMethodCall(SendMessageInst *SendMsg, bool IsMethod,
                                   llvm::Value *Restc, llvm::Value *Restv);
  llvm::CallInst *createSuperMethodCall(SendMessageInst *SendMsg,
                                        llvm::Value *Restc, llvm::Value *Restv);

  llvm::CallInst *delegateCall(llvm::CallInst *StubCall, llvm::Function *Func,
                               unsigned Prefixc, llvm::Value *self,
                               llvm::Value *Sel, llvm::Value *Restc,
                               llvm::Value *Restv, llvm::ConstantInt *Argc,
                               llvm::CallInst::op_iterator ArgI);

  llvm::Function *getOrCreateDelegator(llvm::ConstantInt *Argc,
                                       llvm::Module *M);

  llvm::Function *getSendMessageFunction(llvm::Module *M);
  llvm::Function *getGetPropertyFunction(llvm::Module *M);
  llvm::Function *getSetPropertyFunction(llvm::Module *M);
  llvm::Function *getCallMethodFunction(llvm::Module *M);

  llvm::Function *getOrCreateKernelFunction(unsigned ID, llvm::Module *M);

  llvm::IntegerType *SizeTy;
  llvm::IntegerType *Int32Ty;
  llvm::ConstantInt *SizeBytesVal;
  unsigned SizeAlign;

  llvm::PointerType *Int8PtrTy;

  llvm::ConstantInt *Zero;
  llvm::ConstantInt *One;
  llvm::ConstantInt *AllOnes;
  llvm::ConstantPointerNull *NullPtr;

  llvm::Function *SendMsgFunc;
  llvm::Function *GetPropFunc;
  llvm::Function *SetPropFunc;
  llvm::Function *CallMethodFunc;

  static StringRef KernelNames[];
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_PASSES_MUTATECALLINTRINSICSPASS_HPP
