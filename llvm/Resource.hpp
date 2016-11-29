#pragma once
#ifndef _Resource_HPP_
#define _Resource_HPP_

#include "Types.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include "../SCI/src/Resource.h"
#include "../SCI/src/FarData.h"


// Bits in the -info- property.
#define CLASSBIT    0x8000
#define CLONEBIT    0x0001
#define NODISPOSE   0x0002
#define NODISPLAY   0x0004  // Don't display in ShowObj()


#define SEG_NULL        0   // End of script resource
#define SEG_OBJECT      1   // Object
#define SEG_CODE        2   // Code
#define SEG_SYNONYMS    3   // Synonym word lists
#define SEG_SAIDSPECS   4   // Said specs
#define SEG_STRINGS     5   // Strings
#define SEG_CLASS       6   // Class
#define SEG_EXPORTS     7   // Exports
#define SEG_RELOC       8   // Relocation table
#define SEG_TEXT        9   // Preload text
#define SEG_LOCALS      10  // Local variables


#ifdef __WINDOWS__
#pragma warning(push)
#pragma warning(disable: 4200) // zero-sized array in struct/union
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
#endif


typedef struct SegHeader
{
    uint16_t type;
    uint16_t size;
} SegHeader;

#define NextSegment(seg)    ((SegHeader *)((byte *)(seg) + (seg)->size))


typedef struct ResClassEntry
{
    uint16_t obj;        // pointer to Obj
    uint16_t scriptNum;  // script number
} ResClassEntry;


typedef struct RelocTable
{
    uint16_t numEntries;
    uint16_t ptrSeg;
    uint16_t table[0];
} RelocTable;


typedef struct ExportTableEntry
{
    uint16_t ptrOff;
    uint16_t ptrSeg;
} ExportTableEntry;


typedef struct ExportTable
{
    uint16_t numEntries;
    ExportTableEntry entries[0];
} ExportTable;


struct ObjRes
{
    uint16_t magic;
    uint16_t localVarOffset;
    uint16_t funcSelOffset;
    uint16_t varSelNum;
    union {
        int16_t sels[0];

        struct {
            ObjID speciesSel;
            ObjID superSel; // This is the class selector for "OBJECT"
            ObjID infoSel;
            ObjID nameSel;
        };
    };


    enum {
        SPECIES_OFFSET,
        SUPER_OFFSET,
        INFO_OFFSET,
        NAME_OFFSET,
        VALUES_OFFSET
    };


    bool isClass() const {
        return (infoSel & CLASSBIT) != 0;
    }

    bool isObject() const {
        return !isClass();
    }

    uint getPropertyCount() const {
        return static_cast<uint>(varSelNum) - static_cast<uint>(ObjRes::INFO_OFFSET);
    }

    const int16_t* getPropertyValues() const {
        return sels + ObjRes::INFO_OFFSET;
    }

    const ObjID* getPropertySelectors() const {
        return reinterpret_cast<const ObjID *>(getPropertyValues() + varSelNum);
    }

    uint getMethodCount() const {
        const ObjID *funcs = reinterpret_cast<const ObjID *>(reinterpret_cast<const byte *>(sels) + funcSelOffset);
        return funcs[-1];
    }

    const ObjID* getMethodSelectors() const {
        const ObjID *funcs = reinterpret_cast<const ObjID *>(reinterpret_cast<const byte *>(sels) + funcSelOffset);
        return funcs;
    }

    const uint16_t* getMethodOffsets() const {
        const ObjID *funcs = reinterpret_cast<const ObjID *>(reinterpret_cast<const byte *>(sels) + funcSelOffset);
        return funcs + funcs[-1] + 1;
    }
};


#define OBJID  ((uint16_t)0x1234)


inline bool IsUnsignedValue(int16_t val)
{
    // If this seems like a flag, then it is unsigned.
    return ((((uint16_t)val & 0xE000) != 0xE000) && ((((uint16_t)val & 0xF000) | 0x8000) == (uint16_t)val));
}


inline uintptr_t WidenValue(int16_t val)
{
    return IsUnsignedValue(val) ?
        static_cast<uint64_t>(static_cast<uint16_t>(val)) :
        static_cast<uint64_t>(static_cast<int16_t>(val));
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


#ifdef __WINDOWS__
#pragma warning(pop)
#endif


#ifdef __cplusplus
} // extern "C"
#endif

#endif // !_Resource_HPP_
