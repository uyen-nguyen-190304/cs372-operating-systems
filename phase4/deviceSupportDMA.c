/*************************** DEVICESUPPORTDMA.c ***********************************
 * 
 * [DESCRIPTION LATER]
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/05/06
 * 
 ***********************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/initial.h"
#include "../h/initProc.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/*
 * To perform a disk/flash read operation
 *      1. The requested disk sector/flash block is read into the device's DMA buffer
 *      2. The data is copied from the DMA buffer into the requesting U-Proc's address space starting from the provided address
 */

/*
 * A write operation is isomorphic, only the two steps are reversed:
 *      1. The data is copied from the requesting U-proc's address space into the device's DMA buffer
 *      2. The targeted disk sector/flask block is overwritten with the contents of the DMA buffer
 */

/******************************* HELPER FUNCTION *****************************/

HIDDEN void copyData(char *source, char *destination, int size) {
    /* Copy data from source to destination */
    int i;
    for (i = 0; i < size; i++) {
        /* Copy one byte at a time */
        *(destination + i) = *(source + i);
    }
}

/******************************* DISK OPERATIONS *****************************/

void diskOperation(support_t *currentSupportStruct, int operation) {
    /* Retrieve parameters from the except state */
    int *logicalAddress = (int *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int diskNumber      = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a2;
    int sectionNumber   = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a3;

    /* Retrieve the three dimensional parameters */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;
    int maxCylinder = (devRegArea->devreg[diskNumber].d_data1) >> CYLINDERSHIFT;
    int maxHead     = ((devRegArea->devreg[diskNumber].d_data1) & HEADMASK) >> HEADSHIFT ;
    int maxSector   = (devRegArea->devreg[diskNumber].d_data1) & SECTORMASK;
    int maxCount    = maxCylinder * maxHead * maxSector;

    /* Compute the disk index */
    int diskIndex = ((DISKINT - OFFSET) * DEVPERINT) + diskNumber;

    /* Defensive programming: check the virtual address and section number */
    if ((sectionNumber < 0) || ((int) logicalAddress < KUSEG) || (sectionNumber < 0) || (sectionNumber > maxCount)) {
        /* Terminate the U-proc */
        SYSCALL(SYS9CALL, 0, 0, 0);
    }

    /* Gain mutual exclusion over the device's device register */
    SYSCALL(SYS3CALL, (unsigned int) &devSemaphores[diskNumber], 0, 0);

    /* Compute DMA buffer address */
    memaddr *dmaBufferAddress = (memaddr *) (DISKSTART + (diskNumber * PAGESIZE));

    /* Convert sector number to CHS */
    int cylinder = sectionNumber / (maxHead * maxSector);
    int head     = (sectionNumber / maxSector) % maxHead;
    int sector   = sectionNumber % maxSector;

    /* If it is a read operation: Copy the data from the disk sector -> device's DMA buffer */
    if (operation == READBLK) {
        copyData(logicalAddress, dmaBufferAddress, PAGESIZE);
    }

    /* Disable interrupts for atomic operations: COMMAND + SYS5 */
    setSTATUS(getSTATUS() & IECOFF);

    /* Place the cylinder number and SEEK command in disk's COMMAND field */
    devRegArea->devreg[diskIndex].d_command = (cylinder << CYLNUMSHIFT) | SEEKCYL;

    /* Issue a SYS5 with the appropriate parameters */
    int status = SYSCALL(SYS5CALL, DISKINT, diskIndex, FALSE);

    /* Re-enable interrupts now that the atomic operation is complete */
    setSTATUS(getSTATUS() | IECON);

    /* If the seek device was successful, continue with the write operation */
    if (status == SUCCESS) {
        /* Write the starting address of the DMA buffer in the device's DATA0 field */
        devRegArea->devreg[diskIndex].d_data0 = (unsigned int) dmaBufferAddress;

        /* Disable interrupts for atomic operations: COMMAND + SYS5 */
        setSTATUS(getSTATUS() & IECOFF);

        /* Place the head number, section number, and READBLK command in disk's COMMAND field */
        devRegArea->devreg[diskIndex].d_command = (head << HEADNUMSHIFT) | (sector << SECTORNUMSHIFT) | WRITEBLK;

        /* Issue a SYS5 with the appropriate parameters */
        status = SYSCALL(SYS5CALL, DISKINT, diskNumber, FALSE);
    
        /* Re-enable interrupts now that the atomic operation is complete */
        setSTATUS(getSTATUS() | IECON);
    } 

    /* If any of the operation was unsuccessful */
    if (status != SUCCESS) {
        /* Set v0 to the negative of the status code to signal an error */
        currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -1 * status;
    } else {
        /* If it is a write operation: Copy the data from DMA buffer -> disk sector*/
        if (operation == WRITEBLK) {
            copyData(logicalAddress, dmaBufferAddress, PAGESIZE);
        }
        /* Set v0 to the number of bytes written */
        currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = status;
    }

    /* Release mutual exclusion over the device's device register */
    SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[diskIndex], 0, 0);
}

/******************************* FLASH OPERATIONS *****************************/

void flashOperation(support_t *currentSupportStruct, int operation) {
    /* Retrieve parameters from the except state */
    int *logicalAddress = (int *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int flashNumber     = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a2;
    int blockNumner     = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a3;
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Compute the device index */
    int flashIndex = ((FLASHINT - OFFSET) * DEVPERINT) + flashNumber;

    /* Defensive programming: check the virtual address and block number */
    if ((blockNumner < 0) || (blockNumner >= MAXBLOCK) || (int) logicalAddress < KUSEG) {
        /* Terminate the U-proc */
        SYSCALL(SYS9CALL, 0, 0, 0);
    }

    /* Gain mutual exclusion over the device's device register */
    SYSCALL(SYS3CALL, (unsigned int) &devSemaphores[flashIndex], 0, 0);

    /* Compute DMA buffer address */
    memaddr *dmaBufferAddress = (memaddr *) (DISKSTART + (flashNumber * PAGESIZE));

    /* If it is a read operation: Copy the data from the flash block -> device's DMA buffer */
    if (operation == FLASHREAD) {
        copyData(logicalAddress, dmaBufferAddress, PAGESIZE);
    }

    /* Write DMA buffer starting address into DATA0 */
    devRegArea->devreg[flashIndex].d_data0 = (unsigned int) dmaBufferAddress;

    /* Disable interrupts so that COMMAND + SYS5 is atomic */
    setSTATUS(getSTATUS() & IECOFF);

    /* If the operation requested is a READ opeartion */
    if (operation == FLASHREAD) {
        devRegArea->devreg[flashIndex].d_command = (blockNumner << BLOCKSHIFT) | READBLK;
    } else {
        devRegArea->devreg[flashIndex].d_command = (blockNumner << BLOCKSHIFT) | WRITEBLK;
    }

    /* Wait for the device to complete the operation */
    int status = SYSCALL(SYS5CALL, FLASHINT, flashNumber, operation);

    /* Re-enable interrupts now that the atomic operation is complete */
    setSTATUS(getSTATUS() | IECON);

    /* If any of the operation was unsuccessful */
    if (status != SUCCESS) {
        /* Set v0 to the negative of the status code to signal an error */
        currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -1 * status;
    } else {
        if (operation == WRITEBLK) {
            copyData(logicalAddress, dmaBufferAddress, PAGESIZE);
        }
        /* Set v0 to the number of bytes written */
        currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = status;
    }

    /* Release mutual exclusion over the device's device register */
    SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[flashIndex], 0, 0);
}
