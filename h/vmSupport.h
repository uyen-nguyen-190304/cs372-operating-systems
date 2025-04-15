#ifndef VMSUPPORT
#define VMSUPPORT

/************************* VMSUPPORT.h *****************************
 *
 * 
 * 
 * Written by   : Uyen Nguyen
 * Last update  : 2025/04/15
 *
 *****************************************************************/

#include "../h/const.h"
#include "../h/types.h"
 
extern int swapPoolSemaphore;                          /* Semaphore for the Swap Pool Table */
extern swap_t swapPoolTable[SWAPPOOLSIZE];             /* THE Swap Pool Table: one entry per swap pool frame */

extern void initSwapStructs();                         /* Initialize the Swap Pool table */
extern void pager();                                    /* Pager function */

#endif /* VMSUPPORT */