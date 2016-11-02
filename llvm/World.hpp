#pragma once
#ifndef _World_HPP_
#define _World_HPP_

#include "Intrinsics.hpp"
#include "SelectorTable.hpp"
#include "ScriptIterator.hpp"
#include <llvm/IR/DataLayout.h>
#include <llvm/ADT/iterator_range.h>

struct ObjRes;


BEGIN_NAMESPACE_SCI

class Class;
class Object;
class Procedure;

class World
{
public:
    World();
    ~World();

    void setDataLayout(const llvm::DataLayout &dl);

    bool load();

    llvm::LLVMContext& getContext() const { return m_ctx; }
    llvm::IntegerType* getSizeType() const { return m_sizeTy; }
    llvm::ConstantInt* getConstantValue(int16_t val) const;

    Class* addClass(const ObjRes &res, Script &script);
    Class* getClass(uint id);
    llvm::StructType* getAbstractClassType() const { return m_absClassTy; }

    SelectorTable& getSelectorTable() { return m_sels; }
    StringRef getSelectorName(uint id);

    uint getGlobalVariablesCount() const;

    llvm::Function* getIntrinsic(Intrinsic::ID iid) const { return m_intrinsics.get(iid); }

    Script* getScript(llvm::Module &module) const;
    Script* getScript(uint id) const;

    Object* lookupObject(llvm::GlobalVariable &var) const;

    bool registerProcedure(Procedure &proc);
    Procedure* getProcedure(const llvm::Function &func) const;


    const_script_iterator begin() const {
        return const_script_iterator(&m_scripts[0], &m_scripts[1000]);
    }
    script_iterator begin() {
        return script_iterator(&m_scripts[0], &m_scripts[1000]);
    }

    const_script_iterator end() const {
        return const_script_iterator(&m_scripts[1000], &m_scripts[1000]);
    }
    script_iterator end() {
        return script_iterator(&m_scripts[1000], &m_scripts[1000]);
    }

    llvm::iterator_range<script_iterator> scripts() {
        return llvm::make_range(begin(), end());
    }

    llvm::iterator_range<const_script_iterator> scripts() const {
        return llvm::make_range(begin(), end());
    }

private:
    Script* acquireScript(uint id);


    llvm::DataLayout m_dataLayout;
    llvm::LLVMContext &m_ctx;
    llvm::IntegerType *m_sizeTy;
    llvm::StructType *m_absClassTy;
    std::unique_ptr<Script> m_scripts[1000];
    Class *m_classes;
    uint m_classCount;

    llvm::DenseMap<const llvm::Function *, Procedure *> m_funcMap;
    SelectorTable m_sels;

    Intrinsic m_intrinsics;
};


World& GetWorld();

END_NAMESPACE_SCI

#endif // !_World_HPP_
