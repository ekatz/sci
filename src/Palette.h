#ifndef PALETTE_H
#define PALETTE_H

#include "Types.h"

#define PAL_FILE_SIZE 0x504
#define PAL_CLUT_SIZE 256

// Mode values.
#define PAL_MATCH   1
#define PAL_REPLACE 2

// Guns "flags" definitions.
#define PAL_IN_USE   1  // this entry is in use
#define PAL_NO_MATCH 2  // never match this color when remapping
#define PAL_NO_PURGE 4  // never overwrite this color when adding a palette
#define PAL_NO_REMAP 8  // never remap this value to another color
#define PAL_MATCHED  16 // in sys pal, shows someone is sharing it

#define MAX_INTENSITY 100

typedef struct Guns {
    uint8_t flags;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Guns;

typedef struct RPalette {
    uint8_t  mapTo[PAL_CLUT_SIZE];
    uint32_t valid;
    Guns     gun[PAL_CLUT_SIZE];
    int16_t  palIntensity[PAL_CLUT_SIZE];
} RPalette;

extern RPalette g_sysPalette;

void InitPalette(void);

// Add this palette to system palette.
void RSetPalette(RPalette *srcPal, int mode);

// Add this palette to system palette.
void SetResPalette(uint num, int mode);

void SetPalIntensity(RPalette *palette, int first, int last, int intensity);

#endif // PALETTE_H
