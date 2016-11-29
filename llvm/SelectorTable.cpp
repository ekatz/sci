#include "SelectorTable.hpp"
#include "World.hpp"
#include "Script.hpp"
#include "Object.hpp"
#include "Method.hpp"
#include "Property.hpp"
#include "Resource.hpp"
#include <llvm/IR/Instructions.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


SelectorTable::SelectorTable() :
    m_table(512),
    m_newMethodSel(selector_cast<uint>(-1))
{
}


SelectorTable::~SelectorTable()
{
    for (auto i = m_table.begin(), e = m_table.end(); i != e; ++i)
    {
        Entry *entry = *i;
        if (entry != nullptr)
        {
            if (entry->props.count > 1)
            {
                delete[] entry->props.sels;
            }
            if (entry->methods.count > 1)
            {
                delete[] entry->methods.sels;
            }
            delete[] reinterpret_cast<char*>(entry);
        }
    }
}


size_t SelectorTable::size() const
{
    for (size_t sz = m_table.size(); sz != 0; --sz)
    {
        if (m_table[sz - 1] != nullptr)
        {
            return sz;
        }
    }
    return 0;
}


StringRef SelectorTable::getSelectorName(uint id) const
{
    const Entry *entry = m_table[id];
    return (entry != nullptr) ? StringRef(entry->name, entry->nameLength) : StringRef();
}


ArrayRef<Method *> SelectorTable::getMethods(uint id) const
{
    ArrayRef<void *> sels = getSelectors(id, true);
    return makeArrayRef(reinterpret_cast<Method * const *>(sels.data()), sels.size());
}


ArrayRef<Property *> SelectorTable::getProperties(uint id) const
{
    ArrayRef<void *> sels = getSelectors(id, false);
    return makeArrayRef(reinterpret_cast<Property * const *>(sels.data()), sels.size());
}


ArrayRef<void *> SelectorTable::getSelectors(uint selector, bool isMethod) const
{
    void * const *sels = nullptr;
    uint count = 0;
    const Entry *entry = m_table[selector];
    if (entry != nullptr)
    {
        const Entry::SelData &data = isMethod ? entry->methods : entry->props;
        count = data.count;
        if (count == 1)
        {
            sels = reinterpret_cast<void * const *>(&data.singleSel);
        }
        else
        {
            sels = reinterpret_cast<void * const *>(data.sels);
        }
    }
    return makeArrayRef(sels, count);
}


bool SelectorTable::addProperty(Property &prop, uint index)
{
    return addSelector(&prop, prop.getSelector(), index, false);
}


bool SelectorTable::addMethod(Method &method, uint vftblIndex)
{
    return addSelector(&method, method.getSelector(), vftblIndex, true);
}


bool SelectorTable::addSelector(void *ptr, uint selector, uint index, bool isMethod)
{
    Entry *entry = loadEntry(selector);
    Entry::SelData &data = isMethod ? entry->methods : entry->props;

    if (data.index == (uint16_t)-1)
    {
        data.index = index;
    }
    else if (data.index != index)
    {
#if 0
        if (data.index != (uint16_t)-2)
        {
            if (isMethod)
                printf("%s [%d -> %d]\n", getSelectorName(selector).data(), data.index, index);
        }
#endif
        data.index = (uint16_t)-2;
    }

    if (data.count == 0)
    {
        data.singleSel = ptr;
        data.count = 1;
        return true;
    }

    if (data.count == 1)
    {
        if (data.singleSel == ptr)
        {
            return false;
        }

        void *singleSel = data.singleSel;
        data.capacity = 8;
        data.sels = new void*[data.capacity];
        data.sels[0] = singleSel;
    }
    else
    {
        for (uint i = 0, n = data.count; i < n; ++i)
        {
            if (data.sels[i] == ptr)
            {
                return false;
            }
        }

        if ((data.count + 1) > data.capacity)
        {
            void **oldSels = data.sels;
            data.capacity += 8;
            data.sels = new void*[data.capacity];
            memcpy(data.sels, oldSels, data.count * sizeof(void*));
            delete[] oldSels;
        }
    }
    data.sels[data.count++] = ptr;
    return true;
}


SelectorTable::Entry* SelectorTable::loadEntry(uint id)
{
    if (id == selector_cast<uint>(-1))
    {
        return nullptr;
    }

    if (m_table.size() <= id)
    {
        m_table.resize(id + 1);
    }

    if (!m_table[id])
    {
        char selName[32];
        if (GetFarStr(SELECTOR_VOCAB, id, selName) == nullptr)
        {
            sprintf(selName, "sel@%u", id);
        }
        size_t nameLen = strlen(selName);

        if (m_newMethodSel == selector_cast<uint>(-1))
        {
            if (nameLen == 3 && memcmp(selName, "new", 3) == 0)
            {
                m_newMethodSel = id;
            }
        }

        char *buf = new char[sizeof(Entry) + nameLen];
        Entry *entry = reinterpret_cast<Entry*>(buf);

        InitializeSelData(entry->props);
        InitializeSelData(entry->methods);

        entry->nameLength = static_cast<uint8_t>(nameLen);
        memcpy(entry->name, selName, nameLen);
        entry->name[nameLen] = '\0';

        m_table[id] = entry;
    }
    return m_table[id];
}


void SelectorTable::InitializeSelData(Entry::SelData &data)
{
    data.sels = nullptr;
    data.count = 0;
    data.capacity = 0;
    data.index = (uint16_t)-1;
}


END_NAMESPACE_SCI
