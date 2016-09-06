#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <set>
#include "Script.hpp"
#include "PMachine.hpp"

extern "C" {
#include "../SCI/src/Resource.h"
#include "../SCI/src/FarData.h"
}

using namespace llvm;

extern IntegerType *g_sizeTy;

#define SEG_NULL        0   // End of script resource
#define SEG_OBJECT      1   // Object
#define SEG_CODE        2   // Code
#define SEG_SYNONYMS    3   // Synonym word lists
#define SEG_SAIDSPECS   4   // Said specs
#define SEG_STRINGS     5   // Strings
#define SEG_CLASS       6   // Class
#define SEG_EXPORTS     7   // Exports
#define SEG_RELOC       8   // Relocation table
#define SEG_TEXT        9   // Preload text
#define SEG_LOCALS      10  // Local variables


#define NextSegment(seg)    ((SegHeader *)((byte *)(seg) + (seg)->size))


#pragma warning(push)
#pragma warning(disable: 4200) // zero-sized array in struct/union


typedef struct SegHeader
{
    uint16_t type;
    uint16_t size;
} SegHeader;


typedef struct ResClassEntry
{
    uint16_t obj;           // pointer to Obj
    uint16_t scriptNum;  // script number
} ResClassEntry;


typedef struct RelocTable
{
    uint16_t numEntries;
    uint16_t ptrSeg;
    uint16_t table[0];
} RelocTable;


typedef struct ExportTableEntry
{
    uint16_t ptrOff;
    uint16_t ptrSeg;
} ExportTableEntry;


typedef struct ExportTable
{
    uint16_t numEntries;
    ExportTableEntry entries[0];
} ExportTable;


#pragma warning(pop)


char* GetSelectorName(uint id, char *str)
{
    if (GetFarStr(SELECTOR_VOCAB, id, str) == NULL)
    {
        sprintf(str, "sel@%u", id);
    }
    return str;
}


static inline bool IsUnsignedValue(int16_t val)
{
    // If this seems like a flag, then it is unsigned.
    return ((((uint16_t)val & 0xE000) != 0xE000) && ((((uint16_t)val & 0xF000) | 0x8000) == (uint16_t)val));
}


ConstantInt* GetConstantValue(int16_t val)
{
    ConstantInt *c;
    if (IsUnsignedValue(val))
    {
        c = ConstantInt::get(g_sizeTy, static_cast<uint64_t>(static_cast<uint16_t>(val)));
    }
    else
    {
        c = ConstantInt::get(g_sizeTy, static_cast<uint64_t>(static_cast<int16_t>(val)));
    }
    return c;
}


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


static ConstantStruct* CreateInitializer(StructType *s, const int16_t *begin, const int16_t *end, const int16_t *&ptr, const int16_t *nameVal, GlobalVariable *name, GlobalVariable *vftbl)
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
                c = ConstantExpr::getPtrToInt(vftbl, g_sizeTy);
            }
            else if (ptr == nameVal)
            {
                c = ConstantExpr::getPtrToInt(name, g_sizeTy);
            }
            else
            {
                c = GetConstantValue(*ptr);
            }
            ptr++;
        }
        else
        {
            c = CreateInitializer(cast<StructType>(*i), begin, end, ptr, nameVal, name, vftbl);
        }
        args.push_back(c);
    }

    return static_cast<ConstantStruct *>(ConstantStruct::get(s, args));
}


static ConstantStruct* CreateInitializer(StructType *s, const int16_t *vals, uint len, GlobalVariable *vftbl, GlobalVariable *name)
{
    const int16_t *ptr = vals - 1;
    return CreateInitializer(s, vals, vals + len, ptr, vals + 1, name, vftbl);
}


static bool AddElementIndex(SmallVector<Value *, 16> &indices, StructType *s, uint &idx)
{
    size_t pos = indices.size();

    // Make place for the value.
    indices.push_back(NULL);

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
    indices.push_back(Constant::getNullValue(g_sizeTy));
    if (!AddElementIndex(indices, s, idx))
    {
        indices.pop_back();
    }
    return indices;
}


