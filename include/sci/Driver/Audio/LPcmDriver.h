#ifndef SCI_DRIVER_AUTDIO_LPCMDRIVER_H
#define SCI_DRIVER_AUTDIO_LPCMDRIVER_H

#include "sci/Utils/Types.h"

// Codes to be sent to the audio driver.
#define A_INIT      0
#define A_STAT      1
#define A_TERMINATE 2
#define A_MEMPLAY   3
#define A_MEMCHECK  4
#define A_MEMSTOP   5
#define A_RATE      8  // 6
#define A_PAUSE     11 // 7
#define A_RESUME    12 // 8
#define A_SELECT    3  // 9
#define A_WPLAY     4  // 10
#define A_PLAY      5  // 11
#define A_STOP      6  // 12
#define A_LOC       7  // 13
#define A_VOLUME    14
#define A_FILLBUFF  1  // 15
#define A_QUEUE     16

int AudioDrv(int function, uintptr_t qualifier);

#endif // SCI_DRIVER_AUTDIO_LPCMDRIVER_H
