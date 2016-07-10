#include "VolLoad.h"
#include "Crypt.h"
#include "ErrMsg.h"
#include "FileIO.h"
#include "ResName.h"
#include "Resource.h"

#define RESMAPNAME "RESOURCE.MAP"
#define RESVOLNAME "RESOURCE"

#define MAXMASKS 10

#define RESID(resType, resNum)                                                 \
    ((uint16_t)((uint16_t)(resNum) | ((uint16_t)(resType) << 11)))
#define INVALID_RESID ((uint16_t)-1)

#pragma pack(push, 2)
// Structure of information in resource map file.
typedef struct ResDirEntry {
    uint16_t resId;
    uint32_t offset : 26;
    uint32_t volNum : 6;
} ResDirEntry;

typedef struct ResDirEntryWin {
    uint16_t resId;
    uint32_t offset : 28;
    uint32_t volNum : 4;
} ResDirEntryWin;
#pragma pack(pop)

typedef struct ResSegHeader {
    uint16_t resId;
    uint16_t segmentLength;
    uint16_t length;
    uint16_t compression;
} ResSegHeader;

bool g_isExternal = false;

static void *s_resourceMap = NULL;
static int   s_resVolFd    = -1;
static short s_resVolNum   = 1;

// Allocate buffer and load resource map into it.
static void *LoadResMap(const char *mapName);
static bool  FindDirEntry(short    *volNum,
                          uint32_t *offset,
                          int       resType,
                          size_t    resNum);

void InitResource(const char *resDir)
{
    InitList(&g_loadList);

    g_resDir[0] = '\0';
    if (resDir != NULL) {
        size_t len = strlen(resDir);
        if (len != 0 && len < 255) {
            memcpy(g_resDir, resDir, len);
            if (resDir[len - 1] != '\\' || resDir[len - 1] != '/') {
#ifdef __WINDOWS__
                g_resDir[len++] = '\\';
#else
                g_resDir[len++] = '/';
#endif
            }
            g_resDir[len] = '\0';
        }
    }

    s_resourceMap = LoadResMap(RESMAPNAME);
    if (s_resourceMap == NULL) {
        Panic(E_CANT_LOAD, RESMAPNAME);
    }

    InitPatches();
}

static void *LoadResMap(const char *mapName)
{
    char  path[256];
    int   fd;
    uint  size;
    void *buffer = NULL;

    sprintf(path, "%s%s", g_resDir, mapName);
    fd = open(path, O_RDONLY | O_BINARY);
    if (fd >= 0) {
        size   = (uint)filelength(fd);
        buffer = malloc(size);
        if (buffer != NULL) {
            if (-1 == read(fd, buffer, size)) {
                free(buffer);
                buffer = NULL;
            }
        }
        close(fd);
    }
    return buffer;
}

Handle DoLoad(int resType, size_t resNum)
{
    char         fileName[64];
    ResSegHeader dataInfo;
    bool         loadedFromFile = false;
    int          fd             = -1;
    Handle       outHandle      = NULL;

#ifdef __WINDOWS__
    if (RES_SOUND == resType) {
        resNum += 1000;
    }
#endif

    if (FindPatchEntry(resType, resNum)) {
        ResNameMake(fileName, resType, resNum);
        fd = ROpenResFile(resType, resNum, fileName);
    }

    if (fd != -1) {
        uint8_t typeLen = 0;

        loadedFromFile       = true;
        dataInfo.compression = 0;
        dataInfo.length      = (uint16_t)(filelength(fd) - 2);

        read(fd, &typeLen, 1);
        if (resType != (int)typeLen) {
            close(fd);
            Panic(E_RESRC_MISMATCH);
            return NULL;
        }

        read(fd, &typeLen, 1);
        lseek(fd, (int)typeLen, SEEK_SET);
    } else {
        short    volNum = 0;
        uint32_t offset = 0;
        if (!FindDirEntry(&volNum, &offset, resType, resNum)) {
            if (!g_isExternal) {
                Panic(E_NOT_FOUND, ResNameMake(fileName, resType, resNum));
            }
            return NULL;
        }

        // TODO: write the real code, that support different volNums

        if (s_resVolFd == -1 || s_resVolNum != volNum) {
            char path[256];
            sprintf(fileName, "%s.%03u", RESVOLNAME, volNum);
            sprintf(path, "%s%s", g_resDir, fileName);
            s_resVolFd  = open(path, O_RDONLY | O_BINARY);
            s_resVolNum = volNum;
        }

        fd = s_resVolFd;
        if (fd != -1) {
            lseek(fd, (int)offset, SEEK_SET);
            read(fd, &dataInfo, sizeof(ResSegHeader));
            if (dataInfo.resId != RESID(resType, resNum)) {
                close(fd);
                s_resVolFd = -1;
                return NULL;
            }
        }
    }

    if (fd != -1) {
        uint size = (uint)dataInfo.segmentLength -
                    (sizeof(ResSegHeader) - offsetof(ResSegHeader, length));
        outHandle = GetResHandle(size);

        read(fd, outHandle, size);

        if (loadedFromFile) {
            close(fd);
        }

        // TODO: handle different compression types
        if (dataInfo.compression == 2) {
            Handle decompHandle = GetResHandle(dataInfo.length);
            if (DecompressLZW_1((uint8_t *)decompHandle,
                                (uint8_t *)outHandle,
                                dataInfo.length,
                                dataInfo.segmentLength)) {
                DisposeResHandle(outHandle);
                outHandle = decompHandle;
            } else {
                assert(0);
            }
        } else {
            assert(dataInfo.compression == 0);
        }
    }
    return outHandle;
}

static bool FindDirEntryDos(short    *volNum,
                            uint32_t *offset,
                            int       resType,
                            size_t    resNum)
{
    uint16_t           resId;
    uint               vol;
    const ResDirEntry *entry;
    const ResDirEntry *foundEntry = NULL;

    resId = RESID(resType, resNum);
    vol   = (uint)*volNum;

    for (entry = (ResDirEntry *)s_resourceMap; entry->resId != INVALID_RESID;
         ++entry) {
        if (entry->resId == resId) {
            *offset = entry->offset;
            if (entry->volNum == vol) {
                return true;
            }

            foundEntry = entry;
        }
    }

    if (foundEntry != NULL) {
        *volNum = (short)foundEntry->volNum;
        return true;
    }
    return false;
}

static bool FindDirEntryWin(short    *volNum,
                            uint32_t *offset,
                            int       resType,
                            size_t    resNum)
{
    uint16_t              resId;
    uint                  vol;
    const ResDirEntryWin *entry;
    const ResDirEntryWin *foundEntry = NULL;

    resId = RESID(resType, resNum);
    vol   = (uint)*volNum;

    for (entry = (ResDirEntryWin *)s_resourceMap; entry->resId != INVALID_RESID;
         ++entry) {
        if (entry->resId == resId) {
            *offset = entry->offset;
            if (entry->volNum == vol) {
                return true;
            }

            foundEntry = entry;
        }
    }

    if (foundEntry != NULL) {
        *volNum = (short)foundEntry->volNum;
        return true;
    }
    return false;
}

static bool FindDirEntry(short    *volNum,
                         uint32_t *offset,
                         int       resType,
                         size_t    resNum)
{
    if (((ResDirEntryWin *)s_resourceMap)->volNum != 0) {
        return FindDirEntryWin(volNum, offset, resType, resNum);
    } else {
        return FindDirEntryDos(volNum, offset, resType, resNum);
    }
}
