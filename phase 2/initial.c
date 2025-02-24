/******************************* PCB.c ***************************************
 *
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
#include "/usr/include/umps3/umps/libumps.h"

/*************************  NUCLEUS GLOBAL VARIABLES  ************************/

int processCount;                       /*  */
int softBlockCount;                     /*  */
pcb_PTR readyQueue;                     /*  */
pcb_PTR currentProcess;                 /*  */
int deviceSemaphores[MAXDEVICES];       /*  */

/******************************* FUNCTION IMPLEMENTATION *****************************/

int main() 
{
    /*--------------------------------------------------------------*
     * Populate the Processor 0 Pass Up Vector
     *--------------------------------------------------------------*/
    passupvector_t *pv = (passupvector_t *) PASSUPVECTOR;
    pv->tlb_refill_handler  = (memaddr) uTLB_RefillHandler;
    pv->tlb_refill_stackPtr = NUCLEUSSTACKTOP;
    pv->exception_handler   = (memaddr) generalExceptionHandler;
    pv->exception_stackPtr  = NUCLEUSSTACKTOP;


    /*--------------------------------------------------------------*
     * Initialize Phase 1 Data Structures (pcb and ASL)
     *--------------------------------------------------------------*/
    initPcbs();
    initASL();


    /*--------------------------------------------------------------*
     * Initialize Nucleus Globals
     *--------------------------------------------------------------*/
    processCount = 0;
    softBlockCount = 0;
    readyQueue = mkEmptyProcQ();
    currentProcess = NULL;

    /* Initialize the device semaphores to 0 (synchronization, not mutual exclusion) */
    int i;
    for (i = 0; i < MAXDEVICES; i++) {
        deviceSemaphores[i] = 0;
    }


    /*--------------------------------------------------------------*
     * Load Interval Timer for Pseudo-Clock (100 milliseconds)
     *--------------------------------------------------------------*/
    LDIT(100000);           /* 100,000 microseconds = 100 ms */


    
    /*--------------------------------------------------------------*
     * Instantiate the Initial Process
     *--------------------------------------------------------------*/
    pcb_PTR initialProc = allocPcb();


    /* Set up the initial processor state */


    /*--------------------------------------------------------------*
     * Call the Scheduler
     *--------------------------------------------------------------*/
    scheduler();            /* Send control over to the scheduler */

    /* If the scheduler returns, something went wrong */
    PANIC();                /* Should never reach here if implement correctly */

    return 0;
}
