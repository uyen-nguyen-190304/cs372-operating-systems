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

/* Nucleus global variables */
extern int processCount;
extern int softBlockCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currentProcess;

/* Define time slice constant (5ms expressed in microseconds) */
#define TIMESLICE 5000

/* Global variables used for CPU time tracking */
cpu_t startTOD;      /* Time when the current process was dispatched */
cpu_t currentTOD;    /* Temporary variable for current time (used locally) */

/*******************************  HELPER FUNCTION  *******************************/

/*
* Function      :   copyState
* Purpose       :   Copies the processor state from the source to the destination.
* Parameters    :   source - pointer to the source state.
*                   dest   - pointer to the destination state.
*/
void copyState(state_PTR source, state_PTR dest) {
    dest->s_entryHI = source->s_entryHI;
    dest->s_cause   = source->s_cause;    
    dest->s_status  = source->s_status;
    dest->s_pc      = source->s_pc;
    
    int i;
    for (i = 0; i < STATEREGNUM; i++) {
        dest->s_reg[i] = source->s_reg[i];
    }
}

/*
* Function      :   switchContext
* Purpose       :   Updates the global currentProcess pointer, records the dispatch time,
*                   and loads the processor state from the given PCB to resume its execution.
* Parameters    :   curr_proc - pointer to the PCB of the process to be dispatched.
*/
void switchContext(pcb_PTR currProc) {
    /* Update the global pointer to the current process */
    currentProcess = currProc;
    
    /* Record the current Time-of-Day as the dispatch time */
    STCK(startTOD);
    
    /* Load the processor state from the PCB and transfer control */
    LDST(&(currProc->p_s));
}

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
    if (readyQueue == NULL) {
        if (processCount == 0) {
            HALT();  /* No processes remain; halt the system */
        } else if (softBlockCount > 0) {
            /* Processes exist but are all blocked: enable interrupts and disable the local timer */
            setSTATUS(getSTATUS() | ALLOFF | IMON | IECON);
            setTIMER(NEVER);                                    /* Prevent PLT from firing */
            WAIT();                                             /* Wait for an external interrupt */
        } else {
            /* Deadlock: processes exist but none are ready */
            PANIC();
        }
    }

    /* Ready queue is not empty: remove the head process and dispatch it */
    nextProcess = removeProcQ(&readyQueue);
    currentProcess = nextProcess;

    /* Load the time slice (5ms) into the processor's local timer */
    loadLocalTimer(TIMESLICE);

    /* Switch context to dispatch next process */
    switchContext(nextProcess);

    /* Should never reach here if LDST works correctly */
    PANIC();
}

/******************************* END OF SCHEDULER.c *******************************/
