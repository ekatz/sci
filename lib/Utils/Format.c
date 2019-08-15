#include "Format.h"
#include <stdarg.h>

static char *CopyString(char *sp, const char *tp);

static int  s_justified, s_fieldWidth;
static bool s_leadingZeros;

#define RIGHT  0
#define CENTER 1
#define LEFT   2

int sci_sprintf(char *str, char *fp, ...)
{
    va_list ap;
    int     rc;

    va_start(ap, fp);
    rc = sci_vsprintf(str, fp, ap);
    va_end(ap);
    return rc;
}

int sci_vsprintf(char *str, char *fp, va_list ap)
{
    char *sp;
    char  theStr[20];

    sp = str;
    while (*fp) {
        if (*fp != '%') {
            *sp++ = *fp++;
        } else {
            ++fp;
            s_fieldWidth   = 0;
            s_leadingZeros = false;

            // Check for justification in a field.
            switch (*fp) {
                case '-':
                    ++fp;
                    s_justified = LEFT;
                    break;

                case '=':
                    ++fp;
                    s_justified = CENTER;
                    break;

                default:
                    s_justified = RIGHT;
            }

            // Check for a field width.
            if (isdigit(*fp)) {
                s_leadingZeros = (*fp == '0') && (s_justified == RIGHT);
                s_fieldWidth   = atoi(fp);
                while (isdigit(*fp)) {
                    ++fp;
                }
            }

            switch (*fp++) {
                case 'd':
                    sprintf(theStr, "%d", va_arg(ap, short));
                    sp = CopyString(sp, theStr);
                    break;

                case 'u':
                    sprintf(theStr, "%u", va_arg(ap, ushort));
                    sp = CopyString(sp, theStr);
                    break;

                case 'U':
                    sprintf(theStr, "%u", va_arg(ap, uint));
                    sp = CopyString(sp, theStr);
                    break;

                case 'x':
                    sprintf(theStr, "%x", va_arg(ap, ushort));
                    sp = CopyString(sp, theStr);
                    break;

                case 'X':
                    sprintf(theStr, "%x", va_arg(ap, uint));
                    sp = CopyString(sp, theStr);
                    break;

                case 'c':
                    theStr[0] = (char)va_arg(ap, int);
                    theStr[1] = '\0';
                    sp        = CopyString(sp, theStr);
                    break;

                case 's':
                    sp = CopyString(sp, va_arg(ap, char *));
                    break;

                default:
                    *sp++ = *(fp - 1);
            }
        }
    }

    *sp = '\0';

    return (sp - str);
}

static char *CopyString(char *sp, const char *tp)
{
    char  c;
    char *fp;
    int   fw, sLen;

    // Get the length of the string to print.
    // If the length is greater than the field width,
    // set the field width to 0 (i.e. print the entire string).
    sLen = (int)strlen(tp);
    if (sLen >= s_fieldWidth) {
        s_fieldWidth = 0;
    }

    // If there is a field width, clear the field to the appropriate character,
    // point sp to the place at which to start copying the string, and point fp
    // at the end of the string (do the latter even if there is no field width).
    if (s_fieldWidth > 0) {
        c = (char)(s_leadingZeros ? '0' : ' ');

        for (fw = s_fieldWidth, fp = sp; fw > 0; --fw, ++fp) {
            *fp = c;
        }

        switch (s_justified) {
            case RIGHT:
                sp += s_fieldWidth - sLen;
                break;

            case CENTER:
                sp += ((s_fieldWidth - sLen) / 2);
                break;
        }
    } else {
        fp = sp + sLen;
    }

    while (*tp) {
        *sp++ = *tp++;
    }

    return fp;
}