Script::Script(ScriptManager &mgr, uint id, Handle hunk) :
    m_mgr(mgr),
    m_id(id),
    m_hunk(hunk),
    m_numObjects(0),
    m_numVars(0),
    m_numExports(0),
    m_funcPush(NULL),
    m_funcPop(NULL)
{
    char name[10];
    sprintf(name, "Script%03u", m_id);
    m_module.reset(new Module(name, m_mgr.getContext()));
}


Script::~Script()
{
    if (m_hunk != NULL)
    {
        ResUnLoad(RES_SCRIPT, m_id);
    }
}


bool Script::load()
{
    SegHeader *seg;
    ExportTable *exports = NULL;
    uint numObjects = 0;

    addStubFunctions();

    seg = reinterpret_cast<SegHeader *>(m_hunk);
    while (seg->type != SEG_NULL)
    {
        switch (seg->type)
        {
        case SEG_OBJECT:
            numObjects++;
            break;

        case SEG_EXPORTS:
            exports = reinterpret_cast<ExportTable *>(seg + 1);
            break;

        default:
            break;
        }

        seg = NextSegment(seg);
    }

    m_numObjects = 0;
    m_objects.release();
    if (numObjects > 0)
    {
        m_objects.reset(new Object[numObjects]);
        memset(m_objects.get(), 0, numObjects * sizeof(Object));
    }

    uint numValidExports = 0;
    m_numExports = 0;
    m_exports.release();
    if (exports != NULL && exports->numEntries > 0)
    {
        for (uint i = 0, n = exports->numEntries; i < n; ++i)
        {
            if (exports->entries[i].ptrOff >= sizeof(SegHeader))
            {
                numValidExports++;
            }
        }

        m_exports.reset(new GlobalObject*[numValidExports]);
        memset(m_exports.get(), 0, numValidExports * sizeof(GlobalObject *));
    }

    m_numVars = 0;
    m_vars.release();

    seg = reinterpret_cast<SegHeader *>(m_hunk);
    while (seg->type != SEG_NULL)
    {
        switch (seg->type)
        {
        case SEG_OBJECT: {
            Object *obj = addObject(reinterpret_cast<ObjRes *>(seg + 1));
            if (obj != NULL && exports != NULL && m_numExports != exports->numEntries)
            {
                const char *res = reinterpret_cast<char *>(seg + 1) + offsetof(ObjRes, sels);
                for (uint i = 0, n = exports->numEntries; i < n; ++i)
                {
                    assert(exports->entries[i].ptrSeg == 0);
                    if (getDataAt(exports->entries[i].ptrOff) == res)
                    {
                        obj->instance->setLinkage(GlobalValue::ExternalLinkage);
                        m_exports[i] = obj->instance;
                        m_numExports++;
                        break;
                    }
                }
            }
            break;
        }

        case SEG_CLASS:
            m_mgr.addClass(reinterpret_cast<ObjRes *>(seg + 1));
//             heapLen += OBJSIZE(((ObjRes *)(seg + 1))->varSelNum);
// 
//             {
//                 int a = 0;
//                 ObjRes *cls = (ObjRes *)(seg + 1);
//                 ObjID *props = (ObjID *)cls->sels + cls->varSelNum;
//                 for (i = 0, n = cls->varSelNum; i < n; ++i)
//                 {
//                     if (props[i] == 23 && ((char*)hunk + cls->sels[i])[0] != '\0' && isalnum(((char*)hunk + cls->sels[i])[0]))
//                     {
//                         if (cls->speciesSel == 0) {
//                             a = a;
//                         }
//                         a = 1;
//                         LogInfo("class: @%d  \"%s\"", cls->speciesSel, (char*)hunk + cls->sels[i]);
//                         break;
//                     }
//                 }
// 
//                 if (!a)
//                 {
//                     LogInfo("class: @%d", cls->speciesSel);
//                 }
//             }
            break;

        case SEG_CODE:
            if (exports != NULL && m_numExports != exports->numEntries)
            {
                const char *segBegin = reinterpret_cast<char *>(seg + 1);
                const char *segEnd = reinterpret_cast<char *>(seg) + seg->size;
                for (uint i = 0, n = exports->numEntries; i < n; ++i)
                {
                    assert(exports->entries[i].ptrSeg == 0);
                    const char *ptr = getDataAt(exports->entries[i].ptrOff);
                    if (segBegin <= ptr && ptr < segEnd)
                    {
//                         func->setLinkage(GlobalValue::ExternalLinkage);
//                         m_exports[i] = func;
                        if (++m_numExports == exports->numEntries)
                        {
                            break;
                        }
                    }
                }
            }
            break;

        case SEG_SAIDSPECS:
        case SEG_STRINGS:
            break;

        case SEG_LOCALS:
            setLocals(makeArrayRef(reinterpret_cast<int16_t *>(seg + 1), (seg->size - sizeof(SegHeader)) / sizeof(uint16_t)));
            break;

        case SEG_RELOC:
            break;

        default:
            break;
        }

        seg = NextSegment(seg);
    }

    removeStubFunctions();

    // Sanity check.
    assert(m_numExports == numValidExports);

    getModule()->dump();
    return true;
}


