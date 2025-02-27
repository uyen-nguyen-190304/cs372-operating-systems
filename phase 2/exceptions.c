/******************************* EXCEPTIONS.c ***************************************
 *
 *  
 *
 *
 ***********************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initial.h"
/* #include "/usr/include/umps3/umps/libumps.h" */

/*******************************  GLOBAL VARIABLES  *******************************/

/* Nucleus global variables */
extern int processCount;
extern int softBlockCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currentProcess;
extern int deviceSemaphores[MAXDEVICES];

extern int startTOD;
extern int currentTOD;

/*******************************  FUNCTION DECLARATIONS  *******************************/

/* SYSCALL Exception Handling */
HIDDEN void createProcess(state_PTR initState, support_t *supportStruct);
HIDDEN void terminateProcess(pcb_PTR proc);
HIDDEN void passeren(int *semAdd);
HIDDEN void verhogen(int *semAdd);
HIDDEN void waitForIODevice(int lineNum, int devNum);
HIDDEN void getCPUTime();
HIDDEN void waitForClock();
HIDDEN void getSupportData();

/* */
HIDDEN void PassUpOrDie();

/*******************************  FUNCTION IMPLEMENTATION  *******************************/ 
/*
 ! SYS1  
 */
void createProcess(state_PTR initialState, support_t *supportStruct)
{
    /* Attempt to allocate a PCB from the pcbFree list*/
    pcb_PTR newPcb;
    newPcb = allocPcb();

    /* If there are enough resources to create a new process */
    if (newPcb != NULL) {
        /* Copy the initial processor state into the new PCB's state field */
        copyState(initialState, &(newPcb->p_s));

        /* Set the support structure pointer (or NULL if not provided) */
        newPcb->p_supportStruct = supportStruct;

        /* Link the new PCB as a child of the current process */
        insertChild(currentProcess, newPcb);

        /* Insert the new process into the ready queue */
        insertProcQ(&readyQueue, newPcb);

        /* Set the blocking semaphore pointer to NULL, since the process is not blocked */
        newPcb->p_semAdd = NULL;

        /* Initialize accumulated CPU time to zero */
        newPcb->p_time = 0;

        /* Increment the process count */
        processCount++;

        /* Return success (0) to the calling process in its v0 register */
        currentProcess->p_s.s_v0 = 0;
    }

    /* In case there are no more free pcbs */
    else {
        /* Return an error code (-1) in caller's v0 register */
        currentProcess->p_s.s_v0 = -1;
    }

    /* Clock current TOD to currentTOD */
    STCK(currentTOD);

    /* Update the accumulated CPU time for current process */
    currentProcess->p_time += (currentProcess - startTOD);

    /* Return control to the current process */
    switchContext(currentProcess);
}


/*
 ! SYS2 
 ! DO THIS AGAIN 
 */
void terminateProcess(pcb_PTR proc)
{
    /* Recursively terminate all progeny of proc */
    while (!(emptyChild(proc))) {
        /* While the process that will be terminated still has children */
        terminateProcess(removeChild(proc));
    }

    /* If proc has a parent, unlink it from the parent's child list */
    if (proc->p_prnt != NULL) {
        outChild(proc);
    }

    /* Determine if the proc is blocked on a semaphore or in the ready queue */
    if (proc->p_semAdd != NULL) {
        /* The proc is blocked on a semaphore */
        outBlocked(proc);       /* Remove the process from the ASL */

        /* Adjust semaphore or softBlockCount depending on semaphore type */
        if (proc->p_semAdd >= &deviceSemaphores[0] &&
            proc->p_semAdd <= &deviceSemaphores[MAXDEVICES - 1]) {
            /* If the process is blocked on a device semaphore */
            softBlockCount--;
        } else {
            /* If the process is blocked on a synchronization semaphore */
            (*proc->p_semAdd)++;
        }
    }

    /* Else, proc is on the ready queue */ 
    else {
        /* Remove it from the ready queue */
        outProcQ(&readyQueue, proc);
    }

    /* Return the PCB to the free list and update process count */
    freePcb(proc);
    processCount--;

    /* Setting the process pointer to NULL */
    proc = NULL;

    /* If terminating the currently running process, adjust and call scheduler */
    if (proc == currentProcess) {
        /* Adjust the currentProcess to NULL*/
        currentProcess = NULL;

        /* Call the scheduler */
        scheduler();

        /* If the scheduler() returns, something went wrong, be PANIC */
        PANIC();
    }
}


/*
 ! SYS3
 */
void passeren(int *semAdd) {
    /* Decrement the semaphore's value by 1 */
    (*semAdd)--;

    /* If the semaphore is less than 0, then the process will be blocked */
    if (*semAdd < 0) {
        /* Block the process by inserting it into the ASL for the given semaphore */
        insertBlocked(semAdd, currentProcess);
        
        /* Call the scheduler to dispatch another process */
        scheduler();            /* This should never return */

        /* If the scheduler() returns, something went wrong, be PANIC */
        PANIC();
    }

    /* Else, return to the current process */
    switchContext(currentProcess);
}   


/*
 ! SYS4 
 */
void verhogen(int *semAdd) {
    /* Increment the semaphore's value by 1 */
    (*semAdd)++;

    /* If the new value is less than or equal to 0, then one or more processes are waiting */
    if (*semAdd <= 0) {
        /* Remove the first blocked process from the semaphore's queue */
        pcb_PTR unblockedProc = removeBlocked(semAdd);      

        /* Insert the unblocked process into the ready queue */
        insertProcQ(&readyQueue, unblockedProc);
    }
    switchContext(currentProcess);
}


