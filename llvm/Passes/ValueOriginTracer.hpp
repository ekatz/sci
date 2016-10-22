#pragma once
#ifndef _ValueOriginTracer_HPP_
#define _ValueOriginTracer_HPP_

#include "../Types.hpp"
#include <llvm/IR/Instructions.h>
#include <unordered_map>

BEGIN_NAMESPACE_SCI

class ValueOriginTracer
{
public:
    ValueOriginTracer();
    ~ValueOriginTracer();

    void run();

private:
    void addReference(llvm::CallInst *call);

    llvm::Value* backtraceValue(llvm::Value *val);
    llvm::Value* backtraceReturnValue(llvm::CallInst *call);
    llvm::Value* traceStoredLocal(llvm::Value *ptrVal);
    llvm::Value* backtraceArgument(llvm::Argument *arg);
    llvm::Value* traceStoredGlobal(llvm::GlobalVariable *var);
    llvm::Value* traceStoredScriptVariable(llvm::GlobalVariable *var, llvm::GetElementPtrInst &gep);

    llvm::GetElementPtrInst *m_gep;
    std::unique_ptr<std::unique_ptr<llvm::CallInst*[]>[]> m_refs;
};

END_NAMESPACE_SCI

#endif // !_ValueOriginTracer_HPP_
