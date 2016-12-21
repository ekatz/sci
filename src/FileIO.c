#include "FileIO.h"
#include "Path.h"
#ifndef __WINDOWS__
#include <copyfile.h>
#include <dirent.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

int fileopen(const char *name, int mode)
{
    int  fd;
    char path[PATH_MAX];

    if (mode == O_RDONLY) {
        DosToLocalPath(path, name, true);
        fd = open(path, mode | O_BINARY);
        if (fd != -1) {
            return fd;
        }
    }

    DosToLocalPath(path, name, false);
    return open(path, mode | O_BINARY);
}

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
    char srcPath[PATH_MAX];
    char dstPath[PATH_MAX];
#ifdef __WINDOWS__
    int fd;
    int length;

    DosToLocalPath(srcPath, srcFilename, false);
    DosToLocalPath(dstPath, dstFilename, false);

    if (CopyFile(srcPath, dstPath, TRUE) == FALSE) {
        return -1;
    }

    if ((fd = open(dstPath, O_RDONLY | O_BINARY)) == -1) {
        return -1;
    }
    length = (int)filelength(fd);
    close(fd);
    return length;
#else
    int input, output;
    int result;

    DosToLocalPath(srcPath, srcFilename, false);
    DosToLocalPath(dstPath, dstFilename, false);

    if ((input = open(srcPath, O_RDONLY)) == -1) {
        return -1;
    }
    if ((output = open(dstPath, O_RDWR | O_CREAT)) == -1) {
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
    char path[PATH_MAX];
#ifdef __WINDOWS__
    WIN32_FIND_DATAA findFileData;

    if (s_hFind != INVALID_HANDLE_VALUE) {
        FindClose(s_hFind);
    }

    DosToLocalPath(path, spec, true);
    s_hFind = FindFirstFileA(spec, &findFileData);
    if (s_hFind == INVALID_HANDLE_VALUE) {
        DosToLocalPath(path, spec, false);
        s_hFind = FindFirstFileA(spec, &findFileData);
        if (s_hFind == INVALID_HANDLE_VALUE) {
            return false;
        }
    }

    strncpy(dta->name, findFileData.cFileName, ARRAYSIZE(dta->name) - 1);
    dta->name[ARRAYSIZE(dta->name) - 1] = '\0';
    dta->atr  = (uint8_t)findFileData.dwFileAttributes;
    dta->size = ((uint64_t)findFileData.nFileSizeHigh << 32) |
                (uint64_t)findFileData.nFileSizeLow;
    return true;
#else
    if (s_dirp != NULL) {
        closedir(s_dirp);
        s_dirp = NULL;
    }

    DosToLocalPath(path, spec, true);
    spec = strrchr(path, '/');
    if (spec == NULL) {
        return false;
    }
    *spec++ = '\0';
    strcpy(s_findSpec, spec);

    s_dirp = opendir(path);
    if (s_dirp == NULL) {
        DosToLocalPath(path, spec, false);
        spec = strrchr(path, '/');
        if (spec == NULL) {
            return false;
        }
        *spec++ = '\0';

        s_dirp = opendir(path);
    }
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
    } while (!MatchSpec(entry.d_name, s_findSpec));

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
