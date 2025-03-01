/******************************* INTERRUPTS.c ***************************************
 *
 * This file implements various interrupt service routines and 
 * 
 * Written by Uyen Nguyen
 * Last updated: 2025/02/28
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

/****************************** GLOBAL VARIABLES ******************************/

cpu_t remainingTime;        /* Remaining time left on current process's quantum*/

/****************************  HELPER FUNCTION  *******************************/

/*
 * Function     :   findDeviceNumer
 * Purpose      :   Determine which specifc device triggered an interrupt on a given line.
 *                  The system's device register area maintains a bit map for each interrupt line.
 *                  This function extracts the bitmap corresponding to the provided interrupt line
 *                  (adjust for the predefined OFFSET (3)), then checks each bit to identify which
 *                  device (from DEV0 to DEV7) triggered the interrupt 
 * Parameters   :   lineNumber - The interrupt line number, within range [3..7]
 * Returns      :   The device number (0-7) that caused the interrupt  
 */
int findDeviceNumber(int lineNumber) {
    /* Map the base RAM address to the device register area structure */
    devregarea_t *devRegArea;
    devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Obtain the interrupt bitmap for the specific line (adjusted by OFFSET)*/
    unsigned int bitMap;
    bitMap = devRegArea->interrupt_dev[lineNumber - OFFSET];

    /* Check each device bit in order: if a bit is set, return that device number */
    if ((bitMap & DEV0INT) != ALLOFF) {
        /* Device 0 triggered the interrupt */
        return DEV0;
    } else if ((bitMap & DEV1INT) != ALLOFF) {
        /* Device 1 triggered the interrupt */
        return DEV1;
    } else if ((bitMap & DEV2INT) != ALLOFF) {
        /* Device 2 triggired the interrupt */
        return DEV2;
    } else if ((bitMap & DEV3INT) != ALLOFF) {
        /* Device 3 triggered the interrupt */
        return DEV3;
    } else if ((bitMap & DEV4INT) != ALLOFF) {
        /* Device 4 triggered the interrupt */
        return DEV4;
    } else if ((bitMap & DEV5INT) != ALLOFF) {
        /* Device 5 triggered the interrupt */
        return DEV5;
    } else if ((bitMap & DEV6INT) != ALLOFF) {
        /* Device 6 triggered the interrupt */
        return DEV6;
    } else {
        /* Device 7 triggered the interrupt */
        return DEV7;
    }
}

/*******************************  FUNCTION IMPLEMENTATION  *******************************/ 

/*
 * Function     :   nonTimerInterrupt
 * Purpose      :   This function handles interrupts from peripheral devices (interrupt line 3-7).
 *                  It retrieves the saved exception state, determines which interrupt line is active, 
 *                  identifies the specific device causing the interrupt, and then acknowledges the 
 *                  interrupt by sending an ACK command to the device. It also unblocks a process waiting
 *                  on the corresponding device semaphore, updates its status, and places it on the ready queue.
 *                  Finally, it returns control to the interrupted process (by loading its saved state) or 
 *                  calls the scheduler if no process is currently active.
 * Parameters   :   None
 */
void nonTimerInterrupt() {
    /* Local variables declaration */
    int lineNumber, deviceNumber, deviceIndex, statusCode;
    pcb_PTR unblockedProc;      

    /* Retrieve the saved processor state at the time of exception */
    state_PTR savedExceptionState;
    savedExceptionState = (state_PTR) BIOSDATAPAGE;

    /* Obtain the base address for device registers */
    devregarea_t *devRegArea;
    devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Determine which peripheral interrupt line is active (priority order: line 3 to 7) */
    if (((savedExceptionState->s_cause) & LINE3INT) != ALLOFF) {
        lineNumber = DISKINT;           /* Interrupt from disk device (line 3) */
    } else if (((savedExceptionState->s_cause) & LINE4INT) != ALLOFF) {
        lineNumber = FLASHINT;          /* Interrupt from flash device (line 4) */
    } else if (((savedExceptionState->s_cause) & LINE5INT) != ALLOFF) {
        lineNumber = NETWINT;           /* Interrupt from network device (line 5)*/
    } else if (((savedExceptionState->s_cause) & LINE6INT) != ALLOFF) {
        lineNumber = PRNTINT;           /* Interrupt from printer (line 6) */
    } else {
        lineNumber = TERMINT;           /* Otherwise, interrupt from terminal (line 7) */
    }

    /* Identify the specific device within the interrupt line that caused the interrupt */
    deviceNumber = findDeviceNumber(lineNumber);

    /* Calculate the index into the deviceSemaphores array and the device register area */
    deviceIndex = ((lineNumber - OFFSET) * DEVPERINT) + deviceNumber;
    
    /* For terminal device, check if it's a write or read*/
    if (lineNumber == LINE7) {
        if ((devRegArea->devreg[deviceIndex].t_transm_status & STATUSON) != READY) {
            /* It's a write interrupt */
            statusCode = devRegArea->devreg[deviceIndex].t_transm_status;  /* (2) Save */
            devRegArea->devreg[deviceIndex].t_transm_command = ACK;       /* (3) Ack */
            
            /* Perform a V operation */
            unblockedProc = removeBlocked(&deviceSemaphores[deviceIndex + DEVPERINT]);
            deviceSemaphores[deviceIndex + DEVPERINT]++;
        }

        else {
            /* It's a read interrupt */
            statusCode = devRegArea->devreg[deviceIndex].t_recv_status;    /* (2) Save */
            devRegArea->devreg[deviceIndex].t_recv_command = ACK;         /* (3) Ack */

            /* Perform a V operation */
            unblockedProc = removeBlocked(&deviceSemaphores[deviceIndex]);
            deviceSemaphores[deviceIndex]++;
        }

    /* For non-terminal device interrupt */
    } else {
        statusCode = devRegArea->devreg[deviceIndex].d_status;   /* (2) Save */
        devRegArea->devreg[deviceIndex].d_command = ACK;        /* (3) Ack */

        /* Perform a V operation */
        unblockedProc = removeBlocked(&deviceSemaphores[deviceIndex]);
        deviceSemaphores[deviceIndex]++;
    }

    /* Place the stored off status code in newly unblocked pcb's v0 register */
    if (unblockedProc != NULL) {
        unblockedProc->p_s.s_v0 = statusCode;
        
        /* Insert it on the ready queue */
        insertProcQ(&readyQueue, unblockedProc);

        /* Reduce the soft block count */
        softBlockCount--;
    }

    /* Return control to the current process */
    if (currentProcess != NULL) {
        setTIMER(remainingTime);
        LDST(savedExceptionState);
    }

    /* If there is no current process, call the scheduler */
    scheduler();                            /* This should never return */
    PANIC();    
}



