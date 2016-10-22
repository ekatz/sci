#include "Class.hpp"
#include "Object.hpp"
#include "Method.hpp"
#include "Property.hpp"
#include "World.hpp"
#include "Script.hpp"
#include "Resource.hpp"
#include <set>

using namespace llvm;


BEGIN_NAMESPACE_SCI


static uint CalcNumInheritedElements(StructType *s)
{
    uint num = 0;
    for (auto i = s->element_begin(), e = s->element_end(); i != e; ++i)
    {
        if ((*i)->isIntegerTy())
        {
            num++;
        }
        else
        {
            num += CalcNumInheritedElements(cast<StructType>(*i));
        }
    }
    return num;
}


StructType* Class::GetAbstractType()
{
    return GetWorld().getAbstractClassType();
}


Class* Class::Get(uint id)
{
    return GetWorld().getClass(id);
}


Class::Class(const ObjRes &res, Script &script) :
    m_script(script),
    m_vftbl(nullptr),
    m_global(nullptr),
    m_ctor(nullptr),
    m_props(nullptr),
    m_overloadMethods(nullptr)
{
    assert(res.magic == OBJID);

    World &world = GetWorld();
    SelectorTable &sels = world.getSelectorTable();
    IntegerType *sizeTy = world.getSizeType();
    IntegerType *i16Ty = Type::getInt16Ty(world.getContext());
    uint i, n;

    m_id = res.speciesSel;
    m_super = world.getClass(selector_cast<uint>(res.superSel));
    m_depth = (m_super != nullptr) ? m_super->m_depth + 1 : 0;

    const int16_t *propVals = res.getPropertyValues();
    m_propCount = res.getPropertyCount();
    if (m_propCount > 0)
    {
        size_t size = m_propCount * sizeof(Property);
        m_props = reinterpret_cast<Property *>(malloc(size));
        memset(m_props, 0, size);

        if (res.isClass())
        {
            const ObjID *propSels = res.getPropertySelectors();
            for (i = 0, n = m_propCount; i < n; ++i)
            {
                new(&m_props[i]) Property(propSels[i], propVals[i], *this);
                sels.addProperty(m_props[i], i);
            }

            if (m_super != nullptr)
            {
                ArrayRef<Property> superProps = m_super->getProperties();
                assert(superProps.size() <= m_propCount);
                for (i = 0, n = superProps.size(); i < n; ++i)
                {
                    assert(m_props[i].getSelector() == superProps[i].getSelector());
                }
            }
        }
        else
        {
            assert(m_super != nullptr && m_super->getPropertyCount() <= m_propCount);
            ArrayRef<Property> superProps = m_super->getProperties();
            for (i = 0, n = superProps.size(); i < n; ++i)
            {
                new(&m_props[i]) Property(static_cast<ObjID>(superProps[i].getSelector()), propVals[i], *this);
                sels.addProperty(m_props[i], i);
            }
        }
    }

    std::vector<Type *> args;
    n = m_propCount;
    if (m_super != nullptr)
    {
        args.push_back(m_super->getType());
      //  i = CalcNumInheritedElements(m_super->getType());
        i = m_super->getPropertyCount();
        assert(i != 0);
        assert((i + 1) == CalcNumInheritedElements(m_super->getType()));
      //  // Do not count the the virtual table.
      //  i--;
    }
    else
    {
        // Add the virtual table type.
        args.push_back(sizeTy);
        i = 0;
    }
    for (; i < ObjRes::NAME_OFFSET; ++i)
    {
        args.push_back(i16Ty);
    }
    for (; i < n; ++i)
    {
        args.push_back(sizeTy);
    }

    const char *name = script.getDataAt(res.nameSel);
    std::string nameOrdinal = "Class@";
    if (name == nullptr || name[0] == '\0')
    {
        nameOrdinal += utostr_32(m_id);
        name = nameOrdinal.data();
    }

    m_type = StructType::create(world.getContext(), args, name);

    const uint16_t *methodOffs = res.getMethodOffsets();
    const ObjID *methodSels = res.getMethodSelectors();
    m_overloadMethodCount = res.getMethodCount();
    if (m_overloadMethodCount > 0)
    {
        size_t size = m_overloadMethodCount * sizeof(Method);
        m_overloadMethods = reinterpret_cast<Method *>(malloc(size));
        memset(m_overloadMethods, 0, size);

        for (i = 0, n = m_overloadMethodCount; i < n; ++i)
        {
            new(&m_overloadMethods[i]) Method(methodSels[i], methodOffs[i], *this);
        }
    }

    createMethods();
    m_vftbl = createVirtualTable();
}


Class::~Class()
{
    if (m_overloadMethods != nullptr)
    {
        for (uint i = 0, n = m_overloadMethodCount; i < n; ++i)
        {
            m_overloadMethods[i].~Method();
        }
        free(m_overloadMethods);
    }

    if (m_props != nullptr)
    {
        for (uint i = 0, n = m_propCount; i < n; ++i)
        {
            m_props[i].~Property();
        }
        free(m_props);
    }
}


StringRef Class::getName() const
{
    return m_type->getName();
}


ArrayRef<Method> Class::getOverloadedMethods() const
{
    return makeArrayRef(m_overloadMethods, m_overloadMethodCount);
}


ArrayRef<Method*> Class::getMethods() const
{
    return makeArrayRef(m_methods.get(), m_methodCount);
}


