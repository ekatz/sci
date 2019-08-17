#include "sci/Kernel/SaveGame.h"
#include "sci/PMachine/PMachine.h"
#include "sci/Utils/FileIO.h"
#include "sci/Utils/Path.h"

#define ret(val) *acc = ((uintptr_t)(val))

void KGetSaveDir(argList)
{
    ret(GetSaveDosDir());
}

void KCheckSaveGame(argList)
{
#ifndef NOT_IMPL
#error Not finished
#endif
}

void KGetSaveFiles(argList)
{
#ifndef NOT_IMPL
#error Not finished
#endif
}

void KSaveGame(argList)
{
#ifndef NOT_IMPL
#error Not finished
#endif
}

void KRestoreGame(argList)
{
#ifndef NOT_IMPL
#error Not finished
#endif
}

#ifndef NOT_IMPL

static void Save(uchar *start, uchar *end)
{
    long offset1, offset2;

    // Find out where we are in file and save a position for length field.
    offset1 = lseek(fd, 0L, LSEEK_CUR);
    outword(0);
    pkImplode(fd, start, (uint)(end - start));

    // Get length of data written and store in front of data.
    offset2 = lseek(fd, 0L, LSEEK_CUR);
    lseek(fd, offset1, LSEEK_BEG);
    outword((uint)(offset2 - offset1 - sizeof(uint)));
    lseek(fd, offset2, LSEEK_BEG);
}

static void outbyte(ubyte c)
{
    if (write(fd, &c, 1) != 1) {
        longjmp(saveErr, 1);
    }
}

static void outword(word w)
{
    if (write(fd, (memptr)&w, 2) != 2) {
        longjmp(saveErr, 1);
    }
}

static void outstr(const char *str)
{
    int n = strlen(str);

    if (write(fd, str, strlen(str)) != n)
        longjmp(saveErr, 1);
    outbyte('\n');
}
#endif
