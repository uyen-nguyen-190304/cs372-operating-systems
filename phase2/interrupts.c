/******************************* INTERRUPTS.c ***************************************
 *
 * This module implements the kernel’s interrupt handling mechanism for a uniprocessor system 
 * based on the UMPS/MIPS architecture. Its primary responsibilities include managing interrupts 
 * from peripheral devices, processor local timer (PLT), and interval timer (pseudo-clock). 
 * For peripheral devices (interrupt lines 3–7), the module decodes the interrupt by reading 
 * the interrupt bitmap from the device register area, determines the specific device that triggered 
 * the interrupt, and then acknowledges the event by writing an ACK command to the corresponding 
 * device register. Once acknowledged, it unblocks any process waiting on the associated semaphore, 
 * updates that process’s return status with the device’s status code, and enqueues the process 
 * back onto the ready queue.
 * 
 * In the case of PLT interrupts (line 1), which signal the expiration of the current process’s
 *  CPU quantum, the handler stops the timer by setting it to an effectively infinite value, 
 * saves the current process state from BIOSDATAPAGE, updates the accumulated CPU time, and 
 * requeues the process for later execution. For interval timer interrupts (line 2), the module 
 * reloads the timer with a predefined interval (100 milliseconds), unblocks all processes
 * waiting on the pseudo-clock semaphore, and resets that semaphore to ensure that the system 
 * correctly wakes up processes that are delayed on the clock.
 * 
 * The top-level interruptHandler function serves as the central dispatcher. It records the 
 * current time-of-day and the remaining time on the current process’s quantum, retrieves the 
 * saved exception state, and then examines the cause register to determine the type of interrupt. 
 * Based on this determination, it dispatches control to one of the specialized handlers: 
 * pltInterrupt for processor local timer events, intervalTimerInterrupt for pseudo-clock events, 
 * or nonTimerInterrupt for peripheral device interrupts. 
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

    /* Special handling for terminal interrupts (line 7) */
    if (lineNumber == LINE7) {
        /* For terminal devices, check if the interrupt is due to transmissing (write) or receiving (read) */
        if ((devRegArea->devreg[deviceIndex].t_transm_status & STATUSON) != READY) {
            /* It's a write interrupt */
            statusCode = devRegArea->devreg[deviceIndex].t_transm_status;  /* Save the transmission code */
            
            /* Acknowledge the transmission interruption by writing ACK to the transmit command register */
            devRegArea->devreg[deviceIndex].t_transm_command = ACK;       
            
            /* Unblock the process waiting for terminal transmission by removing it from the semaphore queue */
            unblockedProc = removeBlocked(&deviceSemaphores[deviceIndex + DEVPERINT]);
            
            /* Increment the semaphore count for the transmit channel */
            deviceSemaphores[deviceIndex + DEVPERINT]++;
        }
        
        /* Otherwise, the terminal interrupt is for receiving data */
        else {
            /* Save the receiving status code */
            statusCode = devRegArea->devreg[deviceIndex].t_recv_status;   
            
            /* Acknowledge the receive interrupt by writing ACK to the receive command register */
            devRegArea->devreg[deviceIndex].t_recv_command = ACK;        

            /* Unblock the process waiting for terminal reception */
            unblockedProc = removeBlocked(&deviceSemaphores[deviceIndex]);

            /* Increment the semaphore count for the receive channel */
            deviceSemaphores[deviceIndex]++;
        }

    /* For non-terminal device interrupt */
    } else {
        /* Retrieve the device status from the device register */
        statusCode = devRegArea->devreg[deviceIndex].d_status;  

        /* Acknowledge the interrupt by writing ACK to the device's command register */
        devRegArea->devreg[deviceIndex].d_command = ACK;        

        /* Unblock the process waiting on this device's semaphore */
        unblockedProc = removeBlocked(&deviceSemaphores[deviceIndex]);

        /* Increment the device semaphore count associated with the device by 1 */
        deviceSemaphores[deviceIndex]++;
    }

    /* If a process was unblocked */
    if (unblockedProc != NULL) {
        /* Stored the saved status code in the process's return register */
        unblockedProc->p_s.s_v0 = statusCode;
        
        /* Insert the process into the ready queue */
        insertProcQ(&readyQueue, unblockedProc);

        /* Decrement the count of processes that are blocked */
        softBlockCount--;
    }

    /* If there is a currently running process */
    if (currentProcess != NULL) {
        /* Restore the remaining quantum time for the process */
        setTIMER(remainingTime);

        /* Load the saved state to resume execution */
        LDST(savedExceptionState);
    }

    /* If there is no current process, call the scheduler */
    scheduler();                            /* This should never return */

    /* If the scheduler return, something went wrong, be PANIC */
    PANIC();    
}