void pltInterrupt() {
    /* Check if there is a running process at time of interrupt */
    if (currentProcess != NULL) {
        /* Load the PLT with a very large value (INFINITE) */        
        setTIMER(INFINITE);

        /* Copy the processor state at time of exception into current process pcb's */
        copyState((state_PTR) BIOSDATAPAGE, &(currentProcess->p_s));

        /* Update the accumulated CPU time */
        STCK(currentTOD);             
        currentProcess->p_time += (currentTOD - startTOD);

        /* Place the current process on ready queue */
        insertProcQ(&readyQueue, currentProcess);

        /* Set currentProccess to NULL since there is no currently running process until scheduler */
        currentProcess = NULL;

        /* Call the scheduler to dispatch the next process */
        scheduler();                        /* This should never return */

        /* If scheduler returns, something went wrong, be PANIC */
        PANIC();
    }
    /* If the currentProcess is NULL but there is an pltInterrupt, be PANIC */
    PANIC();
}



void intervalTimerInterrupt() {
    /* Temporary pointer for processes to be unblocked */
    pcb_PTR unblockedProc;

    /* Acknowledge the interrupt by reloading the Interval Timer with 100 milliseconds */
    LDIT(INITIALINTTIMER);

    /* Unblock all processes waiting on the pseudo-clock semaphore:
       Remove each process from the ASL for the pseudo-clock semaphore and insert it into the Ready Queue. */
    while (headBlocked(&deviceSemaphores[PCLOCKIDX]) != NULL) {
        /* Unlock the first pcb from the pseudo-clock semaphore's process*/
        unblockedProc = removeBlocked(&deviceSemaphores[PCLOCKIDX]);

        /* Place the unblockedProc onto the ready queue */
        insertProcQ(&readyQueue, unblockedProc);

        /* Decrease the soft block count */
        softBlockCount--;
    }

    /* Reset the pseudo-clock to zero to block SYS7 and
       ensure the pseudo-clock semaphore does not grow positive */
       deviceSemaphores[PCLOCKIDX] = 0;

    /* Return control to the current process (when there is actually a current process) */
    if (currentProcess != NULL) {
        LDST((state_PTR) BIOSDATAPAGE);         /* This should never return */
    }
    
    /* If there is no current process to resume, call scheduler */
    scheduler();                                /* This should never return */

    /* If scheduler returns, something went wrong, be *PANIC* */
    PANIC();
}

void interruptHandler() {
    /* Save the Time-Of-Day when an interrupt occurs */
    STCK(currentTOD);

    /* Store the remaining time left on current process's quantum */
    remainingTime = getTIMER();

    /* Retrieve the processor state at the time of exception */
    state_PTR savedExceptionState;
    savedExceptionState = (state_PTR) BIOSDATAPAGE;

    /* Given that Pandos is intended for uniprocessor environment, 
       interrupt line 0 (inter-processor interrupts) can be ignored */

    /* Check if the interrupt is from the PLT (interrupt line 1) */
    if (((savedExceptionState->s_cause) & LINE1INT) != ALLOFF) {
        pltInterrupt();
    }

    /* Check if the interrupt is from the Interval Timer (interrupt line 2) */
    else if (((savedExceptionState->s_cause) & LINE2INT) != ALLOFF) {
        intervalTimerInterrupt();
    }

    /* Otherwise, interrupts are from peripheral devices (interrupt line 3-7) */
    else {
        nonTimerInterrupt();
    }
}

/******************************* END OF INTERRUPTS.c *******************************/
