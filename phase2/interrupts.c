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

cpu_t remainingTime;


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
    int lineNumber, deviceNumber, index, statusCode;
    devregarea_t *devRegArea;
    pcb_PTR unblockedProc;

    /* Retrieve the saved processor state at the time of exception */
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

    /* Determine the specific device number causing the interrupt */
    deviceNumber = findDeviceNumber(lineNumber);

    /* Compute the index in the deviceSemaphores and device register array */
    index = ((lineNumber - OFFSET) * DEVPERINT) + deviceNumber;
    
    /* Initialize devRegArea pointer to the base address of device registers */
    devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Handle terminal device interrupts (line 7) specially:
       For a terminal, distinguish between a write (transmission) interrupt and a read (reception) interrupt. */
       if ((lineNum == LINE7) && (((temp->devreg[index].t_transm_status) & STATUSON) != READY)) {
        /* Terminal write interrupt: device is not ready (STATUS != READY) */
        statusCode = temp->devreg[index].t_transm_status;
        /* Acknowledge the transmission interrupt */
        temp->devreg[index].t_transm_command = ACK;
        /* Unblock the process waiting for a terminal write using an offset in the semaphore array */
        unblockedPcb = removeBlocked(&deviceSemaphores[index + DEVPERINT]);
        /* Perform the V operation: increment the semaphore */
        deviceSemaphores[index + DEVPERINT]++;
    }
    else {
        /* Either non-terminal or terminal read interrupt */
        statusCode = temp->devreg[index].t_recv_status;
        /* Acknowledge the reception interrupt */
        temp->devreg[index].t_recv_command = ACK;
        /* Unblock the process waiting for a terminal read */
        unblockedPcb = removeBlocked(&deviceSemaphores[index]);
        /* Increment the semaphore */
        deviceSemaphores[index]++;
    }


    /* If a process was unblocked by this interrupt, update its return value and move it to the ready queue */
    if (unblockedProc != NULL) { 
        /* Return the device's status in register v0 */
        unblockedProc->p_s.s_v0 = statusCode;

        /* Insert the unblocked process into the ready queue so it can be dispatched later */
        insertProcQ(&readyQueue, unblockedProc);

        /* Decrement the soft block count as one process has been unblocked */
        softBlockCount--;
    }

    /* If a process is still currently running, resume its execution */
    if (currentProcess != NULL) {
        setTIMER(remainingTime);
        LDST(savedExceptionState);
    }

    /* Otherwise, invoke the scheduler to dispatch the next process */
    scheduler();                /* This should never return */

    /* If the scheduler returns, something went wrong, be PANIC */
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
