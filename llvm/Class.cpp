#include "Class.hpp"
#include "Object.hpp"
#include "Method.hpp"
#include "Property.hpp"
#include "World.hpp"
#include "Script.hpp"
#include "Resource.hpp"
#include "Decl.hpp"

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
    m_species(nullptr),
    m_methodOffs(nullptr),
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
        i = m_super->getPropertyCount();
        assert(i != 0);
        assert((i + 1) == CalcNumInheritedElements(m_super->getType()));
    }
    else
    {
        // Add the species pointer.
        args.push_back(sizeTy);
        i = 0;
    }
    for (; i < n; ++i)
    {
        args.push_back(sizeTy);
    }

    StringRef name;
    std::string nameStr;
    const char *nameSel = m_script.getDataAt(res.nameSel);
    if (nameSel != nullptr && nameSel[0] != '\0')
    {
        nameStr = nameSel;
    }
    else
    {
        nameStr = res.isClass() ? "Class" : "obj";
        nameStr += '@' + utostr(getId());
    }
    nameStr += '@' + utostr(m_script.getId());
    name = nameStr;

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
    createSpecies();
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


void Class::createSpecies()
{
    World &world = GetWorld();
    LLVMContext &ctx = world.getContext();
    Module &module = getModule();
    PointerType *int8PtrTy = Type::getInt8PtrTy(ctx);
    IntegerType *int16Ty = Type::getInt16Ty(ctx);

    m_methodOffs = new GlobalVariable(module,
                                      ArrayType::get(int8PtrTy, m_methodCount),
                                      true,
                                      GlobalValue::LinkOnceODRLinkage,
                                      nullptr,
                                      std::string("?methodOffs@") + getName());
    m_methodOffs->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    m_methodOffs->setAlignment(world.getTypeAlignment(int8PtrTy));

    GlobalVariable *methodSels;
    methodSels = new GlobalVariable(module,
                                    ArrayType::get(int16Ty, m_methodCount + 1),
                                    true,
                                    GlobalValue::LinkOnceODRLinkage,
                                    nullptr,
                                    std::string("?methodSels@") + getName());
    methodSels->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    methodSels->setAlignment(world.getTypeAlignment(int16Ty));

    GlobalVariable *propSels;
    propSels = new GlobalVariable(module,
                                  ArrayType::get(int16Ty, m_propCount + 1),
                                  true,
                                  GlobalValue::LinkOnceODRLinkage,
                                  nullptr,
                                  std::string("?propSels@") + getName());
    propSels->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    propSels->setAlignment(world.getTypeAlignment(int16Ty));

    m_species = new GlobalVariable(module,
                                   ArrayType::get(int8PtrTy, 4),
                                   true,
                                   GlobalValue::LinkOnceODRLinkage,
                                   nullptr,
                                   std::string("?species@") + getName());
    m_species->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    m_species->setAlignment(world.getTypeAlignment(int8PtrTy));


    std::unique_ptr<Constant*[]> initVals(new Constant*[std::max({ m_methodCount + 1, m_propCount + 1, 4U })]);
    Constant *c;
    ArrayType *arrTy;
    Type *elemTy;


    arrTy = cast<ArrayType>(methodSels->getType()->getElementType());
    initVals[0] = ConstantInt::get(int16Ty, m_methodCount);

    for (uint i = 0, n = m_methodCount; i < n; ++i)
    {
        initVals[i + 1] = ConstantInt::get(int16Ty, m_methods[i]->getSelector());
    }

    c = ConstantArray::get(arrTy, makeArrayRef(initVals.get(), m_methodCount + 1));
    methodSels->setInitializer(c);


    arrTy = cast<ArrayType>(propSels->getType()->getElementType());
    initVals[0] = ConstantInt::get(int16Ty, m_propCount);

    for (uint i = 0, n = m_propCount; i < n; ++i)
    {
        initVals[i + 1] = ConstantInt::get(int16Ty, m_props[i].getSelector());
    }

    c = ConstantArray::get(arrTy, makeArrayRef(initVals.get(), m_propCount + 1));
    propSels->setInitializer(c);


    arrTy = cast<ArrayType>(m_species->getType()->getElementType());
    elemTy = arrTy->getElementType();

    if (m_super != nullptr)
    {
        GlobalVariable *superSpecies = GetGlobalVariableDecl(m_super->getSpecies(), &module);
        initVals[0] = ConstantExpr::getBitCast(superSpecies, elemTy);
    }
    else
    {
        initVals[0] = Constant::getNullValue(elemTy);
    }
    initVals[1] = ConstantExpr::getBitCast(propSels, elemTy);
    initVals[2] = ConstantExpr::getBitCast(methodSels, elemTy);
    initVals[3] = ConstantExpr::getBitCast(m_methodOffs, elemTy);

    c = ConstantArray::get(arrTy, makeArrayRef(initVals.get(), 4));
    m_species->setInitializer(c);
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
    if (m_methodOffs->hasInitializer())
    {
        return true;
    }

    SelectorTable &sels = GetWorld().getSelectorTable();
    ArrayType *arrTy = cast<ArrayType>(m_methodOffs->getType()->getElementType());
    Type *elemTy = arrTy->getElementType();

    std::unique_ptr<Constant*[]> offsInit(new Constant*[m_methodCount]);

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

        offsInit[i] = ConstantExpr::getBitCast(func, elemTy);
    }

    Constant *c = ConstantArray::get(arrTy, makeArrayRef(offsInit.get(), m_methodCount));
    m_methodOffs->setInitializer(c);
    return true;
}


bool Class::isObject() const
{
    return (GetWorld().getClass(m_id) != this);
}


END_NAMESPACE_SCI
