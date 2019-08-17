#pragma once
#ifndef _AnalyzeObjectsPass_HPP_
#define _AnalyzeObjectsPass_HPP_

#include "../Types.hpp"
#include <llvm/IR/Instructions.h>

BEGIN_NAMESPACE_SCI

class Class;

class AnalyzeObjectsPass
{
public:
    AnalyzeObjectsPass();
    ~AnalyzeObjectsPass();

    void run();

private:
    void analyzeObjects();
    void analyzeClassObjects();

    void analyzeObject(llvm::Value *obj, Class *cls);
    void analyzeStoredObject(llvm::Value *ptrVal, llvm::GetElementPtrInst *gep, Class *cls);

    void analyzeValue(llvm::Value *val, Class *cls);
};

class ObjectAnalyzer
{
public:
    ObjectAnalyzer(Class *cls);
    ~ObjectAnalyzer();

    void evolve(llvm::Value *val);

private:
    void analyzeAsArgument(llvm::Value *arg, llvm::CallInst *call);
    void analyzeAsProperty();

    Class *m_class;
};

END_NAMESPACE_SCI

#endif // !_AnalyzeObjectsPass_HPP_
