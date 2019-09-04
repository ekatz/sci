//===- Script.cpp ---------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "Script.hpp"
#include "Object.hpp"
#include "PMachine.hpp"
#include "Procedure.hpp"
#include "World.hpp"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Debug.h"

using namespace sci;
using namespace llvm;

Script::Script(unsigned ID, Handle Hunk) : ID(ID), Hunk(Hunk) {}

Script::~Script() {
  if (Procs) {
    for (unsigned I = 0, N = ProcCount; I < N; ++I)
      Procs[I].~Procedure();
    free(Procs);
  }
  if (Objects) {
    for (unsigned I = 0, N = ObjectCount; I < N; ++I)
      Objects[I].~Object();
    free(Objects);
  }
  if (Hunk)
    ResUnLoad(RES_SCRIPT, ID);
}

bool Script::load() {
  if (Mod)
    return true;
  else {
    char Name[16];
    sprintf(Name, "Script%03u", ID);
    Mod.reset(new Module(Name, GetWorld().getContext()));
    Mod->setDataLayout(GetWorld().getDataLayout());
  }

  World &W = GetWorld();
  SegHeader *Seg;
  ExportTable *ExportTbl = nullptr;
  RelocTable *Relocs = nullptr;
  SmallVector<unsigned, 8> ProcOffsets;
  unsigned NumObjects = 0;

  StringSegCount = 0;
  LocalCount = 0;
  ProcCount = 0;

  Seg = reinterpret_cast<SegHeader *>(Hunk);
  while (Seg->type != SEG_NULL) {
    switch (Seg->type) {
    case SEG_OBJECT:
      NumObjects++;
      break;

    case SEG_EXPORTS:
      ExportTbl = reinterpret_cast<ExportTable *>(Seg + 1);
      break;

    case SEG_RELOC:
      Relocs = reinterpret_cast<RelocTable *>(Seg + 1);
      break;

    case SEG_LOCALS:
      LocalCount = (Seg->size - sizeof(SegHeader)) / sizeof(uint16_t);
      break;

    case SEG_STRINGS:
      StringSegCount++;
      break;

    default:
      break;
    }

    Seg = NextSegment(Seg);
  }

  ObjectCount = 0;
  if (Objects) {
    free(Objects);
    Objects = nullptr;
  }
  if (NumObjects > 0) {
    size_t Size = NumObjects * sizeof(Object);
    Objects = reinterpret_cast<Object *>(malloc(Size));
    memset(Objects, 0, Size);
  }

  if (ExportTbl && ExportTbl->numEntries > 0) {
    ExportCount = ExportTbl->numEntries;
    Exports.reset(new GlobalObject *[ExportCount]);
    memset(Exports.get(), 0, ExportCount * sizeof(GlobalObject *));
  } else {
    ExportCount = 0;
    Exports.release();
  }

  if (StringSegCount) {
    StringSegs.reset(new SegHeader *[StringSegCount]);

    Seg = reinterpret_cast<SegHeader *>(Hunk);
    for (unsigned I = 0; I < StringSegCount; Seg = NextSegment(Seg)) {
      if (Seg->type == SEG_STRINGS) {
        StringSegs[I++] = Seg;

        const uint8_t *Res = reinterpret_cast<uint8_t *>(Seg + 1);
        unsigned OffsetBegin = getOffsetOf(Res);
        unsigned OffsetEnd = OffsetBegin + (static_cast<unsigned>(Seg->size) -
                                            sizeof(SegHeader));
        int Idx;

        Idx = lookupExportTable(ExportTbl, OffsetBegin, OffsetEnd);
        while (Idx >= 0) {
          GlobalObject *&Slot = Exports[Idx];
          assert(!Slot);

          unsigned Offset = ExportTbl->entries[Idx].ptrOff;
          const char *Str = getDataAt(Offset);
          Slot = getString(Str);

          Idx = lookupExportTable(ExportTbl, Idx + 1, Offset + 1, OffsetEnd);
        }

        Idx = lookupRelocTable(Relocs, OffsetBegin, OffsetEnd);
        while (Idx >= 0) {
          unsigned Offset = *reinterpret_cast<const uint16_t *>(
              getDataAt(Relocs->table[Idx]));
          Value *&Slot = RelocTbl[Offset];
          assert(Slot == nullptr);

          const char *Str = getDataAt(Offset);
          Slot = getString(Str);

          Idx = lookupRelocTable(Relocs, Idx + 1, Offset + 1, OffsetEnd);
        }
        break;
      }
    }
  }

  Seg = reinterpret_cast<SegHeader *>(Hunk);
  while (Seg->type != SEG_NULL) {
    switch (Seg->type) {
    case SEG_OBJECT: {
      const uint8_t *Res = reinterpret_cast<uint8_t *>(Seg + 1);
      Object *Obj = addObject(*reinterpret_cast<const ObjRes *>(Res));
      if (Obj) {
        unsigned Offset = getOffsetOf(Res + offsetof(ObjRes, sels));

        updateExportTable(ExportTbl, Offset, Obj->getGlobal());
        updateRelocTable(Relocs, Offset, Obj->getGlobal());
      }
      break;
    }

    case SEG_CLASS: {
      const uint8_t *Res = reinterpret_cast<uint8_t *>(Seg + 1);
      Object *Cls = W.addClass(*reinterpret_cast<const ObjRes *>(Res), *this);
      if (Cls) {
        unsigned Offset = getOffsetOf(Res + offsetof(ObjRes, sels));

        updateExportTable(ExportTbl, Offset, Cls->getGlobal());
        updateRelocTable(Relocs, Offset, Cls->getGlobal());
      }
      break;
    }

    case SEG_CODE: {
      const uint8_t *Res = reinterpret_cast<uint8_t *>(Seg + 1);
      unsigned OffsetBegin = getOffsetOf(Res);
      unsigned OffsetEnd =
          OffsetBegin + (static_cast<unsigned>(Seg->size) - sizeof(SegHeader));

      int Idx = lookupExportTable(ExportTbl, OffsetBegin, OffsetEnd);
      while (Idx >= 0) {
        unsigned Offset = ExportTbl->entries[Idx].ptrOff;
        ProcOffsets.push_back(Offset);

        Idx = lookupExportTable(ExportTbl, Idx + 1, Offset + 1, OffsetEnd);
      }
      break;
    }

    case SEG_SAIDSPECS:
      assert(false); // TODO: Add support for Said!!
      break;

    case SEG_LOCALS:
      if (LocalCount != 0) {
        const uint8_t *Res = reinterpret_cast<uint8_t *>(Seg + 1);
        ArrayRef<int16_t> Vals(reinterpret_cast<const int16_t *>(Res),
                               LocalCount);
        unsigned OffsetBegin = getOffsetOf(Res);
        unsigned OffsetEnd = OffsetBegin + (Vals.size() * sizeof(uint16_t));

        bool Exported = false;
        if (lookupExportTable(ExportTbl, OffsetBegin, OffsetEnd) >= 0) {
          assert(false && "Currently not supported!");
          Exported = true;
        }

        setLocals(Vals, Exported);

        int Idx = lookupRelocTable(Relocs, OffsetBegin, OffsetEnd);
        while (Idx >= 0) {
          unsigned Offset = *reinterpret_cast<const uint16_t *>(
              getDataAt(Relocs->table[Idx]));
          // Must be 16-bit aligned.
          assert((Offset % sizeof(uint16_t)) == 0);
          unsigned Idx = (Offset - OffsetBegin) / sizeof(uint16_t);

          Value *&Slot = RelocTbl[Offset];
          assert(!Slot);
          Slot = getLocalVariable(Idx);

          Idx = lookupRelocTable(Relocs, Idx + 1, Offset + sizeof(uint16_t),
                                 OffsetEnd);
        }
      }
      break;

    default:
      break;
    }

    Seg = NextSegment(Seg);
  }

  Seg = reinterpret_cast<SegHeader *>(Hunk);
  while (Seg->type != SEG_NULL) {
    switch (Seg->type) {
    case SEG_CLASS: {
      Class *Cls = Class::get(reinterpret_cast<ObjRes *>(Seg + 1)->speciesSel);
      Cls->loadMethods();
      break;
    }

    default:
      break;
    }

    Seg = NextSegment(Seg);
  }

  for (unsigned I = 0, N = ObjectCount; I < N; ++I)
    Objects[I].loadMethods();

  dbgs() << "Translating script " << Mod->getName().take_back(3) << '\n';
  loadProcedures(ProcOffsets, ExportTbl);
  sortFunctions();
  return true;
}

