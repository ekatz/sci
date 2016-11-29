#include "Procedure.hpp"
#include "Method.hpp"
#include "Script.hpp"
#include "PMachine.hpp"

using namespace llvm;


BEGIN_NAMESPACE_SCI


Procedure::Procedure(uint16_t offset, Script &script) :
    Procedure((ObjID)-1, offset, script)
{
}


Procedure::Procedure(ObjID selector, uint16_t offset, Script &script) :
    m_selector(selector),
    m_offset(offset),
    m_func(nullptr),
    m_script(script)
{
}


Function* Procedure::load()
{
    if (m_func == nullptr)
    {
        std::string name = "proc@";
        name += utohexstr(m_offset, true);
        name += '@';
        name += utostr(m_script.getId());
        load(name);
    }
    return m_func;
}


Function* Procedure::load(StringRef name, Class *cls)
{
    PMachine pmachine(m_script);
    const uint8_t *code = reinterpret_cast<const uint8_t *>(m_script.getDataAt(m_offset));
    m_func = pmachine.interpretFunction(code, name, selector_cast<uint>(m_selector), cls);

    if (m_func != nullptr)
    {
        GetWorld().registerProcedure(*this);
    }
    return m_func;
}


bool Procedure::isMethod() const
{
    return (m_selector != (uint)-1);
}


const Method* Procedure::asMethod() const
{
    return isMethod() ? static_cast<const Method *>(this) : nullptr;
}


Method* Procedure::asMethod()
{
    return isMethod() ? static_cast<Method *>(this) : nullptr;
}


END_NAMESPACE_SCI
