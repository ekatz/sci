#pragma once
#ifndef _PMachine_HPP_
#define _PMachine_HPP_

#include "Script.hpp"
#include <map>

class PMachineInterpreter
{
public:
    PMachineInterpreter(Script &script);

    llvm::Function* createFunction(const uint8_t *code, uint id);

private:
    uint8_t getByte() { return *m_pc++; }
    int8_t getSByte() { return (int8_t)*m_pc++; }
    uint16_t getWord() { return *((uint16_t*)(m_pc += 2) - 1); }
    int16_t getSWord() { return *((int16_t*)(m_pc += 2) - 1); }

    uint getUInt(uint8_t opcode) { return (opcode & 1) != 0 ? (uint)getByte() : (uint)getWord(); }
    int getSInt(uint8_t opcode) { return (opcode & 1) != 0 ? (int)getSByte() : (int)getSWord(); }

    void fixAcc();
    void storeAcc();
    llvm::Value* loadAcc();
    void storePrevAcc();
    llvm::Value* loadPrevAcc();

    llvm::Module* getModule() const { return m_script.getModule(); }

    llvm::BasicBlock* getBasicBlock(const uint8_t *label, const llvm::StringRef &name = "");

    llvm::Function* getKernelFunction(uint id);

    void processBasicBlocks();

    // Return true if more instructions left in the block.
    bool processNextInstruction();

    typedef bool (PMachineInterpreter::*PfnOp)(uint8_t opcode);

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
    const uint8_t *m_pc;
    llvm::Value *m_acc;
    llvm::AllocaInst *m_accAddr;
    llvm::AllocaInst *m_paccAddr;
    llvm::BasicBlock *m_bb;
    llvm::Value *m_temp;

    llvm::Argument *m_self;
    llvm::Argument *m_argc;
    llvm::Function::ArgumentListType m_args;
    uint m_numArgs;

    std::map<const uint8_t *, llvm::BasicBlock *> m_labels;
    llvm::SmallVector<llvm::BasicBlock *, 4> m_worklist;

    llvm::Function *m_funcPush;
    llvm::Function *m_funcPop;
};


#endif // !_PMachine_HPP_
