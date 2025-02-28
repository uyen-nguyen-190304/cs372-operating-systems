/******************************* INITIAL.c ***************************************
 * 
 * This module serves as the entry point for Phase 2 of the Pandos operating system.
 * It initializes the kernel's data structure (PCBs and ASL), sets up global variables
 * that provide kernel's shared state configures the exception vectors (for handling 
 * interrupts, TLB exceptions, system calls and program traps), and creates the 
 * initial process that begins executing the test() function. Once the initial process 
 * is placed on the ready queue, the scheduler is called to dispatch processes.
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/02/28 
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

/*************************  NUCLEUS GLOBAL VARIABLES  ************************/

int processCount;                       /* Number of started, but not yet terminated processes */
int softBlockCount;                     /* Number of started, but not yet terminated that are in blocked state (due to an I/O or timer request) */
pcb_PTR readyQueue;                     /* Tail pointer for the ready queue */
pcb_PTR currentProcess;                 /* Pointer to the running process */
int deviceSemaphores[MAXDEVICES];       /* Semaphores for external devices & pseudo-clock */

/******************************* EXTERNAL ELEMENTS *******************************/

extern void test();                     /* Given in the test file */
extern void uTLB_RefillHandler();       /* TLB-refill handler (stub as for Phase 2) */
HIDDEN void generalExceptionHandler();  /* Declaration of the general exception handler */

/******************************* FUNCTION IMPLEMENTATION *****************************/

/* 
 * Function     :   generalExceptionHandler
 * Purpose      :   This handler is invoked when an exception occurs. It retrieves the 
 *                  saved processor state from the BIOS data page, extracts the exception
 *                  code, and dispatches control to the appropriate handler:
 *                  - For interrupts (exception code 0), it calls interruptHandler()
 *                  - For TLB exceptions (exception code 1-3), it calls TLBExceptionHandler()
 *                  - For system calls (exception code 8), it calls syscallExceptionHandler()
 *                  - For all other exceptions, it calls programTrapExceptionHandler()
 * Parameters   :   None
 */
void generalExceptionHandler() {
    /* Retrieve the saved state from the BIOS Data Page */
    state_PTR savedExceptionState;
    savedExceptionState = (state_PTR) BIOSDATAPAGE;        

    /* Extract the exception code from cause register */
    int exceptionCode;
    exceptionCode = (savedExceptionState->s_cause & GETEXCEPTIONCODE) >> CAUSESHIFT;   

    /* Dispatch to the correct handler based on the exception code */
    if (exceptionCode == INTCONST) {
        /* Code 0: Interrupts -> pass to interrupt handler */
        interruptHandler();
    } else if (exceptionCode >= TLBMIN && exceptionCode <= TLBMAX) {
        /* Code 1-3: TLB exceptions -> pass to TLB exception handler */
        TLBExceptionHandler();
    } else if (exceptionCode == SYSCALLCONST) {
        /* Code 8: SYSCALL -> pass to SYSCALL exception handler */
        syscallExceptionHandler();
    } else {
        /* Code 4-7, 9-12: Program Traps -> Pass to Program Trap Handler */
        programTrapExceptionHandler();
    }
}

/*
 * Function     :   main
 * Purpose      :   The kernel's entry point. This function perform the following steps:
 *                  1. Populate the Processor 0 Pass Up Vector with the address of the 
 *                     TLB-refill handler and the general exception handler, and their
 *                     corresponding stack pointers (set to NUCLEUSSTACKTOP)
 *                  2. Initialize Phase 1 data structures: the free list of PCBs and the ASL
 *                  3. Initialize nucleus global variables: processCount (0), softBlockCount (0),
 *                     readyQueue (NULL), currentProcess (NULL), and deviceSemaphores
 *                  4. Load the system-wide interval timer with a 100-milisecond interval
 *                  5. Create the initial process, set up its processor state (stack pointer, PC, status),
 *                     and insert it into the ready queue
 *                  6. Increment the processCount to reflect the new process
 *                  7. Call the scheduler to dispatch processes
 *                  8. If the scheduler return (which should not), PANIC is called
 * Parameters   :   None
 * Return       :   Never returns normally
 *                  Return -1 if the execution reaches the final return statement, signifying something went wrong
 */
