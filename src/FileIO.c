#include "FileIO.h"

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
