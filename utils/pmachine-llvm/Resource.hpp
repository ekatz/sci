//===- Resource.hpp -------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_RESOURCE_HPP
#define SCI_UTILS_PMACHINE_LLVM_RESOURCE_HPP

#include "Types.hpp"

extern "C" {
#include "sci/Kernel/FarData.h"
#include "sci/Kernel/Resource.h"
}

namespace sci {

namespace detail {
#include "sci/PMachine/Object.h"
}

using detail::ExportTable;
using detail::ExportTableEntry;
using detail::RelocTable;
using detail::ResClassEntry;
using detail::SegHeader;

struct ObjRes : public detail::ObjRes {
  enum {
    SPECIES_OFFSET,
    SUPER_OFFSET,
    INFO_OFFSET,
    NAME_OFFSET,
    VALUES_OFFSET
  };

  bool isClass() const { return (infoSel & CLASSBIT) != 0; }

  bool isObject() const { return !isClass(); }

  unsigned getPropertyCount() const {
    return static_cast<unsigned>(varSelNum) -
           static_cast<unsigned>(INFO_OFFSET);
  }

  const int16_t *getPropertyValues() const { return sels + INFO_OFFSET; }

  const ObjID *getPropertySelectors() const {
    return reinterpret_cast<const ObjID *>(getPropertyValues() + varSelNum);
  }

  unsigned getMethodCount() const {
    const ObjID *Funcs = reinterpret_cast<const ObjID *>(
        reinterpret_cast<const byte *>(sels) + funcSelOffset);
    return Funcs[-1];
  }

  const ObjID *getMethodSelectors() const {
    const ObjID *Funcs = reinterpret_cast<const ObjID *>(
        reinterpret_cast<const byte *>(sels) + funcSelOffset);
    return Funcs;
  }

  const uint16_t *getMethodOffsets() const {
    const ObjID *Funcs = reinterpret_cast<const ObjID *>(
        reinterpret_cast<const byte *>(sels) + funcSelOffset);
    return Funcs + Funcs[-1] + 1;
  }
};

inline bool isUnsignedValue(int16_t Val) {
  // If this seems like a flag, then it is unsigned.
  return ((((uint16_t)Val & 0xE000) != 0xE000) &&
          ((((uint16_t)Val & 0xF000) | 0x8000) == (uint16_t)Val));
}

inline uintptr_t widenValue(int16_t Val) {
  return isUnsignedValue(Val)
             ? static_cast<uint64_t>(static_cast<uint16_t>(Val))
             : static_cast<uint64_t>(static_cast<int16_t>(Val));
}


#define KORDINAL_ScriptID       2
#define KORDINAL_DisposeScript  3
#define KORDINAL_Clone          4
#define KORDINAL_MapKeyToDir    31
#define KORDINAL_NewNode        48
#define KORDINAL_FirstNode      49
#define KORDINAL_LastNode       50
#define KORDINAL_NextNode       52
#define KORDINAL_PrevNode       53
#define KORDINAL_NodeValue      54
#define KORDINAL_AddAfter       55
#define KORDINAL_AddToFront     56
#define KORDINAL_AddToEnd       57
#define KORDINAL_FindKey        58

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_RESOURCE_HPP