void Script::setLocals(ArrayRef<int16_t> vals)
{
    if (vals.empty())
    {
        m_vars.release();
        return;
    }

    m_numVars = vals.size();
    m_vars.reset(new GlobalVariable*[m_numVars]);

    GlobalValue::LinkageTypes linkage = (m_id == 0) ? GlobalValue::ExternalLinkage : GlobalValue::InternalLinkage;
    for (uint i = 0, n = m_numVars; i < n; ++i)
    {
        m_vars[i] = new GlobalVariable(*getModule(), g_sizeTy, false, linkage, GetConstantValue(vals[i]));
    }
}


GlobalVariable* Script::getString(const StringRef &str)
{
    GlobalVariable *var;
    auto it = m_strings.find(str);
    if (it == m_strings.end())
    {
        Constant *c = ConstantDataArray::getString(m_mgr.getContext(), str);
        var = new GlobalVariable(*getModule(),
                                 c->getType(),
                                 true,
                                 GlobalValue::LinkOnceODRLinkage,
                                 c);
        var->setUnnamedAddr(true);
        m_strings.insert(std::pair<std::string, GlobalVariable *>(str, var));
    }
    else
    {
        var = it->second;
    }
    return var;
}


Function* Script::getProcedure(const uint8_t *ptr)
{
    Function *proc;
    auto it = m_procs.find(ptr);
    if (it == m_procs.end())
    {
        proc = parseFunction(ptr);
        if (proc != NULL)
        {
            m_procs.insert(std::make_pair(ptr, proc));
        }
    }
    else
    {
        proc = it->second;
    }
    return proc;
}



ScriptManager::ScriptManager(LLVMContext &ctx) :
    m_ctx(ctx),
    m_classes(NULL),
    m_numClasses(0),
    m_sels(500)
{
}


ScriptManager::~ScriptManager()
{
}


bool ScriptManager::load()
{
    ResClassEntry *res = (ResClassEntry *)ResLoad(RES_VOCAB, CLASSTBL_VOCAB);
    if (res == NULL)
    {
        return false;
    }

    m_numClasses = ResHandleSize(res) / sizeof(ResClassEntry);
    m_classes.reset(new Object[m_numClasses]);
    memset(m_classes.get(), 0, sizeof(Object) * m_numClasses);
    for (uint i = 0; i < m_numClasses; ++i)
    {
        m_classes[i].script = getScript(res[i].scriptNum);
    }

    for (uint i = 0; i < 1000; ++i)
    {
        Script *script = getScript(i);
        if (script != NULL)
        {
            script->load();
        }
    }
    return true;
}


