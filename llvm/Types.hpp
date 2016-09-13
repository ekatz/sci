#pragma once
#ifndef _Types_HPP_
#define _Types_HPP_

#ifdef __cplusplus
extern "C" {
#endif

#include "../SCI/src/Types.h"

#define BEGIN_NAMESPACE_SCI namespace sci {
#define END_NAMESPACE_SCI   }

#ifdef __cplusplus
} // extern "C"

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <memory>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringExtras.h>

using llvm::StringRef;
using llvm::ArrayRef;

template <typename T>
inline T cast_selector(ObjID sel) {
    return static_cast<T>(static_cast<int16_t>(sel));
}
#endif

#endif // !_Types_HPP_