#include "sci/PMachine/Script.h"
#include "sci/PMachine/Object.h"
#include "sci/PMachine/PMachine.h"
#include "sci/Kernel/Resource.h"

#define NIL NullNode(Script)

#define HEAP_MAX ((uint)((uint16_t)-1) + 1U)

static List s_scriptList = LIST_INITIALIZER;
static byte s_scriptHeap[HEAP_MAX * HEAP_MUL] = { 0 };
static uint s_scriptHeapSize = 0;
static uint s_scriptHeapMap[1000] = { 0 };

static Handle GetHeapHandle(uint size, uint num);
static void DisposeHeapHandle(Handle handle);

// Return a pointer to the node for script n if it is in the script list,
// or NULL if it is not in the list.
static Script *FindScript(uint num);

// Remove all classes belonging to script number n from the class table.
static void TossScriptClasses(uint num);

// Scan through the script unmarking the objects.
static void TossScriptObjects(Handle heap);

static void TossScript(Script *script, bool checkClones);
static void InitHunkRes(Handle hunk, Script *script, bool alloc);
static void DoFixups(Script      *script,
                     RelocTable  *relocTable,
                     uint         numRelocEntries,
                     ExportTable *exportTable,
                     uint         numExports);
static void FixRelocTable(SegHeader  *seg,
                          byte       *hunk,
                          byte       *heap,
                          uint        heapOffset,
                          RelocTable *relocTable,
                          bool       *fixDone);
static void FixExportsTable(SegHeader   *seg,
                            byte        *hunk,
                            byte        *heap,
                            uint         heapOffset,
                            ExportTable *exportTable,
                            bool        *fixDone);

void InitScripts(void)
{
    InitList(&s_scriptList);
}

Script *ScriptPtr(uint num)
{
    Script *script;

    script = FindScript(num);
    if (script == NULL) {
        script = LoadScript(num);
    }

    return script;
}

Script *LoadScript(uint num)
{
    Script *script;
    Handle  hunk;

    hunk = ResLoad(RES_SCRIPT, num);
    if (hunk == NULL) {
        return NULL;
    }
    ResLock(RES_SCRIPT, num, true);

    script = (Script *)malloc(sizeof(Script));
    memset(script, 0, sizeof(Script));
    AddKeyToFront(&s_scriptList, ToNode(script), num);

    InitHunkRes(hunk, script, true);
    if (script->text) {
        ResLoad(RES_TEXT, num);
    }
    return script;
}

void DisposeAllScripts(void)
{
    Script *script;

    while ((script = FromNode(FirstNode(&s_scriptList), Script)) != NIL) {
        TossScript(script, false);
    }
}

void DisposeScript(uint num)
{
    Script *script;

    script = FindScript(num);
    if (script != NULL) {
        TossScript(script, true);
    }
}

static void TossScript(Script *script, bool checkClones)
{
    uint num = script->num;

    // Dispose the heap resource.
    TossScriptClasses(num);

    if (script->heap != NULL) {
        TossScriptObjects(script->heap);
        DisposeHeapHandle(script->heap);
    }

    if (checkClones && script->clones != 0) {
        PError(PE_LEFT_CLONE, num, 0);
    }

    script->num = 9999;
    DeleteNode(&s_scriptList, ToNode(script));
    free(script);
}

static Script *FindScript(uint num)
{
    return FromNode(FindKey(&s_scriptList, (intptr_t)num), Script);
}

