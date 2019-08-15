#ifndef SCI_KERNEL_PALETTE_H
#define SCI_KERNEL_PALETTE_H

#include "sci/Utils/Types.h"

#define PAL_FILE_SIZE 0x504
#define PAL_CLUT_SIZE 256

enum palFunc {
    PALNULLENUM,
    PALLoad,
    PALSet,
    PALReset,
    PALIntensity,
    PALMatch,
    PALCycle,
    PALSave,
    PALRestore
};

// Maximum number of ranges for palette cycling.
#define MAXPALCYCLE 16

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

#define BLACK    0
#define BLUE     PalMatch(0x15, 0x15, 0x77)
#define GREEN    PalMatch(0x15, 0x77, 0x15)
#define CYAN     PalMatch(0x15, 0x77, 0x77)
#define RED      PalMatch(0x77, 0x15, 0x15)
#define MAGENTA  PalMatch(0x77, 0x15, 0x77)
#define BROWN    PalMatch(0x77, 0x46, 0x15)
#define LGREY    PalMatch(0x9f, 0x9f, 0x9f)
#define GREY     PalMatch(0x5f, 0x5f, 0x5f)
#define LBLUE    PalMatch(0x26, 0x26, 0xd8)
#define LGREEN   PalMatch(0x26, 0xd8, 0x26)
#define LCYAN    PalMatch(0x26, 0xd8, 0xd8)
#define LRED     PalMatch(0xd8, 0x26, 0x26)
#define LMAGENTA PalMatch(0xd8, 0x26, 0xd8)
#define YELLOW   PalMatch(0xd8, 0xd8, 0x26)
#define WHITE    255

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

// All SCRIPT palette functions dispatch through KPalette.
void KPalette(uintptr_t *args);

uint8_t PalMatch(uint8_t r, uint8_t g, uint8_t b);

#endif // SCI_KERNEL_PALETTE_H