int Script::lookupExportTable(const ExportTable *Table, unsigned OffsetBegin,
                              unsigned OffsetEnd) {
  return lookupExportTable(Table, 0, OffsetBegin, OffsetEnd);
}

int Script::lookupExportTable(const ExportTable *Table, int Pos,
                              unsigned OffsetBegin, unsigned OffsetEnd) {
  if (Table) {
    assert(ExportCount == Table->numEntries);
    for (unsigned N = Table->numEntries; static_cast<unsigned>(Pos) < N;
         ++Pos) {
      assert(Table->entries[Pos].ptrSeg == 0);
      unsigned Offset = Table->entries[Pos].ptrOff;
      if (OffsetBegin <= Offset && Offset < OffsetEnd)
        return Pos;
    }
  }
  return -1;
}

GlobalObject **Script::updateExportTable(const ExportTable *Table,
                                         unsigned Offset, GlobalObject *Val) {
  return updateExportTable(Table, Offset, Offset + 1, Val);
}

GlobalObject **Script::updateExportTable(const ExportTable *Table,
                                         unsigned OffsetBegin,
                                         unsigned OffsetEnd,
                                         GlobalObject *Val) {
  int Idx = lookupExportTable(Table, OffsetBegin, OffsetEnd);
  if (Idx >= 0) {
    GlobalObject *&Slot = Exports[Idx];
    if (!Slot) {
      if (Val)
        Val->setLinkage(GlobalValue::ExternalLinkage);
      Slot = Val;
      return &Slot;
    }
  }
  return nullptr;
}