static void InitHunkRes(Handle hunk, Script *script, bool alloc)
{
    script->hunk = hunk;

    SegHeader   *seg;
    byte        *heap = NULL;
    size_t       heapLen;
    RelocTable  *relocTable      = NULL;
    uint         numRelocEntries = 0;
    ExportTable *exportTable     = NULL;
    uint         numExports      = 0;
    uint         i, n;

    heapLen = 0;
    seg     = (SegHeader *)hunk;
    while (seg->type != SEG_NULL) {
        switch (seg->type) {
            case SEG_OBJECT:
            case SEG_CLASS:
                heapLen += OBJSIZE(((ObjRes *)(seg + 1))->varSelNum);
                break;

            case SEG_SAIDSPECS:
            case SEG_STRINGS:
                heapLen += seg->size - sizeof(SegHeader);
                break;

            case SEG_LOCALS:
                heapLen += (seg->size - sizeof(SegHeader)) / sizeof(uint16_t) *
                           sizeof(uintptr_t);
                break;

            case SEG_EXPORTS:
                exportTable = (ExportTable *)(seg + 1);
                numExports  = exportTable->numEntries;
                break;

            case SEG_RELOC:
                relocTable      = (RelocTable *)(seg + 1);
                numRelocEntries = relocTable->numEntries;
                break;

            default:
                break;
        }

        seg = NextSegment(seg);
    }

    if (alloc) {
        if (heapLen != 0) {
            heap = (byte *)GetHeapHandle(heapLen, script->num);
        }
        script->heap = heap;
    } else {
        heap = (byte *)script->heap;
    }

    seg = (SegHeader *)hunk;
    while (seg->type != SEG_NULL) {
        switch (seg->type) {
            case SEG_CLASS:
                g_classTbl[((ObjRes *)(seg + 1))->speciesSel].obj =
                  (Obj *)(heap + sizeof(ObjHeader));
            case SEG_OBJECT:
                if (alloc) {
                    Obj       *obj;
                    ObjRes    *cls       = (ObjRes *)(seg + 1);
                    ObjHeader *objHeader = (ObjHeader *)heap;
                    objHeader->magic     = cls->magic;
                    objHeader->script    = script;
                    objHeader->funcSelList =
                      (ObjID *)((byte *)(cls->sels) + cls->funcSelOffset);
                    objHeader->scriptSuper = script;
                    objHeader->varSelNum   = cls->varSelNum;

                    obj = (Obj *)(objHeader + 1);

                    // SEG_CLASS: List of Selector ID of varselectors
                    // SEG_OBJECT: Pointer to Number of function selectors
                    // (function selectors list header)
                    obj->props = (ObjID *)cls->sels + cls->varSelNum;

                    for (i = 2, n = cls->varSelNum; i < n; ++i) {
                        obj->vars[i] = (uintptr_t)cls->sels[i];
                    }

                    obj->super = GetClass(cls->superSel);

                    if (seg->type == SEG_OBJECT) {
                        Obj *objSuper = obj->super;
                        if (objSuper != NULL) {
                            objHeader->scriptSuper =
                              OBJHEADER(objSuper)->script;
                            obj->props = objSuper->props;
                        }
                    }
                }

                heap += OBJSIZE(((ObjRes *)(seg + 1))->varSelNum);
                break;

            case SEG_LOCALS:
                script->vars = (uintptr_t *)heap;
                if (alloc) {
                    uint16_t *segVars = (uint16_t *)(seg + 1);
                    n = (seg->size - sizeof(SegHeader)) / sizeof(uint16_t);
                    for (i = 0; i < n; ++i) {
                        script->vars[i] = segVars[i];
                    }
                }

                heap += (seg->size - sizeof(SegHeader)) / sizeof(uint16_t) *
                        sizeof(uintptr_t);
                break;

            case SEG_SAIDSPECS:
            case SEG_STRINGS:
                if (alloc) {
                    memcpy(heap, seg + 1, seg->size - sizeof(SegHeader));
                }

                heap += seg->size - sizeof(SegHeader);
                break;

            case SEG_SYNONYMS:
                script->synonyms = seg;
                break;

            case SEG_TEXT:
                script->text = true;
                break;

            case SEG_EXPORTS:
                script->exports = (ExportTable *)(seg + 1);
                break;

            default:
                break;
        }

        seg = NextSegment(seg);
    }

    DoFixups(script, relocTable, numRelocEntries, exportTable, numExports);

    if (relocTable != NULL) {
        // TODO: Should this be a pointer or an offset?
        //  relocTable->offset = heap;
    }
}

static void DoFixups(Script      *script,
                     RelocTable  *relocTable,
                     uint         numRelocEntries,
                     ExportTable *exportTable,
                     uint         numExports)
{
    uint       alignedNumExports, patchSize, heapPos = 0;
    bool      *exportTableFixes;
    bool      *relocTableFixes;
    byte      *hunk;
    byte      *heap;
    SegHeader *seg;

    alignedNumExports = ALIGN_UP(numExports, 2);
    patchSize         = alignedNumExports + ALIGN_UP(numRelocEntries, 2);

    exportTableFixes = (bool *)alloca(patchSize * sizeof(bool));
    relocTableFixes  = exportTableFixes + alignedNumExports;

    memset(exportTableFixes, 0, patchSize);

    hunk = (byte *)script->hunk;
    heap = (byte *)script->heap;
    seg  = (SegHeader *)hunk;
    while (seg->type != SEG_NULL) {
        switch (seg->type) {
            case SEG_OBJECT:
            case SEG_CLASS:
                FixRelocTable(
                  seg, hunk, heap, heapPos, relocTable, relocTableFixes);
                FixExportsTable(
                  seg, hunk, heap, heapPos, exportTable, exportTableFixes);
                heapPos += OBJSIZE(((ObjRes *)(seg + 1))->varSelNum);
                break;

            case SEG_SAIDSPECS:
            case SEG_STRINGS:
                FixRelocTable(
                  seg, hunk, heap, heapPos, relocTable, relocTableFixes);
                FixExportsTable(
                  seg, hunk, heap, heapPos, exportTable, exportTableFixes);
                heapPos += seg->size - sizeof(SegHeader);
                break;

            case SEG_LOCALS:
                FixRelocTable(
                  seg, hunk, heap, heapPos, relocTable, relocTableFixes);
                FixExportsTable(
                  seg, hunk, heap, heapPos, exportTable, exportTableFixes);
                heapPos += (seg->size - sizeof(SegHeader)) / sizeof(uint16_t) *
                           sizeof(uintptr_t);
                break;

            default:
                break;
        }

        seg = NextSegment(seg);
    }
}

