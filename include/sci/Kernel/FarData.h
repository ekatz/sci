#ifndef SCI_KERNEL_FARDATA_H
#define SCI_KERNEL_FARDATA_H

#include "sci/Utils/Types.h"

char *GetFarStr(uint moduleNum, uint entryNum, char *buffer);
char *GetFarText(uint moduleNum, uint offset, char *buffer);

#endif // SCI_KERNEL_FARDATA_H
