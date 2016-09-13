#include "Script.hpp"
#include "World.hpp"
#include "Object.hpp"
#include "PMachine.hpp"
#include "Resource.hpp"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>

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
    m_exportCount(0)
{
    char name[10];
    sprintf(name, "Script%03u", m_id);
    m_module.reset(new Module(name, GetWorld().getContext()));
}


Script::~Script()
{
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
    World &world = GetWorld();
    SegHeader *seg;
    ExportTable *exports = nullptr;
    RelocTable *relocs = nullptr;
    uint numObjects = 0;

    m_localCount = 0;

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
                    GlobalVariable *obj = cls->getObject();
                    obj->setLinkage(GlobalValue::ExternalLinkage);
                    *slot = obj;
                }

                Value **slotVal = updateRelocTable(relocs, offset);
                if (slotVal != nullptr)
                {
                    *slotVal = cls->getObject();
                }
            }
            break;
        }

        case SEG_CODE:
            break;

        case SEG_SAIDSPECS:
            assert(0); //TODO: Add support for Said!!

        case SEG_STRINGS: {
            const uint8_t *res = reinterpret_cast<uint8_t *>(seg + 1);
            uint offsetBegin = getOffsetOf(res);
            uint offsetEnd = offsetBegin + (static_cast<uint>(seg->size) - sizeof(SegHeader));
            uint offset;

            offset = offsetBegin;
            for (int idx; (idx = lookupExportTable(exports, offset, offsetEnd)) >= 0;)
            {
                GlobalObject *&slot = m_exports[idx];
                assert(slot == nullptr);

                offset = exports->entries[idx].ptrOff;
                const char *str = getDataAt(offset);
                slot = getString(str);

                offset++;
            }


            offset = offsetBegin;
            while ((offset = lookupRelocTable(relocs, offset, offsetEnd)) != (uint)-1)
            {
                Value *&slot = m_relocTable[offset];
                assert(slot == nullptr);

                const char *str = getDataAt(offset);
                slot = getString(str);

                offset += sizeof(uint16_t);
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

                uint offset = offsetBegin;
                while ((offset = lookupRelocTable(relocs, offset, offsetEnd)) != (uint)-1)
                {
                    // Must be 16-bit aligned.
                    assert((offset % sizeof(uint16_t)) == 0);
                    uint idx = (offset - offsetBegin) / sizeof(uint16_t);

                    Value *&slot = m_relocTable[offset];
                    assert(slot == nullptr);
                    slot = getLocalVariable(idx);

                    offset += sizeof(uint16_t);
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

    //getModule()->dump();
    return true;
}


int Script::lookupExportTable(const ExportTable *table, uint offsetBegin, uint offsetEnd)
{
    if (table != nullptr)
    {
        assert(m_exportCount == table->numEntries);
        for (uint i = 0, n = table->numEntries; i < n; ++i)
        {
            assert(table->entries[i].ptrSeg == 0);
            uint offset = static_cast<uint>(table->entries[i].ptrOff);
            if (offsetBegin <= offset && offset < offsetEnd)
            {
                return static_cast<int>(i);
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


uint Script::lookupRelocTable(const RelocTable *table, uint offsetBegin, uint offsetEnd)
{
    if (table != nullptr)
    {
        assert(table->ptrSeg == 0);
        for (uint i = 0, n = table->numEntries; i < n; ++i)
        {
            const uint16_t *fixPtr = reinterpret_cast<const uint16_t *>(getDataAt(table->table[i]));
            if (fixPtr != nullptr)
            {
                uint offset = static_cast<uint>(*fixPtr);
                if (offsetBegin <= offset && offset <= offsetEnd)
                {
                    return offset;
                }
            }
        }
    }
    return (uint)-1;
}


Value** Script::updateRelocTable(const RelocTable *table, uint offset, GlobalObject *val)
{
    return updateRelocTable(table, offset, offset + 1, val);
}


Value** Script::updateRelocTable(const RelocTable *table, uint offsetBegin, uint offsetEnd, GlobalObject *val)
{
    uint offset = lookupRelocTable(table, offsetBegin, offsetEnd);
    if (offset != (uint)-1)
    {
        Value *&slot = m_relocTable[offset];
        if (slot == nullptr)
        {
            slot = val;
        }
        return &slot;
    }
    return nullptr;
}


void Script::setLocals(const ArrayRef<int16_t> &vals, bool exported)
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
            char name[10];
            sprintf(name, "locals%03u", m_id);
            GlobalValue::LinkageTypes linkage = (exported || m_id == 0) ? GlobalValue::ExternalLinkage : GlobalValue::InternalLinkage;
            m_locals = new GlobalVariable(*getModule(), arrTy, false, linkage, c, name);
        }
    }
}


GlobalVariable* Script::getString(const StringRef &str)
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
    //TODO: impl
    Function *proc;
    auto it = m_procs.find(ptr);
    if (it == m_procs.end())
    {
      //  proc = parseFunction(ptr);
        if (proc != nullptr)
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
        m_globals = new GlobalVariable(*getModule(), arrTy, false, GlobalValue::ExternalLinkage, nullptr, "globals");
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
