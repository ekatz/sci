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
    m_paramCount(0),
    m_func(nullptr),
    m_script(script)
{
    m_flags.value = 0;
}


Function* Procedure::load()
{
    if (m_func == nullptr)
    {
        std::string name = "proc@";
        name += utohexstr(m_offset, true);
        name += '@';
        name += utostr_32(m_script.getId());
        load(name);
    }
    return m_func;
}


Function* Procedure::load(StringRef name)
{
    PMachine pmachine(m_script);
    const uint8_t *code = reinterpret_cast<const uint8_t *>(m_script.getDataAt(m_offset));
    m_func = pmachine.interpretFunction(code, name, selector_cast<uint>(m_selector));

    if (m_func != nullptr)
    {
        m_paramCount = pmachine.getParamCount();
        m_flags.argc = pmachine.hasArgc();
        m_flags.vaList = pmachine.hasVaList();
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


int Procedure::getParamNo(Argument *arg) const
{
    assert(arg->getParent() == m_func && "Parameter does not belong to this procedure!");
    uint argNo = arg->getArgNo();
    if (isMethod())
    {
        if (argNo == 0)
        {
            return -1;
        }
        argNo--;
    }
    if (!hasArgc())
    {
        argNo++;
    }
    return static_cast<int>(argNo);
}


END_NAMESPACE_SCI
