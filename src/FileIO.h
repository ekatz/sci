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

char *fgets_no_eol(char *str, int len, int fd);

bool fexists(const char *filename);

int fcopy(const char *srcFilename, const char *dstFilename);

#endif // FILEIO_H
