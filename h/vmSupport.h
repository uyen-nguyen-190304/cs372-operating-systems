#ifndef VMSUPPORT
#define VMSUPPORT

/************************* VMSUPPORT.h *****************************
 *
 * This header declares the global data structures and function prototypes
 * for Pandos kernel's Phase 3 virtual memory implementation, which includes
 * Swap Pool table and semaphore, initSwapStructs() to initialize those two
 * at boot, and the pager() function that handle TLB misses and page faults
 * 
 * Written by   : Uyen Nguyen
 * Last update  : 2025/04/17
 *
 *****************************************************************/

#include "../h/const.h"
#include "../h/types.h"

/* Global variables */
extern int swapPoolSemaphore;                       /* Semaphore for the Swap Pool Table */
extern swap_t swapPoolTable[SWAPPOOLSIZE];   /* THE Swap Pool Table: one entry per swap pool frame */

/* Function declarations */
extern void initSwapStructs(void);                  /* Initialize the Swap Pool table */
extern void pager(void);                            /* Pager function */

#endif /* VMSUPPORT */