#ifndef SCHEDULER
#define SCHEDULER

/************************* SCHEDULER.h *****************************
 *
 * This header file declares global variables and function prototypes
 * used in the scheduler module. 
 * 
 * Written by   : Uyen Nguyen
 * Last update  : 2025/02/28 
 *
 *******************************************************************/

extern cpu_t startTOD;              /* Hold TOD value at process dispatch */
extern cpu_t currentTOD;            /* Hold current TOD when STCK */

extern void copyState();            /* Helper function to copy a processor state */
extern void scheduler();            /* Round-robin scheduler */

#endif /* SCHEDULER */