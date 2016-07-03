#ifndef FILEIO_H
#define FILEIO_H

#include "Types.h"

enum fileFuncs {
    fileOpen,
    fileClose,
    fileRead,
    fileWrite,
    fileUnlink,
    fileFGets,
    fileFPuts,
    fileSeek,
    fileFindFirst,
    fileFindNext,
    fileExists,
    fileRename,
    fileCopy
};

// Definitions for various file opening modes.
#define F_APPEND 0
#define F_READ   1
#define F_TRUNC  2

typedef struct DirEntry {
    uint8_t  atr;
    uint64_t size;
    char     name[256];
} DirEntry;

// Attribute bits for 'atr'.
#define F_READONLY 0x01
#define F_HIDDEN   0x02
#define F_SYSTEM   0x04
#define F_LABEL    0x08
#define F_SUBDIR   0x10
#define F_DIRTY    0x20

char *fgets_no_eol(char *str, int len, int fd);

bool fexists(const char *filename);

int fcopy(const char *srcFilename, const char *dstFilename);

bool firstfile(const char *spec, uint atr, DirEntry *dta);

bool nextfile(DirEntry *dta);

#ifndef __WINDOWS__
long filelength(int fd);
#endif

#endif // FILEIO_H
