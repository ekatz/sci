#include "Script.hpp"
#include "World.hpp"
#include "Object.hpp"
#include "Procedure.hpp"
#include "PMachine.hpp"
#include "Resource.hpp"
#include <llvm/ADT/STLExtras.h>
#include <set>

using namespace llvm;


BEGIN_NAMESPACE_SCI


Script::Script(uint id, Handle hunk) :
    m_id(id),
    m_hunk(hunk),
    m_objects(nullptr),
    m_objectCount(0),
    m_globals(nullptr),
    m_locals(nullptr),
    m_localCount(0),
    m_exportCount(0),
    m_procs(nullptr),
    m_procCount(0)
{
}


Script::~Script()
{
    if (m_procs != nullptr)
    {
        for (uint i = 0, n = m_procCount; i < n; ++i)
        {
            m_procs[i].~Procedure();
        }
        free(m_procs);
    }
    if (m_objects != nullptr)
    {
        for (uint i = 0, n = m_objectCount; i < n; ++i)
        {
            m_objects[i].~Object();
        }
        free(m_objects);
    }
    if (m_hunk != nullptr)
    {
        ResUnLoad(RES_SCRIPT, m_id);
    }
}


bool Script::load()
{
    if (m_module)
    {
        return true;
    }
    else
    {
        char name[10];
        sprintf(name, "Script%03u", m_id);
        m_module.reset(new Module(name, GetWorld().getContext()));
    }


    World &world = GetWorld();
    SegHeader *seg;
    ExportTable *exports = nullptr;
    RelocTable *relocs = nullptr;
    SmallVector<uint, 8> procOffsets;
    uint numObjects = 0;

    m_localCount = 0;
    m_procCount = 0;

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

        case SEG_RELOC:
            relocs = reinterpret_cast<RelocTable *>(seg + 1);
            break;

        case SEG_LOCALS:
            m_localCount = (seg->size - sizeof(SegHeader)) / sizeof(uint16_t);
            break;

        default:
            break;
        }

        seg = NextSegment(seg);
    }

    m_objectCount = 0;
    if (m_objects != nullptr)
    {
        free(m_objects);
        m_objects = nullptr;
    }
    if (numObjects > 0)
    {
        size_t size = numObjects * sizeof(Object);
        m_objects = reinterpret_cast<Object *>(malloc(size));
        memset(m_objects, 0, size);
    }

    if (exports != nullptr && exports->numEntries > 0)
    {
        m_exportCount = exports->numEntries;
        m_exports.reset(new GlobalObject*[m_exportCount]);
        memset(m_exports.get(), 0, m_exportCount * sizeof(GlobalObject *));
    }
    else
    {
        m_exportCount = 0;
        m_exports.release();
    }

    seg = reinterpret_cast<SegHeader *>(m_hunk);
    while (seg->type != SEG_NULL)
    {
        switch (seg->type)
        {
        case SEG_OBJECT: {
            const uint8_t *res = reinterpret_cast<uint8_t *>(seg + 1);
            Object *obj = addObject(*reinterpret_cast<const ObjRes *>(res));
            if (obj != nullptr)
            {
                uint offset = getOffsetOf(res + offsetof(ObjRes, sels));

                updateExportTable(exports, offset, obj->getGlobal());
                updateRelocTable(relocs, offset, obj->getGlobal());
            }
            break;
        }

        case SEG_CLASS: {
            const uint8_t *res = reinterpret_cast<uint8_t *>(seg + 1);
            Class *cls = world.addClass(*reinterpret_cast<const ObjRes *>(res), *this);
            if (cls != nullptr)
            {
                uint offset = getOffsetOf(res + offsetof(ObjRes, sels));

                GlobalObject **slot = updateExportTable(exports, offset);
                if (slot != nullptr)
                {
                    GlobalVariable *obj = cls->loadObject();
                    obj->setLinkage(GlobalValue::ExternalLinkage);
                    *slot = obj;
                }

                Value **slotVal = updateRelocTable(relocs, offset);
                if (slotVal != nullptr)
                {
                    *slotVal = cls->loadObject();
                }
            }
            break;
        }

        case SEG_CODE: {
            const uint8_t *res = reinterpret_cast<uint8_t *>(seg + 1);
            uint offsetBegin = getOffsetOf(res);
            uint offsetEnd = offsetBegin + (static_cast<uint>(seg->size) - sizeof(SegHeader));

            int idx = lookupExportTable(exports, offsetBegin, offsetEnd);
            while (idx >= 0)
            {
                uint offset = exports->entries[idx].ptrOff;
                procOffsets.push_back(offset);

                idx = lookupExportTable(exports, idx + 1, offset + 1, offsetEnd);
            }
            break;
        }

        case SEG_SAIDSPECS:
            assert(0); //TODO: Add support for Said!!

        case SEG_STRINGS: {
            const uint8_t *res = reinterpret_cast<uint8_t *>(seg + 1);
            uint offsetBegin = getOffsetOf(res);
            uint offsetEnd = offsetBegin + (static_cast<uint>(seg->size) - sizeof(SegHeader));
            int idx;

            idx = lookupExportTable(exports, offsetBegin, offsetEnd);
            while (idx >= 0)
            {
                GlobalObject *&slot = m_exports[idx];
                assert(slot == nullptr);

                uint offset = exports->entries[idx].ptrOff;
                const char *str = getDataAt(offset);
                slot = getString(str);

                idx = lookupExportTable(exports, idx + 1, offset + 1, offsetEnd);
            }


            idx = lookupRelocTable(relocs, offsetBegin, offsetEnd);
            while (idx >= 0)
            {
                uint offset = *reinterpret_cast<const uint16_t *>(getDataAt(relocs->table[idx]));
                Value *&slot = m_relocTable[offset];
                assert(slot == nullptr);

                const char *str = getDataAt(offset);
                slot = getString(str);

                idx = lookupRelocTable(relocs, idx + 1, offset + 1, offsetEnd);
            }
            break;
        }

        case SEG_LOCALS:
            if (m_localCount != 0)
            {
                const uint8_t *res = reinterpret_cast<uint8_t *>(seg + 1);
                ArrayRef<int16_t> vals(reinterpret_cast<const int16_t *>(res), m_localCount);
                uint offsetBegin = getOffsetOf(res);
                uint offsetEnd = offsetBegin + (vals.size() * sizeof(uint16_t));

                bool exported = false;
                if (lookupExportTable(exports, offsetBegin, offsetEnd) >= 0)
                {
                    exported = true;
                }

                setLocals(vals, exported);

                int idx = lookupRelocTable(relocs, offsetBegin, offsetEnd);
                while (idx >= 0)
                {
                    uint offset = *reinterpret_cast<const uint16_t *>(getDataAt(relocs->table[idx]));
                    // Must be 16-bit aligned.
                    assert((offset % sizeof(uint16_t)) == 0);
                    uint idx = (offset - offsetBegin) / sizeof(uint16_t);

                    Value *&slot = m_relocTable[offset];
                    assert(slot == nullptr);
                    slot = getLocalVariable(idx);

                    idx = lookupRelocTable(relocs, idx + 1, offset + sizeof(uint16_t), offsetEnd);
                }
            }
            break;

        default:
            break;
        }

        seg = NextSegment(seg);
    }


    seg = reinterpret_cast<SegHeader *>(m_hunk);
    while (seg->type != SEG_NULL)
    {
        switch (seg->type)
        {
        case SEG_CLASS: {
            Class *cls = Class::Get(reinterpret_cast<ObjRes *>(seg + 1)->speciesSel);
            cls->loadMethods();
            break;
        }

        default:
            break;
        }

        seg = NextSegment(seg);
    }

    for (uint i = 0, n = m_objectCount; i < n; ++i)
    {
        m_objects[i].loadMethods();
    }

    loadProcedures(procOffsets, exports);
    sortFunctions();
    return true;
}


