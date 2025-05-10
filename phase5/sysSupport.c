/******************************* SYSSUPPORT.c ***************************************
 *  
 * This module implements the Support Level's system call handlers for user process in the Pandos kernal. 
 * It provides:
 *  - SYS9  :   terminateUserProcess
 *              Terminate a U-Proc by releasing any device semaphores it holds, signaling InitProc's
 *              masterSemaphore, then invoking the kernel's SYS2 to terminate the process and its progeny
 *  - SYS10 :   Return the number of microseconds since system boot to the U‑Proc
 *  - SYS11 :   Perform mutual‑exclusion protected output of a user‑supplied string to the printer, 
 *              character by character; validate parameters and propagate any device errors
 *  - SYS12 :   Analogous to SYS11 but for terminal output
 *  - SYS13 :   Mutual‑exclusion protected input from the terminal into a user buffer until EOL, validating parameters
 * 
 * It also provides the exception dispatchers, which includes:
 *  - VMgeneralExceptionHandler     : Top‑level support‑level exception dispatcher for SYSCALL and program trap
 *  - VMsyscallExceptionHandler     : Dispatch support-level SYSCALLs (SYS9-13)
 *  - VMprogramTrapExceptionHandler : Terminates the U-Proc on any support-level program trap 
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/04/17
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
#include "../h/deviceSupportDMA.h"
#include "../h/delayDaemon.h"
#include "/usr/include/umps3/umps/libumps.h"

/******************************* FUNCTION DECLARATIONS *******************************/ 

/* Phase 3 */
HIDDEN void terminateUserProcess(support_t *currentSupportStruct);                    /* SYS9  */
HIDDEN void getTOD(state_PTR savedState);                                             /* SYS10 */
HIDDEN void writeToPrinter(state_PTR savedState, support_t *currentSupportStruct);    /* SYS11 */
HIDDEN void writeToTerminal(state_PTR savedState, support_t *currentSupportStruct);   /* SYS12 */
HIDDEN void readFromTerminal(state_PTR savedState, support_t *currentSupportStruct);  /* SYS13 */

/* Phase 4 */
extern void diskPut(support_t *currentSupportStruct);                                 /* SYS14 */
extern void diskGet(support_t *currentSupportStruct);                                 /* SYS15 */
extern void flashPut(support_t *currentSupportStruct);                                /* SYS16 */
extern void flashGet(support_t *currentSupportStruct);                                /* SYS17 */

/* Phase 5 */
extern void delay(support_t *currentSupportStruct);                                   /* SYS18 */

/******************************* SYSCALL IMPLEMENTATIONS *******************************/

/* 
 * Function     :   terminateUserProcess
 * Purpose      :   Implement SYS9 to terminate a User Process. First, it will release 
 *                  any device semaphores held by the U-proc. Then, it performs a V operation
 *                  on the masterSemaphore so InitProc can wake up and reclaim resources.
 *                  Finally, it invokes a SYS2 to terminate this U-Proc and its progeny
 * Parameters   :   currentSupportStruct - pointer to the support structure of the U-Proc to be terminated
 * Returns      :   None
 */
void terminateUserProcess(support_t *currentSupportStruct)
{
    /* ---------------------------------------------------------- *
     * 0.  Declare local variable
     * ---------------------------------------------------------- */
    int asid = currentSupportStruct->sup_asid;   /* 1‑8 */

    /* ---------------------------------------------------------- *
     * 1. Release all device semaphores this U-Proc may hold
     * ---------------------------------------------------------- */
    int line;
    /* Plus 1 since for each terminal, there are a transmitter and a receiver */
    for (line = 0; line < DEVTYPES + 1; line++) {
        int index = (line * DEVPERINT) + (asid - 1);
        if (devSemaphores[index] == 0) {
            /* Release it */
            SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[index], 0, 0);
        }
    }

    /* ---------------------------------------------------------- *
     * 2. V the masterSemaphore so InitProc can wake up
     * ---------------------------------------------------------- */
    SYSCALL(SYS4CALL, (unsigned int) &masterSemaphore, 0, 0);

    /* ---------------------------------------------------------- *
     * 3. Finally, invoke SYS2 to terminate this U-Proc
     * ---------------------------------------------------------- */
    SYSCALL(SYS2CALL, 0, 0, 0);                         /* never returns */
}

