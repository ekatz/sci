//===- Script.hpp ---------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_SCRIPT_HPP
#define SCI_UTILS_PMACHINE_LLVM_SCRIPT_HPP

#include "Resource.hpp"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"

namespace sci {

class Object;
class Procedure;

class Script {
public:
  Script(unsigned ID, Handle Hunk);
  ~Script();

  unsigned getId() const { return ID; }
  llvm::Module *getModule() const { return Mod.get(); }
  std::unique_ptr<llvm::Module> takeModule() { return std::move(Mod); }
  llvm::GlobalVariable *getString(StringRef Str);
  llvm::GlobalVariable *getLocalString(unsigned Offset);

  Procedure *getProcedure(unsigned Offset) const;
  ArrayRef<Procedure> getProcedures() const;
  unsigned getProcedureCount() const { return ProcCount; }

  unsigned getLocalVariablesCount() const { return LocalCount; }
  llvm::Value *getLocalVariable(unsigned Idx) const;
  llvm::Value *getGlobalVariable(unsigned Idx);
  llvm::GlobalObject *getExportedValue(unsigned Idx) const;
  llvm::Value *getRelocatedValue(unsigned Offset) const;

  const char *getDataAt(unsigned Offset) const;
  unsigned getOffsetOf(const void *Data) const;

  unsigned getObjectId(const Object &Obj) const;
  Object *getObject(unsigned OID) const;
  ArrayRef<Object> getObjects() const {
    return llvm::makeArrayRef(Objects, ObjectCount);
  }

  Object *lookupObject(llvm::GlobalVariable &Var) const;

  bool load();

private:
  Object *addObject(const ObjRes &Res);
  void setLocals(ArrayRef<int16_t> Vals, bool Exported);
  llvm::GlobalVariable *getGlobalVariables();

  int lookupExportTable(const ExportTable *Table, unsigned OffsetBegin,
                        unsigned OffsetEnd);
  int lookupExportTable(const ExportTable *Table, int Pos, unsigned OffsetBegin,
                        unsigned OffsetEnd);
  llvm::GlobalObject **updateExportTable(const ExportTable *Table,
                                         unsigned Offset,
                                         llvm::GlobalObject *Val = nullptr);
  llvm::GlobalObject **updateExportTable(const ExportTable *Table,
                                         unsigned OffsetBegin,
                                         unsigned OffsetEnd,
                                         llvm::GlobalObject *Val = nullptr);

  int lookupRelocTable(const RelocTable *Table, unsigned OffsetBegin,
                       unsigned OffsetEnd);
  int lookupRelocTable(const RelocTable *Table, int Pos, unsigned OffsetBegin,
                       unsigned OffsetEnd);
  llvm::Value **updateRelocTable(const RelocTable *Table, unsigned Offset,
                                 llvm::GlobalObject *Val = nullptr);
  llvm::Value **updateRelocTable(const RelocTable *Table, unsigned OffsetBegin,
                                 unsigned OffsetEnd,
                                 llvm::GlobalObject *Val = nullptr);

  void loadProcedures(llvm::SmallVectorImpl<unsigned> &ProcOffsets,
                      const ExportTable *ExportTbl);
  void sortFunctions();

  void growProcedureArray(unsigned Count);

  const unsigned ID;
  Handle Hunk;
  std::unique_ptr<llvm::Module> Mod;

  std::map<std::string, llvm::GlobalVariable *> Strings;
  std::unique_ptr<SegHeader *[]> StringSegs;
  unsigned StringSegCount = 0U;

  Object *Objects = nullptr;
  unsigned ObjectCount = 0U;

  llvm::GlobalVariable *Globals = nullptr;
  llvm::GlobalVariable *Locals = nullptr;
  unsigned LocalCount = 0U;

  std::unique_ptr<llvm::GlobalObject *[]> Exports;
  unsigned ExportCount = 0U;

  std::map<unsigned, llvm::Value *> RelocTbl;

  Procedure *Procs = nullptr;
  unsigned ProcCount = 0U;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_SCRIPT_HPP