int Script::lookupRelocTable(const RelocTable *table, unsigned OffsetBegin,
                             unsigned OffsetEnd) {
  return lookupRelocTable(table, 0, OffsetBegin, OffsetEnd);
}

int Script::lookupRelocTable(const RelocTable *Table, int Pos,
                             unsigned OffsetBegin, unsigned OffsetEnd) {
  if (Table) {
    assert(Table->ptrSeg == 0);
    for (unsigned N = Table->numEntries; static_cast<unsigned>(Pos) < N;
         ++Pos) {
      const uint16_t *FixPtr =
          reinterpret_cast<const uint16_t *>(getDataAt(Table->table[Pos]));
      if (FixPtr != nullptr) {
        unsigned Offset = *FixPtr;
        if (OffsetBegin <= Offset && Offset <= OffsetEnd)
          return Pos;
      }
    }
  }
  return -1;
}

Value **Script::updateRelocTable(const RelocTable *table, unsigned Offset,
                                 GlobalObject *Val) {
  return updateRelocTable(table, Offset, Offset + 1, Val);
}

Value **Script::updateRelocTable(const RelocTable *table, unsigned OffsetBegin,
                                 unsigned OffsetEnd, GlobalObject *Val) {
  int Idx = lookupRelocTable(table, OffsetBegin, OffsetEnd);
  if (Idx >= 0) {
    unsigned Offset =
        *reinterpret_cast<const uint16_t *>(getDataAt(table->table[Idx]));
    Value *&Slot = RelocTbl[Offset];
    if (!Slot)
      Slot = Val;
    return &Slot;
  }
  return nullptr;
}

void Script::setLocals(ArrayRef<int16_t> Vals, bool Exported) {
  if (!Vals.empty()) {
    World &W = GetWorld();
    IntegerType *SizeTy = W.getSizeType();

    ArrayType *ArrTy = ArrayType::get(SizeTy, LocalCount);

    std::unique_ptr<Constant *[]> Consts(new Constant *[LocalCount]);
    for (unsigned I = 0, N = LocalCount; I < N; ++I)
      Consts[I] = W.getConstantValue(Vals[I]);
    Constant *C =
        ConstantArray::get(ArrTy, makeArrayRef(Consts.get(), LocalCount));

    if (ID == 0) {
      Locals = getGlobalVariables();
      Locals->setInitializer(C);
    } else {
      char Name[16];
      sprintf(Name, "?locals%03u", ID);
      GlobalValue::LinkageTypes linkage = (Exported || ID == 0)
                                              ? GlobalValue::ExternalLinkage
                                              : GlobalValue::InternalLinkage;
      Locals = new GlobalVariable(*getModule(), ArrTy, false, linkage, C, Name);
      Locals->setAlignment(16);
    }
  }
}

