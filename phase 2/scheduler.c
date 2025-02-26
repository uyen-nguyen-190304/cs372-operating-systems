/******************************* SCHEDULER.c ***************************************
 *
 * ! Add documentation
 * ! Remove the comment for libumps
 * 
 * 
 *
 ***********************************************************************************/

#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
/* #include "/usr/include/umps3/umps/libumps.h" */

/*******************************  GLOBAL VARIABLES  *******************************/

/* Nucleus global variables */
extern int processCount;
extern int softBlockCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currentProcess;

/* Define time slice constant */
#define TIMESLICE 5000              /* Time slice in microseconds (5ms) */

/******************************* SCHEDULING IMPLEMENTATION *******************************

/*
 * Function     :   scheduler
 * Purpose      :   
 *                  
 * Parameters   :   None
 */
void scheduler()
{
    /* Local variable: Next process to dispatch in ready queue */
    pcb_t *nextProcess;            

    /* Check if the ready queue is empty */
    if (readyQueue == NULL) {
        /* If no processes remained, HALT the system */
        if (processCount == 0) {
            HALT();
        } 

        /* Processes exist but all are blocked: WAIT for IO interrupt */
        else if (softBlockCount > 0) {
            /* First, adjust the Status register and PLT */
            setSTATUS(getSTATUS() | ALLOFF | IMON | IECON);         /* Set the Status register to enable interrupts */     
            setTIMER(NEVER);                                        /* Set the PLT to a very large value */

            /* Wait for an interrupt */
            WAIT();
        }
        /* Deadlock: processes exist but none are ready or waiting on devices */
        else {
            PANIC();
        }
    }

    /* Ready queue is not empty */
    nextProcess = removeProcQ(&readyQueue);         /* Remove the head process from the ready queue */
    currentProcess = nextProcess;                   /* Set as current process */

    /* Load the time slice (5ms) into the processor's Local Timer */
    loadLocalTimer(TIMESLICE);

    /* Dispatch the process by loading its state into the processor */
    LDST(&(nextProcess->p_s));                      /* LDST() transfers control to the process and does not return */

    /* Should never reach here; it it does, PANIC */
    PANIC();
}

/******************************* END OF SCHEDULER.c *******************************/
