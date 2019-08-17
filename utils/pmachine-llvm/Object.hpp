#pragma once
#ifndef _Object_HPP_
#define _Object_HPP_

#include "Class.hpp"

BEGIN_NAMESPACE_SCI

class Object : public Class
{
public:
    uint getId() const;

    virtual StringRef getName() const;

    llvm::GlobalVariable* getGlobal() const { return m_global; }

private:
    void setInitializer() const;
    llvm::ConstantStruct* createInitializer(llvm::StructType *s, const Property *begin, const Property *end, const Property *&ptr) const;

    Object(const ObjRes &res, Script &script);
    ~Object();

    llvm::GlobalVariable *m_global;


    friend class World;
    friend class Script;
};

END_NAMESPACE_SCI

#endif // !_Object_HPP_
