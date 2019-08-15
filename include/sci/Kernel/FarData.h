#ifndef FARDATA_H
#define FARDATA_H

#include "Types.h"

char *GetFarStr(uint moduleNum, uint entryNum, char *buffer);
char *GetFarText(uint moduleNum, uint offset, char *buffer);

#endif // FARDATA_H