/*
 * Function     :   pltInterrupt
 * Purpose      :   Handles Processor Local Timer (PLT) interrupts.
 *                  When a PLT interrupt occurs, this handler stops the timer by loading a 
 *                  very large value, saves the current process state, updates the process's 
 *                  accumulated CPU time, requeues the current process, and clears the 
 *                  current process pointer. It then calls the scheduler to dispatch the next process. 
 *                  If no process is running when the PLT interrupt occurs, the function calls PANIC
 * Parameters   :   None
 */
void pltInterrupt() {
    /* Check if there is a running process at time of interrupt */
    if (currentProcess != NULL) {
        /* Set the timer to a very large value (INFINITE) to disable further PLT interrupts */
        setTIMER(INFINITE);

        /* Save the current processor state from BIOSDATAPAGE into currentProcess's pcb */
        copyState((state_PTR) BIOSDATAPAGE, &(currentProcess->p_s));

        /* Update the current process's CPU time */
        STCK(currentTOD);             
        currentProcess->p_time += (currentTOD - startTOD);

        /* Place the current process onto the ready queue for later scheduling */
        insertProcQ(&readyQueue, currentProcess);

        /* Clear the pointer since no process is currently running */
        currentProcess = NULL;

        /* Call the scheduler to dispatch the next process */
        scheduler();                /* This should never return */

        /* If scheduler returns, something went wrong, be PANIC */
        PANIC();
    }
    /* If there is no current process, this is an error, be PANIC */
    PANIC();
}


/*
 * Function     :   intervalTimerInterrupt
 * Purpose      :   Handles Interval Timer (Pseudo-Clock) interrupts.
 *                  This function is invoked when the pseudo-clock (interval timer) interrupt occurs.
 *                  It reloads the interval timer with the INITIALINTTIMER (100ms) value, then 
 *                  unblocks all processes waiting on the pseudo-clock semaphore. After unblocking,
 *                  it resets the pseudo-clock semaphore to zero and either resumes the current process
 *                  (if available) or calls the scheduler to select a new process
 * Parameters   :   None         
 */
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

        /* Decrement the soft block counter for each process unblocked */
        softBlockCount--;
    }

    /* Reset the pseudo-clock to zero to block SYS7 and ensure the pseudo-clock semaphore does not grow positive */
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


/*
 * Function     :   interruptHandler
 * Purpose      :   Top-level Interrupt Handler.
 *                  This is the main entry point for all interrupts. When an interrupt occurs:
 *                  - The current Time-Of-Day is recorded for CPU time accounting
 *                  - The remaining time on the current process's quantum is saved
 *                  - The saved processor state is retrieved from BIOSDATAPAGE
 *                  - The handler examines the cause register to determine which type of interrupt occurred 
 *                      * If the PLT interrupt (line 1) is detected, pltInterrupt() is called
 *                      * If the interval timer interrupt (line 2) is detected, intervalTimerInterrupt() is called
 *                      * Otherwise, a peripheral device interrupt (lines 3-7) is assumed and nonTimerInterrupt() is called
 *                  In this uniprocessor system, the inter-processor interrupts (line 0) are ignored
 */
void interruptHandler() {
    /* Save the Time-Of-Day when an interrupt occurs for CPU time accounting */
    STCK(currentTOD);

    /* Store the remaining time left on current process's quantum */
    remainingTime = getTIMER();

    /* Retrieve the processor state at the time of exception */
    state_PTR savedExceptionState;
    savedExceptionState = (state_PTR) BIOSDATAPAGE;


    /* Check if the interrupt is from the PLT (interrupt line 1) */
    if (((savedExceptionState->s_cause) & LINE1INT) != ALLOFF) {
        pltInterrupt();             /* Call PLT interrupt handler */
    }

    /* Check if the interrupt is from the Interval Timer (interrupt line 2) */
    else if (((savedExceptionState->s_cause) & LINE2INT) != ALLOFF) {
        intervalTimerInterrupt();   /* Call Interval Timer handler */
    }

    /* Otherwise, interrupts are from peripheral devices (interrupt line 3-7) */
    else {
        nonTimerInterrupt();        /* Call non-timer interrupt handler */
    }
}

/******************************* END OF INTERRUPTS.c *******************************/