static SegHeader *FindSegment(byte *hunk, byte **heap, void *fixPtr)
{
    SegHeader *seg;

    seg = (SegHeader *)hunk;
    while (seg->type != SEG_NULL) {
        if ((uintptr_t)fixPtr < ((uintptr_t)seg + seg->size)) {
            assert((uintptr_t)fixPtr > (uintptr_t)seg);
            return seg;
        }

        switch (seg->type) {
            case SEG_OBJECT:
            case SEG_CLASS:
                *heap += OBJSIZE(((ObjRes *)(seg + 1))->varSelNum);
                break;

            case SEG_SAIDSPECS:
            case SEG_STRINGS:
                *heap += seg->size - sizeof(SegHeader);
                break;

            case SEG_LOCALS:
                *heap += (seg->size - sizeof(SegHeader)) / sizeof(uint16_t) *
                         sizeof(uintptr_t);
                break;

            default:
                break;
        }

        seg = NextSegment(seg);
    }
    return seg;
}

static const uint8_t s_lofsOpcodeModifiers[] = { 0, 1, 5, 9 };

static void FixRelocPtr(byte *hunk, byte *heap, void *fixPtr, uint fixedValue)
{
    SegHeader *seg;

    seg = FindSegment(hunk, &heap, fixPtr);
    switch (seg->type) {
        case SEG_CODE: {
            uint16_t truncValue = (uint16_t)(fixedValue / HEAP_MUL);

            *((uint8_t *)fixPtr - 1) +=
              s_lofsOpcodeModifiers[fixedValue % HEAP_MUL];
            *(uint16_t *)fixPtr = truncValue;
            break;
        }

        case SEG_OBJECT:
        case SEG_CLASS: {
            ObjRes *cls = (ObjRes *)(seg + 1);
            Obj    *obj = (Obj *)((ObjHeader *)heap + 1);
            size_t  i   = (int16_t *)fixPtr - cls->sels;

            assert(i < (size_t)cls->varSelNum);
            obj->vars[i] = fixedValue;
            break;
        }

        default:
            break;
    }
}

static void FixRelocTable(SegHeader  *seg,
                          byte       *hunk,
                          byte       *heap,
                          uint        heapOffset,
                          RelocTable *relocTable,
                          bool       *fixDone)
{
    byte     *segBegin;
    byte     *segEnd;
    byte     *ptr;
    byte     *heapEntry;
    byte     *fixAdjust;
    uint16_t *entry;
    uint16_t *fixPtr;
    uint16_t  relPtr;
    uint      i, n;

    if (NULL == relocTable) {
        return;
    }

    segBegin  = (byte *)(seg + 1);
    segEnd    = (byte *)seg + seg->size;
    fixAdjust = (relocTable->ptrSeg != 0) ? (hunk + relocTable->ptrSeg) : NULL;
    entry     = relocTable->table;
    for (i = 0, n = relocTable->numEntries; i < n; ++i) {
        relPtr = *entry;
        fixPtr = (uint16_t *)(hunk + relPtr);
        relPtr = *fixPtr;
        ptr    = hunk + relPtr;

        if (fixAdjust != NULL) {
            ptr -= (fixAdjust - segBegin) + heapOffset;
            if (seg->type == SEG_OBJECT || seg->type == SEG_CLASS) {
                ptr -= sizeof(ObjHeader) - offsetof(ObjRes, sels);
            }
        }

        // If the pointer is inside the segment and we have not fixed it yet.
        if (!*fixDone && segBegin <= ptr && ptr < segEnd) {
            // Mark as "Fixed".
            *fixDone = true;

            heapEntry = heap + heapOffset;
            heapEntry += ptr - segBegin;
            if (seg->type == SEG_OBJECT || seg->type == SEG_CLASS) {
                heapEntry += sizeof(ObjHeader) - offsetof(ObjRes, sels);
            }

            // Make the pointer relative to the heap instead of the hunk.
            FixRelocPtr(hunk, heap, fixPtr, (uint)(heapEntry - s_scriptHeap));
        }

        ++fixDone;
        ++entry;
    }
}

