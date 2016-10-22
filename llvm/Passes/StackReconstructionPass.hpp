#pragma once
#ifndef _StackReconstructionPass_HPP_
#define _StackReconstructionPass_HPP_

#include "../Types.hpp"
#include <llvm/IR/Instructions.h>
#include <set>

BEGIN_NAMESPACE_SCI

class StackReconstructionPass
{
public:
    StackReconstructionPass();
    ~StackReconstructionPass();

    void run();

    bool verifyFunction(llvm::Function &func);
    int calcBasicBlockBalance(llvm::BasicBlock &bb);

private:
    llvm::Instruction* runOnPop(llvm::CallInst *callPop, llvm::AllocaInst *stackAddr = nullptr);
    llvm::Instruction* runOnPush(llvm::CallInst *callPush, llvm::AllocaInst *stackAddr = nullptr);

    llvm::CallInst* createStubStore(llvm::CallInst *callPush, llvm::AllocaInst *stackAddr);
    llvm::CallInst* createStubLoad(llvm::CallInst *callPop, llvm::AllocaInst *stackAddr);

    uint countStores(llvm::AllocaInst &inst, llvm::Constant *&c);
    void eliminateRedundantStores();
    void mutateStubs();

    bool verifyPath(llvm::BasicBlock &bb, int balance, const std::map<llvm::BasicBlock *, int> &balances, std::set<llvm::BasicBlock *> &visits);

    llvm::Instruction *m_insertPt;
    std::unique_ptr<llvm::Function> m_fnStubStore;
    std::unique_ptr<llvm::Function> m_fnStubLoad;
};

END_NAMESPACE_SCI

#endif // !_StackReconstructionPass_HPP_