/*
 * Function     :   getTOD
 * Purpose      :   Implement SYS10 to return the current Time-Of-Day clock
 *                  (microseconds since system boot) to the calling U-Proc.
 * Parameters   :   savedState - pointer to the user's saved processor state
 * Returns      :   None
 */
void getTOD(state_PTR savedState) {
    /* ------------------------------------------------------------ *
     * 1. Get the number of microseconds since system was last booted/reset
     * ------------------------------------------------------------ */
    cpu_t currentTime;
    STCK(currentTime);

    /* ------------------------------------------------------------ *
    * 2. Place the number in U-proc's v_0 register for return value
     * ------------------------------------------------------------ */
    savedState->s_v0 = currentTime; 

    /* ------------------------------------------------------------ *
     * 3. Return control to the instruction after SYSCALL instruction
     * ------------------------------------------------------------ */
    LDST(savedState); 
}

/*
 * Function     :   writeToPrinter
 * Purpose      :   Implement SYS11 to write a user-supplied string to the printer.
 *                  First, it fetches the user arguments (virtual address & length) from
 *                  the support structure. Then, it validates the virtual address lies in
 *                  user space and that the string length is of supported length. Next,
 *                  it computes the printer's semaphore index based on ASID. Continue, it
 *                  performs P on the printer's semaphore to enforce mutual exclusion, before
 *                  looping over each character to write the char into device register's d_data0
 *                  and issue SYS5 to block on the hardware. In each loop iterative, it will check
 *                  the return status to make sure that no error occurs (else, it aborts). Finally,
 *                  it places the number of character written, releases the semaphore (V) and 
 *                  returns to the user
 * Parameters   :   savedState - pointer to the saved processor state
 *                  currentSupportStruct - user’s support struct (holds a1, a2 in its state)
 * Returns      :   None
 */
void writeToPrinter(state_PTR savedState, support_t *currentSupportStruct) {
    /* ------------------------------------------------------------ *
     * 0. Initialize Local Variables 
     * ------------------------------------------------------------ */
    int deviceNum;                      /* Device number (derived from the support struct's ASID) */
    int index;                          /* Index into the device register array and semaphore array */
    unsigned int status;                /* Variable to hold the device status returned by SYS5 */
    unsigned int statusCode;            /* Extracted status code from the device status */

    /* ------------------------------------------------------------ *
     * 1. Retrieve the SYSCALL parameters from the support structure
     * ------------------------------------------------------------ */
    /* Extract the virtual address from register a1 and string length from a2 */
    char *virtualAddress = (char *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int stringLength = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a2;

    /* ------------------------------------------------------------ *
     * 2. Check if the parameters are valid
     * ------------------------------------------------------------ */
    /* Validate that the address is in the user segment (KUSEG) and that string length is within bound */
    if (((int) virtualAddress < KUSEG) || (stringLength < 0) || (stringLength > MAXSTRINGLENGTH)) {
        /* Be brutal: SYS9 on bad argument(s) */
        terminateUserProcess(currentSupportStruct);
    } 

    /* ------------------------------------------------------------ *
     * 3. Identify the device number for the printer
     * ------------------------------------------------------------ */
    /* Pointer to device register area */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Get the processor number from the ASID stored in the support struct */
    deviceNum = currentSupportStruct->sup_asid - 1;                /* Subtract 1 to get an index 0..7 */

    /* Compute the index into the device register array */
    index = ((PRNTINT - OFFSET) * DEVPERINT) + deviceNum;            

    /* ------------------------------------------------------------ *
     * 4. Gain mutual exclusion over the device 
     * ------------------------------------------------------------ */
    SYSCALL(SYS3CALL, (unsigned int) &devSemaphores[index], 0, 0);

    /* ------------------------------------------------------------ *
     * 5. Transmit each character to the printer
     * ------------------------------------------------------------ */
    int i;
    for (i = 0; i < stringLength; i++) {
        /* Disable interrupt so that COMMAND + SYS5 is atomic */
        setSTATUS(getSTATUS() & IECOFF);

        /* Write the character to DATA0, issue the transmit command in COMMAND */
        devRegArea->devreg[index].d_data0   = *(virtualAddress + i);
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
            SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[index], 0, 0);

            /* Return control to the instruction after SYSCALL instruction */
            LDST(savedState);
        }
    }

    /* ------------------------------------------------------------ *
     * 6. On success: return the number of characters transmitted
     * ------------------------------------------------------------ */
    savedState->s_v0 = stringLength;

    /* ------------------------------------------------------------ *
     * 7. Release device semaphore
     * ------------------------------------------------------------ */
    SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[index], 0, 0); 

    /* ------------------------------------------------------------ *
     * 8. Return control to the instruction after SYSCALL instruction
     * ------------------------------------------------------------ */
    LDST(savedState);
}