Script* ScriptManager::getScript(uint id)
{
    Script *script = m_scripts[id].get();
    if (script == NULL)
    {
        Handle hunk = ResLoad(RES_SCRIPT, id);
        if (hunk != NULL)
        {
            script = new Script(*this, id, hunk);
            m_scripts[id].reset(script);
        }
    }
    return script;
}


uint ScriptManager::countMethods(const Object *obj)
{
    std::set<ObjID> methods;
    while (obj != NULL)
    {
        for (uint i = 0, n = obj->numMethods; i < n; ++i)
        {
            methods.insert(obj->methodSels[i]);
        }

        obj = obj->super;
    }
    return methods.size();
}


StringRef ScriptManager::getSelectorName(uint id)
{
    const char *data = getSelectorData(id);
    if (data != NULL)
    {
        data += sizeof(void*);
        return StringRef(data + sizeof(uint8_t), *reinterpret_cast<const uint8_t *>(data));
    }
    return StringRef();
}


FunctionType*& ScriptManager::getMethodSignature(uint id)
{
    return *reinterpret_cast<FunctionType **>(getSelectorData(id));
}


char* ScriptManager::getSelectorData(uint id)
{
    if (m_sels.size() <= id)
    {
        m_sels.resize(id + 1);
    }

    if (!m_sels[id])
    {
        char selName[32];
        size_t nameLen = strlen(GetSelectorName(id, selName));
        char *buf = new char[sizeof(void*) + sizeof(uint8_t) + (nameLen + 1)];
        m_sels[id].reset(buf);

        *reinterpret_cast<void **>(buf) = NULL;
        buf += sizeof(void*);

        *reinterpret_cast<uint8_t *>(buf) = static_cast<uint8_t>(nameLen);
        buf += sizeof(uint8_t);

        memcpy(buf, selName, nameLen);
        buf[nameLen] = '\0';
    }
    return m_sels[id].get();
}


Object* ScriptManager::getClass(uint id)
{
    if (id >= m_numClasses)
    {
        return NULL;
    }

    Object *obj = &m_classes[id];
    if (obj->type == NULL)
    {
        if (!obj->script->load() || obj->type == NULL)
        {
            obj = NULL;
        }
    }
    return obj;
}


Object* ScriptManager::addClass(const ObjRes *res)
{
    if ((uint)res->speciesSel >= m_numClasses)
    {
        return NULL;
    }
    
    Object *obj = &m_classes[res->speciesSel];
    if (m_classes[res->speciesSel].type != NULL)
    {
        return obj;
    }

    Script *script = obj->script;

    obj->propVals = res->getPropertyValues(2).data();
    obj->propSels = res->getPropertySelectors(2).data();
    obj->numProps = res->getPropertyValues(2).size();

    const char *name = script->getDataAt(res->nameSel);
    char nameOrdinal[] = "Class@0000";
    if (name == NULL || name[0] == '\0')
    {
        itoa(res->speciesSel, nameOrdinal + 6, 10);
        name = nameOrdinal;
    }

    std::vector<Type *> args;
    uint i = 0;
    obj->super = getClass((int16_t)res->superSel);
    if (obj->super != NULL)
    {
        args.push_back(obj->super->type);
        i += CalcNumInheritedElements(obj->super->type);
    }
    else
    {
        // Add the virtual table type.
        args.push_back(g_sizeTy);
        i++;
    }
    for (uint n = obj->numProps + 1; i < n; ++i)
    {
        args.push_back(g_sizeTy);
    }

    obj->type = StructType::create(m_ctx, args, name);

    script->initObject(obj, name);

    const uint16_t *methodOffs = res->getMethodOffsets().data();
    const ObjID *methodSels = res->getMethodSelectors().data();
    uint numMethods = res->getMethodSelectors().size();
    for (i = 0; i < numMethods; ++i)
    {
        AnalyzedFunction *func = script->analyzeFunction(methodSels[i], methodOffs[i]);
        assert(func != NULL);
        obj->methods.push_back(func);
    }
    return obj;
}


