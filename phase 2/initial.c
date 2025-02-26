/******************************* INITIAL.c ***************************************
 *
 * ! Recheck for RMATOP calculation + constants that set for the initial process creation
 * 
 * 
 * 
 *
 *****************************************************************************/

#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
/* #include "/usr/include/umps3/umps/libumps.h"*/

/*************************  NUCLEUS GLOBAL VARIABLES  ************************/

int processCount;                       /* Number of started, but not yet terminated processes */
int softBlockCount;                     /* Number of started, but not yet terminated that are in blocked state (due to an I/O or timer request) */
pcb_PTR readyQueue;                     /* Tail pointer for the ready queue */
pcb_PTR currentProcess;                 /* Pointer to the running process */
int deviceSemaphores[MAXDEVICES];       /* Semaphores for external devices & pseudo-clock */

/******************************* FUNCTION DECLARATION /*******************************/

extern void test();                     /* Given in the test file */
extern void uTLB_RefillHandler();       /* TLB-refill handler (stub as for Phase 2) */
extern void exceptionHandler();         /* Nucleus exception handler (implemented in exceptions.c) */

/******************************* FUNCTION IMPLEMENTATION *****************************/

int main() 
{
    /*--------------------------------------------------------------*
     * Local Variables Initialization
     *--------------------------------------------------------------*/
    int i;                              /* Loop index */
    unsigned int RAMTOP;                /* Top of RAM */
    devregarea_t *devRegArea;           /* Device Register Area */

    /* Calculate the RAMTOP */
    devRegArea = (devregarea_t *) RAMBASEADDR;  
    RAMTOP = devRegArea->rambase + devRegArea->ramsize;
    
    /*--------------------------------------------------------------*
     * Populate the Processor 0 Pass Up Vector
     *--------------------------------------------------------------*/
    passupvector_t *pv = (passupvector_t *) PASSUPVECTOR;
    pv->tlb_refill_handler  = (memaddr) uTLB_RefillHandler;
    pv->tlb_refill_stackPtr = NUSCLEUSSTACKTOP;                      /* Top of nucleus stack page */
    pv->exception_handler   = (memaddr) exceptionHandler;
    pv->exception_stackPtr  = NUSCLEUSSTACKTOP;                      /* Top of nucleus stack page */


    /*--------------------------------------------------------------*
     * Initialize Phase 1 Data Structures (pcb and ASL)
     *--------------------------------------------------------------*/
    initPcbs();
    initASL();


    /*--------------------------------------------------------------*
     * Initialize Nucleus Globals
     *--------------------------------------------------------------*/
    processCount   = 0;
    softBlockCount = 0;
    readyQueue     = mkEmptyProcQ();
    currentProcess = NULL;

    /* Initialize the device semaphores to 0 (synchronization, not mutual exclusion) */
    for (i = 0; i < MAXDEVICES; i++) {
        deviceSemaphores[i] = 0;
    }


    /*--------------------------------------------------------------*
     * Load Interval Timer for Pseudo-Clock (100 milliseconds)
     *--------------------------------------------------------------*/
    LDIT(100000);           /* 100,000 microseconds = 100 ms */


    /*--------------------------------------------------------------*
     * Instantiate the Initial Process and Place it in the Ready Queue
     *--------------------------------------------------------------*/
    pcb_PTR initialProc = allocPcb();
    if (initialProc == NULL) {
        PANIC();            /* Be *panic* if it can't even create one process */
    }
    /* Set up the initial processor state */
    initialProc->p_s.s_sp = RAMTOP;                             /* Set the stack pointer to the top of RAM */
    initialProc->p_s.s_pc = (memaddr) test;                     /* Start execution at test */
    initialProc->p_s.s_t9 = (memaddr) test;                     /* Set t9 to test as well */
    initialProc->p_s.s_status = ALLOFF | IEON | IMON | TEON;    /* Enable interrupts and timer */
    
    /* Set all the Process Tree fields to NULL */
    initialProc->p_prnt       = NULL;
    initialProc->p_child      = NULL;
    initialProc->p_sibNext    = NULL;
    initialProc->p_sibPrev    = NULL;

    /* Set the accumulated time field to zero */
    initialProc->p_time = 0;

    /* Set the blocking semaphore address to NULL */
    initialProc->p_semAdd = NULL;

    /* Set the Support Structure pointer to NULL */
    initialProc->p_supportStruct = NULL;

    /* Place the initial proc in the ready queue and increment the process count*/
    insertProcQ(&readyQueue, initialProc);          
    processCount++;                                 


    /*--------------------------------------------------------------*
     * Call the Scheduler
     *--------------------------------------------------------------*/
    scheduler();            /* Send control over to the scheduler */


    /* If the scheduler returns, something went wrong */
    PANIC();                /* Should never reach here if implement correctly */

    return -1;               /* Just to placate the compiler, also never reached */
}

/******************************* END OF INITIAL.c *******************************/