Method* Class::getMethod(uint id) const
{
    int index;
    return getMethod(id, index);
}


Method* Class::getMethod(uint id, int &index) const
{
    index = findMethod(id);
    return (index >= 0) ? m_methods[index] : nullptr;
}


Property* Class::getProperty(uint id) const
{
    int index;
    return getProperty(id, index);
}


Property* Class::getProperty(uint id, int &index) const
{
    index = findProperty(id);
    return (index >= 0) ? &m_props[index] : nullptr;
}


ArrayRef<Property> Class::getProperties() const
{
    return makeArrayRef(m_props, m_propCount);
}


Module& Class::getModule() const
{
    return *m_script.getModule();
}


GlobalVariable* Class::createVirtualTable() const
{
    GlobalVariable *vftbl;
    if (m_overloadMethodCount == 0 && m_super != nullptr)
    {
        GlobalVariable *superVftbl = m_super->m_vftbl;
        if (&m_super->m_script == &m_script)
        {
            vftbl = superVftbl;
        }
        else
        {
            vftbl = getModule().getGlobalVariable(superVftbl->getName());
            if (vftbl == nullptr)
            {
                vftbl = new GlobalVariable(getModule(),
                                           superVftbl->getType()->getElementType(),
                                           superVftbl->isConstant(),
                                           GlobalValue::ExternalLinkage,
                                           nullptr,
                                           superVftbl->getName(),
                                           nullptr,
                                           superVftbl->getThreadLocalMode(),
                                           superVftbl->getType()->getAddressSpace());
                vftbl->copyAttributesFrom(superVftbl);
            }
        }
    }
    else
    {
        vftbl = new GlobalVariable(getModule(),
                                   ArrayType::get(Type::getInt8PtrTy(GetWorld().getContext()), m_methodCount),
                                   true,
                                   GlobalValue::LinkOnceODRLinkage,
                                   nullptr,
                                   std::string("?vftbl@") + getName());
        vftbl->setUnnamedAddr(true);
    }
    return vftbl;
}


GlobalVariable* Class::loadObject()
{
    if (m_global == nullptr)
    {
        Object *obj = static_cast<Object *>(this);
        m_global = new GlobalVariable(getModule(), m_type, false, GlobalValue::InternalLinkage, nullptr, getName());
        m_ctor = obj->createConstructor();
        obj->setInitializer();
    }
    return m_global;
}


int Class::findProperty(uint id) const
{
    for (uint i = 0, n = m_propCount; i < n; ++i)
    {
        if (m_props[i].getSelector() == id)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}


int Class::findMethod(uint id) const
{
    for (uint i = 0, n = m_methodCount; i < n; ++i)
    {
        if (m_methods[i]->getSelector() == id)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}


uint Class::countMethods() const
{
    uint count;
    if (m_super != nullptr && m_super->getMethodCount() > 0)
    {
        count = m_super->getMethodCount();
        for (uint i = 0, n = m_overloadMethodCount; i < n; ++i)
        {
            if (m_super->findMethod(m_overloadMethods[i].getSelector()) < 0)
            {
                count++;
            }
        }
    }
    else
    {
        count = m_overloadMethodCount;
    }
    return count;
}


void Class::createMethods()
{
    m_methodCount = countMethods();
    if (m_methodCount > 0)
    {
        m_methods.reset(new Method*[m_methodCount]);

        if (m_super != nullptr && m_super->getMethodCount() > 0)
        {
            uint count = m_super->getMethodCount();
            memcpy(m_methods.get(), m_super->m_methods.get(), count * sizeof(Method*));

            for (uint i = 0, n = m_overloadMethodCount; i < n; ++i)
            {
                Method *method = &m_overloadMethods[i];
                int pos = m_super->findMethod(method->getSelector());
                if (pos >= 0)
                {
                    m_methods[pos] = method;
                }
                else
                {
                    m_methods[count++] = method;
                }
            }
        }
        else
        {
            for (uint i = 0, n = m_overloadMethodCount; i < n; ++i)
            {
                m_methods[i] = &m_overloadMethods[i];
            }
        }
    }
}


bool Class::loadMethods()
{
    if (m_vftbl->hasInitializer())
    {
        return true;
    }

    SelectorTable &sels = SelectorTable::Get();
    ArrayType *arrTy = cast<ArrayType>(m_vftbl->getType()->getElementType());
    Type *elemTy = arrTy->getElementType();

    std::unique_ptr<Constant*[]> vftblInit(new Constant*[m_methodCount]);

    for (uint i = 0, n = m_methodCount; i < n; ++i)
    {
        Method *method = m_methods[i];
        sels.addMethod(*method, i);

        Function *func = method->load();
        if (func == nullptr)
        {
            return false;
        }

        if (method->getScript().getId() != getScript().getId())
        {
            func = cast<Function>(getModule().getOrInsertFunction(func->getName(), func->getFunctionType()));
        }

        vftblInit[i] = ConstantExpr::getBitCast(func, elemTy);
    }

    Constant *c = ConstantArray::get(arrTy, makeArrayRef(vftblInit.get(), m_methodCount));
    m_vftbl->setInitializer(c);
    return true;
}


bool Class::isObject() const
{
    return (GetWorld().getClass(m_id) != this);
}


END_NAMESPACE_SCI
