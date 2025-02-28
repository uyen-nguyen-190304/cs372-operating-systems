#ifndef EXCEPTIONS
#define EXCEPTIONS

#include "const.h"      /* Constants and macro definitions */
#include "types.h"      /* Definitions for state_t, pcb_t, etc. */
#include "pcb.h"        /* Process control block definitions */
#include "asl.h"        /* Active semaphore list definitions */
#include "scheduler.h"  /* Scheduler function prototypes */
#include "interrupts.h" /* Interrupt handling prototypes */
#include "initial.h"    /* For initialization and global variables */

extern void generalExceptionHandler();
extern void programTrapExceptionHandler();
extern void TLBExceptionHandler();
extern void syscallExceptionHandler();
extern void uTLB_RefillHandler();






#endif /* EXCEPTIONS */