/*
 ! SYS5 
 */
void waitForIODevice(int lineNum, int deviceNum, int readBoolean) {
    int index;
    index = ((lineNum - OFFSET) * DEVPERINT) + deviceNum;
    
    /* decrement the semaphore */
    /* test if < 0*/
    /* update cpu time , update state */
    /* insert into blocked */
    /* scheduler */

    LDST(currentProcess);

}


/*
 ! SYS6
 */
void getCPUTime() {
    /* Clock current time of day into currentTOD */
    STCK(currentTOD);

    /* Calculate the elapsed time since dispatched and place in caller's v0 register */
    currentProcess->p_s.s_v0 = (currentProcess->p_time) + (currentTOD - startTOD);

    /* Update the accumulated CPU time*/
    currentProcess->p_time  += (currentTOD - startTOD);

    /* Return control to the current process */
    switchContext(currentProcess);
}


/*
 ! SYSC7
 */
void waitForClock() {
    /* 
     * Subtract 1 from the pclock sema4
     * Update state in the pcbs
     * Update CPU time
     * Insert Blocked
     * Scheduler
     * 
    */
    /* Initiate the semaphore pseudo-clock */
    int *pclockSem = &deviceSemaphores[MAXDEVICES - 1];         /* Pseudo-clock semaphore is stored at last index */  

    /* Decrement the pseudo-clock semaphore */
    (*pclockSem)--;

    /* Update the state of the current process */
    copyState()

    /* Update the CPU time for the current process */
    STCK(currentTOD);
    currentProcess->p_time += (currentTOD - startTOD);

    /* Insert the current process into blocked queue */
    insertBlocked(pclockSem, currentProcess);

    /* Increment the softBlockCount */
    softBlockCount++;

    /* Clear the currentProcess (since it's blocked) */
    currentProcess = NULL;

    /* Call the scheduler to dispatch the next process */
    scheduler();

    /* If the scheduler() returns, something went wrong, be PANIC */
    PANIC();    
}


/*
 ! SYS8
 */
void getSupportData() {
    /* Place the support structure pointer in v0 */
    currentProcess->p_s.s_v0 = (int)(currentProcess->p_supportStruct);

    /* Update CPU usage time*/
    STCK(currentTOD);                                   /* Read current Time-Of-Day */
    currentProcess->p_time += (currentTOD - startTOD);  /* Accumulate the elapsed time since dispatch*/

    /* Return control to the current process */
    switchContext(currentProcess);
}


/*
 ! Pass Up Or Die 
 ! Here, still missing savedExceptState to be defined somewhere
 */
void passUpOrDie(int exceptionCode) {
    /*--------------------------------------------------------------*
     * Pass Up Operation
     *--------------------------------------------------------------*/
    if (currentProcess->p_supportStruct != NULL) {
        /* 
         * Copy the saved exception state (from the BIOS Data Page) into the appropriate 
         * sup_exceptState field of the current process's support structure.
         */
        copyState(savedExceptState, &(currentProcess->p_supportStruct->sup_exceptState[exceptionCode]));
            
        /* Update the accumulated CPU time for the current process */
        STCK(currentTOD);
        currentProcess->p_time += (currentTOD - startTOD);

        /*
         * Pass up the exception by performing a LDCXT using the context stored in 
         * the corresponding sup_exceptContext field (which includes the stack pointer,
         * status register, and program counter for the exception handler).
         */
        LDCXT(currentProcess->p_supportStruct->sup_exceptContext[exceptionCode].c_stackPtr,
            currentProcess->p_supportStruct->sup_exceptContext[exceptionCode].c_status,
            currentProcess->p_supportStruct->sup_exceptContext[exceptionCode].c_pc);
    }

    /*--------------------------------------------------------------*
     * Die Operation
     *--------------------------------------------------------------*/
    else {
        /* 
         * The current process has no support structure and cannot handle the exception
         * Handle the exception as fatal -> terminate the current process and all its progeny
         */
        terminateProcess(currentProcess);       /* Call SYS2 handler */
        currentProcess = NULL;                  /* Set the currentProcess pointer to NULL */
        scheduler();                            /* Call the scheduler to dispatch the next process */
    }
}






void exceptionHandler() {




    switch (Cause.ExcCode) {
    }
}


/*
 * Function     :   programTrapExceptionHandler
 * Purpose      :   The function handles the Program Trap Exception.
 *                  A program trap exception occurs when the current process attempts to perform
 *                  some illegal or undefined action. In such case, the handler will perform a
 *                  standard Pass Up Or Die operation using GENERALEXCEPT index value.
 * Parameters   :   None 
 */
void programTrapExceptionHandler() {
    passUpOrDie(GENERALEXCEPT);
} 


/*
 * Function     :   TLBExceptionHandler
 * Purpose      :   The function handles the TLB Exception.
 *                  A TLB exception occurs when uMPS3 fails in an attempt to translate a logical
 *                  address to a physical address. A TLB exception is defined as an exception
 *                  with Cause.ExcCodes of 1-3. In such case, the handler will perform a standard
 *                  Pass Up Or Die operation using PGFAULTEXCEPT index value.
 * Parameters   :   None 
 */
void TLBExceptionHandler() {
    passUpOrDie(PGFAULTEXCEPT);
}

/******************************* END OF EXCEPTIONS.c *******************************/
