#include "Object.hpp"
#include "Script.hpp"
#include "World.hpp"
#include "Resource.hpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


static ConstantStruct* CreateInitializer(StructType *s, const int16_t *begin, const int16_t *end, const int16_t *&ptr, GlobalVariable *name, GlobalVariable *vftbl)
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
                int idx = ObjRes::VALUES_OFFSET - (begin - ptr);
                switch (idx)
                {
                case -1:
                    c = ConstantExpr::getPtrToInt(vftbl, *i);
                    break;

                case ObjRes::NAME_OFFSET:
                    c = ConstantExpr::getPtrToInt(name, *i);
                    break;

                default:
                    c = ConstantInt::get(*i, static_cast<uint16_t>(*ptr));
                    break;
                }
            }
            else
            {
                c = GetWorld().getConstantValue(*ptr);
            }
            ptr++;
        }
        else
        {
            c = CreateInitializer(cast<StructType>(*i), begin, end, ptr, name, vftbl);
        }
        args.push_back(c);
    }

    return static_cast<ConstantStruct *>(ConstantStruct::get(s, args));
}


static ConstantStruct* CreateInitializer(StructType *s, const int16_t *vals, uint len, GlobalVariable *vftbl, GlobalVariable *name)
{
    // Make one room for the vftbl.
    const int16_t *ptr = vals - 1;
    return CreateInitializer(s, vals + ObjRes::VALUES_OFFSET, vals + len, ptr, name, vftbl);
}


static bool AddElementIndex(SmallVector<Value *, 16> &indices, StructType *s, uint &idx)
{
    size_t pos = indices.size();

    // Make place for the value.
    indices.push_back(nullptr);

    uint n = 0;
    for (auto i = s->element_begin(), e = s->element_end(); i != e; ++i, ++n)
    {
        if ((*i)->isIntegerTy())
        {
            if (idx == 0)
            {
                indices[pos] = ConstantInt::get(Type::getInt32Ty(s->getContext()), n);
                return true;
            }

            idx--;
        }
        else
        {
            if (AddElementIndex(indices, cast<StructType>(*i), idx))
            {
                indices[pos] = ConstantInt::get(Type::getInt32Ty(s->getContext()), n);
                return true;
            }
        }
    }

    // Remove the value.
    indices.pop_back();
    return false;
}


static SmallVector<Value *, 16> CreateElementSelectorIndices(StructType *s, uint idx)
{
    SmallVector<Value *, 16> indices;
    indices.push_back(Constant::getNullValue(GetWorld().getSizeType()));
    if (!AddElementIndex(indices, s, idx))
    {
        indices.pop_back();
    }
    return indices;
}


Object::Object(const ObjRes &res, Script &script) : Class(res, script)
{
    const char *name = m_script.getDataAt(res.nameSel);
    std::string nameOrdinal = "obj@";
    if (name == nullptr || name[0] == '\0')
    {
        nameOrdinal += utostr_32(getId());
        name = nameOrdinal.data();
    }

    m_global = new GlobalVariable(getModule(), m_type, false, GlobalValue::InternalLinkage, nullptr, name);
    m_ctor = createConstructor();
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


Function* Object::createConstructor() const
{
    assert(m_global != nullptr);
    assert(m_vftbl != nullptr);

    World &world = GetWorld();
    LLVMContext &ctx = world.getContext();
    IntegerType *sizeTy = world.getSizeType();
    IntegerType *i16Ty = Type::getInt16Ty(ctx);

    std::string ctorName = "?ctor@";
    ctorName += getName();

    PointerType *clsPtrTy = m_type->getPointerTo();
    FunctionType *ctorFuncTy = FunctionType::get(clsPtrTy, clsPtrTy, false);

    Function *ctorFunc = Function::Create(ctorFuncTy, Function::ExternalLinkage, ctorName, &getModule());
    BasicBlock *bb = BasicBlock::Create(ctx, "", ctorFunc);
    Argument *selfArg = &*ctorFunc->arg_begin();
    selfArg->setName("self");

    SmallVector<Value *, 16> indices;
    GetElementPtrInst *elem;
    Value *val;

    // Add the virtual table.
    indices = CreateElementSelectorIndices(m_type, 0);
    elem = GetElementPtrInst::CreateInBounds(m_type, selfArg, indices, "-vftbl-", bb);
    val = ConstantExpr::getPtrToInt(m_vftbl, sizeTy);
    new StoreInst(val, elem, bb);

    for (uint i = 0, n = m_propCount; i < n; ++i)
    {
        indices = CreateElementSelectorIndices(m_type, i + 1);
        elem = GetElementPtrInst::CreateInBounds(m_type, selfArg, indices, world.getSelectorName(m_propSels[i]), bb);
        if (i < ObjRes::NAME_OFFSET)
        {
            val = ConstantInt::get(i16Ty, static_cast<uint16_t>(m_propVals[i]));
        }
        else if (i == ObjRes::NAME_OFFSET)
        {
            const char *str = m_script.getDataAt(m_propVals[1]);
            if (str == nullptr)
            {
                str = "";
            }
            val = ConstantExpr::getPtrToInt(m_script.getString(str), sizeTy);
        }
        else
        {
            val = world.getConstantValue(m_propVals[i]);
        }
        new StoreInst(val, elem, bb);
    }

    ReturnInst::Create(ctx, selfArg, bb);
    return ctorFunc;
}


void Object::setInitializer() const
{
    ConstantStruct *initStruct = CreateInitializer(m_type,
                                                   m_propVals,
                                                   m_propCount,
                                                   m_vftbl,
                                                   m_script.getString(m_global->getName()));
    m_global->setInitializer(initStruct);
}


END_NAMESPACE_SCI
