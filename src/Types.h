#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#include <direct.h>

#ifdef RES_CURSOR
#undef RES_CURSOR
#endif

#ifdef SRCCOPY
#undef SRCCOPY
#endif

#ifdef SRCAND
#undef SRCAND
#endif

#else
#include <unistd.h>
#endif // _WIN32


#if !defined(_Noreturn)
#   if (3 <= __GNUC__ || (__GNUC__ == 2 && 8 <= __GNUC_MINOR__) || 0x5110 <= __SUNPRO_C)
#       define _Noreturn __attribute__ ((__noreturn__))
#   elif 1200 <= _MSC_VER
#       define _Noreturn __declspec (noreturn)
#   else
#       define _Noreturn
#   endif
#endif


#if defined(_WIN32)
#define __WINDOWS__  1
#elif defined(__ANDROID__)
#elif defined(__APPLE__)
#define __IOS__  1
#elif defined(__QNX__)
#define __BLACKBERRY__  1
#else
#error Unsupported Operating System!
#endif

#if defined(DEBUG) && !defined(_DEBUG)
#define _DEBUG 1
#endif

#if !defined(DEBUG) && defined(_DEBUG)
#define DEBUG 1
#endif

#if defined(__WINDOWS__) && defined(_DLL)
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API
#endif


#ifdef __WINDOWS__
typedef intptr_t    ssize_t;
#else
typedef int8_t      byte;
#endif // __WINDOWS__

typedef uint8_t     ubyte;

typedef int16_t     word;
typedef uint16_t    uword;

typedef int32_t     dword;
typedef uint32_t    udword;

typedef int64_t     qword;
typedef uint64_t    uqword;

// Shorthand for unsigned values.
typedef unsigned char   uchar;
typedef unsigned short  ushort;
typedef unsigned int    uint;
typedef unsigned long   ulong;

#ifndef __cplusplus
typedef byte bool;
#define true    1
#define false   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

typedef void*   Handle;

// Pseudo-machine object (or other address) references.
typedef uword   ObjID;

typedef	bool (*boolfptr)(const char*);

#ifndef O_BINARY
#define O_BINARY 0
#endif


#ifndef CONTAINING_RECORD
#define CONTAINING_RECORD(address, type, field) ((type *)( \
                                                  (byte*)(address) - \
                                                  (uintptr_t)(&((type *)0)->field)))
#endif


#define ALIGN_DOWN(val, alignment)  (((uintptr_t)(val)) & ~(((uintptr_t)(alignment)) - 1))
#define ALIGN_UP(val, alignment)    (((uintptr_t)(val)) + (((uintptr_t)(alignment)) - 1)) & ~(((uintptr_t)(alignment)) - 1)

#define ROUND(val, alignment)       ((((uintptr_t)(val)) + ((((uintptr_t)(alignment)) / 2) + (((uintptr_t)(alignment)) % 2))) - \
                                    ((((uintptr_t)(val)) + ((((uintptr_t)(alignment)) / 2) + (((uintptr_t)(alignment)) % 2))) % ((uintptr_t)(alignment))))


#ifndef ARRAYSIZE
#define ARRAYSIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif


#define forever()   for (;;)


#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif


#define MAXWIDTH    320
#define MAXHEIGHT   200


#define NOT_IMPL 1

#include "Log.h"

#endif // TYPES_H
