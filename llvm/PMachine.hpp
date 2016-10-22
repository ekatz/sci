#pragma once
#ifndef _PMachine_HPP_
#define _PMachine_HPP_

#include "World.hpp"
#include "Script.hpp"
#include <llvm/IR/Instructions.h>

BEGIN_NAMESPACE_SCI

class PMachine
{
public:
    PMachine(Script &script);

    llvm::Function* interpretFunction(const uint8_t *code, StringRef name = StringRef(), uint id = (uint)-1);

    bool hasArgc() const { return (m_argc != nullptr); }
    bool hasVaList() const { return (m_vaList.get() != nullptr); }
    uint getParamCount() const;

private:
    uint8_t getByte() { return *m_pc++; }
    int8_t getSByte() { return (int8_t)*m_pc++; }
    uint16_t getWord() { return *((uint16_t*)(m_pc += 2) - 1); }
    int16_t getSWord() { return *((int16_t*)(m_pc += 2) - 1); }

    uint getUInt(uint8_t opcode) { return (opcode & 1) != 0 ? (uint)getByte() : (uint)getWord(); }
    int getSInt(uint8_t opcode) { return (opcode & 1) != 0 ? (int)getSByte() : (int)getSWord(); }

    llvm::Value* castValueToSizeType(llvm::Value *val, llvm::BasicBlock *bb);
    llvm::Value* castValueToSizeType(llvm::Value *val);
    void castAccToSizeType();
    void storeAcc();
    llvm::Value* loadAcc();
    void storePrevAcc();
    llvm::Value* loadPrevAcc();

    llvm::Value* callPop();
    void callPush(llvm::Value *val);

    llvm::Module* getModule() const { return m_script.getModule(); }

    llvm::BasicBlock* getBasicBlock(const uint8_t *label, StringRef name = "label");

    llvm::Function* getStubCallFunction();
    llvm::Function* getKernelFunction(uint id);

    void processBasicBlocks();

    // Return true if more instructions left in the block.
    bool processNextInstruction();

    void emitSend(llvm::Value *obj);
    void emitCall(llvm::Function *func, ArrayRef<llvm::Constant*> constants);

    llvm::Value* getIndexedPropPtr(uint8_t opcode);
    llvm::Value* getValueByOffset(uint8_t opcode);
    llvm::Value* getIndexedVariablePtr(uint8_t opcode, uint idx);
    llvm::Instruction* getParameter(uint idx);
    llvm::Argument* getVaList(uint idx);

    typedef bool (PMachine::*PfnOp)(uint8_t opcode);

    bool bnotOp(uint8_t opcode);
    bool addOp(uint8_t opcode);
    bool subOp(uint8_t opcode);
    bool mulOp(uint8_t opcode);
    bool divOp(uint8_t opcode);
    bool modOp(uint8_t opcode);
    bool shrOp(uint8_t opcode);
    bool shlOp(uint8_t opcode);
    bool xorOp(uint8_t opcode);
    bool andOp(uint8_t opcode);
    bool orOp(uint8_t opcode);
    bool negOp(uint8_t opcode);
    bool notOp(uint8_t opcode);
    bool cmpOp(uint8_t opcode);
    bool btOp(uint8_t opcode);
    bool jmpOp(uint8_t opcode);
    bool loadiOp(uint8_t opcode);
    bool pushOp(uint8_t opcode);
    bool pushiOp(uint8_t opcode);
    bool tossOp(uint8_t opcode);
    bool dupOp(uint8_t opcode);
    bool linkOp(uint8_t opcode);
    bool callOp(uint8_t opcode);
    bool callkOp(uint8_t opcode);
    bool callbOp(uint8_t opcode);
    bool calleOp(uint8_t opcode);
    bool retOp(uint8_t opcode);
    bool sendOp(uint8_t opcode);
    bool classOp(uint8_t opcode);
    bool selfOp(uint8_t opcode);
    bool superOp(uint8_t opcode);
    bool restOp(uint8_t opcode);
    bool leaOp(uint8_t opcode);
    bool selfIdOp(uint8_t opcode);
    bool pprevOp(uint8_t opcode);
    bool pToaOp(uint8_t opcode);
    bool aTopOp(uint8_t opcode);
    bool pTosOp(uint8_t opcode);
    bool sTopOp(uint8_t opcode);
    bool ipToaOp(uint8_t opcode);
    bool dpToaOp(uint8_t opcode);
    bool ipTosOp(uint8_t opcode);
    bool dpTosOp(uint8_t opcode);
    bool lofsaOp(uint8_t opcode);
    bool lofssOp(uint8_t opcode);
    bool push0Op(uint8_t opcode);
    bool push1Op(uint8_t opcode);
    bool push2Op(uint8_t opcode);
    bool pushSelfOp(uint8_t opcode);
    bool ldstOp(uint8_t opcode);
    bool badOp(uint8_t opcode);

    static PfnOp s_opTable[];
    static const char* s_kernelNames[];

    Script &m_script;
    llvm::LLVMContext &m_ctx;
    llvm::IntegerType *m_sizeTy;
    llvm::Function *m_funcStubCall;
    const uint8_t *m_pc;

    llvm::Value *m_acc;
    llvm::AllocaInst *m_accAddr;
    llvm::AllocaInst *m_paccAddr;

    llvm::BasicBlock *m_bb;
    llvm::BasicBlock *m_entry;

    llvm::AllocaInst *m_temp;
    uint m_tempCount;

    llvm::CallInst *m_rest;
    llvm::Type *m_retTy;

    std::unique_ptr<llvm::Argument> m_self;
    std::unique_ptr<llvm::Argument> m_vaList;
    uint m_vaListIndex;
    llvm::AllocaInst *m_argc;
    llvm::AllocaInst *m_param1;
    uint m_paramCount;

    std::map<uint, llvm::BasicBlock *> m_labels;
    llvm::SmallVector<std::pair<const uint8_t *, llvm::BasicBlock *>, 4> m_worklist;
};

END_NAMESPACE_SCI

#endif // !_PMachine_HPP_
