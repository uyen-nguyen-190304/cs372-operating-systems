#ifndef INITPROC
#define INITPROC

/************************* INITPROC.h *****************************
 *
 * This header declares the global variables and entry point for the Phase 3
 * initialization for Pandos kernal. It includes masterSemaphore for synchronization,
 * devSemaphores array for mutual exclusion, and test() function that set up the 
 * Swap Pool structures, related semaphores, builds initial proccess states,
 * and launch U-Procs via SYS1
 * 
 * Written by   : Uyen Nguyen
 * Last update  : 2025/04/17
 *
 *****************************************************************/

#include "../h/const.h"
#include "../h/types.h"

/* Global variables */
extern int masterSemaphore;                    /* Semaphore for synchronization */
extern int devSemaphores[MAXIODEVICES];        /* Semaphore for mutual exclusion */

/* Function declaration */
extern void test();                            /* Instantiator process function */

#endif /* INITPROC */