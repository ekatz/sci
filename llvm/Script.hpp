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

class Script
{
public:
    Script(uint id, Handle hunk);
    ~Script();

    uint getId() const { return m_id; }
    llvm::Module* getModule() const { return m_module.get(); }
    llvm::GlobalVariable* getString(const llvm::StringRef &str);
    llvm::Function* getProcedure(const uint8_t *ptr);

    uint getLocalVariablesCount() const { return m_localCount; }
    llvm::Value* getLocalVariable(uint idx) const;
    llvm::Value* getGlobalVariable(uint idx);
    llvm::GlobalObject* getExportedValue(uint idx) const;
    llvm::Value* getRelocatedValue(uint offset) const;

    uint getObjectId(const Object &obj) const;

    const char* getDataAt(uint offset) const;
    uint getOffsetOf(const void *data) const;

    bool load();

    //AnalyzedFunction* analyzeFunction(uint selector, uint offset);

private:
    Object* addObject(const ObjRes &res);
    void setLocals(const llvm::ArrayRef<int16_t> &vals, bool exported);
    llvm::GlobalVariable* getGlobalVariables();

    int lookupExportTable(const ExportTable *table, uint offsetBegin, uint offsetEnd);
    llvm::GlobalObject** updateExportTable(const ExportTable *table, uint offset, llvm::GlobalObject *val = nullptr);
    llvm::GlobalObject** updateExportTable(const ExportTable *table, uint offsetBegin, uint offsetEnd, llvm::GlobalObject *val = nullptr);

    uint lookupRelocTable(const RelocTable *table, uint offsetBegin, uint offsetEnd);
    llvm::Value** updateRelocTable(const RelocTable *table, uint offset, llvm::GlobalObject *val = nullptr);
    llvm::Value** updateRelocTable(const RelocTable *table, uint offsetBegin, uint offsetEnd, llvm::GlobalObject *val = nullptr);

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

    std::map<const uint8_t *, llvm::Function *> m_procs;
};

END_NAMESPACE_SCI

#endif // !_Script_HPP_
