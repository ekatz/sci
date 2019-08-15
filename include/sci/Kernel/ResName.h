#ifndef RESNAME_H
#define RESNAME_H

#include "Types.h"

#define MAXMASKS 10

#ifndef __MACTYPES__
typedef struct ResType {
    const char *name;
    const char *defaultMask;
    char       *masks[MAXMASKS];
} ResType;

extern ResType g_resTypes[];
#endif

char *ResNameMake(char *dest, int resType, size_t resNum);
char *ResNameMakeWildCard(char *dest, int resType);
const char *ResName(int resType);

// Open a resource file using any of the resource's masks.
// If successful, copies the full name to 'name', else nulls it
int ROpenResFile(int resType, size_t resNum, char *name);

char *addSlash(char *dir);

#endif // RESNAME_H