GlobalVariable *Script::getString(StringRef Str) {
  GlobalVariable *Var;
  auto It = Strings.find(Str);
  if (It == Strings.end()) {
    std::string Name =
        "?string@" + utostr(getId()) + '@' + utostr(Strings.size());
    Constant *C = ConstantDataArray::getString(Mod->getContext(), Str);
    Var = new GlobalVariable(*getModule(), C->getType(), true,
                             GlobalValue::LinkOnceODRLinkage, C, Name);
    Var->setAlignment(GetWorld().getTypeAlignment(C->getType()));
    Var->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    Strings.insert(std::pair<std::string, GlobalVariable *>(Str, Var));
  } else
    Var = It->second;
  return Var;
}

GlobalVariable *Script::getLocalString(unsigned Offset) {
  if (Offset <= sizeof(SegHeader)) {
    return nullptr;
  }

  for (unsigned I = 0, N = StringSegCount; I < N; ++I) {
    SegHeader *Seg = StringSegs[I];
    const uint8_t *Res = reinterpret_cast<uint8_t *>(Seg + 1);
    unsigned OffsetBegin = getOffsetOf(Res);
    unsigned OffsetEnd =
        OffsetBegin + (static_cast<unsigned>(Seg->size) - sizeof(SegHeader));

    if (OffsetBegin <= Offset && Offset < OffsetEnd) {
      const char *Str = getDataAt(Offset);
      return getString(Str);
    }
  }
  return nullptr;
}

void Script::sortFunctions() {
  World &W = GetWorld();
  std::map<unsigned, Function *> SortedFuncs;
  for (auto I = Mod->begin(), E = Mod->end(); I != E;) {
    Function &Func = *I++;
    Procedure *Proc = W.getProcedure(Func);
    if (Proc) {
      bool IsNew = SortedFuncs.emplace(Proc->getOffset(), &Func).second;
      assert(IsNew);
      Func.removeFromParent();
    }
  }

  auto &FuncList = Mod->getFunctionList();
  for (auto &P : SortedFuncs)
    FuncList.push_back(P.second);
}

void Script::growProcedureArray(unsigned Count) {
  size_t Size = (ProcCount + Count) * sizeof(Procedure);
  void *Mem = realloc(Procs, Size);
  if (Mem != Procs) {
    if (!Mem) {
      Mem = malloc(Size);
      if (ProcCount > 0) {
        memcpy(Mem, Procs, ProcCount * sizeof(Procedure));
        free(Procs);
      }
    }

    // Re-register all procedures.
    World &W = GetWorld();
    Procedure *Proc = reinterpret_cast<Procedure *>(Mem);
    for (unsigned I = 0, N = ProcCount; I < N; ++I, ++Proc)
      W.registerProcedure(*Proc);
  }
  Procs = reinterpret_cast<Procedure *>(Mem);
  ProcCount += Count;
}

void Script::loadProcedures(SmallVectorImpl<unsigned> &ProcOffsets,
                            const ExportTable *ExportTbl) {
  World &world = GetWorld();
  Function *FuncGlobalCallIntrin = Intrinsic::Get(Intrinsic::call);
  Function *FuncLocalCallIntrin = nullptr;

  do {
    if (!ProcOffsets.empty()) {
      unsigned Count = ProcCount;
      // Only the first set of procedures are from the export table.
      bool Exported = (Count == 0);

      growProcedureArray(static_cast<unsigned>(ProcOffsets.size()));

      Procedure *Proc = &Procs[Count];
      for (unsigned Offset : ProcOffsets) {
        new (Proc) Procedure(Offset, *this);
        Function *Func = Proc->load();
        assert(Func != nullptr);

        if (Exported)
          updateExportTable(ExportTbl, Offset, Func);
        else
          Func->setLinkage(GlobalValue::InternalLinkage);

        Proc++;
      }
      ProcOffsets.clear();
    }

    if (FuncLocalCallIntrin || (FuncLocalCallIntrin = getModule()->getFunction(
                                    FuncGlobalCallIntrin->getName()))) {
      for (User *U : FuncLocalCallIntrin->users()) {
        CallInst *Call = cast<CallInst>(U);
        unsigned Offset = static_cast<unsigned>(
            cast<ConstantInt>(Call->getArgOperand(0))->getZExtValue());

        if (!getProcedure(Offset) && !is_contained(ProcOffsets, Offset))
          ProcOffsets.push_back(Offset);
      }
      FuncLocalCallIntrin->replaceAllUsesWith(FuncGlobalCallIntrin);
    }
  } while (!ProcOffsets.empty());

  if (FuncLocalCallIntrin) {
    assert(FuncLocalCallIntrin->user_empty());
    FuncLocalCallIntrin->eraseFromParent();
  }
}

