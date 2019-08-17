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
using llvm::MutableArrayRef;

template <typename T>
inline T selector_cast(ObjID sel) {
    return static_cast<T>(static_cast<int16_t>(sel));
}


template <bool flag, class IsTrue, class IsFalse>
struct choose_type;

template <class IsTrue, class IsFalse>
struct choose_type<true, IsTrue, IsFalse>
{
    typedef IsTrue type;
};

template <class IsTrue, class IsFalse>
struct choose_type<false, IsTrue, IsFalse>
{
    typedef IsFalse type;
};

#endif

#endif // !_Types_HPP_