int Script::lookupExportTable(const ExportTable *table, uint offsetBegin, uint offsetEnd)
{
    return lookupExportTable(table, 0, offsetBegin, offsetEnd);
}


int Script::lookupExportTable(const ExportTable *table, int pos, uint offsetBegin, uint offsetEnd)
{
    if (table != nullptr)
    {
        assert(m_exportCount == table->numEntries);
        for (uint n = table->numEntries; static_cast<uint>(pos) < n; ++pos)
        {
            assert(table->entries[pos].ptrSeg == 0);
            uint offset = table->entries[pos].ptrOff;
            if (offsetBegin <= offset && offset < offsetEnd)
            {
                return pos;
            }
        }
    }
    return -1;
}


GlobalObject** Script::updateExportTable(const ExportTable *table, uint offset, GlobalObject *val)
{
    return updateExportTable(table, offset, offset + 1, val);
}


GlobalObject** Script::updateExportTable(const ExportTable *table, uint offsetBegin, uint offsetEnd, GlobalObject *val)
{
    int idx = lookupExportTable(table, offsetBegin, offsetEnd);
    if (idx >= 0)
    {
        GlobalObject *&slot = m_exports[idx];
        if (slot == nullptr)
        {
            if (val != nullptr)
            {
                val->setLinkage(GlobalValue::ExternalLinkage);
            }
            slot = val;
            return &slot;
        }
    }
    return nullptr;
}


int Script::lookupRelocTable(const RelocTable *table, uint offsetBegin, uint offsetEnd)
{
    return lookupRelocTable(table, 0, offsetBegin, offsetEnd);
}

