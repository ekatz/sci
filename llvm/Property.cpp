#include "Property.hpp"
#include "SelectorTable.hpp"
#include "World.hpp"

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
    return GetWorld().getSelectorTable().getSelectorName(getSelector());
}


END_NAMESPACE_SCI
