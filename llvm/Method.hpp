#pragma once
#ifndef _Method_HPP_
#define _Method_HPP_

#include "Procedure.hpp"

BEGIN_NAMESPACE_SCI

class Class;

class Method : public Procedure
{
public:
    Method(ObjID selector, uint16_t offset, Class &cls);

    llvm::Function* load();

    uint getSelector() const { return selector_cast<uint>(m_selector); }
    Class& getClass() const { return m_class; }

    StringRef getName() const;

private:
    Class &m_class;
};

END_NAMESPACE_SCI

#endif // !_Method_HPP_
