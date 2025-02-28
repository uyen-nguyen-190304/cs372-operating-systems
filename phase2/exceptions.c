/******************************* EXCEPTIONS.c ***************************************
 * 
 * This module 
 *  
 *
 * Written by Uyen Nguyen
 * Last updated: 2025/02/28
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
#include "/usr/include/umps3/umps/libumps.h"

/*******************************  GLOBAL VARIABLES  *******************************/

/* CPU timer global variable */
extern int startTOD;
extern int currentTOD;

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

    /* Load the processor state at time SYSCALL was executed */
    LDST((state_PTR) BIOSDATAPAGE);
}


/*
 ! SYS2 
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
        
        /* Copy the saved processor state into current process's pcb */        
        state_PTR savedExceptionState;
        savedExceptionState = (state_PTR) BIOSDATAPAGE;
        copyState(savedExceptionState, &(currentProcess->p_s));

        /* Update the accumulated CPU time for currentProcess */
        STCK(currentTOD);
        currentProcess->p_time += (currentTOD - startTOD);

        /* Call the scheduler to dispatch another process */
        scheduler();                    /* This should never return */

        /* If the scheduler() returns, something went wrong, be PANIC */
        PANIC();
    }

    /* Load the processor state at time SYSCALL was executed */
    LDST((state_PTR) BIOSDATAPAGE);
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
        pcb_PTR unblockedProc;
        unblockedProc = removeBlocked(semAdd);      

        /* Insert the unblocked process into the ready queue */
        insertProcQ(&readyQueue, unblockedProc);
    }

    /* Load the processor state at time SYSCALL was executed */
    LDST((state_PTR) BIOSDATAPAGE);
}


/*
 ! SYS5 
 */
void waitForIODevice(int lineNum, int deviceNum, int readBoolean) {
    /* Calculate the index in deviceSemaphores associated with the device requesting IO */
    int index;
    index = ((lineNum - OFFSET) * DEVPERINT) + deviceNum;
    
    /* If the device is terminal device and it  is waiting for a write operation */
    if (lineNum == LINE7 && readBoolean == FALSE) {
        /* Increment the index, given that write semaphore is DEVPERINT (8) indices ahead */
        index += DEVPERINT;
    }

    /* Decrement the semaphore by 1 (P operation) */
    (deviceSemaphores[index])--;   

    /* For synchronous IO, the semaphore should now be negative, indicating that the process must be blocked */
    if (deviceSemaphores[index] < 0) {
        /* Update the CPU usage time for the current process */
        STCK(currentTOD);
        currentProcess->p_time += (currentTOD - startTOD);

        /* Copy the saved processor state into current process's pcb */
        state_PTR savedExceptionState;
        savedExceptionState = (state_PTR) BIOSDATAPAGE;
        copyState(savedExceptionState, &(currentProcess->p_s));

        /* Block the current process on the device semaphore's ASL */
        insertBlocked(&(deviceSemaphores[index]), currentProcess);

        /* Clear currentProcess since it's blocked */
        currentProcess = NULL;

        /* Call the scheduler to dispatch the next process */
        scheduler();                /* This will never return */

        /* If the scheduler returns, something went wrong, be PANIC */
        PANIC();
    }

    /* If the semaphore does not go negative (unlikely for synchronous IO), 
       load the processor state at time SYSCALL was executed */
    LDST((state_PTR) BIOSDATAPAGE);
}


/*
 ! SYS6
 */
void getCPUTime() {
    /* Read the current Time-Of-Day into currentTOD */
    STCK(currentTOD);

    /* Calculate elapsed time since dispatch and place the total CPU time in v0 */
    currentProcess->p_s.s_v0 = currentProcess->p_time + (currentTOD - startTOD);

    /* Update the accumulated CPU time for the current process */
    currentProcess->p_time += (currentTOD - startTOD);

    /* Update startTOD for the next time slice */
    STCK(startTOD);

    /* Load the processor state at time SYSCALL was executed */
    LDST((state_PTR) BIOSDATAPAGE);
}


/*
 ! SYSC7
 */
