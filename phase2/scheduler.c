/******************************* SCHEDULER.c ***************************************
 *
 * This module implements a preemptive round-robin scheduler with a 5ms time slice.
 * Its primary responsibilities are:
 *  - Dispatch processes from the ready queue so that each ready process gets a chance
 *    to execute
 *  - Track CPU time for processes using the global variables startTOD and currentTOD
 *  - Handle idle conditions:
 *      a) If no processes remain (processCount == 0), the system halts
 *      b) If processes exist but all are blockedd (softBlockedCount > 0), the scheduler
 *         waits for an external interrupt
 *      c) If processes exist but none are ready (deadlock), the system panic
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/02/28 
 *  
 ***********************************************************************************/

#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "/usr/include/umps3/umps/libumps.h"

/*******************************  GLOBAL VARIABLES  *******************************/

/* Global variables used for CPU time tracking */
cpu_t startTOD;         /* Time when the current process was dispatched */
cpu_t currentTOD;       /* Temporary variable  to store the current TOD for CPU time accounting */

/*******************************  HELPER FUNCTION  *******************************/

/*
 * Function      :   copyState
 * Purpose       :   Copies the processor state from the source to the destination
 *                   This function is used to save or restore the processor state during context switches
 * Parameters    :   source - pointer to the source state from which to copy
 *                   dest   - pointer to the destination state where the state will be copied
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

/******************************* SCHEDULING IMPLEMENTATION *******************************/

/*
 * Function      :   scheduler
 * Purpose       :   Implements a round-robin scheduler with a 5ms time slice.
 *                   - If the ready queue is not empty, it dispatches the next process in round-robin fashion
 *                   - If the ready queue is empty:
 *                      a) If no processes remain (processCount = 0), it halts the system
 *                      b) If processes exists but are all blocked (softBlockCount > 0), it disable the 
 *                         local timer by loading a very large value and enable interrupts to wait 
 *                         for an external interruption to unblock a process
 *                      c) If processes exist but none are ready (indicating deadlock), it panics
 * Parameters    :   None
*/
void scheduler() {
    /* Pointer to hold the next process to be dispatched dispatch */
    pcb_PTR nextProcess;  
    
    /* Check if the ready queue is empty */
    if (emptyProcQ(readyQueue)) {
        if (processCount == 0) {
            /* No processes remain; halt the system */
            HALT();  
        } 
        
         /* Processes exist but are all blocked */
        else if (softBlockCount > 0) {
            /* Disable the local timer, enable interrupts, and wait for an external interrupt to unblock a process */
            setSTATUS(ALLOFF | IMON | IECON);                   /* Enable interrupts */
            setTIMER(INFINITE);                                 /* Prevent PLT from firing */
            WAIT();                                             /* Wait for an external interrupt */
        } 
        
        /* Deadlock: processes exist but none are ready */
        else {
            /* System panics */
            PANIC();
        }
    }

    /* Ready queue is not empty: remove the next process for execution */
    nextProcess = removeProcQ(&readyQueue);
    currentProcess = nextProcess;

    /* Record the dispatch time for CPU time accounting */
    STCK(startTOD);

    /* Set the processor local timer to initial time slice (5ms) */
    setTIMER(INITIALPLT);

    /* Load the state of the next process, transferring control to it */
    LDST(&(currentProcess->p_s));                                  /* This should never return if context switching is successful */

    /* If control reaches here, something went wrong with LDST */
    PANIC();
}

/******************************* END OF SCHEDULER.c *******************************/
