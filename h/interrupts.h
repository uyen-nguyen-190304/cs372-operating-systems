#ifndef INTERRUPTS
#define INTERRUPTS

/************************* INTERRUPTS.h *****************************
 *
 * This header file declares the interface for the interrupt handling 
 * module used in the kernel
 *
 * Written by   : Uyen Nguyen
 * Last update  : 2025/02/28 
 *
 *****************************************************************/

#include "../h/types.h"
#include "../h/const.h"

extern void interruptHandler();

#endif  /* INTERRUPTS */