/*
 * Function     :   writeToTerminal
 * Purpose      :   Implement SYS12 to write a user-supplied string to the terminal.
 *                  First, it fetches the user arguments (virtual address & length) from
 *                  the support structure. Then, it validates the virtual address lies in
 *                  user space and that the string length is of supported length. Next,
 *                  it computes the terminal's semaphore index based on ASID. Continue, it
 *                  performs P on the terminal's semaphore to enforce mutual exclusion, before
 *                  looping over each character to place the write command to terminal's transmitter 
 *                  register (t_transm_command) and shift the character into higher bits. Then, it
 *                  issues SYS5 to block on the hardware. In each loop iterative, it will check
 *                  the return status to make sure that no error occurs (else, it aborts). Finally,
 *                  it places the number of character written, releases the semaphore (V) and 
 *                  returns to the user
 * Parameters   :   savedState - pointer to the saved processor state
 *                  currentSupportStruct - user’s support struct (holds a1, a2 in its state)
 * Returns      :   None
 */
void writeToTerminal(state_PTR savedState, support_t *currentSupportStruct) {
    /* ------------------------------------------------------------ *
     * 0. Initialize Local Variables 
     * ------------------------------------------------------------ */
    int deviceNum;                  /* Device number (derived from the support struct's ASID) */                       
    int index;                      /* Index into the device register array and semaphore array */
    unsigned int status;            /* Variable to hold the device status returned by SYS5 */
    unsigned int statusCode;        /* Extracted status code from the device status */

    /* ------------------------------------------------------------ *
     * 1. Retrieve the SYSCALL parameters from the support structure
     * ------------------------------------------------------------ */
    /* Extract the virtual address from register a1 and string length from a2 */
    char *virtualAddress = (char *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int stringLength = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a2;

    /* ------------------------------------------------------------ *
     * 2. Check if the parameters are valid
     * ------------------------------------------------------------ */
    /* Validate that the address is in the user segment (KUSEG) and that string length is within bound */
    if (((int) virtualAddress < KUSEG) || (stringLength < 0) || (stringLength > MAXSTRINGLENGTH)) {
        /* Be brutal: SYS9 on bad argument(s) */
        terminateUserProcess(currentSupportStruct);
    }

    /* ------------------------------------------------------------ *
     * 3. Identify the device number for the terminal
     * ------------------------------------------------------------ */
   /* Pointer to device register area */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Get the processor number from the ASID stored in the support struct */
    deviceNum = currentSupportStruct->sup_asid - 1;                /* Subtract 1 to get an index 0..7 */

    /* Compute the index into the device register array */
    index = ((TERMINT - OFFSET) * DEVPERINT) + deviceNum;

    /* ------------------------------------------------------------ *
     * 4. Gain mutual exclusion over the device 
     * ------------------------------------------------------------ */
   /* Note that the transmitter is DEVPERINT (8) index behind the receiver */
    SYSCALL(SYS3CALL, (unsigned int) &devSemaphores[index + DEVPERINT], 0, 0);   

    /* ------------------------------------------------------------ *
     * 5. Transmit each character to the terminal
     * ------------------------------------------------------------ */
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
            SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[index + DEVPERINT], 0, 0);

            /* Return control to the instruction after SYSCALL instruction */
            LDST(savedState);
        }
    }

    /* ------------------------------------------------------------ *
     * 6. On success: return the number of characters transmitted
     * ------------------------------------------------------------ */
    savedState->s_v0 = stringLength;

    /* ------------------------------------------------------------ *
     * 7. Release device semaphore
     * ------------------------------------------------------------ */
    SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[index + DEVPERINT], 0, 0);

    /* ------------------------------------------------------------ *
     * 8. Return control to the instruction after SYSCALL instruction
     * ------------------------------------------------------------ */
    LDST(savedState);
}

