#pragma once
#ifndef _Method_HPP_
#define _Method_HPP_

#include "Procedure.hpp"

BEGIN_NAMESPACE_SCI

class Class;

class Method : public Procedure
{
public:
    Method(uint selector, uint offset, Class &cls);

    llvm::Function* load();

    uint getSelector() const { return m_selector; }

private:
    const uint m_selector;
    Class &m_class;
};

END_NAMESPACE_SCI

#endif // !_Method_HPP_