int Script::lookupRelocTable(const RelocTable *table, int pos, uint offsetBegin, uint offsetEnd)
{
    if (table != nullptr)
    {
        assert(table->ptrSeg == 0);
        for (uint n = table->numEntries; static_cast<uint>(pos) < n; ++pos)
        {
            const uint16_t *fixPtr = reinterpret_cast<const uint16_t *>(getDataAt(table->table[pos]));
            if (fixPtr != nullptr)
            {
                uint offset = *fixPtr;
                if (offsetBegin <= offset && offset <= offsetEnd)
                {
                    return pos;
                }
            }
        }
    }
    return -1;
}


Value** Script::updateRelocTable(const RelocTable *table, uint offset, GlobalObject *val)
{
    return updateRelocTable(table, offset, offset + 1, val);
}


Value** Script::updateRelocTable(const RelocTable *table, uint offsetBegin, uint offsetEnd, GlobalObject *val)
{
    int idx = lookupRelocTable(table, offsetBegin, offsetEnd);
    if (idx >= 0)
    {
        uint offset = *reinterpret_cast<const uint16_t *>(getDataAt(table->table[idx]));
        Value *&slot = m_relocTable[offset];
        if (slot == nullptr)
        {
            slot = val;
        }
        return &slot;
    }
    return nullptr;
}


void Script::setLocals(ArrayRef<int16_t> vals, bool exported)
{
    if (!vals.empty())
    {
        World &world = GetWorld();
        IntegerType *sizeTy = world.getSizeType();

        ArrayType *arrTy = ArrayType::get(sizeTy, m_localCount);

        std::unique_ptr<Constant*[]> consts(new Constant*[m_localCount]);
        for (uint i = 0, n = m_localCount; i < n; ++i)
        {
            consts[i] = world.getConstantValue(vals[i]);
        }
        Constant *c = ConstantArray::get(arrTy, makeArrayRef(consts.get(), m_localCount));

        if (m_id == 0)
        {
            m_locals = getGlobalVariables();
            m_locals->setInitializer(c);
        }
        else
        {
            char name[12];
            sprintf(name, "?locals%03u", m_id);
            GlobalValue::LinkageTypes linkage = (exported || m_id == 0) ? GlobalValue::ExternalLinkage : GlobalValue::InternalLinkage;
            m_locals = new GlobalVariable(*getModule(), arrTy, false, linkage, c, name);
        }
    }
}


GlobalVariable* Script::getString(StringRef str)
{
    GlobalVariable *var;
    auto it = m_strings.find(str);
    if (it == m_strings.end())
    {
        Constant *c = ConstantDataArray::getString(m_module->getContext(), str);
        var = new GlobalVariable(*getModule(),
                                 c->getType(),
                                 true,
                                 GlobalValue::LinkOnceODRLinkage,
                                 c,
                                 "?string");
        var->setUnnamedAddr(true);
        m_strings.insert(std::pair<std::string, GlobalVariable *>(str, var));
    }
    else
    {
        var = it->second;
    }
    return var;
}


void Script::sortFunctions()
{
    World &world = GetWorld();
    std::map<uint, Function *> sortedFuncs;
    for (auto i = m_module->begin(), e = m_module->end(); i != e;)
    {
        Function &func = *i++;
        Procedure *proc = world.getProcedure(func);
        if (proc != nullptr)
        {
            bool isNew = sortedFuncs.emplace(proc->getOffset(), &func).second;
            assert(isNew);
            func.removeFromParent();
        }
    }

    auto &funcList = m_module->getFunctionList();
    for (auto &p : sortedFuncs)
    {
        funcList.push_back(p.second);
    }
}


void Script::growProcedureArray(uint count)
{
    size_t size = (m_procCount + count) * sizeof(Procedure);
    void *mem = realloc(m_procs, size);
    if (mem != m_procs)
    {
        if (mem == nullptr)
        {
            mem = malloc(size);
            if (m_procCount > 0)
            {
                memcpy(mem, m_procs, m_procCount * sizeof(Procedure));
                free(m_procs);
            }
        }

        // Re-register all procedures.
        World &world = GetWorld();
        Procedure *proc = reinterpret_cast<Procedure *>(mem);
        for (uint i = 0, n = m_procCount; i < n; ++i, ++proc)
        {
            world.registerProcedure(*proc);
        }
    }
    m_procs = reinterpret_cast<Procedure *>(mem);
    m_procCount += count;
}


