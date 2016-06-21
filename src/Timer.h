#ifndef TIMER_H
#define TIMER_H

#include "Types.h"

#define TICKS_PER_SECOND 60

// Returns the system time in nanoseconds.
uint64_t GetHighResolutionTime(void);

#if !defined(__WINDOWS__)
// Sleep for the amount of milliseconds.
void Sleep(uint milliseconds);
#endif

// Wait until the specified number of ticks passed.
void WaitUntil(uint ticks);

// Returns the number of ticks elapsed since the game was started.
uint RTickCount(void);

// Return system time data.
uint SysTime(int func);

#endif // TIMER_H
