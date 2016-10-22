#include "Property.hpp"
#include "SelectorTable.hpp"

using namespace llvm;


BEGIN_NAMESPACE_SCI


Property::Property(ObjID selector, int16_t value, Class &cls) :
    m_selector(selector),
    m_defaultValue(value),
    m_class(cls)
{
}


StringRef Property::getName() const
{
    return SelectorTable::Get().getSelectorName(getSelector());
}


END_NAMESPACE_SCI
