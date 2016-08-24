#include "FileIO.h"
#ifndef __WINDOWS__
#include <copyfile.h>
#include <dirent.h>
#endif

char *fgets_no_eol(char *str, int len, int fd)
{
    int   count = 0;
    char  c;
    char *cp = str;

    // Account for trailing 0.
    --len;
    while (count < len) {
        if (read(fd, &c, 1) <= 0) {
            break;
        }
        count++;
        if (c == '\n') {
            break;
        }
        if (c == '\r') {
            continue;
        }
        *cp++ = c;
    };
    *cp = '\0';

    return (count != 0) ? str : NULL;
}

bool fexists(const char *filename)
{
#ifdef __WINDOWS__
    if (GetFileAttributesA(filename) == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        return !(err == ERROR_FILE_NOT_FOUND    ||
                 err == ERROR_PATH_NOT_FOUND    ||
                 err == ERROR_INVALID_NAME      ||
                 err == ERROR_INVALID_DRIVE     ||
                 err == ERROR_NOT_READY         ||
                 err == ERROR_INVALID_PARAMETER ||
                 err == ERROR_BAD_PATHNAME      ||
                 err == ERROR_BAD_NETPATH);
    }
    return true;
#else
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
#endif
}

int fcopy(const char *srcFilename, const char *dstFilename)
{
#ifdef __WINDOWS__
    int fd;
    int length;

    if (CopyFile(srcFilename, dstFilename, TRUE) == FALSE) {
        return -1;
    }

    if ((fd = open(dstFilename, O_RDONLY | O_BINARY)) == -1) {
        return -1;
    }
    length = (int)filelength(fd);
    close(fd);
    return length;
#else
    int input, output;
    int result;

    if ((input = open(srcFilename, O_RDONLY)) == -1) {
        return -1;
    }
    if ((output = open(dstFilename, O_RDWR | O_CREAT)) == -1) {
        close(input);
        return -1;
    }

    // Here we use kernel-space copying for performance reasons
#if defined(__APPLE__) || defined(__FreeBSD__)
    // fcopyfile works on FreeBSD and OS X 10.5+
    result = fcopyfile(input, output, 0, COPYFILE_ALL);
#else
    // sendfile will work with non-socket output (i.e. regular file) on
    // Linux 2.6.33+
    {
        off_t       bytesCopied = 0;
        struct stat fileinfo    = { 0 };
        fstat(input, &fileinfo);
        result = sendfile(output, input, &bytesCopied, fileinfo.st_size);
    }
#endif

    close(input);
    close(output);
    return result;
#endif
}

#ifdef __WINDOWS__
static HANDLE s_hFind = INVALID_HANDLE_VALUE;
#else
static DIR *s_dirp = NULL;
static char s_findSpec[256] = { 0 };

static bool MatchSpec(const char *str, const char *spec)
{
    char s, c;

    while (true) {
        s = *spec++;
        c = *str++;

        // If this is a wild card.
        if (s == '*') {
            // Skip following wild cards.
            do {
                s = *spec++;
            } while (s == '*');

            // If this is the last wild card then we have found a match!
            if (s == '\0') {
                return true;
            }

            // Match the next character in the spec.
            while (c != s) {
                c = *str++;

                // If we have reached the end of the string without matching the
                // spec.
                if (c == '\0') {
                    return false;
                }
            }
        }
        // If the string does not match.
        else if (c != s) {
            return false;
        }
        // If we have finished processing.
        else if (s == '\0') {
            return true;
        }
    }
}
#endif

bool firstfile(const char *spec, uint atr, DirEntry *dta)
{
#ifdef __WINDOWS__
    WIN32_FIND_DATAA findFileData;

    if (s_hFind != INVALID_HANDLE_VALUE) {
        FindClose(s_hFind);
    }
    s_hFind = FindFirstFileA(spec, &findFileData);
    if (s_hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    strncpy(dta->name, findFileData.cFileName, ARRAYSIZE(dta->name) - 1);
    dta->name[ARRAYSIZE(dta->name) - 1] = '\0';
    dta->atr  = (uint8_t)findFileData.dwFileAttributes;
    dta->size = ((uint64_t)findFileData.nFileSizeHigh << 32) |
                (uint64_t)findFileData.nFileSizeLow;
    return true;
#else
    char path[260];
    const char *cp;
    size_t specLen, pathLen;

    if (s_dirp != NULL) {
        closedir(s_dirp);
        s_dirp = NULL;
    }

    pathLen = 0;
    if (*spec != '/') {
        path[0] = '.';
        path[1] = '/';
        pathLen = 2;
    }

    cp = spec;
    specLen = strlen(spec);
    spec += specLen;
    while (cp != spec) {
        char c = *(--spec);
        if (c == '/' || c == '\\') {
            specLen -= (spec - cp) + 1;
            while (cp != spec) {
                c = *cp++;
                if (c == '\\') {
                    c = '/';
                }
                path[pathLen++] = c;
            }
            ++spec;
            break;
        }
    }
    path[pathLen] = '\0';

    if (specLen == 0) {
        return false;
    }
    memcpy(s_findSpec, spec, specLen);
    s_findSpec[specLen] = '\0';

    s_dirp = opendir(path);
    return nextfile(dta);
#endif
}

bool nextfile(DirEntry *dta)
{
#ifdef __WINDOWS__
    WIN32_FIND_DATAA findFileData;

    if (s_hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (FindNextFileA(s_hFind, &findFileData) == FALSE) {
        FindClose(s_hFind);
        s_hFind = INVALID_HANDLE_VALUE;
        return false;
    }

    strncpy(dta->name, findFileData.cFileName, ARRAYSIZE(dta->name) - 1);
    dta->name[ARRAYSIZE(dta->name) - 1] = '\0';
    dta->atr  = (uint8_t)findFileData.dwFileAttributes;
    dta->size = ((uint64_t)findFileData.nFileSizeHigh << 32) |
                (uint64_t)findFileData.nFileSizeLow;
    return true;
#else
    struct dirent entry;
    struct dirent *endp;

    if (s_dirp == NULL) {
        return false;
    }

    do {
        if (readdir_r(s_dirp, &entry, &endp) != 0 || endp == NULL) {
            closedir(s_dirp);
            s_dirp = NULL;
            return false;
        }
    } while (!MatchSpec(dta->name, s_findSpec));

    strncpy(dta->name, entry.d_name, ARRAYSIZE(dta->name) - 1);
    dta->name[ARRAYSIZE(dta->name) - 1] = '\0';
    dta->size = 0;
    dta->atr = 0;
    if (entry.d_type == DT_DIR) {
        dta->atr |= F_SUBDIR;
    }
    return true;
#endif
}

#ifndef __WINDOWS__
long filelength(int fd)
{
    struct stat stat_buf;
    int         rc = fstat(fd, &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}
#endif
