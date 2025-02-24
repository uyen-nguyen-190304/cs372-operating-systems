/******************************* PCB.c ***************************************
 *
 * 
 * 
 * 
 * 
 *
 *****************************************************************************/



extern int processCount;
extern int softBlockCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currentProcess;




#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/scheduler.h"


void scheduler()
{
    pcb_t *nextProcess;             /* Next process to dispatch in the Ready Queue */

    if (readyQueue == NULL) {
        if (processCount == 0) {
            /* No processes remained; system is done */
            HALT();
        } 
        else if (softBlockCount > 0) {
            /* Need to add more here */
            WAIT();
        }
        else {
            /* Deadlock: processes exist but none are ready for waiting on devices */
            PANIC();
        }
    }

    /* Remove the head process from the Ready Queue */
    nextProcess = removeProcQ(&readyQueue);
    currentProcess = nextProcess;

    /* Load the time slice (5ms) into the processor's Local Timer */
    loadLocalTimer(TIME_SLICE_MICROSECONDS);

    /* Dispatch the process by loading its saved processor state */
    /* LDST() transfers control to the process and does not return */
    LDST(&(nextProcess->p_s));


    /* Should never reach here; it it does, PANIC */
    PANIC();
}