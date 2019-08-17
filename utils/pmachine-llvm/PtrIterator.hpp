#pragma once
#ifndef _PtrIterator_HPP_
#define _PtrIterator_HPP_

#include "Types.hpp"

template <class Ptr> // Predecessor Iterator
class PtrIterator : public std::iterator<std::forward_iterator_tag, Ptr, ptrdiff_t, Ptr*, Ptr*>
{
    typedef std::iterator<std::forward_iterator_tag, Ptr, ptrdiff_t, Ptr*, Ptr*> super;
    typedef PtrIterator<Ptr> Self;
    Ptr *m_ptr;

    void advancePastNull() {
        // Loop to ignore non-terminator uses (for example BlockAddresses).
        while (m_ptr != nullptr) {
            ++m_ptr;
        }
    }

public:
    typedef typename super::pointer pointer;
    typedef typename super::reference reference;

    PtrIterator() {}
    PtrIterator(Ptr *p) : m_ptr(p) {
        if (m_ptr != nullptr) {
            advancePastNonTerminators();
        }
    }

    bool operator==(const Self& x) const { return It == x.It; }
    bool operator!=(const Self& x) const { return !operator==(x); }

    reference operator*() const {
        assert(!It.atEnd() && "pred_iterator out of range!");
        return cast<TerminatorInst>(*It)->getParent();
    }
    pointer *operator->() const { return &operator*(); }

    Self& operator++() {   // Preincrement
        assert(!It.atEnd() && "pred_iterator out of range!");
        ++It; advancePastNonTerminators();
        return *this;
    }

    Self operator++(int) { // Postincrement
        Self tmp = *this; ++*this; return tmp;
    }

    /// getOperandNo - Return the operand number in the predecessor's
    /// terminator of the successor.
    unsigned getOperandNo() const {
        return It.getOperandNo();
    }

    /// getUse - Return the operand Use in the predecessor's terminator
    /// of the successor.
    Use &getUse() const {
        return It.getUse();
    }
};

typedef PredIterator<BasicBlock, Value::user_iterator> pred_iterator;
typedef PredIterator<const BasicBlock,
    Value::const_user_iterator> const_pred_iterator;

#endif // !_PtrIterator_HPP_
