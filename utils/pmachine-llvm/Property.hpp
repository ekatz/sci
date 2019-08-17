#pragma once
#ifndef _Property_HPP_
#define _Property_HPP_

#include "Types.hpp"

BEGIN_NAMESPACE_SCI

class Class;

class Property
{
public:
    Property(ObjID selector, int16_t value, Class &cls);

    int16_t getDefaultValue() const { return m_defaultValue; }
    uint getSelector() const { return selector_cast<uint>(m_selector); }
    Class& getClass() const { return m_class; }

    StringRef getName() const;

private:
    const ObjID m_selector;
    const int16_t m_defaultValue;
    Class &m_class;
};

END_NAMESPACE_SCI

#endif // !_Property_HPP_
