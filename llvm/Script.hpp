#pragma once
#ifndef _Script_HPP_
#define _Script_HPP_

#include "Types.hpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>

struct ObjRes;
struct ExportTable;
struct RelocTable;

BEGIN_NAMESPACE_SCI

class Object;
class Procedure;

class Script
{
public:
    Script(uint id, Handle hunk);
    ~Script();

    uint getId() const { return m_id; }
    llvm::Module* getModule() const { return m_module.get(); }
    llvm::GlobalVariable* getString(StringRef str);

    Procedure* getProcedure(uint offset) const;
    ArrayRef<Procedure> getProcedures() const;
    uint getProcedureCount() const { return m_procCount; }

    uint getLocalVariablesCount() const { return m_localCount; }
    llvm::Value* getLocalVariable(uint idx) const;
    llvm::Value* getGlobalVariable(uint idx);
    llvm::GlobalObject* getExportedValue(uint idx) const;
    llvm::Value* getRelocatedValue(uint offset) const;

    const char* getDataAt(uint offset) const;
    uint getOffsetOf(const void *data) const;

    uint getObjectId(const Object &obj) const;
    Object* getObject(uint id) const;
    ArrayRef<Object> getObjects() const { return llvm::makeArrayRef(m_objects, m_objectCount); }

    Object* lookupObject(llvm::GlobalVariable &var) const;

    bool load();

private:
    Object* addObject(const ObjRes &res);
    void setLocals(ArrayRef<int16_t> vals, bool exported);
    llvm::GlobalVariable* getGlobalVariables();

    int lookupExportTable(const ExportTable *table, uint offsetBegin, uint offsetEnd);
    int lookupExportTable(const ExportTable *table, int pos, uint offsetBegin, uint offsetEnd);
    llvm::GlobalObject** updateExportTable(const ExportTable *table, uint offset, llvm::GlobalObject *val = nullptr);
    llvm::GlobalObject** updateExportTable(const ExportTable *table, uint offsetBegin, uint offsetEnd, llvm::GlobalObject *val = nullptr);

    int lookupRelocTable(const RelocTable *table, uint offsetBegin, uint offsetEnd);
    int lookupRelocTable(const RelocTable *table, int pos, uint offsetBegin, uint offsetEnd);
    llvm::Value** updateRelocTable(const RelocTable *table, uint offset, llvm::GlobalObject *val = nullptr);
    llvm::Value** updateRelocTable(const RelocTable *table, uint offsetBegin, uint offsetEnd, llvm::GlobalObject *val = nullptr);

    void loadProcedures(llvm::SmallVector<uint, 8> &procOffsets, const ExportTable *exports);
    void sortFunctions();

    void growProcedureArray(uint count);

    const uint m_id;
    Handle m_hunk;
    std::unique_ptr<llvm::Module> m_module;

    std::map<std::string, llvm::GlobalVariable *> m_strings;

    Object *m_objects;
    uint m_objectCount;

    llvm::GlobalVariable *m_globals;
    llvm::GlobalVariable *m_locals;
    uint m_localCount;

    std::unique_ptr<llvm::GlobalObject *[]> m_exports;
    uint m_exportCount;

    std::map<uint, llvm::Value *> m_relocTable;

    Procedure *m_procs;
    uint m_procCount;
};

END_NAMESPACE_SCI

#endif // !_Script_HPP_
