#include "Object.hpp"
#include "Script.hpp"
#include "World.hpp"
#include "Property.hpp"
#include "Resource.hpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


Object::Object(const ObjRes &res, Script &script) : Class(res, script), m_global(nullptr)
{
    assert(res.isClass() || getMethodCount() == getSuper()->getMethodCount());

    m_global = new GlobalVariable(getModule(), m_type, false, GlobalValue::InternalLinkage, nullptr, m_type->getName());
    m_global->setAlignment(16);
    setInitializer();
}


Object::~Object()
{
}


uint Object::getId() const
{
    return m_script.getObjectId(*this);
}


StringRef Object::getName() const
{
    return m_global->getName();
}


void Object::setInitializer() const
{
    // Make one room for the species.
    const Property *ptr = m_props - 1;
    ConstantStruct *initStruct = createInitializer(m_type,
                                                   m_props + (ObjRes::VALUES_OFFSET - ObjRes::INFO_OFFSET),
                                                   m_props + m_propCount,
                                                   ptr);

    m_global->setInitializer(initStruct);
}


ConstantStruct* Object::createInitializer(StructType *s, const Property *begin, const Property *end, const Property *&ptr) const
{
    std::vector<Constant *> args;

    for (auto i = s->element_begin(), e = s->element_end(); i != e; ++i)
    {
        Constant *c;
        if ((*i)->isIntegerTy())
        {
            assert(ptr < end);
            if (ptr < begin)
            {
                int idx = (ObjRes::VALUES_OFFSET - ObjRes::INFO_OFFSET) - (begin - ptr);
                switch (idx)
                {
                case -1:
                    c = ConstantExpr::getPtrToInt(m_species, *i);
                    break;

                case (ObjRes::NAME_OFFSET - ObjRes::INFO_OFFSET): {
                    const char *str = m_script.getDataAt(static_cast<uint16_t>(ptr->getDefaultValue()));
                    c = ConstantExpr::getPtrToInt(m_script.getString(str), *i);
                    break;
                }

                default:
                    c = ConstantInt::get(*i, static_cast<uint16_t>(ptr->getDefaultValue()));
                    break;
                }
            }
            else
            {
                int16_t val = ptr->getDefaultValue();
                c = m_script.getLocalString(static_cast<uint16_t>(val));
                if (c != nullptr)
                {
                    c = ConstantExpr::getPtrToInt(c, *i);
                }
                else
                {
                    c = GetWorld().getConstantValue(val);
                }
            }
            ptr++;
        }
        else
        {
            c = createInitializer(cast<StructType>(*i), begin, end, ptr);
        }
        args.push_back(c);
    }

    return static_cast<ConstantStruct *>(ConstantStruct::get(s, args));
}


END_NAMESPACE_SCI
