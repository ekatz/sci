#pragma once
#ifndef _Procedure_HPP_
#define _Procedure_HPP_

#include "Types.hpp"
#include <llvm/IR/Function.h>

BEGIN_NAMESPACE_SCI

class Script;
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

    bool hasArgc() const { return m_flags.argc; }
    bool hasVaList() const { return m_flags.vaList; }
    uint getParamCount() const { return m_paramCount; }
    int getParamNo(llvm::Argument *arg) const;

    Script& getScript() const { return m_script; }

protected:
    Procedure(ObjID selector, uint16_t offset, Script &script);
    llvm::Function* load(StringRef name);

    const ObjID m_selector;
    const uint16_t m_offset;
    uint m_paramCount;

    union {
        struct {
            bool argc : 1;
            bool vaList : 1;
            bool indexParam : 1;
        };
        uint32_t value;
    } m_flags;


    llvm::Function *m_func;
    Script &m_script;
};

END_NAMESPACE_SCI

#endif // !_Procedure_HPP_
