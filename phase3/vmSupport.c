/******************************* VMSUPPORT.c ***************************************
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
#include "/usr/include/umps3/umps/libumps.h"

/************************* VMSUPPORT GLOBAL VARIABLES *************************/

int swapPoolSemaphore;                          /* Semaphore for the Swap Pool Table */
swap_t swapPoolTable[SWAPPOOLSIZE];             /* THE Swap Pool Table: one entry per swap pool frame */

/************************* GLOBAL VARIABLES INITIALIZATION *************************/

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
    if (operation == READBLK) {
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
 * Function     :   pageReplacement (first free, then round-robin) 
 */
int pageReplacement() {
    /* Static hand pointer to remember where we left off last time */
    static int hand = 0;
    
    /* Initialize a victim variable - act as a return frame index */
    int victim = -1;

    /* First, scan through the entire Swap Pool for a free frame */
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        /* Compute the candidate index (wrap around via modulo) */
        int index = (hand + i) % SWAPPOOLSIZE;

        /* If we found a free frame */
        if (swapPoolTable[index].asid == EMPTYFRAME) {
            /* Choose it (why not?) */
            victim = index;

            /* Advance hand to the slot after the one we just took */
            hand = (index + 1) % SWAPPOOLSIZE;

            /* Return the index to the free frame in the Swap Pool we just found */
            return victim;
        }
    }

    /* Here, no free frame was found. Then, we'll evict the frame at hand */
    victim = hand;

    /* Advance hand for next round (round-robin) */
    hand = (hand + 1) % SWAPPOOLSIZE;

    /* Return the index into swapPoolTable of the chosen victim frame */
    return victim;
}


/************************* TLB UPDATE FUNCTION *************************/

void updateTLB(pte_t *ptEntry) {
    /* Load the VPN into the TLB's ENTRYHI register */
    setENTRYHI(ptEntry->pt_entryHI);

    /* Probe the TLB for an existing entry matching ENTRYHI */
    TLBP();       

    /* Test the INDEX register’s invalid bit: if it's 0, we found a valid entry */
    if ((getINDEX() & INDEXMASK) == 0) {
        /* Then, load the physical frame mapping + flags into ENTRYLO */
        setENTRYLO(ptEntry->pt_entryLO);

        /* Also, write the updated TLB entry back at the probed index */
        TLBWI();
    }
    /* If no match was found, leave the TLB unchange */
}

/******************************* PAGER FUNCTION *******************************/

/*
 * Function     :   pager 
 */
void pager() {
    /*--------------------------------------------------------------*
    * 0. Initialize Local Variables 
    *---------------------------------------------------------------*/
    int         exceptionCode;                  /* Exception code for the TLB exception */
    int         missingPageNo;                  /* Page number of the missing TLB entry */
    int         frameNumber;                    /* Frame number of the page to be swapped in */
    int         frameAddress;                   /* Frame address of the page to be swapped in */
    int         status1, status2;               /* Status codes for the flash device operations */ 

    /*--------------------------------------------------------------*
    * 1. Obtain the pointer to the Current Process's Support Structure
    *---------------------------------------------------------------*/
    support_t *currentSupportStruct = (support_t *) SYSCALL(SYS8CALL, 0, 0, 0);

    /*--------------------------------------------------------------*
    * 2. Determine the case of the TLB exception
    *---------------------------------------------------------------*/    
    /* Get the saved exception state in Current Process's Support Structure for TLB exception */
    state_t *savedState = &(currentSupportStruct->sup_exceptState[PGFAULTEXCEPT]);

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

    /*--------------------------------------------------------------*
    * 6. Pick a frame from the Swap Pool
    *---------------------------------------------------------------*/ 
    /* Frame is chosen by the page replacement algorithm provided above */
    frameNumber  = pageReplacement();                         

    /* Calculate the frame address */
    frameAddress = (frameNumber * PAGESIZE) + SWAPPOOLSTART;    
    
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

        /* b. Update the TLB */
        updateTLB(swapPoolTable[frameNumber].pte);      

        /* NOTE: Enable interrupt again, end of atomically steps (a & b) */
        setInterrupt(TRUE); 

        /* c. Update process's backing store */
        status1 = flashDeviceOperation(WRITEBLK, swapPoolTable[frameNumber].asid, frameAddress, swapPoolTable[frameNumber].vpn);

        /* Treat any error status from the write operation as a program trap */
        if (status1 != SUCCESS) {
            /* Release the swapPoolSemaphore since it will get nuked next step anw */
            mutex(&swapPoolSemaphore, FALSE);

            /* Terminate the current process */
            VMprogramTrapExceptionHandler(currentSupportStruct); /* Terminate the process */
        }
    }
    
    /*--------------------------------------------------------------*
    * 9. Read the contents of the Current Process's backing store/flash device
    *---------------------------------------------------------------*/ 
    status2 = flashDeviceOperation(READBLK, currentSupportStruct->sup_asid, frameAddress, missingPageNo);

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

    /*--------------------------------------------------------------*
    * 11. Update the Current Process's Page Table entry 
    *---------------------------------------------------------------*/ 
    /* NOTE: Disable interrupt to perform step 11 & 12 atomically */
    setInterrupt(FALSE);

    /* Page missingPageNo is now present (V bit) and occupying frame frameAddress */
    currentSupportStruct->sup_privatePgTbl[missingPageNo].pt_entryLO = frameAddress | VALIDON | DIRTYON;

    /*--------------------------------------------------------------*
    * 12. Update the TLB
    *---------------------------------------------------------------*/ 
    updateTLB(&(currentSupportStruct->sup_privatePgTbl[missingPageNo]));
    
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