Object* Script::addObject(const ObjRes *res)
{
    Object *super = m_mgr.getClass((int16_t)res->superSel);
    assert(super != NULL);

    uint objNum = m_numObjects++;
    Object *obj = &m_objects[objNum];

    obj->type = super->type;
    obj->super = super;
    obj->script = this;

    obj->propVals = res->getPropertyValues(2).data();
    obj->propSels = super->propSels;
    obj->numProps = res->getPropertyValues(2).size();

    obj->methodOffs = res->getMethodOffsets().data();
    obj->methodSels = res->getMethodSelectors().data();
    obj->numMethods = res->getMethodSelectors().size();

    StructType *clsTy = super->type;
    PointerType *clsPtrTy = clsTy->getPointerTo();

    const char *name = getDataAt(res->nameSel);
    char nameOrdinal[] = "obj@0000";
    if (name == NULL || name[0] == '\0')
    {
        itoa(objNum, nameOrdinal + 4, 10);
        name = nameOrdinal;
    }
    initObject(obj, name);
    return obj;
}


void Script::initObject(Object *obj, const StringRef &name)
{
    Object *super = obj->super;
    if (obj->numMethods == 0 && super != NULL)
    {
        if (super->script == this)
        {
            obj->vftbl = super->vftbl;
        }
        else
        {
            obj->vftbl = getModule()->getGlobalVariable(super->vftbl->getName());
            if (obj->vftbl == NULL)
            {
                obj->vftbl = new GlobalVariable(*getModule(),
                                                super->vftbl->getType()->getElementType(),
                                                super->vftbl->isConstant(),
                                                GlobalValue::ExternalLinkage,
                                                NULL,
                                                super->vftbl->getName(),
                                                NULL,
                                                super->vftbl->getThreadLocalMode(),
                                                super->vftbl->getType()->getAddressSpace());
                obj->vftbl->copyAttributesFrom(super->vftbl);
            }
        }
    }
    else
    {
        uint numMethods = m_mgr.countMethods(obj);
        obj->vftbl = new GlobalVariable(*getModule(),
                                        ArrayType::get(Type::getInt8PtrTy(m_mgr.getContext()), numMethods),
                                        true,
                                        GlobalValue::LinkOnceODRLinkage,
                                        NULL,
                                        std::string("?vftbl@") + name);
        obj->vftbl->setUnnamedAddr(true);
    }

    for (uint i = 0, n = obj->numMethods; i < n; ++i)
    {
        FunctionType *&methodTy = m_mgr.getMethodSignature(obj->methodSels[i]);
        if (methodTy == NULL)
        {
            methodTy = FunctionType::get(Type::getVoidTy(m_mgr.getContext()), false);
        }
    }

    ConstantStruct *initStruct = CreateInitializer(obj->type, obj->propVals, obj->numProps, obj->vftbl, getString(name));
    obj->instance = new GlobalVariable(*getModule(), obj->type, false, GlobalValue::InternalLinkage, initStruct, name);

    createConstructor(obj);
}


void ScriptManager::dumpClasses() const
{
    for (uint i = 0; i < m_numClasses; ++i)
    {
        if (m_classes[i].type != NULL)
        {
            m_classes[i].type->dump();
        }
    }
}


