#include "../h/const.h"
#include "../h/types.h"
#include "/usr/include/umps3/umps/libumps.h"





HIDDEN void createProcess(state_PTR initState, support_t *supportStruct);
HIDDEN void terminateProcess(pcb_PTR proc);


HIDDEN void getCPUTime();
HIDDEN void waitForIO();
HIDDEN void getSupportData()

/*
 * 
 * SYS1
 *  
 */
void createProcess(state_PTR initState, support_t *supportStruct)
{
    /* Attempt to allocate a PCB from the pcbFree list*/
    pcb_PTR newPcb;
    newPcb = allocPcb();

    /* If there are enough resources to create a new process */
    if (newPcb != NULL) {
        /* Initialize the state of the newPcb */
        
        /*  */

        /*  */
        insertChild(currentProcess, newPcb);



        currentProc->p_s.s_v0 = SUCCESSCONST;

        processCount++;
    }

    /* In case there are no more free pcbs */
    else {
        /* Place an error code of -1 into the caller's v0 */
        currentProc->p_s.s_v0 = ERRORCONST;
    }
    

}



/*
 * 
 * SYS2
 *  
 */
void terminateProcess(pcb_PTR proc)
{
    /* Recursively terminating all progeny of the current process */
    while (!(emptyChild(proc))) {
        /* While the process that will be terminated still has children */
        terminateProcess(removeChild(proc));
    }

    /* If proc has a parent, unlink it from the parent's child list */
    if (proc->p_prnt != NULL) {
        outChild(proc);
    }

    /* Figure out if the proc is blocked on a semaphore or in the ready queue */
    if (proc->p_semAdd != NULL) {
        /* The proc is blocked on some semaphore */
        outBlocked(proc);       /* Remove the process from the ASL */

        /* Figure out if it is a device semaphore or normal semaphore */
        if (proc->p_semAdd >= &deviceSemaphores[0] &&
            proc->p_semAdd <= &deviceSemaphores[MAXDEVICES - 1]) {
            /* If the process is blocked on a device semaphore */
            softBlockCount--;
        } else {
            /* If the process is blocked on a normal semaphore */
            (*proc->p_semAdd)++;
        }
    } else {
        /* if p_semAdd == NULL, then proc is on the ready queue */
        outProcQ(&readyQueue, proc);
    }
    /* Return the PCB to the free list and decrement processCount */
    freePcb(proc);
    processCount--;

    /* Setting the process pointer to NULL */
    proc = NULL;
}



/*
 * 
 * SYS8
 *  
 */





