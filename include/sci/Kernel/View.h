#ifndef SCI_KERNEL_VIEW_H
#define SCI_KERNEL_VIEW_H

#include "sci/Utils/Types.h"

#pragma warning(push)
#pragma warning(disable : 4200) // zero-sized array in struct/union

#define MAXLOOPS 8
#define MAXCELS  16

// Defines for vFlags.
#define NOCOMPRESS 0x40 // The view is not RLE compressed
#define COLOR256   0x80 // A 256 color view

// Cel structure.
typedef struct Cel {
    int16_t xDim;      // x dimension of cel
    int16_t yDim;      // y dimension of cel
    int8_t  xOff;      // x axis offset
    int8_t  yOff;      // y axis offset
    uint8_t skipColor; // skip color for cel
    byte    compressType;
    uint8_t data[0];
} Cel;

// Loop structure.
typedef struct Loop {
    uint16_t numCels;         // number of cels in the loop
    uint16_t loopSpare;       // spare, used in editor currently
    uint16_t celOff[MAXCELS]; // offsets to the cels
} Loop;

// View structure.
typedef struct View {
    uint8_t  numLoops;
    uint8_t  vFlags;
    uint16_t mirrorBits; // if set, requires mirror of cel in loop
    uint16_t equalBits;  // if set, indicates that loop data is shared
    uint16_t paletteOffset;
    uint16_t loopOff[MAXLOOPS]; // offsets to the loops
} View;

#pragma warning(pop)

#endif // SCI_KERNEL_VIEW_H
