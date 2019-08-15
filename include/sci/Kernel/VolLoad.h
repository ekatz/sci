#pragma once
#ifndef SCI_KERNEL_VOLLOAD_H
#define SCI_KERNEL_VOLLOAD_H

#include "sci/Kernel/ResTypes.h"
#include "sci/Utils/Types.h"

// Init global resource list.
void InitResource(void);

Handle DoLoad(int resType, size_t resNum);

#endif // SCI_KERNEL_VOLLOAD_H
