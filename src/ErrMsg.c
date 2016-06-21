#include "ErrMsg.h"

#define ERRBUFSIZE  400
#define FILEBUFSIZE 1000

static bool FormatStringProc(boolfptr func, const char *format, va_list args);
static const char *ReadErrMsg(int errnum);
static _Noreturn void DoPanic(const char *text);

static boolfptr s_alertProc = (boolfptr)DoPanic;

void SetAlertProc(boolfptr func)
{
    s_alertProc = func;
}

bool DoAlert(const char *text)
{
#ifndef NOT_IMPL
#error Not finished
#endif
    return true;
}

bool RAlert(int errnum, ...)
{
    va_list args;
    bool    res;

    va_start(args, errnum);
    res = FormatStringProc(s_alertProc, ReadErrMsg(errnum), args);
    va_end(args);
    return res;
}

_Noreturn void Panic(int errnum, ...)
{
    va_list args;

    va_start(args, errnum);
    FormatStringProc((boolfptr)DoPanic, ReadErrMsg(errnum), args);
    va_end(args);
}

static bool FormatStringProc(boolfptr func, const char *format, va_list args)
{
    bool res = false;
    uint len;

    len = (uint)vsnprintf(NULL, 0, format, args);
    if (((int)len) >= 0) {
        char  stackBuffer[ERRBUFSIZE];
        char *buffer = stackBuffer;

        len++;
        if (len >= sizeof(stackBuffer)) {
            buffer = malloc(len + 1);
        }

        buffer[0] = '\0';
        vsnprintf(buffer, len + 1, format, args);
        buffer[len - 1] = '\n';
        buffer[len]     = '\0';

        res = func(buffer);

        if (buffer != stackBuffer) {
            free(buffer);
        }
    }
    return res;
}

static const char *ReadErrMsg(int errnum)
{
    switch (errnum) {
        case E_DISK_ERROR:
            return "Disk Error: %s";
        case E_CANCEL:
            return "Cancel";
        case E_QUIT:
            return "Quit";

        case E_OOPS_TXT:
            return "You did something that we weren't expecting.\n"
                   "Whatever it was, you don't need to do it to finish the "
                   "game.\n"
                   "Try taking a different approach to the situation.\n"
                   "Error %d.  SCI Version %s";
        case E_OOPS:
            return "Oops!";
        case E_NO_DEBUG:
            return "Debug not available! error #%d";
        case E_NO_AUDIO_DRVR:
            return "Can't find audio driver '%s'!";
        case E_NO_AUDIO:
            return "Unable to initialize your audio hardware.";
        case E_NO_CD_MAP:
            return "Unable to locate the cd-language map file.";
        case E_INSERT_DISK:
            return "Please insert\n"
                   "Disk %d in drive %s\n"
                   "and press ENTER.%s";
        case E_NO_LANG_MAP:
            return "Unable to locate the language map file.";
        case E_INSERT_STARTUP:
            return "Please insert\n"
                   "Startup Disk in drive %s\n"
                   "and press ENTER.%s";
        case E_NO_KBDDRV:
            return "No keyboard driver!";
        case E_NO_HUNK:
            return "Out of hunk!";
        case E_NO_HANDLES:
            return "Out of handles!";
        case E_NO_VIDEO:
            return "Couldn't install video driver!";
        case E_CANT_FIND:
            return "Couldn't find %s.";
        case E_NO_PATCH:
            return "Couldn't find PatchBank.";
        case E_NO_MUSIC:
            return "Unable to initialize your music hardware.";
        case E_CANT_LOAD:
            return "Can't load %s!";
        case E_NO_INSTALL:
            return "Can't find configuration file: %s";
        case E_RESRC_MISMATCH:
            return "Resource type mismatch";
        case E_NOT_FOUND:
            return "%s not found.";
        case E_WRONG_RESRC:
            return "Wrong resource type! (Looking for $%x #%d)";
        case E_LOAD_ERROR:
            return "Load error: %s";
        case E_MAX_SERVE:
            return "Maximum number of servers exceeded.";
        case E_NO_DEBUGGER:
            return "No Dubugger Available.";
        case E_NO_MINHUNK:
            return "Your config file must have a minHunk value.";
        case E_NO_MEMORY:
            return "You do not have enough memory available to run this game.";
        case E_NO_CONF_STORAGE:
            return "Out of configuration storage.";
        case E_BAD_PIC:
            return "Bad picture code";

        case E_SAVEBITS:
            return "SaveBits - Not enough hunk.";
        case E_BRESEN:
            return "Bresen failed!";

        case E_CONFIG_STORAGE:
            return "Out of configuration storage.";
        case E_CONFIG_ERROR:
            return "Config file error: no '%c' in '%s'";

        case E_OPEN_WINDOW:
            return "Can't open window.";
    }
    return "Unknown error";
}

static _Noreturn void DoPanic(const char *text)
{
#ifdef __WINDOWS__
    MessageBoxA(NULL, text, "PANIC", MB_OK | MB_ICONERROR);
#else
#error Not implemented
#endif
    exit(1);
}
