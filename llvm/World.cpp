#include "World.hpp"
#include "Script.hpp"
#include "Class.hpp"
#include "Resource.hpp"
#include "Passes/StackReconstructionPass.hpp"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


static World s_world;


World& GetWorld()
{
    return s_world;
}


World::World() :
    m_dataLayout(""),
    m_ctx(getGlobalContext()),
    m_classes(nullptr),
    m_classCount(0),
    m_sels(500)
{
    m_sizeTy = Type::getInt32Ty(m_ctx);

    Type *i16Ty = Type::getInt16Ty(m_ctx);
    Type *elems[] = {
        m_sizeTy,                   // vftbl
        i16Ty,                      // species
        i16Ty,                      // super
        i16Ty,                      // info
        m_sizeTy,                   // name
        ArrayType::get(m_sizeTy, 0) // vars
    };
    m_absClassTy = StructType::create(m_ctx, elems);

    createStubFunctions();
}


World::~World()
{
    for (uint i = 0, n = ARRAYSIZE(m_stubs.funcs); i < n; ++i)
    {
        assert(!m_stubs.funcs[i] || m_stubs.funcs[i]->getNumUses() == 0);
    }

    if (m_classes != nullptr)
    {
        for (uint i = 0, n = m_classCount; i < n; ++i)
        {
            if (m_classes[i].m_type != nullptr)
            {
                m_classes[i].~Class();
            }
        }
        free(m_classes);
    }
}


ConstantInt* World::getConstantValue(int16_t val) const
{
    ConstantInt *c;
    if (IsUnsignedValue(val))
    {
        c = ConstantInt::get(getSizeType(), static_cast<uint64_t>(static_cast<uint16_t>(val)));
    }
    else
    {
        c = ConstantInt::get(getSizeType(), static_cast<uint64_t>(static_cast<int16_t>(val)));
    }
    return c;
}


void World::setDataLayout(const llvm::DataLayout &dl)
{
    m_dataLayout = dl;
}


bool World::load()
{
    ResClassEntry *res = (ResClassEntry *)ResLoad(RES_VOCAB, CLASSTBL_VOCAB);
    if (res == nullptr)
    {
        return false;
    }

    m_classCount = ResHandleSize(res) / sizeof(ResClassEntry);
    size_t size = m_classCount * sizeof(Class);
    m_classes = reinterpret_cast<Class *>(malloc(size));
    memset(m_classes, 0, size);
    for (uint i = 0, n = m_classCount; i < n; ++i)
    {
        Script **classScript = reinterpret_cast<Script **>(&m_classes[i].m_super + 1);
        *classScript = getScript(res[i].scriptNum);
    }

    ResUnLoad(RES_VOCAB, CLASSTBL_VOCAB);

    for (uint i = 0; i < 1000; ++i)
    {
        Script *script = getScript(i);
        if (script != nullptr)
        {
            script->load();
        }
    }

    for (uint i = 0, n = ARRAYSIZE(m_stubs.funcs); i < n; ++i)
    {
        assert(!m_stubs.funcs[i] || m_stubs.funcs[i]->getNumUses() == 0);
    }

    StackReconstructionPass().run();
    return true;
}


Script* World::getScript(uint id)
{
    Script *script = m_scripts[id].get();
    if (script == nullptr)
    {
        Handle hunk = ResLoad(RES_SCRIPT, id);
        if (hunk != nullptr)
        {
            script = new Script(id, hunk);
            m_scripts[id].reset(script);
        }
    }
    return script;
}


StringRef World::getSelectorName(uint id)
{
    const char *data = getSelectorData(id);
    if (data != nullptr)
    {
        data += sizeof(void*);
        return StringRef(data + sizeof(uint8_t), *reinterpret_cast<const uint8_t *>(data));
    }
    return StringRef();
}


FunctionType*& World::getMethodSignature(uint id)
{
    return *reinterpret_cast<FunctionType **>(getSelectorData(id));
}


char* World::getSelectorData(uint id)
{
    if (m_sels.size() <= id)
    {
        m_sels.resize(id + 1);
    }

    if (!m_sels[id])
    {
        char selName[32];
        if (GetFarStr(SELECTOR_VOCAB, id, selName) == nullptr)
        {
            sprintf(selName, "sel@%u", id);
        }
        size_t nameLen = strlen(selName);

        char *buf = new char[sizeof(void*) + sizeof(uint8_t) + (nameLen + 1)];
        m_sels[id].reset(buf);

        *reinterpret_cast<void **>(buf) = nullptr;
        buf += sizeof(void*);

        *reinterpret_cast<uint8_t *>(buf) = static_cast<uint8_t>(nameLen);
        buf += sizeof(uint8_t);

        memcpy(buf, selName, nameLen);
        buf[nameLen] = '\0';
    }
    return m_sels[id].get();
}


Class* World::getClass(uint id)
{
    if (id >= m_classCount)
    {
        return nullptr;
    }

    Class *cls = &m_classes[id];
    if (cls->m_type == nullptr)
    {
        if (!cls->m_script.load() || cls->m_type == nullptr)
        {
            cls = nullptr;
        }
    }
    return cls;
}


Class* World::addClass(const ObjRes &res, Script &script)
{
    assert(cast_selector<uint>(res.speciesSel) < m_classCount);
    assert(&script == &m_classes[res.speciesSel].m_script);

    Class *cls = &m_classes[res.speciesSel];
    if (cls->m_type == nullptr)
    {
        new(cls) Class(res, script);
    }
    return cls;
}


uint World::getGlobalVariablesCount() const
{
    return m_scripts[0]->getLocalVariablesCount();
}


void World::createStubFunctions()
{
    Type *params[] = { m_sizeTy, m_sizeTy, m_sizeTy };
    FunctionType *funcTy;

    funcTy = FunctionType::get(Type::getVoidTy(m_ctx), m_sizeTy, false);
    m_stubs.funcs[Stub::push].reset(Function::Create(funcTy, Function::ExternalLinkage, "push@SCI"));
    m_stubs.funcs[Stub::rest].reset(Function::Create(funcTy, Function::ExternalLinkage, "rest@SCI"));

    funcTy = FunctionType::get(m_sizeTy, false);
    m_stubs.funcs[Stub::pop].reset(Function::Create(funcTy, Function::ExternalLinkage, "pop@SCI"));

    funcTy = FunctionType::get(m_sizeTy, makeArrayRef(params, 2), false);
    m_stubs.funcs[Stub::super].reset(Function::Create(funcTy, Function::ExternalLinkage, "super@SCI"));
    m_stubs.funcs[Stub::send].reset(Function::Create(funcTy, Function::ExternalLinkage, "send@SCI"));
    m_stubs.funcs[Stub::call].reset(Function::Create(funcTy, Function::ExternalLinkage, "call@SCI"));
    m_stubs.funcs[Stub::callb].reset(Function::Create(funcTy, Function::ExternalLinkage, "callb@SCI"));
    m_stubs.funcs[Stub::callk].reset(Function::Create(funcTy, Function::ExternalLinkage, "callk@SCI"));

    funcTy = FunctionType::get(m_sizeTy, makeArrayRef(params, 3), false);
    m_stubs.funcs[Stub::calle].reset(Function::Create(funcTy, Function::ExternalLinkage, "calle@SCI"));
}


END_NAMESPACE_SCI