Procedure *Script::getProcedure(unsigned Offset) const {
  for (unsigned I = 0, N = ProcCount; I < N; ++I)
    if (Procs[I].getOffset() == Offset)
      return &Procs[I];
  return nullptr;
}

ArrayRef<Procedure> Script::getProcedures() const {
  return makeArrayRef(Procs, ProcCount);
}

llvm::Value *Script::getLocalVariable(unsigned Idx) const {
  if (Idx >= LocalCount)
    return nullptr;

  IntegerType *SizeTy = GetWorld().getSizeType();
  Value *Indices[] = {ConstantInt::get(SizeTy, 0),
                      ConstantInt::get(SizeTy, Idx)};
  return GetElementPtrInst::CreateInBounds(Locals, Indices);
}

llvm::Value *Script::getGlobalVariable(unsigned Idx) {
  if (Idx >= GetWorld().getGlobalVariablesCount())
    return nullptr;

  IntegerType *SizeTy = GetWorld().getSizeType();
  Value *Indices[] = {ConstantInt::get(SizeTy, 0),
                      ConstantInt::get(SizeTy, Idx)};
  return GetElementPtrInst::CreateInBounds(getGlobalVariables(), Indices);
}

llvm::GlobalVariable *Script::getGlobalVariables() {
  if (!Globals) {
    World &W = GetWorld();
    ArrayType *ArrTy =
        ArrayType::get(W.getSizeType(), W.getGlobalVariablesCount());
    Globals = new GlobalVariable(*getModule(), ArrTy, false,
                                 GlobalValue::ExternalLinkage, nullptr,
                                 "g_globalVars");
    Globals->setAlignment(16);
  }
  return Globals;
}

GlobalObject *Script::getExportedValue(unsigned Idx) const {
  return (Idx < ExportCount) ? Exports[Idx] : nullptr;
}

Value *Script::getRelocatedValue(unsigned Offset) const {
  auto It = RelocTbl.find(Offset);
  return (It != RelocTbl.end()) ? It->second : nullptr;
}

unsigned Script::getObjectId(const Object &Obj) const {
  assert(Objects <= (&Obj) && (&Obj) < (Objects + ObjectCount));
  return (&Obj) - Objects;
}

Object *Script::getObject(unsigned OID) const {
  return (OID < ObjectCount) ? &Objects[OID] : nullptr;
}

Object *Script::addObject(const ObjRes &Res) {
  Object *Obj = &Objects[ObjectCount++];
  new (Obj) Object(Res, *this);
  return Obj;
}

const char *Script::getDataAt(unsigned Offset) const {
  return (static_cast<int16_t>(Offset) > 0)
             ? reinterpret_cast<const char *>(Hunk) + Offset
             : nullptr;
}

unsigned Script::getOffsetOf(const void *Data) const {
  return (reinterpret_cast<uintptr_t>(Data) > reinterpret_cast<uintptr_t>(Hunk))
             ? reinterpret_cast<const char *>(Data) -
                   reinterpret_cast<const char *>(Hunk)
             : 0;
}

Object *Script::lookupObject(GlobalVariable &Var) const {
  for (unsigned I = 0, N = ObjectCount; I < N; ++I)
    if (Objects[I].getGlobal() == &Var)
      return &Objects[I];
  return nullptr;
}
