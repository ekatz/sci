#ifndef SCI_UTILS_TRIG_H
#define SCI_UTILS_TRIG_H

#define TrigScale 10000

// Calculate the sine of an angle in degrees, scaled by x10000.
int ScaledSin(int angle);

// Calculate the cosine of an angle in degrees, scaled by x10000.
int ScaledCos(int angle);

// Calculate the heading from (x1,y1) to (x2,y2) in degrees (always in the range
// 0-359).
int ATan(int x1, int y1, int x2, int y2);

#define SinMult(a, n) ((ScaledSin(a) * n) / TrigScale)
#define CosMult(a, n) ((ScaledCos(a) * n) / TrigScale)
#define SinDiv(a, n)  ((TrigScale * n) / ScaledSin(a))
#define CosDiv(a, n)  ((TrigScale * n) / ScaledCos(a))

#endif // SCI_UTILS_TRIG_H
