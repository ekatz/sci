#ifndef FORMAT_H
#define FORMAT_H

#include "Types.h"

int sci_sprintf(char *str, char *fp, ...);

int sci_vsprintf(char *str, char *fp, va_list ap);

#endif // FORMAT_H
