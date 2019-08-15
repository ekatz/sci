#ifndef SCI_KERNEL_RESTYPES_H
#define SCI_KERNEL_RESTYPES_H

#include "sci/Utils/Types.h"

// Resource types
#define RES_BASE    0x80
#define RES_VIEW    0x80
#define RES_PIC     0x81
#define RES_SCRIPT  0x82
#define RES_TEXT    0x83
#define RES_SOUND   0x84
#define RES_MEM     0x85
#define RES_VOCAB   0x86
#define RES_FONT    0x87
#define RES_CURSOR  0x88
#define RES_PATCH   0x89
#define RES_BITMAP  0x8a
#define RES_PAL     0x8b
#define RES_CDAUDIO 0x8c
#define RES_AUDIO   0x8d
#define RES_SYNC    0x8e
#define RES_MSG     0x8f

#define NRESTYPES (RES_MSG - RES_BASE + 1)

#endif // SCI_KERNEL_RESTYPES_H
