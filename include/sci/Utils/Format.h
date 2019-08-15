#ifndef SCI_UTILS_FORMAT_H
#define SCI_UTILS_FORMAT_H

#include "sci/Utils/Types.h"

int sci_sprintf(char *str, char *fp, ...);

int sci_vsprintf(char *str, char *fp, va_list ap);

#endif // SCI_UTILS_FORMAT_H
