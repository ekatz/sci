#include "sci/Utils/Trig.h"
#define _USE_MATH_DEFINES
#include <math.h>

int ScaledSin(int angle)
{
    double res = sin(angle / (180.0 / M_PI));
    return (int)round(res * (double)TrigScale);
}

int ScaledCos(int angle)
{
    double res = cos(angle / (180.0 / M_PI));
    return (int)round(res * (double)TrigScale);
}

int ATan(int x1, int y1, int x2, int y2)
{
    double rad = atan2((double)(y2 - y1), (double)(x2 - x1));
    int    deg = (int)round(rad * (180.0 / M_PI));
    return (deg < 0 ? (deg + 360) : deg);
}
