#ifndef TEXT_H
#define TEXT_H

#include "GrTypes.h"

// Text justification.
#define TEJUSTLEFT   0
#define TEJUSTCENTER 1
#define TEJUSTRIGHT  (-1)

// Put the text to the box in mode requested.
RRect *RTextBox(const char *text, bool show, RRect *box, uint mode, uint font);

#endif // TEXT_H
