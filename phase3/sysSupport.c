/******************************* INITPROC.c ***************************************
 *  
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/04/15
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
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/******************************* FUNCTION DECLARATION *******************************/ 

HIDDEN void terminateUserProcess(void);                                               /* SYS9 */
HIDDEN void getTOD(state_PTR savedState);            /* SYS10 */
HIDDEN void writeToPrinter(state_PTR savedState, support_t *currentSupportStruct);    /* SYS11 */
HIDDEN void writeToTerminal(state_PTR savedState, support_t *currentSupportStruct);   /* SYS12 */
HIDDEN void readFromTerminal(state_PTR savedState, support_t *currentSupportStruct);  /* SYS13 */

/******************************* FUNCTIONS IMPLEMENTATION *******************************/

/* 
 ! SYS9: terminateUserProcess
 */
void terminateUserProcess(void) {
    /* Release the master semaphore (V operation) */
    SYSCALL(SYS4CALL, (unsigned int) &masterSemaphore, 0, 0); 

    /* Invoke SYS2 to terminate this process */
    SYSCALL(SYS2CALL, 0, 0, 0);     /* This should never return back here */   
}

/*
 ! SYS10: getTOD
 */
void getTOD(state_PTR savedState) {
    /*--------------------------------------------------------------*
    * 1. Get the number of microseconds since system was last booted/reset
    *---------------------------------------------------------------*/
    cpu_t currentTOD;
    STCK(currentTOD);

    /*--------------------------------------------------------------*
    * 2. Place the number in U-proc's v_0 register for return value
    *---------------------------------------------------------------*/
    savedState->s_v0 = currentTOD; 

    /*--------------------------------------------------------------*
    * 3. Return control to the instruction after SYSCALL instruction
    *---------------------------------------------------------------*/
    LDST(savedState); 
}

/*
 ! SYS11: writeToPrinter
 */
