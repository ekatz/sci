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

private:
    llvm::Function* createConstructor() const;
    void setInitializer() const;
    Object(const ObjRes &res, Script &script);
    ~Object();

    using Class::loadObject;


    friend class Class;
    friend class Script;
};

END_NAMESPACE_SCI

#endif // !_Object_HPP_
