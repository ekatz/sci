#include "Method.hpp"
#include "World.hpp"
#include "Class.hpp"
#include "SelectorTable.hpp"
#include "PMachine.hpp"

using namespace llvm;


BEGIN_NAMESPACE_SCI


Method::Method(ObjID selector, uint16_t offset, Class &cls) :
    Procedure(selector, offset, cls.getScript()),
    m_class(cls)
{
}


StringRef Method::getName() const
{
    return GetWorld().getSelectorTable().getSelectorName(getSelector());
}


Function* Method::load()
{
    if (m_func == nullptr)
    {
        std::string name = getName();
        name += '@';
        name += m_class.getName();
        Procedure::load(name);
    }
    return m_func;
}


END_NAMESPACE_SCI
