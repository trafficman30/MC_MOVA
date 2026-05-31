/* PCError.h — stub for headless/Linux PC_WRAPPER build */
#ifndef PCERROR_H
#define PCERROR_H

#include "MVUtils/MVTypes.h"

#define PCMOVA_ERROR_KERNEL_FATAL 0

static inline void PCError_SetLastKernel(MVUInt16 errID, int data, const char* str, int len)
{ (void)errID; (void)data; (void)str; (void)len; }

static inline void PCError_ExitKernel(int code) { (void)code; }

#endif /* PCERROR_H */
