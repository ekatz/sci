//===- PMachine.hpp -------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_PMACHINE_HPP
#define SCI_UTILS_PMACHINE_LLVM_PMACHINE_HPP

#include "Script.hpp"
#include "World.hpp"
#include "llvm/IR/Instructions.h"

namespace sci {

class PMachine {
public:
  PMachine(Script &S);

  llvm::Function *interpretFunction(const uint8_t *Code,
                                    StringRef Name = StringRef(),
                                    unsigned ID = -1U, Class *Cls = nullptr,
                                    bool Debug = false);

private:
  uint8_t getByte() { return *PC++; }
  int8_t getSByte() { return (int8_t)*PC++; }
  uint16_t getWord() { return *((uint16_t *)(PC += 2) - 1); }
  int16_t getSWord() { return *((int16_t *)(PC += 2) - 1); }

  unsigned getUInt(uint8_t Opcode) {
    return (Opcode & 1) != 0 ? (unsigned)getByte() : (unsigned)getWord();
  }
  int getSInt(uint8_t Opcode) {
    return (Opcode & 1) != 0 ? (int)getSByte() : (int)getSWord();
  }

  llvm::Value *castValueToSizeType(llvm::Value *Val, llvm::BasicBlock *BB);
  llvm::Value *castValueToSizeType(llvm::Value *Val);
  void castAccToSizeType();
  void fixAccConstantInt();
  void storeAcc();
  llvm::Value *loadAcc();
  void storePrevAcc();
  llvm::Value *loadPrevAcc();

  llvm::Value *callPop();
  void callPush(llvm::Value *Val);

  llvm::Module *getModule() const { return TheScript.getModule(); }

  llvm::BasicBlock *getBasicBlock(const uint8_t *Label,
                                  StringRef Name = "label");

  llvm::Function *getCallIntrinsic();

  void processBasicBlocks();

  // Return true if more instructions left in the block.
  bool processNextInstruction();

  void emitDebugLog();

  void emitSend(llvm::Value *Obj);
  void emitCall(llvm::Function *Func, ArrayRef<llvm::Constant *> Constants);

  llvm::Value *getIndexedPropPtr(uint8_t Opcode);
  llvm::Value *getValueByOffset(uint8_t Opcode);
  llvm::Value *getIndexedVariablePtr(uint8_t Opcode, unsigned Idx);

  typedef bool (PMachine::*PfnOp)(uint8_t Opcode);

  bool bnotOp(uint8_t Opcode);
  bool addOp(uint8_t Opcode);
  bool subOp(uint8_t Opcode);
  bool mulOp(uint8_t Opcode);
  bool divOp(uint8_t Opcode);
  bool modOp(uint8_t Opcode);
  bool shrOp(uint8_t Opcode);
  bool shlOp(uint8_t Opcode);
  bool xorOp(uint8_t Opcode);
  bool andOp(uint8_t Opcode);
  bool orOp(uint8_t Opcode);
  bool negOp(uint8_t Opcode);
  bool notOp(uint8_t Opcode);
  bool cmpOp(uint8_t Opcode);
  bool btOp(uint8_t Opcode);
  bool jmpOp(uint8_t Opcode);
  bool loadiOp(uint8_t Opcode);
  bool pushOp(uint8_t Opcode);
  bool pushiOp(uint8_t Opcode);
  bool tossOp(uint8_t Opcode);
  bool dupOp(uint8_t Opcode);
  bool linkOp(uint8_t Opcode);
  bool callOp(uint8_t Opcode);
  bool callkOp(uint8_t Opcode);
  bool callbOp(uint8_t Opcode);
  bool calleOp(uint8_t Opcode);
  bool retOp(uint8_t Opcode);
  bool sendOp(uint8_t Opcode);
  bool classOp(uint8_t Opcode);
  bool selfOp(uint8_t Opcode);
  bool superOp(uint8_t Opcode);
  bool restOp(uint8_t Opcode);
  bool leaOp(uint8_t Opcode);
  bool selfIdOp(uint8_t Opcode);
  bool pprevOp(uint8_t Opcode);
  bool pToaOp(uint8_t Opcode);
  bool aTopOp(uint8_t Opcode);
  bool pTosOp(uint8_t Opcode);
  bool sTopOp(uint8_t Opcode);
  bool ipToaOp(uint8_t Opcode);
  bool dpToaOp(uint8_t Opcode);
  bool ipTosOp(uint8_t Opcode);
  bool dpTosOp(uint8_t Opcode);
  bool lofsaOp(uint8_t Opcode);
  bool lofssOp(uint8_t Opcode);
  bool push0Op(uint8_t Opcode);
  bool push1Op(uint8_t Opcode);
  bool push2Op(uint8_t Opcode);
  bool pushSelfOp(uint8_t Opcode);
  bool ldstOp(uint8_t Opcode);
  bool badOp(uint8_t Opcode);

  static PfnOp OpTable[];

  Script &TheScript;
  llvm::LLVMContext &Context;
  llvm::IntegerType *Int32Ty;
  llvm::IntegerType *SizeTy;
  llvm::Function *FuncCallIntrin = nullptr;
  const uint8_t *PC = nullptr;

  llvm::Value *Acc = nullptr;
  llvm::AllocaInst *AccAddr = nullptr;
  llvm::AllocaInst *PAccAddr = nullptr;

  llvm::BasicBlock *CurrBlock = nullptr;
  llvm::BasicBlock *Entry = nullptr;

  llvm::AllocaInst *Temp = nullptr;
  unsigned TempCount = 0U;

  llvm::CallInst *Rest = nullptr;
  llvm::Type *RetTy = nullptr;

  std::unique_ptr<llvm::Argument> Self;
  std::unique_ptr<llvm::Argument> Args;

  std::map<unsigned, llvm::BasicBlock *> Labels;
  llvm::SmallVector<std::pair<const uint8_t *, llvm::BasicBlock *>, 4> WorkList;

  Class *ThisClass = nullptr;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_PMACHINE_HPP
