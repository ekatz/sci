#pragma once
#ifndef _ValueRetracer_HPP_
#define _ValueRetracer_HPP_

#include "../Types.hpp"
#include <llvm/IR/Instructions.h>
#include <unordered_map>

BEGIN_NAMESPACE_SCI

class ValueRetracer
{
public:
    void clear();
    uint retrace(llvm::Value *val);

    void match(llvm::Value *val, llvm::GetElementPtrInst *gep = nullptr);

private:
    void retraceBasicBlockTransition(llvm::AllocaInst *accAddr, llvm::BasicBlock *bb);
    void retraceValue(llvm::Value *val);
    void retraceStoredValue(llvm::Value *ptrVal);
    void retraceReturnValue(llvm::CallInst *call);
    void retraceFunctionReturnValue(llvm::Function *func);

    bool addBasicBlock(llvm::BasicBlock *bb);
    bool addValue(llvm::Value *val);

    llvm::GetElementPtrInst *m_gep = nullptr;
    llvm::LoadInst *m_load = nullptr;
    llvm::SmallVector<llvm::BasicBlock *, 8> m_blocks;
    llvm::SmallVector<llvm::Value *, 4> m_values;
};

END_NAMESPACE_SCI

#endif // !_ValueRetracer_HPP_