void writeToPrinter(state_PTR savedState, support_t *currentSupportStruct) {
    /*--------------------------------------------------------------*
    * 0. Initialize Local Variables 
    *---------------------------------------------------------------*/
    char *virtualAddress;                /* Virtual address in the U-proc's address space where the string begins */
    int stringLength;                   /* Length of the string to print */
    int deviceNum;                      /* Device number (derived from the support struct's ASID) */
    int index;                          /* Index into the device register array and semaphore array */
    unsigned int status;                /* Variable to hold the device status returned by SYS5 */
    int statusCode;                     /* Extracted status code from the device status */

    /*--------------------------------------------------------------*
    * 1. Retrieve the SYSCALL parameters from the support structure
    *---------------------------------------------------------------*/
    /* Extract the virtual address from register a1 and string length from a2 */
    virtualAddress = (char *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    stringLength = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a2;

    /*--------------------------------------------------------------*
    * 2. Check if the parameters are valid
    *---------------------------------------------------------------*/
    /* Validate that the address is in the user segment (KUSEG) and that string length is within bound */
    if (((int) virtualAddress < KUSEG) || (stringLength < 0) || (stringLength > MAXSTRINGLENGTH)) {
        /* Be brutal: SYS9 on bad argument(s) */
        terminateUserProcess();
    } 

    /*--------------------------------------------------------------*
    * 3. Identify the device number for the printer
    *---------------------------------------------------------------*/
    /* Pointer to device register area */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Get the processor number from the ASID stored in the support struct */
    deviceNum = currentSupportStruct->sup_asid - 1;                /* Subtract 1 to get an index 0..7 */

    /* Compute the index into the device register array */
    index = ((PRNTINT - OFFSET) * DEVPERINT) + deviceNum;            

    /*--------------------------------------------------------------*
    * 4. Gain mutual exclusion over the device 
    *---------------------------------------------------------------*/
    SYSCALL(SYS3CALL, (int) &deviceSemaphores[index], 0, 0);

    /*--------------------------------------------------------------*
    5. Transmit each character to the printer
    *---------------------------------------------------------------*/
    int i;
    for (i = 0; i < stringLength; i++) {
        /* Disable interrupt so that COMMAND + SYS5 is atomic */
        setSTATUS(getSTATUS() & IECOFF);

        /* Write the character to DATA0, issue the transmit command in COMMAND */
        devRegArea->devreg[index].d_data0 = *(virtualAddress + i);
        devRegArea->devreg[index].d_command = PRINTCHR;

        /* Block until the printer operation completes */
        status = SYSCALL(SYS5CALL, PRNTINT, deviceNum, FALSE);

        /* Re-enable interrupts now that the atomic operation is complete */
        setSTATUS(getSTATUS() | IECON);

        /* Mask off low byte to get status code from device status */
        statusCode = status & STATUSMASK ;
        /* Check if the printer reports a "Device Ready" status (1) */
        if (statusCode != DEVICEREADY) {
            /* If not, set v0 to the negative of the status code to signal an error */
            savedState->s_v0 = -1 * statusCode;

            /* Release the device semaphore */
            SYSCALL(SYS4CALL, (int) &deviceSemaphores[index], 0, 0);

            /* Return control to the instruction after SYSCALL instruction */
            LDST(savedState);
        }
    }

    /*--------------------------------------------------------------*
    * 6. On success: return the number of characters transmitted
    *---------------------------------------------------------------*/
    savedState->s_v0 = stringLength;

    /*--------------------------------------------------------------*
    * 7. Release device semaphore
    *---------------------------------------------------------------*/
    SYSCALL(SYS4CALL, (int) &deviceSemaphores[index], 0, 0); 

    /*--------------------------------------------------------------*
    * 8. Return control to the instruction after SYSCALL instruction
    *---------------------------------------------------------------*/
    LDST(savedState);
}



/*
 ! SYS12 : writeToTerminal
 */
void writeToTerminal(state_PTR savedState, support_t *currentSupportStruct) {
    /*--------------------------------------------------------------*
    * 0. Initialize Local Variables 
    *---------------------------------------------------------------*/
    char *virtualAddress;            /* Virtual address in the U-proc's address space where the string begins */
    int stringLength;               /* Length of the string to print */
    int deviceNum;                  /* Device number (derived from the support struct's ASID) */                       
    int index;                      /* Index into the device register array and semaphore array */
    unsigned int status;            /* Variable to hold the device status returned by SYS5 */
    int statusCode;                 /* Extracted status code from the device status */

    /*--------------------------------------------------------------*
    * 1. Retrieve the SYSCALL parameters from the support structure
    *---------------------------------------------------------------*/
    /* Extract the virtual address from register a1 and string length from a2 */
    virtualAddress = (char *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    stringLength = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a2;

    /*--------------------------------------------------------------*
    * 2. Check if the parameters are valid
    *---------------------------------------------------------------*/
    /* Validate that the address is in the user segment (KUSEG) and that string length is within bound */
    if (((int) virtualAddress < KUSEG) || (stringLength < 0) || (stringLength > MAXSTRINGLENGTH)) {
        /* Be brutal: SYS9 on bad argument(s) */
        terminateUserProcess();
    }

    /*--------------------------------------------------------------*
    * 3. Identify the device number for the terminal
    *---------------------------------------------------------------*/
   /* Pointer to device register area */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Get the processor number from the ASID stored in the support struct */
    deviceNum = currentSupportStruct->sup_asid - 1;                /* Subtract 1 to get an index 0..7 */

    /* Compute the index into the device register array */
    index = ((TERMINT - OFFSET) * DEVPERINT) + deviceNum;

    /*--------------------------------------------------------------*
    * 4. Gain mutual exclusion over the device 
    *---------------------------------------------------------------*/
   /* Note that the transmitter is DEVPERINT (8) index behind the receiver */
    SYSCALL(SYS3CALL, (int) &deviceSemaphores[index + DEVPERINT], 0, 0);   

    /*--------------------------------------------------------------*
    * 5. Transmit each character to the terminal
    *---------------------------------------------------------------*/
    int i;
    for (i = 0; i < stringLength; i++) {
        /* Disable interrupt so that COMMAND + SYS5 is atomic */
        setSTATUS(getSTATUS() & IECOFF);

        /* Place the transmit char and transmit command into TRANSM_FIELD */
        devRegArea->devreg[index].t_transm_command = (*(virtualAddress + i) << TERMINALSHIFT) | TRANSMITCHAR;

        /* Block until the terminal operation completes */
        status = SYSCALL(SYS5CALL, TERMINT, deviceNum, FALSE);

        /* Re-enable interrupts now that the atomic operation is complete */
        setSTATUS(getSTATUS() | IECON);

        /* Mask off low byte to get status code from device status */
        statusCode = status & STATUSMASK;
        /* Check if the transmitter of the terminal reports a "Character Transmitted" status (5) */
        if (statusCode != CHARTRANSMITTED) {
            /* If not, set v0 to the negative of the status code to signal an error */
            savedState->s_v0 = -1 * statusCode;

            /* Release the device semaphore */
            SYSCALL(SYS4CALL, (int) &deviceSemaphores[index + DEVPERINT], 0, 0);

            /* Return control to the instruction after SYSCALL instruction */
            LDST(savedState);
        }
    }

    /*--------------------------------------------------------------*
    * 6. On success: return the number of characters transmitted
    *---------------------------------------------------------------*/
    savedState->s_v0 = stringLength;

    /*--------------------------------------------------------------*
    * 7. Release device semaphore
    *---------------------------------------------------------------*/
    SYSCALL(SYS4CALL, (int) &deviceSemaphores[index + DEVPERINT], 0, 0);

    /*--------------------------------------------------------------*
    * 8. Return control to the instruction after SYSCALL instruction
    *---------------------------------------------------------------*/
    LDST(savedState);
}



/*
 ! SYS13: readFromTerminal
 */
void readFromTerminal(state_PTR savedState, support_t *currentSupportStruct) {
    /*--------------------------------------------------------------*
    * 0. Initialize Local Variables 
    *---------------------------------------------------------------*/
    char *virtualAddress;        /* Virtual address of a string buffer where the data read should be placed */
    int deviceNum;                      /* Device number (derived from the support struct's ASID) */
    int index;                          /* Index into the device register array and semaphore array */
    unsigned int status;                /* Variable to hold the device status returned by SYS5 */
    int statusCode;                     /* Extracted status code from the device status */
    int readLength;                     /* Number of characters read from the terminal */

    /*--------------------------------------------------------------*
    * 1. Retrieve the SYSCALL parameter from the support structure
    *---------------------------------------------------------------*/
    /* Extract the virtual address from register a1 */
    virtualAddress = (char *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;

    /*--------------------------------------------------------------*
    * 2. Check if the parameters are valid
    *---------------------------------------------------------------*/
   /* Validate that the address to read is in the user segment (KUSEG) */
   if ((int) virtualAddress < KUSEG) {
    /* Be brutal: SYS9 on bad argument */
    terminateUserProcess();
   }

    /*--------------------------------------------------------------*
    * 3. Identify the device number for the printer
    *---------------------------------------------------------------*/
    /* Pointer to device register area */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Get the processor number from the ASID stored in the support struct */
    deviceNum = currentSupportStruct->sup_asid - 1;                /* Subtract 1 to get an index 0..7 */

    /* Compute the index into the device register array */
    index = ((TERMINT - OFFSET) * DEVPERINT) + deviceNum;            

    /*--------------------------------------------------------------*
    * 4. Gain mutual exclusion over the device 
    *---------------------------------------------------------------*/
   SYSCALL(SYS3CALL, (int) &deviceSemaphores[index], 0, 0);

    /*--------------------------------------------------------------*
    * 5. Read each character from the terminal
    *---------------------------------------------------------------*/
    /* Initialize readLength as 0 before read loop */
    readLength = 0;

    /* Loop until reach EOL ("\n") character or error signal from the terminal  */
    int currentChar;                       /* Char just read from the terminal */
   do {
        /* Disable interrupts so that COMMAND + SYS5 is atomic */
        setSTATUS(getSTATUS() & IECOFF);

        /* Write the receive command in COMMAND */
        devRegArea->devreg[index].t_recv_command = RECEIVECHAR;

        /* Block until the terminal operation completes */
        status = SYSCALL(SYS5CALL, TERMINT, deviceNum, TRUE);

        /* Re-enable interrupts now that the atomic operation is complete */
        setSTATUS(getSTATUS() | IECON);

        /* Mask off low byte to get status code from device status */
        statusCode = status & STATUSMASK;

         /* Check if the receiver of the terminal reports a "Character Received" status (5) */
         if (statusCode != CHARRECEIVED) {
            /* If not, set v0 to the negative of the status code to signal an error */
            savedState->s_v0 = -1 * statusCode;

            /* Release the device semaphore */
            SYSCALL(SYS4CALL, (int) &deviceSemaphores[index], 0, 0);

            /* Return control to the instruction after SYSCALL instruction */
            LDST(savedState);
        } else {
            /* Read the character received */
            currentChar = (char) ((status >> CHARRECEIVEDSHIFT) & CHARRECEIVEDMASK);

            /* Check if the character is EOL */
            if (currentChar != EOL) {
                /* If not, store the character read */
                *(virtualAddress + readLength) = currentChar;

                /* Increment the read length */
                readLength++;
            }
        }
   } while (currentChar != EOL);

    /*--------------------------------------------------------------*
    * 6. On success: return the number of characters received
    *---------------------------------------------------------------*/
    savedState->s_v0 = readLength;

    /*--------------------------------------------------------------*
    * 7. Release device semaphore
    *---------------------------------------------------------------*/
   SYSCALL(SYS4CALL, (int) &deviceSemaphores[index], 0, 0); 

    /*--------------------------------------------------------------*
    * 8. Return control to the instruction after SYSCALL instruction
    *---------------------------------------------------------------*/
    LDST(savedState);
}


void VMgeneralExceptionHandler(void) {
    /* Get the support structure */
    support_t *currentSupportStruct;
    currentSupportStruct = (support_t *) SYSCALL(SYS8CALL, 0, 0, 0);

    /* Retrieve processor state at time of exception */
    state_PTR savedState;
    savedState = &(currentSupportStruct->sup_exceptState[GENERALEXCEPT]);

    /* Check for the cause of the exception */
    int exceptionCode;
    exceptionCode = ((savedState->s_cause) & GETEXCEPTIONCODE) >> CAUSESHIFT;

    /* Pass control to the correct exception handler */
    if (exceptionCode == SYS8CALL) {
        /* Pass control to Support Level's SYSCALL exception handler */
        VMsyscallExceptionHandler(savedState, currentSupportStruct);
    } else {
        /* Pass control to Support Level's Program Trap exception handler */
        VMprogramTrapExceptionHandler();
    }
}


void VMsyscallExceptionHandler(state_PTR savedState, support_t *currentSupportStruct) {
    /* Get the syscall number */
    unsigned int sysNum;
    sysNum = savedState->s_a0;

    /* Increment the PC by WORDLEN (4) to avoid infinite SYSCALL loop */
    savedState->s_pc = savedState->s_pc + WORDLEN;

    /* Dispatch the SYSCALL based on sysNum */
    switch (sysNum) {
        case SYS9CALL:
        /* SYS9 */
            terminateUserProcess();
            break;

        case SYS10CALL:
        /* SYS10: Get TOD */
            getTOD(savedState);
            break;

        case SYS11CALL:
            /* SYS11: Write to Printer */
            writeToPrinter(savedState, currentSupportStruct);
            break;
            
        case SYS12CALL:
            /* SYS12 */
            writeToTerminal(savedState, currentSupportStruct);
            break;

        case SYS13CALL:
            /* SYS13 */
            readFromTerminal(savedState, currentSupportStruct);
            break;

        default:
            VMprogramTrapExceptionHandler();
            break;
    }
}


void VMprogramTrapExceptionHandler() {
    /* Terminate the process in an orderly fashion - SYS9 request */
    terminateUserProcess();
}
