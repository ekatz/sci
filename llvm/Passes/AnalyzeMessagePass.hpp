#pragma once
#ifndef _AnalyzeMessagePass_HPP_
#define _AnalyzeMessagePass_HPP_

#include "../Types.hpp"
#include <llvm/IR/Instructions.h>
#include <set>

BEGIN_NAMESPACE_SCI

class Class;
class Procedure;
class Method;

class AnalyzeMessagePass
{
public:
    AnalyzeMessagePass();
    ~AnalyzeMessagePass();

    void run();

    ArrayRef<llvm::CallInst*> getReferences(uint selector) const;
    ArrayRef<llvm::CallInst*> getPropReferences(uint index) const;

private:
    void initializeReferences();
    
    void mutateSuper(llvm::CallInst *call);

    std::unique_ptr<std::pair<std::unique_ptr<llvm::CallInst*[]>, uint>[]> m_propRefs;
    std::unique_ptr<std::pair<std::unique_ptr<llvm::CallInst*[]>, uint>[]> m_refs;
    std::unique_ptr<llvm::CallInst*[]> m_varRefs;
    uint m_varRefCount;

    llvm::DenseMap<uint32_t, llvm::Constant *> m_storedGlobals;

    uint m_selector;

    enum {
        KListInit_NewNode,
        KListInit_AddAfter,
        KListInit_AddToFront,
        KListInit_AddToEnd
    };
    std::pair<std::unique_ptr<llvm::CallInst*[]>, uint> m_klistInitRefs[4];
};


class MessageAnalyzer
{
public:
    MessageAnalyzer(AnalyzeMessagePass &pass);
    Class* analyze(llvm::CallInst *call);

private:
    Class* analyzeMessage(llvm::CallInst *call);
    bool analyzeValue(llvm::Value *val);
    bool analyzeArgument(llvm::Argument *arg);

    bool analyzeStoredLocal(llvm::Value *ptrVal);
    bool analyzeStoredGlobal(llvm::GlobalVariable *var);
    bool analyzeStoredScriptVariable(llvm::GlobalVariable *var, llvm::GetElementPtrInst *gep);

    bool analyzeReturnValue(llvm::CallInst *call);
    bool analyzeProcedureReturnValue(Procedure *proc);
    bool analyzeMethodReturnValue(Class *cls, Method *method);
    bool analyzeStoredProperty(Class *cls, llvm::CallInst *call);

    bool analyzeKListNodeValue(llvm::CallInst *call);


    AnalyzeMessagePass &m_pass;
    llvm::GetElementPtrInst *m_gep;


    struct PotentialSelector
    {
        bool isProperty = true;
        bool isMethod = true;
    };

    std::vector<Class *> m_classes;
    std::vector<llvm::CallInst *> m_calls;

    std::set<llvm::CallInst *> m_klistCalls;
    llvm::Value *m_klist;

    bool updatePotentialClasses(Class *cls);

    void gatherCallIntersectionBaseClasses(llvm::Value *objVal);
    void gatherValueBaseClasses(llvm::Value *objVal, ArrayRef<llvm::Instruction *> insts, llvm::SmallDenseMap<uint, PotentialSelector> &potentialSels);
    void gatherValueBaseClasses(llvm::Value *objVal, ArrayRef<llvm::Instruction *> insts);
    void gatherArgumentBaseClasses(llvm::Argument *arg, ArrayRef<llvm::Instruction *> insts);
    void addPotentialSelector(llvm::CallInst *call, llvm::SmallDenseMap<uint, PotentialSelector> &potentialSels);
    void intersectClasses(const llvm::SmallDenseMap<uint, PotentialSelector> &potentialSels);
    static Class* TraverseBaseClass(Class *cls, const llvm::SmallDenseMap<uint, PotentialSelector> &potentialSels);
    static Class* TraverseBaseClass(Class *cls, uint sel, bool isProperty, bool isMethod);

    static int KernelOrdinalToRefIndex(uint ordinal);
};


class ObjectAnalyzer
{
public:
    Class* analyze(ObjCastInst *objc);

private:
    Class* analyzeValue(llvm::Value *val);
    Class* analyzeArgument(llvm::Argument *arg);
    Class* analyzeMethodArgument(Method *method, llvm::Argument *arg);

    Class* analyzeProcedureArgument(Procedure *proc, llvm::Argument *arg);
    Class* analyzeInternalProcedureArgument(Procedure *proc, uint argIndex);
    Class* analyzeExternalProcedureArgument(Procedure *proc, uint argIndex);

    void chooseClass(Class *cls);

    void gatherObjectCalls(llvm::Value *anchor);
    uint* lookupSelectorInfo(uint sel);
    void addSelectorInfo(SendMessageInst *sendMsg);

    void intersectClasses();
    Class* traverseBaseClass(Class *cls);
    static Class* TraverseBaseClass(Class *cls, uint sel, bool isProperty, bool isMethod);

    enum {
        SEL_TYPE_PROP = 1 << (8 * sizeof(uint) - 1),
        SEL_TYPE_METHOD = (uint)SEL_TYPE_PROP >> 1,

        SEL_TYPE_MASK = SEL_TYPE_PROP | SEL_TYPE_METHOD
    };

    llvm::GetElementPtrInst *m_gep = nullptr;
    std::vector<ObjCastInst *> m_calls;
    std::vector<uint> m_selInfos;
    std::vector<Class *> m_classes;
};


class ValueTracer
{
public:
    void clear();
    llvm::Value* backtrace(llvm::Value *val);

    void match(llvm::Value *val, llvm::GetElementPtrInst *gep = nullptr);

private:
    bool addBasicBlock(llvm::BasicBlock *bb);
    llvm::Value* backtraceBasicBlockTransition(llvm::AllocaInst *accAddr, llvm::BasicBlock *bb);
    llvm::Value* backtraceValue(llvm::Value *val);
    llvm::Value* backtraceStoredValue(llvm::Value *ptrVal);
    llvm::Value* backtraceReturnValue(llvm::CallInst *call);
    llvm::Value* backtraceFunctionReturnValue(llvm::Function *func);

    llvm::GetElementPtrInst *m_gep = nullptr;
    llvm::LoadInst *m_load = nullptr;
    llvm::SmallVector<llvm::BasicBlock *, 8> m_blocks;
};

END_NAMESPACE_SCI

#endif // !_AnalyzeMessagePass_HPP_
