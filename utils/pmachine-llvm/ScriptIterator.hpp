#pragma once
#ifndef _ScriptIterator_HPP_
#define _ScriptIterator_HPP_

#include "Types.hpp"

BEGIN_NAMESPACE_SCI

class Script;

template <bool isConst = false>
class ScriptIterator
{
public:
    typedef std::forward_iterator_tag iterator_category;
    typedef Script value_type;
    typedef std::ptrdiff_t difference_type;

    typedef typename choose_type<isConst, const Script&, Script&>::type reference;
    typedef typename choose_type<isConst, const Script*, Script*>::type pointer;

    ScriptIterator() : m_ptr(nullptr), m_end(nullptr) {}
    ScriptIterator(const std::unique_ptr<Script> *ptr, const std::unique_ptr<Script> *end) : m_ptr(ptr), m_end(end) {
        if (m_ptr != nullptr && m_ptr->get() == nullptr) {
            advancePastNull();
        }
    }
    ScriptIterator(const ScriptIterator<false> &i) : m_ptr(i.m_ptr), m_end(i.m_end) {}

    reference operator*() const { return *m_ptr->get(); }
    pointer operator->() const { return m_ptr->get(); }
    ScriptIterator& operator++() {
        advancePastNull();
        return *this;
    }
    ScriptIterator operator++(int) {
        ScriptIterator tmp(*this);
        ++*this;
        return tmp;
    }

    friend bool operator==(const ScriptIterator& left, const ScriptIterator& right) {
        return left.m_ptr == right.m_ptr;
    }

    friend bool operator!=(const ScriptIterator& left, const ScriptIterator& right) {
        return left.m_ptr != right.m_ptr;
    }

private:
    void advancePastNull() {
        do
        {
            ++m_ptr;
        }
        while (m_ptr != m_end && m_ptr->get() == nullptr);
    }

    const std::unique_ptr<Script> *m_ptr;
    const std::unique_ptr<Script> *m_end;
};

typedef ScriptIterator<false> script_iterator;
typedef ScriptIterator<true> const_script_iterator;

END_NAMESPACE_SCI

#endif // !_ScriptIterator_HPP_