Function* Script::createConstructor(Object *obj)
{
    StructType *clsTy = obj->type;
    LLVMContext &ctx = clsTy->getContext();
    // obj->propVals, obj->numProps

    std::string ctorName = "?ctor@";
    ctorName += obj->instance->getName();

    PointerType *clsPtrTy = clsTy->getPointerTo();
    FunctionType *ctorFuncTy = FunctionType::get(clsPtrTy, clsPtrTy, false);

    Function *ctorFunc = Function::Create(ctorFuncTy, Function::ExternalLinkage, ctorName, getModule());
    BasicBlock *bb = BasicBlock::Create(ctx, "", ctorFunc);
    Argument *selfArg = &*ctorFunc->arg_begin();
    selfArg->setName("self");

    SmallVector<Value *, 16> indices;
    GetElementPtrInst *elem;
    Value *val;

    // Add the virtual table.
    indices = CreateElementSelectorIndices(clsTy, 0);
    elem = GetElementPtrInst::CreateInBounds(clsTy, selfArg, indices, "-vftbl-", bb);
    val = ConstantExpr::getPtrToInt(obj->vftbl, g_sizeTy);
    new StoreInst(val, elem, bb);

    char selName[128];

    for (uint i = 0, n = obj->numProps; i < n; ++i)
    {
        indices = CreateElementSelectorIndices(clsTy, i + 1);
        elem = GetElementPtrInst::CreateInBounds(clsTy, selfArg, indices, m_mgr.getSelectorName(obj->propSels[i]), bb);
        if (i == 1)
        {
            val = ConstantExpr::getPtrToInt(getString(getDataAt(obj->propVals[1])), g_sizeTy);
        }
        else
        {
            val = GetConstantValue(obj->propVals[i]);
        }
        new StoreInst(val, elem, bb);
    }
    /*
    Function *xx = Function::Create(FunctionType::get(clsPtrTy, clsPtrTy, true), Function::ExternalLinkage, ctorName, getModule(scriptNum));
    BasicBlock *bb2 = BasicBlock::Create(m_ctx, "", xx);
    AllocaInst *valistptr = new AllocaInst(Type::getInt8PtrTy(m_ctx), "", bb2);
    valistptr->setAlignment(4);
    BitCastInst *valist = new BitCastInst(valistptr, Type::getInt8PtrTy(m_ctx), "", bb2);
    Function *vastart = Intrinsic::getDeclaration(getModule(scriptNum), Intrinsic::vastart);
    CallInst::Create(vastart, valist, "", bb2);
    new VAArgInst(valist, Type::getInt32Ty(m_ctx), "", bb2);
    xx->dump();
    */
    ReturnInst::Create(ctx, selfArg, bb);
    return ctorFunc;
}


void Script::addStubFunctions()
{
    LLVMContext &ctx = m_mgr.getContext();
    Module *module = getModule();
    FunctionType *funcTy;

    if (m_funcPush == NULL)
    {
        funcTy = FunctionType::get(Type::getVoidTy(ctx), g_sizeTy, false);
        m_funcPush = Function::Create(funcTy, Function::ExternalLinkage, "push@SCI", module);
    }

    if (m_funcPop == NULL)
    {
        funcTy = FunctionType::get(g_sizeTy, false);
        m_funcPop = Function::Create(funcTy, Function::ExternalLinkage, "pop@SCI", module);
    }

    if (m_funcRest == NULL)
    {
        funcTy = FunctionType::get(Type::getVoidTy(ctx), g_sizeTy, false);
        m_funcRest = Function::Create(funcTy, Function::ExternalLinkage, "rest@SCI", module);
    }

    if (m_funcSend == NULL)
    {
        funcTy = FunctionType::get(g_sizeTy, g_sizeTy, false);
        m_funcSend = Function::Create(funcTy, Function::ExternalLinkage, "send@SCI", module);
    }
}


void Script::removeStubFunctions()
{
    if (m_funcPush != NULL)
    {
        assert(m_funcPush->getNumUses() == 0);
        m_funcPush->removeFromParent();
        delete m_funcPush;
        m_funcPush = NULL;
    }
    if (m_funcPop != NULL)
    {
        assert(m_funcPop->getNumUses() == 0);
        m_funcPop->removeFromParent();
        delete m_funcPop;
        m_funcPop = NULL;
    }
}




Function* Script::parseFunction(const uint8_t *code)
{
    PMachineInterpreter pmachine(*this);
    return pmachine.run(code);
}
