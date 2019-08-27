//===- Types.hpp ----------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_TYPES_HPP
#define SCI_UTILS_PMACHINE_LLVM_TYPES_HPP

extern "C" {
#include "sci/Utils/Types.h"
}

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <memory>
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringExtras.h"

using llvm::StringRef;
using llvm::ArrayRef;
using llvm::MutableArrayRef;

template <typename T>
inline T selector_cast(ObjID sel) {
    return static_cast<T>(static_cast<int16_t>(sel));
}

#endif // SCI_UTILS_PMACHINE_LLVM_TYPES_HPP