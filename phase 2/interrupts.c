/******************************* INTERRUPTS.c ***************************************
 *
 * 
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

/*******************************  HELPER FUNCTION  *******************************/

int findDeviceNumber(int lineNumber) {
    devregarea_t *devRegArea;
    unsigned int bitMap;

    devRegArea = (devregarea_t *) RAMBASEADDR;
    bitMap = devRegArea->interrupt_dev[lineNumber - OFFSET];

    if ((bitMap & DEV0INT) != ALLOFF) {
        return DEV0;
    }

    else if ((bitMap & DEV1INT) != ALLOFF) {
        return DEV1;
    }

    else if ((bitMap & DEV2INT) != ALLOFF) {
        return DEV2;
    }

    else if ((bitMap & DEV3INT) != ALLOFF) {
        return DEV3;
    }

    else if ((bitMap & DEV4INT) != ALLOFF) {
        return DEV4;
    }

    else if ((bitMap & DEV5INT) != ALLOFF) {
        return DEV5;
    }

    else if ((bitMap & DEV6INT) != ALLOFF) {
        return DEV6;
    }

    else {
        return DEV7;
    }
}

/*******************************  FUNCTION IMPLEMENTATION  *******************************/ 

void nonTimerInterrupt() {
    /* Local variables declaration */
    cpu_t currentTOD;
    int lineNumber, deviceNumber, index, statusCode;
    devregarea_t *devRegArea;
    pcb_PTR unblockedProc;

    /* Retrieve the processor state at the time of exception */
    state_PTR savedExceptionState;
    savedExceptionState = (state_PTR) BIOSDATAPAGE;

    /* Determine the highest-priority interrupting line (3-7) */
    if (((savedExceptionState->s_cause) & LINE3INT) != ALLOFF) {
        lineNumber = DISKINT;
    } else if (((savedExceptionState->s_cause) & LINE4INT) != ALLOFF) {
        lineNumber = FLASHINT;
    } else if (((savedExceptionState->s_cause) & LINE5INT) != ALLOFF) {
        lineNumber = NETWINT;
    } else if (((savedExceptionState->s_cause) & LINE6INT) != ALLOFF) {
        lineNumber = PRNTINT;
    } else {
        lineNumber = TERMINT;
    }

    /* Find the device number */
    deviceNumber = findDeviceNumber(lineNumber);
    index = ((lineNumber - OFFSET) * DEVPERINT) + deviceNumber;
 
    /* Handling Terminal Devices (Line 7) */
    if (lineNumber == LINE7) {
        if (devRegArea->devreg[index].t_transm_status & STATUSON) {
            /* Transmission (Write) Interrupt */
            statusCode = devRegArea->devreg[index].t_transm_status;
            devRegArea->devreg[index].t_transm_command = ACK;
            unblockedProc = removeBlocked(&deviceSemaphores[index + DEVPERINT]);
            deviceSemaphores[index + DEVPERINT]++;
        } else {
            /* Reception (Read) Interrupt */
            statusCode = devRegArea->devreg[index].t_recv_status;
            devRegArea->devreg[index].t_recv_command = ACK;
            unblockedProc = removeBlocked(&deviceSemaphores[index]);
            deviceSemaphores[index]++;
        }
    } else {
        /* Non-Terminal Device Interrupt */
        statusCode = devRegArea->devreg[index].d_status;
        devRegArea->devreg[index].d_command = ACK;
        unblockedProc = removeBlocked(&deviceSemaphores[index]);
        deviceSemaphores[index]++;
    }

    /* If a process was unblocked, move it to the Ready Queue */
    if (unblockedProc != NULL) { 
        unblockedProc->p_s.s_v0 = statusCode;
        insertProcQ(&readyQueue, unblockedProc);
        softBlockCount--;
    }

    if (currentProcess != NULL) {
        LDST(savedExceptionState);
    }
    scheduler();

    PANIC();
}



void pltInterrupt() {
    /* Check if there is a running process at time of interrupt */
    if (currentProcess != NULL) {
        /* Local variable used to hold TOD when interruption occurs */
        cpu_t interruptTOD;

        /* Load the PLT with a very large value (INFINITE) */        
        STCK(INFINITE);

        /* Copy the processor state at time of exception into current process pcb's */
        copyState((state_PTR) BIOSDATAPAGE, &(currentProcess->p_s));

        /* Update the accumulated CPU time */
        STCK(interruptTOD);             
        currentProcess->p_time += (interruptTOD - startTOD);

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
    LDIT(INTERVALTIME);

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
        LDST((state_PTR) BIOSDATAPAGE)
    }
    
    /* If there is no current process to resume, call scheduler */
    scheduler();

    /* If scheduler returns, something went wrong, be *PANIC* */
    PANIC();
}

void interruptHandler() {
    /* Save the Time-Of-Day when an interrupt occurs */
    cpu_t interruptTOD;
    STCK(interruptTOD);

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