/*
 * Function     :   readFromTerminal
 * Purpose      :   Implement SYS13 to atomically read characters from the terminal
 *                  receiver into a user buffer until an EOL (end-of-line) character
 *                  is encountered. First, it fetches the user buffer pointer from the
 *                  given supportStruct. Then, it validates to ensure that the pointer is 
 *                  actually in the user segment. Next, it P the corresponding device semaphore
 *                  to lock the receiver. In each of the loop interation, it issues a SYS5
 *                  with read = TRUE, extracts the received char from the status word, and store
 *                  it in the buffer, increments the count until it hits the EOL character 
 *                  or there was an error code from the device. After the loop, it v the receiver
 *                  semaphore and then place the count in savedState->s_v0 for the total number 
 *                  character received
 * Parameters   :   savedState - pointer to the saved processor state
 *                  currentSupportStruct - user’s support struct (holds a1 in its state)
 * Returns      :   None
 */
void readFromTerminal(state_PTR savedState, support_t *currentSupportStruct) {
    /* ------------------------------------------------------------ *
     * 0. Initialize Local Variables 
     * ------------------------------------------------------------ */
    int deviceNum;                      /* Device number (derived from the support struct's ASID) */
    int index;                          /* Index into the device register array and semaphore array */
    unsigned int status;                /* Variable to hold the device status returned by SYS5 */
    unsigned int statusCode;            /* Extracted status code from the device status */
    int readLength;                     /* Number of characters read from the terminal */

    /* ------------------------------------------------------------ *
     * 1. Retrieve the SYSCALL parameters from the support structure
     * ------------------------------------------------------------ */
    /* Extract the virtual address from register a1 */
    char *virtualAddress = (char *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;

    /* ------------------------------------------------------------ *
     * 2. Check if the parameter is indeed valid
     * ------------------------------------------------------------ */
    /* Validate that the address to read is in the user segment (KUSEG) */
    if ((int) virtualAddress < KUSEG) {
        /* Be brutal: SYS9 on bad argument */
        terminateUserProcess(currentSupportStruct);
    }

    /* ------------------------------------------------------------ *
   * 3. Identify the device number for the printer
     * ------------------------------------------------------------ */
    /* Pointer to device register area */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Get the processor number from the ASID stored in the support struct */
    deviceNum = currentSupportStruct->sup_asid - 1;                /* Subtract 1 to get an index 0..7 */

    /* Compute the index into the device register array */
    index = ((TERMINT - OFFSET) * DEVPERINT) + deviceNum;            

    /* ------------------------------------------------------------ *
    * 4. Gain mutual exclusion over the device 
    * ------------------------------------------------------------ */
    SYSCALL(SYS3CALL, (unsigned int) &devSemaphores[index], 0, 0);

    /* ------------------------------------------------------------ *
    * 5. Read each character from the terminal
    * ------------------------------------------------------------ */
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
            SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[index], 0, 0);

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

    /* Actually, also include EOL in the buffer and count it */
    *(virtualAddress + readLength) = EOL;       /* Add the EOL character */
    readLength++;                               /* Include EOL in the length */

    /* ------------------------------------------------------------ *
     * 6. On success: return the number of characters received
     * ------------------------------------------------------------ */
    savedState->s_v0 = readLength;

    /* ------------------------------------------------------------ *
     * 7. Release device semaphore
     * ------------------------------------------------------------ */
    SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[index], 0, 0); 

    /* ------------------------------------------------------------ *
     * 8. Return control to the instruction after SYSCALL instruction
     * ------------------------------------------------------------ */
    LDST(savedState);
}

/*
 * Function     :   VMgeneralExceptionHandler
 * Purpose      :   Top-level support exception dispatcher for U-Procs.
 *                  Retrieves the current process's support structure and saved
 *                  state, decodes the exception code, and routes the exception
 *                  to either syscall or program trap handler
 * Parameters   :   None
 * Returns      :   None
 */
void VMgeneralExceptionHandler(void) {
    /* ---------------------------------------------------------- *
     * 1. Invoke SYS8 to get the support structure 
     * ---------------------------------------------------------- */
    support_t *currentSupportStruct = (support_t *) SYSCALL(SYS8CALL, 0, 0, 0);

    /* ---------------------------------------------------------- *
     * 2. Retrieve processor state at time of exception
     * ---------------------------------------------------------- */
    state_PTR savedState = &(currentSupportStruct->sup_exceptState[GENERALEXCEPT]);

    /* ---------------------------------------------------------- *
     * 3. Decode exception code out of Cause register
     * ---------------------------------------------------------- */
    unsigned int exceptionCode;
    exceptionCode = ((savedState->s_cause) & GETEXCEPTIONCODE) >> CAUSESHIFT;

    /* ---------------------------------------------------------- *
     * 4. Dispatch control to either SYSCALL or program trap handler
     * ---------------------------------------------------------- */
    if (exceptionCode == SYSCALLCONST) {
        /* Pass control to Support Level's SYSCALL exception handler */
        VMsyscallExceptionHandler(savedState, currentSupportStruct);
    } else {
        /* Pass control to Support Level's Program Trap exception handler */
        VMprogramTrapExceptionHandler(currentSupportStruct);
    }
}

