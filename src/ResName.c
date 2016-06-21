#include "ResName.h"
#include "ErrMsg.h"
#include "ResTypes.h"

bool g_newResName = true;

ResType g_resTypes[] = {
    { "view",    "*.v56" },
    { "pic",     "*.p56" },
    { "script",  "*.scr" },
    { "text",    "*.tex" },
    { "sound",   "*.snd" },
    { "memory",  ""      },
    { "vocab",   "*.voc" },
    { "font",    "*.fon" },
    { "cursor",  "*.cur" },
    { "patch",   "*.pat" },
    { "bitmap",  "*.bit" },
    { "palette", "*.pal" },
    { "cdaudio", "*.cda" },
    { "audio",   "*.aud" },
    { "sync",    "*.syn" },
    { "message", "*.msg" },
    NULL,
};

static char *makeName(char       *dest,
                      const char *mask,
                      const char *name,
                      int         resType,
                      size_t      resNum);

char *ResNameMake(char *dest, int resType, size_t resNum)
{
    if (g_newResName) {
        sprintf(dest,
                "%u.%s",
                (uint)resNum,
                g_resTypes[resType - RES_BASE].defaultMask + 2);
    } else {
        sprintf(
          dest, "%s.%03u", g_resTypes[resType - RES_BASE].name, (uint)resNum);
    }
    return dest;
}

char *ResNameMakeWildCard(char *dest, int resType)
{
    if (g_newResName) {
        sprintf(dest, "*.%s", g_resTypes[resType - RES_BASE].defaultMask + 2);
    } else {
        sprintf(dest, "%s.*", g_resTypes[resType - RES_BASE].name);
    }
    return dest;
}

const char *ResName(int resType)
{
    return g_resTypes[resType - RES_BASE].name;
}

int ROpenResFile(int resType, size_t resNum, char *name)
{
    char        fullName[100];
    const char *mask;
    int         fd = -1;

    mask = g_resTypes[resType - RES_BASE].defaultMask;

    makeName(fullName, mask, name, resType, resNum);
    fd = open(fullName, O_RDONLY | O_BINARY);
    if (fd != -1) {
        strcpy(name, fullName);
    } else {
        name = '\0';
    }
    return fd;
}

char *addSlash(char *dir)
{
    int  len;
    char ch;

    if ((len = strlen(dir)) != 0 && (ch = dir[len - 1]) != '\\' && ch != '/' &&
        ch != ':') {
        strcat(dir, "\\");
    }
    return dir;
}

static char *makeName(char       *dest,
                      const char *mask,
                      const char *name,
                      int         resType,
                      size_t      resNum)
{
    char  *cp;
    size_t dotIx;

    if (!g_newResName) {
        strcpy(dest, mask);
        addSlash(dest);

        if (NULL != name && '\0' != *name) {
            strcat(dest, name);
        } else {
            char buf[80];
            strcat(dest, ResNameMake(buf, resType, resNum));
        }
        return dest;
    }

    strcpy(dest, mask);
    cp = strchr(dest, '*');
    if (NULL == cp) {
        Panic(E_CONFIG_ERROR, '*', mask);
    }

    dotIx = cp - dest + 1;
    if (NULL != name && '\0' != name) {
        strcpy(cp, name);
    } else {
        sprintf(cp, "%u", (uint)resNum);
        if (mask[dotIx] != '.') {
            Panic(E_CONFIG_ERROR, '.', mask);
        }
        strcat(dest, mask + dotIx);
    }
    return dest;
}
