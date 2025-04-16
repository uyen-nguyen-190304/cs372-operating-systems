#ifndef INITPROC
#define INITPROC

/************************* INITPROC.h *****************************
 *
 * 
 * 
 * Written by   : Uyen Nguyen
 * Last update  : 2025/04/15
 *
 *****************************************************************/

#include "../h/const.h"
#include "../h/types.h"

extern int masterSemaphores;                   /* Semaphore for synchronization */
extern int devSemaphores[MAXIODEVICES];        /* Semaphore for mutual exclusion */
extern void test();                            /* Instantiator process function */

#endif /* INITPROC */