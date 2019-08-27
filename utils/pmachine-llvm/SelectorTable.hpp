//===- SelectorTable.hpp --------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_SELECTORTABLE_HPP
#define SCI_UTILS_PMACHINE_LLVM_SELECTORTABLE_HPP

#include "Types.hpp"

namespace sci {

class Method;
class Property;

class SelectorTable {
public:
  SelectorTable();
  ~SelectorTable();

  size_t size() const;

  StringRef getSelectorName(unsigned ID) const;
  unsigned getNewMethodSelector() const { return NewMethodSel; }

  ArrayRef<Method *> getMethods(unsigned ID) const;
  ArrayRef<Property *> getProperties(unsigned ID) const;

  bool addProperty(Property &Prop, unsigned Index);
  bool addMethod(Method &Mtd, unsigned VftblIndex);

private:
  bool addSelector(void *Ptr, unsigned Selector, unsigned Index, bool IsMethod);
  ArrayRef<void *> getSelectors(unsigned Selector, bool IsMethod) const;

  struct Entry {
    struct SelData {
      union {
        void **Sels;
        void *SingleSel;
      };

      uint16_t Count;
      uint16_t Capacity;
      uint16_t Index;
    };

    SelData Props;
    SelData Methods;

    uint8_t NameLength;
    char Name[1];
  };

  Entry *loadEntry(unsigned ID);

  static void initializeSelData(Entry::SelData &Data);

  std::vector<Entry *> Table;
  unsigned NewMethodSel;
};

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_SELECTORTABLE_HPP
