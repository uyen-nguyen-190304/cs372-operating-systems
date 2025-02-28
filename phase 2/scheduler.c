/******************************* SCHEDULER.c ***************************************
 *
 * This module implements the Scheduler and deadlock detection for the nucleus.
 * It guarantees that every ready process gets a chance to execute using a
 * preemptive round-robin scheduling algorithm with a 5ms time slice. If the ready
 * queue is empty, it either halts the system (if no processes remain), waits for an
 * external interrupt (if processes are blocked), or panics in the event of deadlock.
 *
 ***********************************************************************************/

#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "/usr/include/umps3/umps/libumps.h"

/*******************************  GLOBAL VARIABLES  *******************************/

/* Global variables used for CPU time tracking */
cpu_t startTOD;         /* Time when the current process was dispatched */
cpu_t currentTOD;       /* Temporary variable for current time (used locally) */

/******************************* SCHEDULING IMPLEMENTATION *******************************/

/*
* Function      :   scheduler
* Purpose       :   Implements a round-robin scheduler with a 5ms time slice.
*                   If the ready queue is non-empty, it dispatches the next process.
*                   If empty, it handles idle conditions: halting, waiting, or panicking on deadlock.
* Parameters    :   None
*/
void scheduler() {
    /* Declare pointer for the next process to dispatch */
    pcb_PTR nextProcess;  
    
    /* Check if the ready queue is empty */
    if (emptyProcQ(&readyQueue)) {
        if (processCount == 0) {
            HALT();  /* No processes remain; halt the system */
        } else if (softBlockCount > 0) {
            /* Processes exist but are all blocked: enable interrupts and disable the local timer */
            setSTATUS(getSTATUS() | ALLOFF | IMON | IECON);
            setTIMER(INFINITE);                                 /* Prevent PLT from firing */
            WAIT();                                             /* Wait for an external interrupt */
        } else {
            /* Deadlock: processes exist but none are ready */
            PANIC();
        }
    }

    /* Ready queue is not empty: remove the head process and dispatch it */
    nextProcess = removeProcQ(&readyQueue);
    currentProcess = nextProcess;

    /* Record the time when the process was dispatched */
    STCK(startTOD);

    /* Load the time slice (5ms) into the processor's local timer */
    loadLocalTimer(TIMESLICE);

    /* Switch context to dispatch next process */
    LDST(&(nextProcess->p_s));                                  /* Load the processor state of the next process */

    /* Should never reach here if LDST works correctly */
    PANIC();
}

/******************************* END OF SCHEDULER.c *******************************/
