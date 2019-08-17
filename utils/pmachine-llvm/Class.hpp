#pragma once
#ifndef _Class_HPP_
#define _Class_HPP_

#include "Types.hpp"
#include <llvm/IR/Constants.h>

struct ObjRes;

BEGIN_NAMESPACE_SCI

class Script;
class Method;
class Property;

class Class
{
public:
    static llvm::StructType* GetAbstractType();
    static Class* Get(uint id);

    bool loadMethods();

    uint getId() const { return m_id; }

    virtual StringRef getName() const;
    llvm::StructType* getType() const { return m_type; }

    Class* getSuper() const { return m_super; }
    ArrayRef<Method> getOverloadedMethods() const;

    Method* getMethod(uint id) const;
    Method* getMethod(uint id, int &index) const;
    ArrayRef<Method*> getMethods() const;
    uint getMethodCount() const { return m_methodCount; }

    Property* getProperty(uint id) const;
    Property* getProperty(uint id, int &index) const;
    ArrayRef<Property> getProperties() const;
    uint getPropertyCount() const { return m_propCount; }

    Script& getScript() const { return m_script; }
    llvm::Module& getModule() const;

    llvm::GlobalVariable* getSpecies() const { return m_species; }

    bool isObject() const;

    uint getDepth() const { return m_depth; }

private:
    uint countMethods() const;
    void createMethods();

protected:
    void createSpecies();
    int findMethod(uint id) const;
    int findProperty(uint id) const;

    Class(const ObjRes &res, Script &script);
    ~Class();


    uint m_id;
    uint m_depth;
    llvm::StructType *m_type;
    llvm::GlobalVariable *m_species;
    llvm::GlobalVariable *m_methodOffs;

    Property *m_props;
    uint m_propCount;

    Method *m_overloadMethods;
    uint m_overloadMethodCount;

    std::unique_ptr<Method*[]> m_methods;
    uint m_methodCount;

    Class *m_super;
    Script &m_script;
};

END_NAMESPACE_SCI

#endif // !_Class_HPP_
