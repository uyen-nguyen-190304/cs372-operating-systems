/******************************* VMSUPPORT.c ***************************************
 * 
 * This module implements virtual memory support routines for the Pandos kernel.
 * It manages a semaphore-protected Swap Pool table, provides flash I/O operations
 * for backing-store swapping, select victim frames using a free-first then round-robin
 * policy, updates TLB entries, and handles page-fault exceptions through the pager functions.
 * The pager coordinates acquiring the Swap Pool lock, evicting pages if necessary, reading 
 * in the requested page from flash, updating the process's page table and TLB, and
 * return control to the faulting process.
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/04/17
 * 
 ***********************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/exceptions.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/************************* VMSUPPORT GLOBAL VARIABLES *************************/

int swapPoolSemaphore;                          /* Semaphore for the Swap Pool Table */
HIDDEN swap_t swapPoolTable[SWAPPOOLSIZE];      /* THE Swap Pool Table: one entry per swap pool frame */

/*
 * Function     :   initSwapStructs
 * Purpose      :   Initialize the Swap Pool semaphore and mark all frames free
 * Parameters   :   None
 * Returns      :   None 
 */
void initSwapStructs(void) {
    /* Initialize the Swap Pool Semaphore to 1 (mutual exclusion) */
    swapPoolSemaphore = 1;

    /* Iteratively initialize the Swap Pool table */
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        swapPoolTable[i].asid = EMPTYFRAME;     /* Set the ASID to EMPTYFRAME (-1) */
    }
}

/******************************* HELPER FUNCTIONS *******************************/

/* 
 * Function     :   setInterrupt
 * Purpose      :   Atomically enable/disable hardware interrupts around critical sections
 * Parameters   :   status - TRUE to enable interrupt, FALSE to disable
 * Returns      :   None
 */
void setInterrupt(int status) {
    /* If the calling function asking to enable interrupt */
    if (status == TRUE) {
        /* Set the interrupt enable bit */
        setSTATUS(getSTATUS() | IECON);    
    } else {
        /* Else, clear the interrupt enable bit */
        setSTATUS(getSTATUS() & IECOFF);   
    }
}

/*
 * Function     :   mutex
 * Purpose      :   Provide mutual exclusion by performing P or V on the given semaphore
 * Parameters   :   semaphore - pointer to the integer semaphore variable for mutual exclusion
 *                  doLock - TRUE for P, FALSE for V 
 * Return       :   None
 */
void mutex(int *semaphore, int doLock) {
    /* If doLock is TRUE, then wait on the semaphore */
    if (doLock == TRUE) {
        SYSCALL(SYS3CALL, (unsigned int) semaphore, 0, 0);  /* P operation */
    } else {
        /* Else, signal the semaphore instead */
        SYSCALL(SYS4CALL, (unsigned int) semaphore, 0, 0);  /* V operation */
    }
}

/*
 * Function     :   flashDeviceOperation 
 * Purpose      :   Perform a synchronous block read or write on the flash backing store device
 *                  corresponding to the given ASID. Ensure atomicity by disabling interrupts 
 *                  during register writes and protecting device access by a semaphore
 * Parameters   :   operation    - FLASHREAD (read page) or FLASHWRITE (write page)
 *                  ASID         - The ASID of the process whose page are swapped
 *                  frameAddress - Physical address of the target frame
 *                  pageNumber   - Logical page/block number for flash device
 * Returns      :   int - Device status code from d_status register (SUCCESS or error code) 
 */
void flashDeviceOperation(support_t *currentSupportStruct, int operation, int asid, int frameAddress, int pageNumber) {
    /* --------------------------------------------------------------
     * 1. Identify the device number for the flash
     * -------------------------------------------------------------- */
    /* Pointer to the device register area */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Compute the index into the device register array */
    int index = ((FLASHINT - OFFSET) * DEVPERINT) + (asid - 1);

    /* --------------------------------------------------------------
     * 2. Gain mutual exclusion over the device 
     * -------------------------------------------------------------- */
    mutex(&devSemaphores[index], TRUE);

    /* --------------------------------------------------------------
     * 3. Perform the flash device operation
     * --------------------------------------------------------------- */
    /* Write the frame's starting address into device's DATA0 field */
    devRegArea->devreg[index].d_data0 = frameAddress;

    /* Disable interrupts so that COMMAND + SYS5 is atomic */
    setInterrupt(FALSE);

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
    
    /* Get the status code from the device's register */
    int statusCode = devRegArea->devreg[index].d_status;

   /* --------------------------------------------------------------
    * 4. Release device semaphore
    * -------------------------------------------------------------- */
    mutex(&devSemaphores[index], FALSE);

   /* --------------------------------------------------------------
    * 5. Check the status code to see if an error occurred
    * -------------------------------------------------------------- */
    /* Any code that is different from READY (-1) is treated as an error */
    if (statusCode != READY) {
        /* Release the Swap Pool semaphore */
        mutex(&swapPoolSemaphore, FALSE);

        /* Terminate the current process */
        VMprogramTrapExceptionHandler(currentSupportStruct); /* Terminate the process */
    }
}

/************************* PAGE REPLACEMENT ALGORITHM *************************/
 
