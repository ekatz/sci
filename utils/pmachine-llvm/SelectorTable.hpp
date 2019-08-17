#pragma once
#ifndef _SelectorTable_HPP_
#define _SelectorTable_HPP_

#include "Types.hpp"

BEGIN_NAMESPACE_SCI

class Method;
class Property;

class SelectorTable
{
public:
    SelectorTable();
    ~SelectorTable();

    size_t size() const;

    StringRef getSelectorName(uint id) const;
    uint getNewMethodSelector() const { return m_newMethodSel; }

    ArrayRef<Method *> getMethods(uint id) const;
    ArrayRef<Property *> getProperties(uint id) const;

    bool addProperty(Property &prop, uint index);
    bool addMethod(Method &method, uint vftblIndex);

private:
    bool addSelector(void *ptr, uint selector, uint index, bool isMethod);
    ArrayRef<void *> getSelectors(uint selector, bool isMethod) const;

    struct Entry
    {
        struct SelData
        {
            union {
                void **sels;
                void *singleSel;
            };

            uint16_t count;
            uint16_t capacity;
            uint16_t index;
        };

        SelData props;
        SelData methods;

        uint8_t nameLength;
        char name[1];
    };

    Entry* loadEntry(uint id);

    static void InitializeSelData(Entry::SelData &data);

    std::vector<Entry*> m_table;
    uint m_newMethodSel;
};

END_NAMESPACE_SCI

#endif // !_SelectorTable_HPP_
