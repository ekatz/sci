#include "sci/Kernel/Resource.h"
#include "sci/Kernel/ResName.h"
#include "sci/Kernel/VolLoad.h"
#include "sci/Utils/FileIO.h"

#define NIL NullNode(LoadLink)

typedef struct ResPatchEntry {
    uint8_t  resType;
    uint16_t resNum;
} ResPatchEntry;

List g_loadList = LIST_INITIALIZER;

static Handle s_patches = NULL;

Handle ResLoad(int resType, size_t resNum)
{
    LoadLink *scan = NULL;

    if (resType == RES_MEM) {
        scan = (LoadLink *)malloc(sizeof(LoadLink));
        if (NULL == scan) {
            return NULL;
        }

        // If resType == RES_MEM, then resNum is size of MEMORY needed.
        scan->data = GetResHandle((uint)resNum);
        if (NULL == scan->data) {
            free(scan);
            return NULL;
        }

        scan->locked = true;

        // resNum now becomes the pointer for disposal purposes
        resNum = (size_t)scan->data;
    } else {
        scan = FindResEntry(resType, resNum);
        if (NULL != scan) {
            MoveToFront(&g_loadList, ToNode(scan));
            return scan->data;
        }

        scan = (LoadLink *)malloc(sizeof(LoadLink));
        if (NULL == scan) {
            return NULL;
        }

        scan->locked = false;
        scan->data   = DoLoad(resType, resNum);
    }

    scan->type = (uint8_t)resType;
    scan->num  = resNum;
    AddToFront(&g_loadList, ToNode(scan));
    return scan->data;
}

void ResUnLoad(int resType, size_t resNum)
{
    LoadLink *scan;
    LoadLink *tmp;

    if (resNum != ALL_IDS) {
        scan = FindResEntry(resType, resNum);
        if (scan != NULL) {
            // Free the data from standard memory.
            if (scan->data != NULL) {
                DisposeResHandle(scan->data);
            }

            // Delete the node and dispose of node memory.
            DeleteNode(&g_loadList, ToNode(scan));
            free((Handle)scan);
        }
    } else {
        for (scan = FromNode(FirstNode(&g_loadList), LoadLink); scan != NIL;
             scan = tmp) {
            tmp = FromNode(NextNode(ToNode(scan)), LoadLink);
            if (scan->type == resType) {
                ResUnLoad(resType, scan->num);
            }
        }
    }
}

void ResLock(int resType, size_t resNum, bool yes)
{
    LoadLink *scan;

    if (resNum != ALL_IDS) {
        scan = FindResEntry(resType, resNum);
        if (scan != NULL) {
            scan->locked = yes;
        }
    } else {
        for (scan = FromNode(FirstNode(&g_loadList), LoadLink); scan != NIL;
             scan = FromNode(NextNode(ToNode(scan)), LoadLink)) {
            if (scan->type == resType) {
                scan->locked = yes;
            }
        }
    }
}

LoadLink *FindResEntry(int resType, size_t resNum)
{
    LoadLink *scan;

    for (scan = FromNode(FirstNode(&g_loadList), LoadLink); scan != NIL;
         scan = FromNode(NextNode(ToNode(scan)), LoadLink)) {
        // if type & num match we break.
        if (scan->type == resType && scan->num == resNum) {
            break;
        }
    }
    return scan;
}

Handle GetResHandle(uint size)
{
    uint32_t *handle;

    handle = (uint32_t *)malloc(sizeof(uint32_t) + size);
    if (handle == NULL) {
        return NULL;
    }

    *handle = size;
    return handle + 1;
}

void DisposeResHandle(Handle handle)
{
    if (handle != NULL) {
        free((uint32_t *)handle - 1);
    }
}

Handle LoadHandle(const char *fileName)
{
    Handle theHandle;
    size_t len;
    int    fd;

    // Open the file.
    if ((fd = fileopen(fileName, O_RDONLY)) == -1) {
        return NULL;
    }

    // Get a handle to storage for it.
    len       = (size_t)filelength(fd);
    theHandle = GetResHandle(len);
    read(fd, theHandle, len);
    close(fd);

    return theHandle;
}

bool FindPatchEntry(int resType, size_t resNum)
{
    if (NULL != s_patches) {
        ResPatchEntry *entry;
        for (entry = (ResPatchEntry *)s_patches; entry->resType != 0; ++entry) {
            if ((int)entry->resType == resType &&
                (size_t)entry->resNum == resNum) {
                return true;
            }
        }
    }
    return false;
}

void InitPatches(void)
{
    int            npatches = 0;
    int            resType;
    char           fileName[64];
    ResPatchEntry *entry;
    DirEntry       fileInfo;

    for (resType = RES_BASE; resType < (RES_BASE + NRESTYPES); ++resType) {
        ResNameMakeWildCard(fileName, resType);
        if (firstfile(fileName, 0, &fileInfo)) {
            do {
                if ((fileInfo.atr & F_SUBDIR) == 0 &&
                    isdigit(fileInfo.name[0])) {
                    npatches++;
                }
            } while (nextfile(&fileInfo));
        }
    }

    if (0 == npatches) {
        return;
    }

    s_patches = ResLoad(RES_MEM, (npatches + 1) * sizeof(ResPatchEntry));
    entry     = (ResPatchEntry *)s_patches;

    for (resType = RES_BASE; resType < (RES_BASE + NRESTYPES); ++resType) {
        ResNameMakeWildCard(fileName, resType);
        if (firstfile(fileName, 0, &fileInfo)) {
            do {
                if ((fileInfo.atr & F_SUBDIR) == 0 &&
                    isdigit(fileInfo.name[0])) {
                    size_t         resNum;
                    ResPatchEntry *tEntry;

                    resNum = (size_t)atoi(fileInfo.name);

                    // If the entry already exists, do not include it.
                    for (tEntry = (ResPatchEntry *)s_patches; tEntry != entry;
                         ++tEntry) {
                        if ((int)tEntry->resType == resType &&
                            (size_t)tEntry->resNum == resNum) {
                            --npatches;
                            break;
                        }
                    }

                    if (tEntry == entry) {
                        entry->resType = (uint8_t)resType;
                        entry->resNum  = (uint16_t)resNum;
                        ++entry;
                    }
                }
            } while (nextfile(&fileInfo));
        }
    }

    entry->resType = 0;
    // ReallocateHandle(patches, (npatches + 1) * sizeof(ResPatchEntry));
}
