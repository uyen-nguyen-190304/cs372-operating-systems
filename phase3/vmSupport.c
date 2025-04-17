/******************************* VMSUPPORT.c ***************************************
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/04/13
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

/************************* VMSUPPORT GLOBAL VARIABLES *************************/

int swapPoolSemaphore;                          /* Semaphore for the Swap Pool Table */
swap_t swapPoolTable[SWAPPOOLSIZE];             /* THE Swap Pool Table: one entry per swap pool frame */

/************************* GLOBAL VARIABLES INITIALIZATION *************************/


void debug1(int a, int b, int c, int d) {
    int i;
    i = 0;
    i++;
}

void debug2(int a, int b, int c, int d) {
    int i;
    i = 0;
    i++;
}

void debug3(int a, int b, int c, int d) {
    int i;
    i = 0;
    i++;
}

void debug4(int a, int b, int c, int d) {
    int i;
    i = 0;
    i++;
}

void debug5(int a, int b, int c, int d) {
    int i;
    i = 0;
    i++;
}

void initSwapStructs() {
    /* Initialize the Swap Pool Semaphore to 1 (mutual exclusion) */
    swapPoolSemaphore = 1;

    /* Iteratively initialize the Swap Pool table */
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        swapPoolTable[i].asid = EMPTYFRAME; /* Set the ASID to EMPTYFRAME (-1) */
    }
}

/******************************* HELPER FUNCTIONS *******************************/

/* 
 * Function     :   setInterrupt 
 */
void setInterrupt(int status) {
    if (status == TRUE) {
        setSTATUS(getSTATUS() | IECON);     /* Turn interrupts on */
    } else {
        setSTATUS(getSTATUS() & IECOFF);    /* Turn interrupts off */
    }
}

/*
 * Function     :   mutex
 */
void mutex(int *semaphore, int doLock) {
    /* If doLock is TRUE, then wait on the semaphore */
    if (doLock == TRUE) {
        SYSCALL(SYS3CALL, (unsigned int) semaphore, 0, 0);
    } else {
        SYSCALL(SYS4CALL, (unsigned int) semaphore, 0, 0);
    }
}

/*
 * Function     :   flashDeviceOperation 
 */
int flashDeviceOperation(int operation, int asid, int frameAddress, int pageNumber) {
    /*--------------------------------------------------------------*
    * 0. Initialize Local Variables 
    *---------------------------------------------------------------*/
    int index;                      /* Index to the device register array and semaphore array*/

    /*--------------------------------------------------------------*
    * 1. Identify the device number for the flash
    *---------------------------------------------------------------*/
    /* Pointer to the device register area */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Compute the index into the device register array */
    index = ((FLASHINT - OFFSET) * DEVPERINT) + (asid - 1);

    /*--------------------------------------------------------------*
    * 2. Gain mutual exclusion over the device 
    *---------------------------------------------------------------*/
    mutex(&devSemaphores[index], TRUE);

    /*--------------------------------------------------------------*
    * 3. Perform the flash device operation
    *---------------------------------------------------------------*/
    /* Disable interrupts so that COMMAND + SYS5 is atomic */
    setInterrupt(FALSE);
    
    /* Write the frame's starting address into device's DATA0 field */
    devRegArea->devreg[index].d_data0 = frameAddress;

    /* If the operation requested is a READ operation */
    if (operation == FLASHREAD) {
        /* Set the device's command register to READ and put in the block number */
        devRegArea->devreg[index].d_command = (pageNumber << BLOCKSHIFT) | READBLK;
    } else {
        /* Set the device's command register to WRITE and put in the block number */
        devRegArea->devreg[index].d_command = (pageNumber << BLOCKSHIFT) | WRITEBLK;
    }

    /* Wait for the device to complete the operation */
    SYSCALL(SYS5CALL, FLASHINT, (asid - 1), operation);

    /* Re-enable interrupts now that the atomic operation is complete */
    setInterrupt(TRUE);
    
   /*--------------------------------------------------------------*
    * 4. Release device semaphore
    *---------------------------------------------------------------*/
    mutex(&devSemaphores[index], FALSE);

   /*--------------------------------------------------------------*
    * 5. Return the status code from the device
    *---------------------------------------------------------------*/
    return (devRegArea->devreg[index].d_status);
}

/************************* PAGE REPLACEMENT ALGORITHM *************************/

/* 
    int pageReplacement() {
        static int hand = 0;
        int victim = -1;

        int i;
        for (i = 0; i < SWAPPOOLSIZE; i++) {
            int index = (hand + i) % SWAPPOOLSIZE;
            if (swapPoolTable[index].asid == EMPTYFRAME) {
                victim = index;
                hand = (index + 1) % SWAPPOOLSIZE;
                return victim;
            }
        }

        victim = hand;
        hand = (hand + 1) % SWAPPOOLSIZE;
        return victim;
    }
*/

/******************************* PAGER FUNCTION *******************************/

/*
 * Function     :   pager 
 */
