#include "Method.hpp"
#include "World.hpp"
#include "Class.hpp"
#include "PMachine.hpp"

using namespace llvm;


BEGIN_NAMESPACE_SCI


Method::Method(uint selector, uint offset, Class &cls) :
    Procedure(offset, cls.getScript()),
    m_selector(selector),
    m_class(cls)
{
}


Function* Method::load()
{
    if (m_func != nullptr)
    {
        return m_func;
    }

    PMachine pmachine(m_script);
    std::string name = GetWorld().getSelectorName(m_selector);
    name += "@";
    name += m_class.getName();
    m_func = pmachine.interpretFunction(
        reinterpret_cast<const uint8_t *>(m_script.getDataAt(m_offset)),
        name,
        m_selector);
    return m_func;
}


END_NAMESPACE_SCI
