#ifndef SCI_PMACHINE_SCRIPT_H
#define SCI_PMACHINE_SCRIPT_H

#include "sci/Utils/List.h"

#pragma warning(push)
#pragma warning(disable : 4200) // zero-sized array in struct/union
#pragma warning(disable : 4201) // nameless struct/union

typedef struct RelocTable {
    uint16_t numEntries;
    uint16_t ptrSeg;
    uint16_t table[0];
} RelocTable;

#pragma pack(push, 2)
typedef union ExportTableEntry {
    struct {
        uint16_t ptrOff;
        uint16_t ptrSeg;
    };
    uint32_t ptr;
} ExportTableEntry;
#pragma pack(pop)

typedef struct ExportTable {
    uint16_t         numEntries;
    ExportTableEntry entries[0];
} ExportTable;

typedef struct Script {
    Node         link;
    size_t       num;
    Handle       heap;
    Handle       hunk;
    ExportTable *exports;
    uintptr_t   *vars;
    void        *synonyms;
    bool         text;
    int          clones;
} Script;

typedef struct SegHeader {
    uint16_t type;
    uint16_t size;
} SegHeader;

typedef struct ObjRes {
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
} ObjRes;

#define SEG_NULL      0  // End of script resource
#define SEG_OBJECT    1  // Object
#define SEG_CODE      2  // Code
#define SEG_SYNONYMS  3  // Synonym word lists
#define SEG_SAIDSPECS 4  // Said specs
#define SEG_STRINGS   5  // Strings
#define SEG_CLASS     6  // Class
#define SEG_EXPORTS   7  // Exports
#define SEG_RELOC     8  // Relocation table
#define SEG_TEXT      9  // Preload text
#define SEG_LOCALS    10 // Local variables

#define NextSegment(seg) ((SegHeader *)((byte *)(seg) + (seg)->size))

#define HEAP_MUL (sizeof(void *) / sizeof(uint16_t))

byte *GetScriptHeapPtr(size_t offset);

// Initialize the list of loaded scripts.
void InitScripts(void);

// Return a pointer to the node for script n.
// Load the script if necessary.
Script *ScriptPtr(uint num);

// Load script number n in hunk space, copy it to the heap, allocate and
// clear its local variables, and return a pointer to its script node.
Script *LoadScript(uint num);

// Dispose of the entire script list (for restart game).
void DisposeAllScripts(void);

// Remove script n from the active script list and deallocate the space
// taken by its code and variables.
void DisposeScript(uint num);

#pragma warning(pop)

#endif // SCI_PMACHINE_SCRIPT_H
