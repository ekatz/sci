#ifndef SCI_KERNEL_GRTYPES_H
#define SCI_KERNEL_GRTYPES_H

#include "sci/Utils/List.h"

// Virtual BitMap selection equates.
#define VMAP 1
#define PMAP 2
#define CMAP 4

// Pen transfer modes.
#define SRCOR     0
#define SRCCOPY   1
#define srcInvert 2
#define SRCAND    3

#define vBLACK 0
#define vWHITE 255

// Character style.
#define PLAIN   0
#define DIM     1
#define BOLD    2
#define INVERSE 4

#define INVALIDPRI ((uint)-1)

typedef struct RPoint {
    int16_t v;
    int16_t h;
} RPoint;

typedef struct RRect {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
} RRect;

typedef struct RGrafPort {
    Node    link;     // standardized LIST node structure
    RPoint  origin;   // offsets local coords for all drawing
    RRect   portRect; // all drawing clipped to here
    RPoint  pnLoc;    // persistent text output location
    uint    fontSize; // point size of tallest character
    uint    fontNum;  // font number of this port
    uint    txFace;   // stylistic variations
    uint8_t fgColor;  // on bits color
    uint8_t bkColor;  // off bits (erase) color
    uint8_t pnMode;   // transfer of bits
} RGrafPort;

typedef struct Font {
    uint16_t lowChar;
    uint16_t highChar;
    uint16_t pointSize;
    uint16_t charRecs[1];
} Font;

#endif // SCI_KERNEL_GRTYPES_H
