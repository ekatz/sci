#include "World.hpp"
#include "Script.hpp"
#include "Object.hpp"
#include "Procedure.hpp"
#include "Resource.hpp"
#include "Passes/StackReconstructionPass.hpp"
#include "Passes/SplitSendPass.hpp"
#include "Passes/MutateCallIntrinsicsPass.hpp"
#include "Passes/TranslateClassIntrinsicPass.hpp"
#include "Passes/ExpandScriptIDPass.hpp"
#include "Passes/FixCodePass.hpp"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


static World s_world;


static void DumpScriptModule(uint id)
{
    std::error_code ec;
    Module *m = GetWorld().getScript(id)->getModule();
    m->print(raw_fd_ostream(("C:\\Temp\\SCI\\" + m->getName()).str() + ".ll", ec, sys::fs::F_Text), nullptr);
}


World& GetWorld()
{
    return s_world;
}


World::World() :
    m_dataLayout(""),
    m_sizeTy(nullptr),
    m_scriptCount(0),
    m_classes(nullptr),
    m_classCount(0)
{
    setDataLayout(m_dataLayout);
}


World::~World()
{
    if (m_classes != nullptr)
    {
        for (uint i = 0, n = m_classCount; i < n; ++i)
        {
            if (m_classes[i].m_type != nullptr)
            {
                m_classes[i].~Object();
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
        c = ConstantInt::get(getSizeType(), static_cast<uint64_t>(static_cast<uint16_t>(val)), false);
    }
    else
    {
        c = ConstantInt::get(getSizeType(), static_cast<uint64_t>(static_cast<int16_t>(val)), true);
    }
    return c;
}


void World::setDataLayout(const llvm::DataLayout &dl)
{
    m_dataLayout = dl;
    m_sizeTy = Type::getIntNTy(m_ctx, m_dataLayout.getPointerSizeInBits());
    Type *elems[] = {
        m_sizeTy,                   // species
        ArrayType::get(m_sizeTy, 0) // vars
    };
    m_absClassTy = StructType::create(m_ctx, elems);
}


bool World::load()
{
    ResClassEntry *res = (ResClassEntry *)ResLoad(RES_VOCAB, CLASSTBL_VOCAB);
    if (res == nullptr)
    {
        return false;
    }

    m_intrinsics.reset(new Intrinsic());
    m_classCount = ResHandleSize(res) / sizeof(ResClassEntry);
    size_t size = m_classCount * sizeof(Object);
    m_classes = reinterpret_cast<Object *>(malloc(size));
    memset(m_classes, 0, size);
    for (uint i = 0, n = m_classCount; i < n; ++i)
    {
        Script **classScript = reinterpret_cast<Script **>(&m_classes[i].m_super + 1);
        *classScript = acquireScript(res[i].scriptNum);
    }

    ResUnLoad(RES_VOCAB, CLASSTBL_VOCAB);

    for (uint i = 0; i < 1000; ++i)
    {
        Script *script = acquireScript(i);
        if (script != nullptr)
        {
            script->load();
        }
    }


    DumpScriptModule(764);
  //  DumpScriptModule(999);
    StackReconstructionPass().run();
    printf("Finished stack reconstruction!\n");

    SplitSendPass().run();
    printf("Finished splitting Send Message calls!\n");

    FixCodePass().run();
    printf("Finished fixing code issues!\n");

    TranslateClassIntrinsicPass().run();
    printf("Finished translating class intrinsic!\n");

    ExpandScriptIDPass().run();
    printf("Finished expanding KScriptID calls!\n");

    MutateCallIntrinsicsPass().run();
    printf("Finished mutating Call intrinsics!\n");

    for (Script &script : scripts())
    {
        DumpScriptModule(script.getId());
    }
//     DumpScriptModule(999);
//     DumpScriptModule(997);
//     DumpScriptModule(255);
//     DumpScriptModule(0);
    return true;
}


Script* World::acquireScript(uint id)
{
    Script *script = m_scripts[id].get();
    if (script == nullptr)
    {
        Handle hunk = ResLoad(RES_SCRIPT, id);
        if (hunk != nullptr)
        {
            printf("%d\n", ResHandleSize(hunk));
            script = new Script(id, hunk);
            m_scripts[id].reset(script);
            m_scriptCount++;
        }
    }
    return script;
}


Script* World::getScript(uint id) const
{
    return m_scripts[id].get();
}


Script* World::getScript(Module &module) const
{
    Script *script = nullptr;
    StringRef name = module.getName();
    if (name.size() == 9 && name.startswith("Script"))
    {
        if (isdigit(name[8]) && isdigit(name[7]) && isdigit(name[6]))
        {
            uint id = (name[6] - '0') * 100 +
                      (name[7] - '0') * 10 +
                      (name[8] - '0') * 1;

            script = m_scripts[id].get();
            assert(script == nullptr || script->getModule() == &module);
        }
    }
    return script;
}


Object* World::lookupObject(GlobalVariable &var) const
{
    Script *script = getScript(*var.getParent());
    return (script != nullptr) ? script->lookupObject(var) : nullptr;
}


StringRef World::getSelectorName(uint id)
{
    return m_sels.getSelectorName(id);
}


Object* World::getClass(uint id)
{
    if (id >= m_classCount)
    {
        return nullptr;
    }

    Object *cls = &m_classes[id];
    if (cls->m_type == nullptr)
    {
        if (!cls->m_script.load() || cls->m_type == nullptr)
        {
            cls = nullptr;
        }
    }
    return cls;
}


Object* World::addClass(const ObjRes &res, Script &script)
{
    assert(selector_cast<uint>(res.speciesSel) < m_classCount);
    assert(&script == &m_classes[res.speciesSel].m_script);

    Object *cls = &m_classes[res.speciesSel];
    if (cls->m_type == nullptr)
    {
        new(cls) Object(res, script);
    }
    return cls;
}


bool World::registerProcedure(Procedure &proc)
{
    bool ret = false;
    Function *func = proc.getFunction();
    if (func != nullptr)
    {
        Procedure *&slot = m_funcMap[func];
        ret = (slot == nullptr);
        slot = &proc;
    }
    return ret;
}


Procedure* World::getProcedure(const Function &func) const
{
    return m_funcMap.lookup(&func);
}


uint World::getGlobalVariablesCount() const
{
    return m_scripts[0]->getLocalVariablesCount();
}


END_NAMESPACE_SCI
