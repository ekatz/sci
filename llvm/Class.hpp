#pragma once
#ifndef _Class_HPP_
#define _Class_HPP_

#include "Types.hpp"
#include <llvm/IR/Constants.h>

struct ObjRes;

BEGIN_NAMESPACE_SCI

class Script;
class Method;

class Class
{
public:
    static llvm::StructType* GetAbstractType();
    static Class* Get(uint id);

    bool loadMethods();

    uint countAggregatedMethods() const;

    virtual StringRef getName() const;
    llvm::StructType* getType() const { return m_type; }

    Class* getSuper() const { return m_super; }
    ArrayRef<Method> getMethods() const;

    ArrayRef<int16_t> getProperties() const;

    Script& getScript() const { return m_script; }
    llvm::Module& getModule() const;

    llvm::GlobalVariable* getObject();

    llvm::GlobalVariable* getVirtualTable() const { return m_vftbl; }
    llvm::Function* getConstructor() const { return m_ctor; }

protected:
    llvm::GlobalVariable* createVirtualTable() const;
    uint aggregateMethods(llvm::MutableArrayRef<Method*> methods) const;

    Class(const ObjRes &res, Script &script);
    ~Class();


    uint m_id;
    llvm::StructType *m_type;
    llvm::GlobalVariable *m_vftbl;

    llvm::GlobalVariable *m_global;
    llvm::Function *m_ctor;

    const int16_t *m_propVals;
    const ObjID *m_propSels;
    uint m_propCount;

    Method *m_methods;
    uint m_methodCount;

    Class *m_super;
    Script &m_script;


    friend class World;
};

END_NAMESPACE_SCI

#endif // !_Class_HPP_
