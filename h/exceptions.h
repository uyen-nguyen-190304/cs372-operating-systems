#ifndef EXCEPTIONS
#define EXCEPTIONS

/************************* EXCEPTIONS.h *****************************
 *
 * This header file declares the function prototypes for handling various
 * exceptions in the kernel, which include program trap, TLB misses or errors,
 * system call, and TLB refill exceptions
 *
 * Written by   : Uyen Nguyen
 * Last update  : 2025/02/28 
 *
 ********************************************************************/

#include "../h/const.h"
#include "../h/types.h"

extern void programTrapExceptionHandler();
extern void TLBExceptionHandler();
extern void syscallExceptionHandler();
extern void uTLB_RefillHandler();

#endif /* EXCEPTIONS */