void pager() {
    /*--------------------------------------------------------------*
    * 0. Initialize Local Variables 
    *---------------------------------------------------------------*/
    support_t   *currentSupportStruct;          /* Pointer to the Current Process's support structure */
    state_t     *savedState;                    /* Pointer to the saved exception state responsible for the TLB exception */
    int         exceptionCode;                  /* Exception code for the TLB exception */
    int         missingPageNo;                  /* Page number of the missing TLB entry */
    int         frameAddress;                   /* Frame address of the page to be swapped in */
    int         status1, status2;               /* Status codes for the flash device operations */ 
    HIDDEN int frameNumber;

    /*--------------------------------------------------------------*
    * 1. Obtain the pointer to the Current Process's Support Structure
    *---------------------------------------------------------------*/
   currentSupportStruct = (support_t *) SYSCALL(SYS8CALL, 0, 0, 0);

    /*--------------------------------------------------------------*
    * 2. Determine the case of the TLB exception
    *---------------------------------------------------------------*/    
    /* Get the saved exception state in Current Process's Support Structure for TLB exception */
    savedState = &(currentSupportStruct->sup_exceptState[PGFAULTEXCEPT]);

    /* Extract the exception code from the saved exception state */
    exceptionCode = ((savedState->s_cause) & GETEXCEPTIONCODE) >> CAUSESHIFT;

    /*--------------------------------------------------------------*
    * 3. If the Cause if a TLB-Modification exception, treat as Program Trap
    *---------------------------------------------------------------*/    
    if (exceptionCode == TLBMODIFICATION) {
        VMprogramTrapExceptionHandler(currentSupportStruct);          /* Terminate the process */
    }

    /*--------------------------------------------------------------*
    * 4. Acquire mutual exclusion over the Swap Pool table
    *---------------------------------------------------------------*/ 
    mutex(&swapPoolSemaphore, TRUE);

    /*--------------------------------------------------------------*
    * 5. Determine the missing page number, found in saved exception state's entryHI
    *---------------------------------------------------------------*/ 
    missingPageNo = ((savedState->s_entryHI) & VPNMASK) >> VPNSHIFT;
    missingPageNo = missingPageNo % NUMPAGES;   /* Ensure the page number is within bounds */
    debug1(0,0,0,0);

    /*--------------------------------------------------------------*
    * 6. Pick a frame from the Swap Pool
    *---------------------------------------------------------------*/ 
    /* Frame is chosen by the page replacement algorithm provided above */
    /*frameNumber  = pageReplacement();                         */
    frameNumber = (frameNumber + 1) % SWAPPOOLSIZE;

    /* Calculate the frame address */
    frameAddress = (frameNumber * PAGESIZE) + SWAPPOOLSTART;    
    
    debug2(0,0,0,0);

    /*--------------------------------------------------------------*
    * 7. Determine if the frame is occupied
    *---------------------------------------------------------------*/ 
    /* Examine frameNumber entry in the Swap Pool table */
    if (swapPoolTable[frameNumber].asid != EMPTYFRAME)  {
        /*--------------------------------------------------------------*
        * 8. If the frame is occupied, swap out the page
        *---------------------------------------------------------------*/ 
        /* NOTE: Disable interrupt to ensure that the next two steps are performed atomically */
        setInterrupt(FALSE); 

        /* a. Update process's Page Table: mark Page Table entry as not valid */
        swapPoolTable[frameNumber].pte->pt_entryLO &= VALIDOFF;

        /* b. Erase all the entries in the TLB */
        TLBCLR();               

        /* NOTE: Enable interrupt again, end of atomically steps (a & b) */
        setInterrupt(TRUE); 

        /* c. Update process's backing store */
        status1 = flashDeviceOperation(FLASHWRITE, swapPoolTable[frameNumber].asid, frameAddress, swapPoolTable[frameNumber].vpn);

        debug3(0,0,0,0);

        /* Treat any error status from the write operation as a program trap */
        if (status1 != SUCCESS) {
            mutex(&swapPoolSemaphore, FALSE);
            VMprogramTrapExceptionHandler(currentSupportStruct); /* Terminate the process */
        }
    }
    
    /*--------------------------------------------------------------*
    * 9. Read the contents of the Current Process's backing store/flash device
    *---------------------------------------------------------------*/ 
    status2 = flashDeviceOperation(FLASHREAD, currentSupportStruct->sup_asid, frameAddress, missingPageNo);

    debug4(0,0,0,0);

    /* Treat any error status from the read operation as a program trap */
    if (status2 != SUCCESS) {
        mutex(&swapPoolSemaphore, FALSE);
        VMprogramTrapExceptionHandler(currentSupportStruct); /* Terminate the process */
    }

    /*--------------------------------------------------------------*
    * 10. Update the Swap Pool table's entry to reflect frame's new content
    *---------------------------------------------------------------*/ 
    swapPoolTable[frameNumber].vpn  = missingPageNo;
    swapPoolTable[frameNumber].asid = currentSupportStruct->sup_asid;
    swapPoolTable[frameNumber].pte  = &(currentSupportStruct->sup_privatePgTbl[missingPageNo]);

    debug5(0,0,0,0);

    /* NOTE: Disable interrupt to perform step 11 & 12 atomically */
    setInterrupt(FALSE);

    /*--------------------------------------------------------------*
    * 11. Update the Current Process's Page Table entry 
    *---------------------------------------------------------------*/ 
    /* Page missingPageNo is now present (V bit) and occupying frame frameAddress */
    currentSupportStruct->sup_privatePgTbl[missingPageNo].pt_entryLO = frameAddress | VALIDON | DIRTYON;

    /*--------------------------------------------------------------*
    * 12. Update the TLB - Erase all entries in the TLB
    *---------------------------------------------------------------*/ 
    TLBCLR();                   

    /* NOTE: Enable interrupt again, end of atomically step (11 & 12) */
    setInterrupt(TRUE);

    /*--------------------------------------------------------------*
    * 13. Release mutual exclusion over the Swap Pool table
    *---------------------------------------------------------------*/ 
    mutex(&swapPoolSemaphore, FALSE);

    /*--------------------------------------------------------------*
    * 14. Return control to the Current Process to retry the instruction that caused the page fault
    *---------------------------------------------------------------*/ 
    LDST(savedState);
}

/******************************* END OF VMSUPPORT.c *******************************/