/*
 * Function     :   VMsyscallExceptionHandler
 * Purpose      :   Dispatch support-level SYSCALL exception (SYS9-13)
 *                  First, it advances the saved PC by WORDLEN (4) to skip the SYSCALL instruction
 *                  and avoid SYSCALL infinite loop. Then, it read the syscall number from
 *                  savedState->s_a0, then switch on syscall number:
 *                      - SYS9    -> terminateUserProcess
 *                      - SYS10   -> getTOD
 *                      - SYS11   -> writeToPrinter
 *                      - SYS12   -> writeToTerminal
 *                      - SYS13   -> readFromTerminal
 *                      - default -> treat as program trap and call program trap handler
 * Parameters   :   savedState - pointer to the saved processor state
 *                  currentSupportStruct - user’s support struct (holds a1, a2 in its state)
 * Returns      :   None              
 */
void VMsyscallExceptionHandler(state_PTR savedState, support_t *currentSupportStruct) {
    /* ------------------------------------------------------------ *
     * 1. Advance the PC past the SYSCALL instruction
     * ------------------------------------------------------------ */
    /* Increment the PC by WORDLEN (4) to avoid infinite SYSCALL loop */
    savedState->s_pc = savedState->s_pc + WORDLEN;

    /* ------------------------------------------------------------ *
     * 2. Read the SYSCALL number from register a0
     * ------------------------------------------------------------ */
    int sysNum = savedState->s_a0;

    /* ------------------------------------------------------------ *
     * 3. Route to the appropriate handler
     * ------------------------------------------------------------ */
    /* Dispatch the SYSCALL based on sysNum */
    switch (sysNum) {
        case SYS9CALL:
            /* SYS9: terminate this U-Proc */
            terminateUserProcess(currentSupportStruct);
            break;

        case SYS10CALL:
            /* SYS10: Return Time-Of-Day to user */
            getTOD(savedState);
            break;

        case SYS11CALL:
            /* SYS11: Write buffer to printer */
            writeToPrinter(savedState, currentSupportStruct);
            break;

        case SYS12CALL:
            /* SYS12: Write buffer to printer */
            writeToTerminal(savedState, currentSupportStruct);
            break;
            
        case SYS13CALL:
            /* SYS13: Read from terminal into user buffer */
            readFromTerminal(savedState, currentSupportStruct);
            break;

        case SYS14CALL:
            /* SYS14: Write buffer to disk */
            diskPut(currentSupportStruct);
            break;

        case SYS15CALL:
            /* SYS15: Read buffer from disk */
            diskGet(currentSupportStruct);
            break;

        case SYS16CALL:
            /* SYS16: Write buffer to flash */
            flashPut(currentSupportStruct);
            break;

        case SYS17CALL:
            /* SYS17: Read buffer from flash */
            flashGet(currentSupportStruct);
            break;

        case SYS18CALL:
            /* SYS18: Delay the U-Proc */
            delay(currentSupportStruct);
            break;

        default:
            /* For anything else, treat as *fatal* program trap */
            VMprogramTrapExceptionHandler(currentSupportStruct);
    }
}

/*
 * Function     :   VMprogramTrapExceptionHandler
 * Purpose      :   Handle any support-level program trap as fatal to the U-Proc.
 *                  Simply invoke terminateUserProcess (SYS9) to kill this U-Proc.
 * Parameters   :   currentSupportStruct - user’s support struct
 * Returns      :   None
 */
void VMprogramTrapExceptionHandler(support_t *currentSupportStruct) {
    /* ------------------------------------------------------------ *
     * 1. Immediately kill the U-Proc (cleanup & SYS2)
     * ------------------------------------------------------------ */
    terminateUserProcess(currentSupportStruct);         /* Should never return */
}