void Script::loadProcedures(SmallVector<uint, 8> &procOffsets, const ExportTable *exports)
{
    World &world = GetWorld();
    Function *funcGlobalCallIntrin = Intrinsic::Get(Intrinsic::call);
    Function *funcLocalCallIntrin = nullptr;

    while (!procOffsets.empty())
    {
        uint count = m_procCount;
        // Only the first set of procedures are from the export table.
        bool exported = (count == 0);

        growProcedureArray(static_cast<uint>(procOffsets.size()));

        Procedure *proc = &m_procs[count];
        for (uint offset : procOffsets)
        {
            new(proc) Procedure(offset, *this);
            Function *func = proc->load();
            assert(func != nullptr);

            if (exported)
            {
                updateExportTable(exports, offset, func);
            }
            else
            {
                func->setLinkage(GlobalValue::InternalLinkage);
            }

            proc++;
        }
        procOffsets.clear();

        if (funcLocalCallIntrin != nullptr || (funcLocalCallIntrin = getModule()->getFunction(funcGlobalCallIntrin->getName())) != nullptr)
        {
            for (User *user : funcLocalCallIntrin->users())
            {
                CallInst *call = cast<CallInst>(user);
                uint offset = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(0))->getZExtValue());

                if (getProcedure(offset) == nullptr && find(procOffsets, offset) == procOffsets.end())
                {
                    procOffsets.push_back(offset);
                }
            }
            funcLocalCallIntrin->replaceAllUsesWith(funcGlobalCallIntrin);
        }
    }

    if (funcLocalCallIntrin != nullptr)
    {
        assert(funcLocalCallIntrin->user_empty());
        funcLocalCallIntrin->eraseFromParent();
    }
}


Procedure* Script::getProcedure(uint offset) const
{
    for (uint i = 0, n = m_procCount; i < n; ++i)
    {
        if (m_procs[i].getOffset() == offset)
        {
            return &m_procs[i];
        }
    }
    return nullptr;
}


ArrayRef<Procedure> Script::getProcedures() const
{
    return makeArrayRef(m_procs, m_procCount);
}


llvm::Value* Script::getLocalVariable(uint idx) const
{
    if (idx >= m_localCount)
    {
        return nullptr;
    }

    IntegerType *sizeTy = GetWorld().getSizeType();
    Value *indices[] = {
        ConstantInt::get(sizeTy, 0),
        ConstantInt::get(sizeTy, idx)
    };
    return GetElementPtrInst::CreateInBounds(m_locals, indices);
}


llvm::Value* Script::getGlobalVariable(uint idx)
{
    if (idx >= GetWorld().getGlobalVariablesCount())
    {
        return nullptr;
    }

    IntegerType *sizeTy = GetWorld().getSizeType();
    Value *indices[] = {
        ConstantInt::get(sizeTy, 0),
        ConstantInt::get(sizeTy, idx)
    };
    return GetElementPtrInst::CreateInBounds(getGlobalVariables(), indices);
}


llvm::GlobalVariable* Script::getGlobalVariables()
{
    if (m_globals == nullptr)
    {
        World &world = GetWorld();
        ArrayType *arrTy = ArrayType::get(world.getSizeType(), world.getGlobalVariablesCount());
        m_globals = new GlobalVariable(*getModule(), arrTy, false, GlobalValue::ExternalLinkage, nullptr, "?globals");
    }
    return m_globals;
}


GlobalObject* Script::getExportedValue(uint idx) const
{
    return (idx < m_exportCount) ? m_exports[idx] : nullptr;
}


Value* Script::getRelocatedValue(uint offset) const
{
    auto it = m_relocTable.find(offset);
    return (it != m_relocTable.end()) ? it->second : nullptr;
}


uint Script::getObjectId(const Object &obj) const
{
    assert(m_objects <= (&obj) && (&obj) < (m_objects + m_objectCount));
    return (&obj) - m_objects;
}


Object* Script::getObject(uint id) const
{
    return (id < m_objectCount) ? &m_objects[id] : nullptr;
}


Object* Script::addObject(const ObjRes &res)
{
    Object *obj = &m_objects[m_objectCount++];
    new(obj) Object(res, *this);
    return obj;
}


const char* Script::getDataAt(uint offset) const
{
    return (static_cast<int16_t>(offset) > 0) ?
        reinterpret_cast<const char *>(m_hunk) + offset :
        nullptr;
}


uint Script::getOffsetOf(const void *data) const
{
    return (reinterpret_cast<uintptr_t>(data) > reinterpret_cast<uintptr_t>(m_hunk)) ?
        reinterpret_cast<const char *>(data) - reinterpret_cast<const char *>(m_hunk) :
        0;
}


Object* Script::lookupObject(GlobalVariable &var) const
{
    for (uint i = 0, n = m_objectCount; i < n; ++i)
    {
        if (m_objects[i].getGlobal() == &var)
        {
            return &m_objects[i];
        }
    }
    return nullptr;
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


END_NAMESPACE_SCI
