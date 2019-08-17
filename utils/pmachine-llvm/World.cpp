#include "World.hpp"
#include "Script.hpp"
#include "Object.hpp"
#include "Procedure.hpp"
#include "Resource.hpp"
#include "Passes/StackReconstructionPass.hpp"
#include "Passes/SplitSendPass.hpp"
#include "Passes/MutateCallIntrinsicsPass.hpp"
#include "Passes/TranslateClassIntrinsicPass.hpp"
#include "Passes/EmitScriptUtilitiesPass.hpp"
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
        if (val < 0 && (uint16_t)val != 0x8000)
            printf("unsigned = %X\n", (uint16_t)val);
        c = ConstantInt::get(getSizeType(), static_cast<uint64_t>(static_cast<uint16_t>(val)), false);
    }
    else
    {
        if (val < 0 && val != -1)
            printf("__signed = %X\n", (uint16_t)val);
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
#if 0
    int fd = open(R"(C:\windos\Jones\TEST)", O_RDONLY | O_BINARY);
    if (fd != -1)
    {
        size_t n = (size_t)filelength(fd);
        uint8_t *buf = new uint8_t[n];
        read(fd, buf, n);
        close(fd);
        static char const* const kNames[] = {
            "KLoad",
            "KUnLoad",
            "KScriptID",
            "KDisposeScript",
            "KClone",
            "KDisposeClone",
            "KIsObject",
            "KRespondsTo",
            "KDrawPic",
            "KShow",
            "KPicNotValid",
            "KAnimate",
            "KSetNowSeen",
            "KNumLoops",
            "KNumCels",
            "KCelWide",
            "KCelHigh",
            "KDrawCel",
            "KAddToPic",
            "KNewWindow",
            "KGetPort",
            "KSetPort",
            "KDisposeWindow",
            "KDrawControl",
            "KHiliteControl",
            "KEditControl",
            "KTextSize",
            "KDisplay",
            "KGetEvent",
            "KGlobalToLocal",
            "KLocalToGlobal",
            "KMapKeyToDir",
            "KDrawMenuBar",
            "KMenuSelect",
            "KAddMenu",
            "KDrawStatus",
            "KParse",
            "KSaid",
            "KSetSynonyms",
            "KHaveMouse",
            "KSetCursor",
            "KSaveGame",
            "KRestoreGame",
            "KRestartGame",
            "KGameIsRestarting",
            "KDoSound",
            "KNewList",
            "KDisposeList",
            "KNewNode",
            "KFirstNode",
            "KLastNode",
            "KEmptyList",
            "KNextNode",
            "KPrevNode",
            "KNodeValue",
            "KAddAfter",
            "KAddToFront",
            "KAddToEnd",
            "KFindKey",
            "KDeleteKey",
            "KRandom",
            "KAbs",
            "KSqrt",
            "KGetAngle",
            "KGetDistance",
            "KWait",
            "KGetTime",
            "KStrEnd",
            "KStrCat",
            "KStrCmp",
            "KStrLen",
            "KStrCpy",
            "KFormat",
            "KGetFarText",
            "KReadNumber",
            "KBaseSetter",
            "KDirLoop",
            "KCantBeHere",
            "KOnControl",
            "KInitBresen",
            "KDoBresen",
            "KDoAvoider",
            "KSetJump",
            "KSetDebug",
            "KInspectObj",
            "KShowSends",
            "KShowObjs",
            "KShowFree",
            "KMemoryInfo",
            "KStackUsage",
            "KProfiler",
            "KGetMenu",
            "KSetMenu",
            "KGetSaveFiles",
            "KGetCWD",
            "KCheckFreeSpace",
            "KValidPath",
            "KCoordPri",
            "KStrAt",
            "KDeviceInfo",
            "KGetSaveDir",
            "KCheckSaveGame",
            "KShakeScreen",
            "KFlushResources",
            "KSinMult",
            "KCosMult",
            "KSinDiv",
            "KCosDiv",
            "KGraph",
            "KJoystick",
            "KShiftScreen",
            "KPalette",
            "KMemorySegment",
            "KIntersections",
            "KMemory",
            "KListOps",
            "KFileIO",
            "KDoAudio",
            "KDoSync",
            "KAvoidPath",
            "KSort",
            "KATan",
            "KLock",
            "KStrSplit",
            "KMessage",
            "KIsItSkip"
        };
        FILE *f = fopen(R"(C:\windos\Jones\TEST.TXT)", "w");
        char str[1024*4];
        uint indent = 0;
        uint16_t *p = reinterpret_cast<uint16_t*>(buf);
        uint16_t *pEnd = reinterpret_cast<uint16_t*>(buf + n);
        while (p < pEnd)
        {
            uint scriptId = *p++;

            if ((uint16_t)scriptId == (uint16_t)-2)
            {
                indent--;
                uint argc = *p++;

                str[2] = '\0';
                char *pstr = str;
                p += 2;
                for (uint i = 2; i < argc; ++i)
                {
                    pstr += (size_t)sprintf(pstr, ", %X", *p++);
                }

                fprintf(f, "%*cobj-1: [%s]\n", indent * 2, ' ', str + 2);
                continue;
            }

            uint offset = *p++;
            uint argc = *p++;

            if ((uint16_t)scriptId == (uint16_t)-3)
            {
                fprintf(f, "%*cGetProperty(%s) = %X\n", indent * 2, ' ', GetWorld().getSelectorName(offset).data(), argc);
                continue;
            }

            if ((uint16_t)scriptId == (uint16_t)-4)
            {
                fprintf(f, "%*cSetProperty(%s, %X)\n", indent * 2, ' ', GetWorld().getSelectorName(offset).data(), argc);
                continue;
            }

            if ((scriptId & 0x8000) != 0)
            {
                str[2] = '\0';
                char *pstr = str;
                argc = offset;
                p--;
                for (uint i = 0; i < argc; ++i)
                {
                    pstr += (size_t)sprintf(pstr, ", %X", *p++);
                }

//                 80: DoBresen - 140
//                 120: Sort - 1E0
//                 11: Animate - 2C
                scriptId &= 0x7fff;
                if (scriptId == 0x140 || scriptId == 0x1E0 || scriptId == 0x2C)
                {
                    fprintf(f, "%*c%s(%s)\n", indent * 2, ' ', kNames[scriptId / 4], str + 2);
                }
                else
                {
                    fprintf(f, "%*c%s(%s) = %X\n", indent * 2, ' ', kNames[scriptId / 4], str + 2, *p++);
                }
                continue;
            }

            Script *script = getScript(scriptId);
            assert(script != nullptr);

            std::string fullName = "entry";
            fullName += '@';
            fullName += utohexstr(offset, true);

            Function *func = nullptr;
            for (Function &f : script->getModule()->functions())
            {
                if (f.begin() == f.end())
                {
                    continue;
                }

                if (f.getEntryBlock().getName() == fullName)
                {
                    func = &f;
                    break;
                }
            }
            assert(func != nullptr);

            str[2] = '\0';
            char *pstr = str;
            for (uint i = 0; i < argc; ++i)
            {
                pstr += (size_t)sprintf(pstr, ", %X", *p++);
            }

            fprintf(f, "%*c%s(%s)\n", indent * 2, ' ', func->getName().data(), str + 2);
#if 1
            argc = *p++;
            if (func->getName().startswith("proc@"))
            {
                p += argc;
                p += 251;
            }
            else
            {
                str[2] = '\0';
                pstr = str;
                p += 2;
                for (uint i = 2; i < argc; ++i)
                {
                    pstr += (size_t)sprintf(pstr, ", %X", *p++);
                }

                fprintf(f, "%*cobj-0: [%s]\n", indent * 2, ' ', str + 2);

                str[2] = '\0';
                pstr = str;
                for (uint i = 0; i < 251; ++i)
                {
                    pstr += (size_t)sprintf(pstr, ", %X", *p++);
                }

                fprintf(f, "%*cglobals: [%s]\n", indent * 2, ' ', str + 2);
            }
            indent++;
#endif
        }
        fclose(f);
    }
#endif

  //  DumpScriptModule(999);
    StackReconstructionPass().run();
    printf("Finished stack reconstruction!\n");

    SplitSendPass().run();
    printf("Finished splitting Send Message calls!\n");

    FixCodePass().run();
    printf("Finished fixing code issues!\n");

    TranslateClassIntrinsicPass().run();
    printf("Finished translating class intrinsic!\n");

    EmitScriptUtilitiesPass().run();
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
