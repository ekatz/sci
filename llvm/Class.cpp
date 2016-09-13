#include "Class.hpp"
#include "Object.hpp"
#include "Method.hpp"
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
    m_methods(nullptr)
{
    assert(res.magic == OBJID);

    World &world = GetWorld();
    IntegerType *sizeTy = world.getSizeType();
    IntegerType *i16Ty = Type::getInt16Ty(world.getContext());

    m_super = world.getClass(cast_selector<uint>(res.superSel));

    m_propVals = res.getPropertyValues();
    m_propSels = res.getPropertySelectors();
    m_propCount = res.getPropertyCount();

    const char *name = script.getDataAt(res.nameSel);
    std::string nameOrdinal = "Class@";
    if (name == nullptr || name[0] == '\0')
    {
        nameOrdinal += utostr_32(res.speciesSel);
        name = nameOrdinal.data();
    }

    std::vector<Type *> args;
    uint i, n = m_propCount + 1;
    if (m_super != nullptr)
    {
        args.push_back(m_super->getType());
        i = CalcNumInheritedElements(m_super->getType());
    }
    else
    {
        // Add the virtual table type.
        args.push_back(sizeTy);
        i = 1;
    }
    for (; i < (ObjRes::NAME_OFFSET + 1); ++i)
    {
        args.push_back(i16Ty);
    }
    for (; i < n; ++i)
    {
        args.push_back(sizeTy);
    }

    m_type = StructType::create(world.getContext(), args, name);

    const uint16_t *methodOffs = res.getMethodOffsets();
    const ObjID *methodSels = res.getMethodSelectors();
    m_methodCount = res.getMethodCount();
    if (m_methodCount > 0)
    {
        size_t size = m_methodCount * sizeof(Method);
        m_methods = reinterpret_cast<Method *>(malloc(size));
        memset(m_methods, 0, size);

        for (i = 0, n = m_methodCount; i < n; ++i)
        {
            new(&m_methods[i]) Method(cast_selector<uint>(methodSels[i]), methodOffs[i], *this);
        }
    }

    m_vftbl = createVirtualTable();
}


Class::~Class()
{
    if (m_methods != nullptr)
    {
        for (uint i = 0, n = m_methodCount; i < n; ++i)
        {
            m_methods[i].~Method();
        }
        free(m_methods);
    }
}


StringRef Class::getName() const
{
    return m_type->getName();
}


ArrayRef<Method> Class::getMethods() const
{
    return makeArrayRef(m_methods, m_methodCount);
}


ArrayRef<int16_t> Class::getProperties() const
{
    return makeArrayRef(m_propVals, m_propCount);
}


Module& Class::getModule() const
{
    return *m_script.getModule();
}


uint Class::countAggregatedMethods() const
{
    const Class *cls = this;
    std::set<uint> all;
    while (cls != nullptr)
    {
        ArrayRef<Method> methods = cls->getMethods();
        for (auto i = methods.begin(), e = methods.end(); i != e; ++i)
        {
            all.insert(i->getSelector());
        }

        cls = cls->getSuper();
    }
    return all.size();
}


GlobalVariable* Class::createVirtualTable() const
{
    GlobalVariable *vftbl;
    if (m_methodCount == 0 && m_super != nullptr)
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
        uint aggMethodCount = countAggregatedMethods();
        vftbl = new GlobalVariable(getModule(),
                                   ArrayType::get(Type::getInt8PtrTy(GetWorld().getContext()), aggMethodCount),
                                   true,
                                   GlobalValue::LinkOnceODRLinkage,
                                   nullptr,
                                   std::string("?vftbl@") + getName());
        vftbl->setUnnamedAddr(true);
    }
    return vftbl;
}


GlobalVariable* Class::getObject()
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


uint Class::aggregateMethods(MutableArrayRef<Method*> methods) const
{
    uint count;
    if (m_super != nullptr)
    {
        count = m_super->aggregateMethods(methods);
    }
    else
    {
        count = 0;
    }

    for (uint i = 0, n = m_methodCount; i < n; ++i)
    {
        uint sel = m_methods[i].getSelector();
        for (uint j = 0; j < count; ++j)
        {
            if (methods[j]->getSelector() == sel)
            {
                methods[j] = &m_methods[i];
                sel = (uint)-1;
                break;
            }
        }

        if (sel != (uint)-1)
        {
            methods[count++] = &m_methods[i];
        }
    }
    return count;
}


bool Class::loadMethods()
{
    ArrayType *arrTy = cast<ArrayType>(m_vftbl->getType()->getElementType());
    Type *elemTy = arrTy->getElementType();
    uint aggMethodCount = arrTy->getNumElements();
    std::unique_ptr<Method*[]> aggMethods(new Method*[aggMethodCount]);
    uint n = aggregateMethods(MutableArrayRef<Method*>(aggMethods.get(), aggMethodCount));
    assert(n == aggMethodCount);

    std::unique_ptr<Constant*[]> vftblInit(new Constant*[n]);

    for (uint i = 0; i < n; ++i)
    {
        Function *func = aggMethods[i]->load();
        if (func == nullptr)
        {
            return false;
        }

        vftblInit[i] = ConstantExpr::getBitCast(func, elemTy);
    }

    Constant *c = ConstantArray::get(arrTy, makeArrayRef(vftblInit.get(), n));
    m_vftbl->setInitializer(c);
    return true;
}


END_NAMESPACE_SCI
