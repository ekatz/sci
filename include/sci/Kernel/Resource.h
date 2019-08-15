#ifndef SCI_KERNEL_RESOURCE_H
#define SCI_KERNEL_RESOURCE_H

#include "sci/Kernel/ResTypes.h"
#include "sci/Utils/List.h"

// Special resource IDs
#define ALL_IDS ((size_t)(-1))

// Conditional execution (test or prod)
#define FILEBASED 0
#define VOLBASED  1

// Special vocab files containing strings/data need by the system
#define KERNEL_VOCAB   999
#define OPCODE_VOCAB   998
#define SELECTOR_VOCAB 997
#define CLASSTBL_VOCAB 996
#define HELP_VOCAB     995
#define PROPOFS_VOCAB  994

typedef struct LoadLink {
    Node    link;
    uint8_t type;
    bool    locked;
    size_t  num;
    Handle  data;
} LoadLink;

extern List g_loadList;

// Return handle to resource.
Handle ResLoad(int resType, size_t resNum);

// Release this resource.
void ResUnLoad(int resType, size_t resNum);

void ResLock(int resType, size_t resNum, bool yes);

// Search loadList for this type and num.
LoadLink *FindResEntry(int resType, size_t resNum);

// Return a Handle in concert with the Resource Manager.
Handle GetResHandle(uint size);

// Free the memory pointed to by the handle.
void DisposeResHandle(Handle handle);

Handle LoadHandle(const char *fileName);

// uint ResHandleSize(Handle handle);
//
// Return the size of the block pointed to by handle.
#define ResHandleSize(handle) (*(((uint32_t *)(handle)) - 1))

bool FindPatchEntry(int resType, size_t resNum);

void InitPatches(void);

#endif // SCI_KERNEL_RESOURCE_H
