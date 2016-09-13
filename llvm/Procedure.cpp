#include "Procedure.hpp"
#include "Script.hpp"
#include "PMachine.hpp"

using namespace llvm;


BEGIN_NAMESPACE_SCI


Procedure::Procedure(uint offset, Script &script) :
    m_offset(offset),
    m_argCount(0),
    m_func(nullptr),
    m_script(script)
{
    m_flags.value = 0;
    //m_script.getDataAt(offset);
}


Function* Procedure::load()
{
    if (m_func != nullptr)
    {
        return m_func;
    }

    PMachine pmachine(m_script);
    m_func = pmachine.interpretFunction(reinterpret_cast<const uint8_t *>(m_script.getDataAt(m_offset)));
    return m_func;
}


END_NAMESPACE_SCI
