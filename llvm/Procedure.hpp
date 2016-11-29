#pragma once
#ifndef _Procedure_HPP_
#define _Procedure_HPP_

#include "Types.hpp"
#include <llvm/IR/Function.h>

BEGIN_NAMESPACE_SCI

class Script;
class Class;
class Method;

class Procedure
{
public:
    Procedure(uint16_t offset, Script &script);

    llvm::Function* load();

    llvm::Function* getFunction() const { return m_func; }
    uint getOffset() const { return m_offset; }

    bool isMethod() const;
    const Method* asMethod() const;
    Method* asMethod();

    Script& getScript() const { return m_script; }

protected:
    Procedure(ObjID selector, uint16_t offset, Script &script);
    llvm::Function* load(StringRef name, Class *cls = nullptr);

    const ObjID m_selector;
    const uint16_t m_offset;

    llvm::Function *m_func;
    Script &m_script;
};

END_NAMESPACE_SCI

#endif // !_Procedure_HPP_
