#include "Path.h"

static uint16_t s_resDirLen  = 0;
static uint16_t s_saveDirLen = 0;
static char s_resDir[PATH_MAX + 1]  = { 0 };
static char s_saveDir[PATH_MAX + 1] = { 0 };

static void LocalizePathSeparator(char *path, size_t len);
static void MakeDosPathSeparator(char *path, size_t len);

void InitPath(const char *resDir, const char *saveDir)
{
    if (resDir != NULL) {
        s_resDirLen = (uint16_t)strlen(resDir);
        memcpy(s_resDir, resDir, s_resDirLen);
    } else {
        s_resDirLen = 0;
    }
    s_resDir[s_resDirLen] = '\0';

    if (saveDir != NULL) {
        s_saveDirLen = (uint16_t)strlen(saveDir);
        memcpy(s_saveDir, saveDir, s_saveDirLen);
    } else {
        s_saveDirLen = 0;
    }
    s_saveDir[s_saveDirLen] = '\0';
}

void DosToLocalPath(char *localPath, const char *dosPath, bool homeRes)
{
    size_t localLen    = 0;
    size_t dosLen      = strlen(dosPath);
    char   driveLetter = 0;

    if (isalpha(dosPath[0]) && dosPath[1] == ':') {
        driveLetter = dosPath[0];
        dosPath += 2;
        dosLen -= 2;
    }

    if ((driveLetter == 'r' || driveLetter == 'R') ||
        (driveLetter == 0 && homeRes)) {
        localLen = strlen(s_resDir);
        memcpy(localPath, s_resDir, localLen);
    } else if ((driveLetter == 'c' || driveLetter == 'C') ||
               (driveLetter == 0 && !homeRes)) {
        localLen = strlen(s_saveDir);
        memcpy(localPath, s_saveDir, localLen);
    }

    localPath += localLen;
    if (*dosPath != '\\' && *dosPath != '/' && localLen != 0) {
#if defined(__WINDOWS__)
        *localPath++ = '\\';
#else
        *localPath++ = '/';
#endif
        localLen++;
    }

    memcpy(localPath, dosPath, dosLen);
    localPath[dosLen] = '\0';

    LocalizePathSeparator(localPath, dosLen);
}

void LocalToDosPath(char *dosPath, const char *localPath)
{
    size_t localLen = strlen(localPath);

    if ((size_t)s_saveDirLen <= localLen &&
        memcmp(localPath, s_saveDir, s_saveDirLen) == 0) {
        *dosPath++ = 'C';
        *dosPath++ = ':';

        localPath += s_saveDirLen;
        localLen -= s_saveDirLen;
    } else if ((size_t)s_resDirLen <= localLen &&
               memcmp(localPath, s_resDir, s_resDirLen) == 0) {
        *dosPath++ = 'R';
        *dosPath++ = ':';

        localPath += s_resDirLen;
        localLen -= s_resDirLen;
    }

    if (localLen == 0) {
        *dosPath++ = '\\';
    } else {
        memcpy(dosPath, localPath, localLen);
    }
    dosPath[localLen] = '\0';

    MakeDosPathSeparator(dosPath, localLen);
}

const char *GetSaveDosDir()
{
    static char s_dosSaveDir[64 + 1] = "C:\\";
    return s_dosSaveDir;
}

char *CleanDir(char *dir)
{
    char *dp;

    dp = &dir[strlen(dir) - 1];
    if (*dp == '/' || *dp == '\\') {
        *dp = '\0';
    }

    return dir;
}

static void LocalizePathSeparator(char *path, size_t len)
{
#if !defined(__WINDOWS__)
    size_t i;
    for (i = 0; i < len; ++i) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
#endif
}

static void MakeDosPathSeparator(char *path, size_t len)
{
#if !defined(__WINDOWS__)
    size_t i;
    for (i = 0; i < len; ++i) {
        if (path[i] == '/') {
            path[i] = '\\';
        }
    }
#endif
}
