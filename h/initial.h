#ifndef INITIAL
#define INITIAL

/************************* INITIAL.h *****************************
 *
 * This header file declares global variables used for process management
 * and device synchronization, ensuring that the following global variable
 * are visible for any file that includes this header, while preventing
 * multiple inclusions
 *
 * Written by   : Uyen Nguyen
 * Last update  : 2025/02/28 
 *
 *****************************************************************/

#include "../h/const.h"
#include "../h/types.h"

extern int processCount;                    /* Number of active processes in the system */
extern int softBlockCount;                  /* Number of processes that are currently blocked */
extern pcb_PTR readyQueue;                  /* Pointer to the queue of processes that are ready to run */
extern pcb_PTR currentProcess;              /* Pointer to the currently executing process */
extern int deviceSemaphores[MAXDEVICES];    /* Array of semaphores for device synchronization */

#endif /* INITIAL */