int main() 
{
    /*--------------------------------------------------------------*
     * Local Variables Initialization
     *--------------------------------------------------------------*/
    int i;                              /* Loop index */
    memaddr ramtop;                     /* Top of RAM */
    devregarea_t *devRegArea;           /* Pointer to device register area */
    
    /*--------------------------------------------------------------*
     * Populate the Processor 0 Pass Up Vector
     *--------------------------------------------------------------*/
    passupvector_t *pv = (passupvector_t *) PASSUPVECTOR;
    pv->tlb_refill_handler  = (memaddr) uTLB_RefillHandler;         /* Set the TLB-refill handler's address */
    pv->tlb_refill_stackPtr = NUCLEUSSTACKTOP;                      /* Set its stack pointer to the top of the nucleus stack */
    pv->exception_handler   = (memaddr) generalExceptionHandler;    /* Set the general exception handler */
    pv->exception_stackPtr  = NUCLEUSSTACKTOP;                      /* Set its stack pointer to the top of the nucleus stack */


    /*--------------------------------------------------------------*
     * Initialize Phase 1 Data Structures (pcb and ASL)
     *--------------------------------------------------------------*/
    initPcbs();             /* Initialize the free list of PCBs */
    initASL();              /* Initialize the Active Semaphore List and its dummy node */


    /*--------------------------------------------------------------*
     * Initialize Nucleus Globals
     *--------------------------------------------------------------*/
    processCount   = 0;                     /* Set the process count to zero */
    softBlockCount = 0;                     /* Set the count of blocked processes to zero */
    readyQueue     = mkEmptyProcQ();        /* Initialize the ready queue as empty */
    currentProcess = NULL;                  /* No process is currently running */

    /* Initialize the device semaphores to 0. These semaphores are used for 
       synchronization with external devices and the pseudo-clock */
    for (i = 0; i < MAXDEVICES; i++) {
        deviceSemaphores[i] = 0;
    }


    /*--------------------------------------------------------------*
     * Load Interval Timer for Pseudo-Clock (100 milliseconds)
     *--------------------------------------------------------------*/
    /* Load the interval timer with INITIALINTTIMER (100,000 microseconds = 100ms) */
    LDIT(INITIALINTTIMER);           


    /*--------------------------------------------------------------*
     * Instantiate the Initial Process and Place it in the Ready Queue
     *--------------------------------------------------------------*/
    pcb_PTR initialProc;
    initialProc = allocPcb();           /* Allocate a new PCB from the free list */
    if (initialProc == NULL) {
        PANIC();                        /* Be *panic* if it can't even create one process */
    }

    /* Calculate the ramtop */
    devRegArea = (devregarea_t *) RAMBASEADDR; 

    /* The top of RAM is calculated by adding the base address of RAM to its size */
    ramtop = devRegArea->rambase + devRegArea->ramsize;

    /* Set up the initial processor state */
    initialProc->p_s.s_sp = ramtop;                                 /* Set the stack pointer to the top of RAM */
    initialProc->p_s.s_pc = (memaddr) test;                         /* Start execution at test */
    initialProc->p_s.s_t9 = (memaddr) test;                         /* Set t9 to test as well */
    initialProc->p_s.s_status = ALLOFF | IEPON | PLTON | IMON;      /* Enable interrupts and timer */
    
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

    /* Insert the initial process into the ready queue and increment the process count */
    insertProcQ(&readyQueue, initialProc);          
    processCount++;                                 


    /*--------------------------------------------------------------*
     * Call the Scheduler
     *--------------------------------------------------------------*/
    /* Send control over to scheduler to dispatch the next process */
    scheduler();            


    /* If the scheduler returns, something went wrong */
    PANIC();                 /* Should never reach here */

    return -1;               /* Just to placate the compiler, also never reached */
}

/******************************* END OF INITIAL.c *******************************/
