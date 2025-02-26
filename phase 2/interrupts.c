/******************************* INITIAL.c ***************************************
 *
 * 
 * 
 * 
 *
 *****************************************************************************/

#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "/usr/include/umps3/umps/libumps.h"


HIDDEN void nonTimerInterrupt();
HIDDEN void pltInterrupt();
HIDDEN void intervalTimerInterrupt();




void pltInterrupt() {
    /* Local variable to hold the TOD at the time of exception */
    cpu_t currentTOD;

    /* Check if there is a running process at time of interrupt */
    if (currentProcess != NULL) {
        /* Load the timer with MAXPLT amount to */

        /* Copy the processor state of BIOS Data Page into current process pcb's */

        /* Update the accumulated CPU time */
        STCK(currentTOD);
        currentProcess->p_time += (currentTOD - startTOD);

        /* Place the current process on ready queue */
        insertProcQ(&readyQueue, currentProcess);

        /* Set currentProccess to NULL since there is no currently running process until scheduler */
        currentProcess = NULL;

        /* Call the scheduler to dispatch the next process */
        scheduler();
    }
    PANIC();
}



void intervalTimerInterrupt() {
    

}

