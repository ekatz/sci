//===- SelectorTable.cpp --------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "SelectorTable.hpp"
#include "Method.hpp"
#include "Object.hpp"
#include "Property.hpp"
#include "Resource.hpp"
#include "Script.hpp"
#include "World.hpp"
#include "llvm/IR/Instructions.h"

using namespace sci;
using namespace llvm;

SelectorTable::SelectorTable()
    : Table(512), NewMethodSel(selector_cast<unsigned>(-1)) {}

SelectorTable::~SelectorTable() {
  for (Entry *E : Table)
    if (E) {
      if (E->Props.Count > 1)
        delete[] E->Props.Sels;

      if (E->Methods.Count > 1)
        delete[] E->Methods.Sels;

      delete[] reinterpret_cast<char *>(E);
    }
}

size_t SelectorTable::size() const {
  for (size_t Size = Table.size(); Size != 0; --Size)
    if (Table[Size - 1] != nullptr)
      return Size;
  return 0;
}

StringRef SelectorTable::getSelectorName(unsigned ID) const {
  const Entry *E = Table[ID];
  return E ? StringRef(E->Name, E->NameLength) : StringRef();
}

ArrayRef<Method *> SelectorTable::getMethods(unsigned ID) const {
  ArrayRef<void *> Sels = getSelectors(ID, true);
  return makeArrayRef(reinterpret_cast<Method *const *>(Sels.data()),
                      Sels.size());
}

ArrayRef<Property *> SelectorTable::getProperties(unsigned ID) const {
  ArrayRef<void *> Sels = getSelectors(ID, false);
  return makeArrayRef(reinterpret_cast<Property *const *>(Sels.data()),
                      Sels.size());
}

ArrayRef<void *> SelectorTable::getSelectors(unsigned Selector,
                                             bool IsMethod) const {
  void *const *Sels = nullptr;
  unsigned Count = 0;
  const Entry *E = Table[Selector];
  if (E) {
    const Entry::SelData &Data = IsMethod ? E->Methods : E->Props;
    Count = Data.Count;
    if (Count == 1)
      Sels = reinterpret_cast<void *const *>(&Data.SingleSel);
    else
      Sels = reinterpret_cast<void *const *>(Data.Sels);
  }
  return makeArrayRef(Sels, Count);
}

bool SelectorTable::addProperty(Property &Prop, unsigned Index) {
  return addSelector(&Prop, Prop.getSelector(), Index, false);
}

bool SelectorTable::addMethod(Method &method, unsigned VftblIndex) {
  return addSelector(&method, method.getSelector(), VftblIndex, true);
}

bool SelectorTable::addSelector(void *Ptr, unsigned Selector, unsigned Index,
                                bool IsMethod) {
  Entry *E = loadEntry(Selector);
  Entry::SelData &Data = IsMethod ? E->Methods : E->Props;

  if (Data.Index == (uint16_t)-1)
    Data.Index = Index;
  else if (Data.Index != Index)
    Data.Index = (uint16_t)-2;

  if (Data.Count == 0) {
    Data.SingleSel = Ptr;
    Data.Count = 1;
    return true;
  }

  if (Data.Count == 1) {
    if (Data.SingleSel == Ptr)
      return false;

    void *SingleSel = Data.SingleSel;
    Data.Capacity = 8;
    Data.Sels = new void *[Data.Capacity];
    Data.Sels[0] = SingleSel;
  } else {
    for (unsigned I = 0, N = Data.Count; I < N; ++I) {
      if (Data.Sels[I] == Ptr)
        return false;
    }

    if ((Data.Count + 1) > Data.Capacity) {
      void **OldSels = Data.Sels;
      Data.Capacity += 8;
      Data.Sels = new void *[Data.Capacity];
      memcpy(Data.Sels, OldSels, Data.Count * sizeof(void *));
      delete[] OldSels;
    }
  }
  Data.Sels[Data.Count++] = Ptr;
  return true;
}

SelectorTable::Entry *SelectorTable::loadEntry(unsigned ID) {
  if (ID == selector_cast<unsigned>(-1)) {
    return nullptr;
  }

  if (Table.size() <= ID) {
    Table.resize(ID + 1);
  }

  if (!Table[ID]) {
    char SelName[32];
    if (!GetFarStr(SELECTOR_VOCAB, ID, SelName))
      sprintf(SelName, "sel@%u", ID);
    size_t NameLen = strlen(SelName);

    if (NewMethodSel == selector_cast<unsigned>(-1))
      if (NameLen == 3 && memcmp(SelName, "new", 3) == 0)
        NewMethodSel = ID;

    char *Buf = new char[sizeof(Entry) + NameLen];
    Entry *E = reinterpret_cast<Entry *>(Buf);

    initializeSelData(E->Props);
    initializeSelData(E->Methods);

    E->NameLength = static_cast<uint8_t>(NameLen);
    memcpy(E->Name, SelName, NameLen);
    E->Name[NameLen] = '\0';

    Table[ID] = E;
  }
  return Table[ID];
}

void SelectorTable::initializeSelData(Entry::SelData &Data) {
  Data.Sels = nullptr;
  Data.Count = 0;
  Data.Capacity = 0;
  Data.Index = (uint16_t)-1;
}
