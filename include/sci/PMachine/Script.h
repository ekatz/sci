#ifndef SCI_PMACHINE_SCRIPT_H
#define SCI_PMACHINE_SCRIPT_H

#include "sci/Utils/List.h"

#pragma warning(push)
#pragma warning(disable : 4200) // zero-sized array in struct/union

typedef struct RelocTable {
    uint16_t numEntries;
    uint16_t ptrSeg;
    uint16_t table[0];
} RelocTable;

typedef struct ExportTableEntry {
    uint16_t ptrOff;
    uint16_t ptrSeg;
} ExportTableEntry;

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

extern byte g_scriptHeap[];

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
