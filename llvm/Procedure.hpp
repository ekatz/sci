#pragma once
#ifndef _Procedure_HPP_
#define _Procedure_HPP_

#include "Types.hpp"
#include <llvm/IR/Function.h>

BEGIN_NAMESPACE_SCI

class Script;

class Procedure
{
public:
    Procedure(uint offset, Script &script);

    llvm::Function* load();

    llvm::Function* getFunction() const { return m_func; }

protected:
    const uint m_offset;
    uint m_argCount;

    union {
        struct {
            uint32_t argc : 1;
            uint32_t rest : 1;
            uint32_t leaParam : 1; // not supported!
            uint32_t indexParam : 1;
        };
        uint32_t value;
    } m_flags;


    llvm::Function *m_func;
    Script &m_script;
};

END_NAMESPACE_SCI

#endif // !_Procedure_HPP_
