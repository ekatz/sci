//===- ScriptIterator.hpp -------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef SCI_UTILS_PMACHINE_LLVM_SCRIPTITERATOR_HPP
#define SCI_UTILS_PMACHINE_LLVM_SCRIPTITERATOR_HPP

#include "Types.hpp"

namespace sci {

class Script;

template <bool IsConst = false> class ScriptIterator {
public:
  typedef std::forward_iterator_tag iterator_category;
  typedef Script value_type;
  typedef std::ptrdiff_t difference_type;

  typedef typename std::conditional<IsConst, const Script &, Script &>::type
      reference;
  typedef typename std::conditional<IsConst, const Script *, Script *>::type
      pointer;

  ScriptIterator() : Ptr(nullptr), End(nullptr) {}
  ScriptIterator(const std::unique_ptr<Script> *Ptr,
                 const std::unique_ptr<Script> *End)
      : Ptr(Ptr), End(End) {
    if (Ptr && !Ptr->get())
      advancePastNull();
  }
  ScriptIterator(const ScriptIterator<false> &I) : Ptr(I.Ptr), End(I.End) {}

  reference operator*() const { return *Ptr->get(); }
  pointer operator->() const { return Ptr->get(); }
  ScriptIterator &operator++() {
    advancePastNull();
    return *this;
  }
  ScriptIterator operator++(int) {
    ScriptIterator Tmp(*this);
    ++*this;
    return Tmp;
  }

  friend bool operator==(const ScriptIterator &LHS, const ScriptIterator &RHS) {
    return LHS.Ptr == RHS.Ptr;
  }

  friend bool operator!=(const ScriptIterator &LHS, const ScriptIterator &RHS) {
    return LHS.Ptr != RHS.Ptr;
  }

private:
  void advancePastNull() {
    do {
      ++Ptr;
    } while (Ptr != End && Ptr->get() == nullptr);
  }

  const std::unique_ptr<Script> *Ptr;
  const std::unique_ptr<Script> *End;
};

typedef ScriptIterator<false> script_iterator;
typedef ScriptIterator<true> const_script_iterator;

} // end namespace sci

#endif // SCI_UTILS_PMACHINE_LLVM_SCRIPTITERATOR_HPP