static void FixExportsTable(SegHeader   *seg,
                            byte        *hunk,
                            byte        *heap,
                            uint         heapOffset,
                            ExportTable *exportTable,
                            bool        *fixDone)
{
    byte             *segBegin;
    byte             *segEnd;
    byte             *ptr;
    byte             *heapEntry;
    byte             *fixAdjust;
    ExportTableEntry *entry;
    uint              i, n;

    if (NULL == exportTable) {
        return;
    }

    segBegin = (byte *)(seg + 1);
    segEnd   = (byte *)seg + seg->size;
    entry    = exportTable->entries;
    for (i = 0, n = exportTable->numEntries; i < n; ++i) {
        ptr = (byte *)(hunk + entry->ptrOff);

        if (entry->ptrSeg != 0) {
            fixAdjust = hunk + entry->ptrSeg;
            ptr -= (fixAdjust - segBegin) + heapOffset;
            if (seg->type == SEG_OBJECT || seg->type == SEG_CLASS) {
                ptr -= sizeof(ObjHeader) - offsetof(ObjRes, sels);
            }
        }

        // If the pointer is inside the segment and we have not fixed it yet.
        if (!*fixDone && segBegin <= ptr && ptr < segEnd) {
            // Mark as "Fixed".
            *fixDone = true;

            heapEntry = heap + heapOffset;
            heapEntry += ptr - segBegin;
            if (seg->type == SEG_OBJECT || seg->type == SEG_CLASS) {
                heapEntry += sizeof(ObjHeader) - offsetof(ObjRes, sels);
            }

            // Make the pointer relative to the heap instead of the hunk.
            entry->ptr = (uint32_t)(heapEntry - s_scriptHeap);
        }

        ++fixDone;
        ++entry;
    }
}

static void FixTable(SegHeader *seg,
                     byte      *hunk,
                     byte      *heap,
                     uint       heapOffset,
                     uint16_t  *table)
{
    byte     *segBegin;
    byte     *segEnd;
    byte     *ptr;
    byte     *heapEntry;
    uint16_t *entry;
    uint16_t *fixPtr;
    uint16_t  tableType;
    uint      i, numEntries;

    if (NULL == table) {
        return;
    }

    segBegin   = (byte *)(seg + 1);
    segEnd     = (byte *)seg + seg->size;
    tableType  = ((SegHeader *)table - 1)->type;
    numEntries = *table;
    entry      = table + 1;
    for (i = 0; i < numEntries; ++i) {
        fixPtr = entry;

        // If this is not an export table.
        if (tableType != SEG_EXPORTS) {
            fixPtr = (uint16_t *)(hunk + *entry);
        }

        ptr = (byte *)(hunk + *fixPtr);

        // If the pointer is inside the segment.
        if (segBegin <= ptr && ptr < segEnd) {
            heapEntry = heap + heapOffset;
            heapEntry += ptr - segBegin;
            if (seg->type == SEG_OBJECT || seg->type == SEG_CLASS) {
                heapEntry += sizeof(ObjHeader) - offsetof(ObjRes, sels);
            }

            // Make the pointer relative to the heap instead of the hunk.
            *fixPtr = (uint16_t)(heapEntry - heap);
        }

        ++entry;
    }
}

static void TossScriptClasses(uint num)
{
    uint i, n;

    for (i = 0, n = g_numClasses; i < n; ++i) {
        if (g_classTbl[i].scriptNum == num) {
            g_classTbl[i].obj = 0;
        }
    }
}

static void TossScriptObjects(Handle heap)
{
    uint16_t *heapCurr = (uint16_t *)heap;
    uint16_t *heapEnd  = (uint16_t *)((byte *)heap + ResHandleSize(heap));
    while (heapCurr < heapEnd) {
        // TODO: This is incorrect, as this value can mean other things as well,
        // especially in 32-bit!
        if (*heapCurr == OBJID) {
            *heapCurr = 0;
        }

        ++heapCurr;
    }
}

static Handle GetHeapHandle(uint size, uint num)
{
    uint32_t *handle;

    if (s_scriptHeapMap[num] == 0) {
        s_scriptHeapMap[num] = s_scriptHeapSize;
        handle  = (uint32_t *)(s_scriptHeap + s_scriptHeapSize);
        *handle = size;
        handle++;

        s_scriptHeapSize +=
          (uint)(sizeof(uint32_t) + ALIGN_UP(size, sizeof(uint32_t)));
        assert(s_scriptHeapSize <= sizeof(s_scriptHeap));
    } else {
        handle = (uint32_t *)(s_scriptHeap + s_scriptHeapMap[num]);
        handle++;
        assert(size == ResHandleSize(handle));
    }
    return handle;
}

static void DisposeHeapHandle(Handle handle)
{
    if (handle != NULL) {
        memset(handle, 0, ResHandleSize(handle));
    }
}

byte *GetScriptHeapPtr(size_t offset)
{
    return offset < sizeof(s_scriptHeap) ? s_scriptHeap + offset
                                         : (byte *)offset;
}