void waitForClock() {
    /* Obtain a pointer to the pseudo-clock semaphore */
    int *pclockSem = &deviceSemaphores[MAXDEVICES - 1];         /* Pseudo-clock semaphore is stored at last index */  

    /* Decrement the pseudo-clock semaphore */
    (*pclockSem)--;

    /* Copy the saved processor state into current process's pcb */
    state_PTR savedExceptionState;
    savedExceptionState = (state_PTR) BIOSDATAPAGE;    
    copyState(savedExceptionState, &(currentProcess->p_s));

    /* Update the accumulated CPU usage time for the current process */
    STCK(currentTOD);
    currentProcess->p_time += (currentTOD - startTOD);

    /* Copy the saved processor state into current process's pcb  */
    copyState(savedExceptionState, &(currentProcess->p_s));

    /* Insert the current process into the blocked queue */
    insertBlocked(pclockSem, currentProcess);

    /* Increment the softBlockCount */
    softBlockCount++;

    /* Clear the currentProcess (since it's blocked) */
    currentProcess = NULL;

    /* Call the scheduler to dispatch the next process */
    scheduler();                /* This should never return */

    /* If the scheduler() returns, something went wrong, be PANIC */
    PANIC();    
}


/*
 ! SYS8
 */
void getSupportData() {
    /* Place the support structure pointer in v0 */
    currentProcess->p_s.s_v0 = (int)(currentProcess->p_supportStruct);

    /* Return control to the current process */
    LDST(&(currentProcess->p_s));
}


/*
 ! Pass Up Or Die 
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
        state_PTR savedExceptionState;
        savedExceptionState = (state_PTR) BIOSDATAPAGE;
        copyState(savedExceptionState, &(currentProcess->p_supportStruct->sup_exceptState[exceptionCode]));
            
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



void syscallExceptionHandler() {
    /* Retrieve the processor state at the time of exception */
    state_PTR savedExceptionState;
    savedExceptionState = (state_PTR) BIOSDATAPAGE;

    /* Retrieve the system call number from the saved state */
    int sysNum; 
    sysNum = savedExceptionState->s_a0;

    /* Increment the PC by WORDLEN (4) to avoid infinite SYSCALL loop */
    savedExceptionState->s_pc = savedExceptionState->s_pc + WORDLEN;
     
    /* Check if the SYSCALL was requested while in user-mode */
    if ((savedExceptionState->s_status & USERPON) != ALLOFF) {
        /* Set the Cause */
        savedExceptionState->s_cause = (savedExceptionState->s_cause) & RESERVEDINSTRUCTION;
    
        /* Handle it as a Program Trap */
        programTrapExceptionHandler();
    }
    
    /* Dispatch the SYSCALL based on the syscall number in a0 */
    switch (sysNum) {
        /* SYS1: Create process */
        case SYS1CALL: 
            /* a1: Pointer to the initial state
               a2: Pointer to the support structure */
            createProcess((state_PTR) currentProcess->p_s.s_a1, (support_t *) currentProcess->p_s.s_a2);
            break;

        /* SYS2: Terminate process (and all its progeny )*/
        case SYS2CALL:
            /* Invoke the SYS2 handler */
            terminateProcess(currentProcess);

            /* Set the currentProcess pointer to NULL */

            currentProcess = NULL;
            /* Call the scheduler to dispatch the next process */
            scheduler();            /* This should never return */

            /* If the scheduler() returns, something went wrong, be PANIC */
            PANIC();            
            break;

        /* SYS3: P operator */
        case SYS3CALL:
            /* a1: Address of the semaphore to be P'ed */
            passeren((int *) currentProcess->p_s.s_a1);
            break;

        /* SYS4: V operator */    
        case SYS4CALL:
            /* a1: Address of the semaphore to be V'ed */
            verhogen((int *) currentProcess->p_s.s_a1);
            break;

        /* SYS5: Wait for IO Device */
        case SYS5CALL:
            waitForIODevice(currentProcess->p_s.s_a1, currentProcess->p_s.s_a2, currentProcess->p_s.s_a3);
            break;

        /* SYS6: Get CPU time*/
        case SYS6CALL:
            getCPUTime();
            break;

        case SYS7CALL:
            waitForClock();
            break;

        /* SYS8: Get Support Data */
        case SYS8CALL:
            getSupportData();
            break;
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