/*
* Function     :   pageReplacement 
* Purpose      :   An optimization to the default page replacement given by Pandos
*                  to select a physical frame in the Swap Pool to satisfy the
*                  page-in request. First, it scans for a free frame. If no free
*                  frame available, it will evict using round-robin
* Parameters   :   None
* Returns      :   int - Index into Swap Pool Table of chosen victim frame
*/
int pageReplacement(void) {
    /* --------------------------------------------------------------
     * 0. Local variables declaration
     * -------------------------------------------------------------- */    
    /* Static hand pointer to keep track of next candidate for round-robin */
    static int hand = 0;
    
    /* Initialize a victim variable */
    int victim = -1;

    /* --------------------------------------------------------------
     * 1. Search for Free Frame
     * -------------------------------------------------------------- */   
    /* First, scan through the entire Swap Pool for a free frame */
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        /* Compute the candidate index (wrap around via modulo) */
        int index = (hand + i) % (SWAPPOOLSIZE);

        /* If we found a free frame */
        if (swapPoolTable[index].asid == EMPTYFRAME) {
            /* Choose it (why not?) */
            victim = index;

            /* Advance hand to the slot after the one we just took */
            hand = (index + 1) % (SWAPPOOLSIZE);

            /* Return the index to the free frame in the Swap Pool we just found */
            return victim;
        }
    }

    /* --------------------------------------------------------------
     * 2. Since none available, evict through round-robin
     * -------------------------------------------------------------- */   
    /* No free frame was found. We need to evict the frame at hand */
    victim = hand;

    /* Advance hand for next round (round-robin) */
    hand = (hand + 1) % (SWAPPOOLSIZE);

    /* Return the index into swapPoolTable of the chosen victim frame */
    return victim;
}

/************************* TLB UPDATE FUNCTION *************************/

/*
 * Function     :   updateTLB
 * Purpose      :   Another optimization for update TLB. Here, the function
 *                  will either refresh or install a TLB entry corresponding
 *                  to the given Page Table entry. If an existing entry is found,
 *                  (TLBP probe success), overwrite it. Otherwise, leave the TLB
 *                  unchanged (hardware will refill later).
 * Parameters   :   ptEntry - Pointer to the page table entry containing ENTRYHI/ENTRYLO
 * Returns      :   None 
 */
void updateTLB(pte_t *ptEntry) {
    /* Load the VPN into the TLB's ENTRYHI register */
    setENTRYHI(ptEntry->pt_entryHI);

    /* Probe the TLB for an existing entry matching ENTRYHI */
    TLBP();       

    /* Test the INDEX register’s invalid bit: if it's 0, we found a valid entry */
    if ((getINDEX() & INDEXMASK) == CACHED) {
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
 * Purpose      :   The pager, acts as a Support Level's TLB exception handler, handles
 *                  TLB refill or page-fault exceptions for the current user process.
 *                  It coordinates swap-out of victim pages, swap-in of requested page, 
 *                  updates page table and TLB, and resumes execution at faulting instruction.
 *                  Workflow includes 14 steps as described in the project description
 * Parameters   :   None
 * Returns      :   None
 *  
 */
void pager(void) {
    /* --------------------------------------------------------------
     * 0. Initialize Local Variables 
     * -------------------------------------------------------------- */
    unsigned int exceptionCode;         /* Exception code for the TLB exception */
    int missingPageNo;                  /* Page number of the missing TLB entry */
    int frameNumber;                    /* Frame number of the page to be swapped in */
    int frameAddress;                   /* Frame address of the page to be swapped in */

    /*--------------------------------------------------------------*
    * 1. Obtain the pointer to the Current Process's Support Structure
    *---------------------------------------------------------------*/
    support_t *currentSupportStruct = (support_t *) SYSCALL(SYS8CALL, 0, 0, 0);

    /*--------------------------------------------------------------*
    * 2. Determine the case of the TLB exception
    *---------------------------------------------------------------*/    
    /* Get the saved exception state in Current Process's Support Structure for TLB exception */
    state_PTR savedState = &(currentSupportStruct->sup_exceptState[PGFAULTEXCEPT]);

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
    missingPageNo = (((savedState->s_entryHI) & VPNMASK) >> VPNSHIFT) % NUMPAGES;

    /*--------------------------------------------------------------*
    * 6. Pick a frame from the Swap Pool
    *---------------------------------------------------------------*/ 
    /* Frame is chosen by the page replacement algorithm provided above */
    frameNumber = pageReplacement();                     

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
        swapPoolTable[frameNumber].pte->pt_entryLO = swapPoolTable[frameNumber].pte->pt_entryLO & VALIDOFF;

        /* b. Update the TLB */
        updateTLB(swapPoolTable[frameNumber].pte);      

        /* NOTE: Enable interrupt again, end of atomically steps (a & b) */
        setInterrupt(TRUE); 

        /* c. Update process's backing store */
        flashDeviceOperation(currentSupportStruct, FLASHWRITE, swapPoolTable[frameNumber].asid, frameAddress, swapPoolTable[frameNumber].vpn);
    }
    
    /*--------------------------------------------------------------*
    * 9. Read the contents of the Current Process's backing store/flash device
    *---------------------------------------------------------------*/ 
    flashDeviceOperation(currentSupportStruct, FLASHREAD, currentSupportStruct->sup_asid, frameAddress, missingPageNo);

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
