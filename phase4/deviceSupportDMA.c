/*************************** DEVICESUPPORTDMA.c ***********************************
 * 
 * [DESCRIPTION LATER]
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/05/10
 * 
 ***********************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/initial.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "../h/deviceSupportDMA.h"
#include "/usr/include/umps3/umps/libumps.h"

/******************************* DISK OPERATIONS *****************************/

/*
 * 
 * 
 *  
 */
void diskPut(support_t *currentSupportStruct) {
    /* Retrieve the parameters from the exception state */
    memaddr *logicalAddress = (memaddr *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int     diskNumber      = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a2;
    int     sectionNumber   = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a3;

    /* Retrieve the three dimensional parameters */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;
    int maxCylinder = (devRegArea->devreg[diskNumber].d_data1) >> CYLINDERSHIFT;
    int maxHead     = ((devRegArea->devreg[diskNumber].d_data1) & HEADMASK) >> HEADSHIFT ;
    int maxSector   = (devRegArea->devreg[diskNumber].d_data1) & SECTORMASK;
    int maxCount    = maxCylinder * maxHead * maxSector;

    /* Defensive programming: check the virtual address and section number */
    if ((sectionNumber < 0) || (sectionNumber > maxCount) || ((int) logicalAddress < KUSEG)) {
        /* Terminate the U-proc */
        SYSCALL(SYS9CALL, 0, 0, 0);
    }

    /* Compute the DMA buffer address */
    memaddr *dmaBufferAddress = (memaddr *) (DISKSTART + (diskNumber * PAGESIZE));

    /* Copy the data from the disk sector -> DMA buffer */
    int i;
    for (i = 0; i < (PAGESIZE / WORDLEN); i++) {
        dmaBufferAddress[i] = logicalAddress[i];
    }

    /* Gain mutual exclusion over the device's device register */
    SYSCALL(SYS3CALL, (unsigned int) &devSemaphores[diskNumber], 0, 0);

    /* Covert sector number to CHS */
    int cylinder = sectionNumber / (maxHead * maxSector);
    int temp     = sectionNumber % (maxHead * maxSector);
    int head     = temp / maxSector;
    int sector   = temp % maxSector;

    /* Disable interrupts for atomic operations: COMMAND + SYS5 */
    setSTATUS(getSTATUS() & IECOFF);

    /* Place the cylinder number and SEEK command in disk's COMMAND field */
    devRegArea->devreg[diskNumber].d_command = (cylinder << CYLNUMSHIFT) | SEEKCYL;

    /* Issue a SYS5 with the appropriate parameters */
    int status = SYSCALL(SYS5CALL, DISKINT, diskNumber, FALSE);

    /* Re-enable interrupts now that the atomic operation is complete */
    setSTATUS(getSTATUS() | IECON);

    /* If the seek device was successful, continue with the write operation */
    if (status == SUCCESS) {
        /* Write the starting address of the DMA buffer in the device's DATA0 field */
        devRegArea->devreg[diskNumber].d_data0 = (unsigned int) dmaBufferAddress;

        /* Disable interrupts for atomic operations: COMMAND + SYS5 */
        setSTATUS(getSTATUS() & IECOFF);

        /* Place the head number, section number, and READBLK command in disk's COMMAND field */
        devRegArea->devreg[diskNumber].d_command = (head << HEADNUMSHIFT) | (sector << SECTORNUMSHIFT) | DISKWRITEBLK;

        /* Issue a SYS5 with the appropriate parameters */
        status = SYSCALL(SYS5CALL, DISKINT, diskNumber, FALSE);
    
        /* Re-enable interrupts now that the atomic operation is complete */
        setSTATUS(getSTATUS() | IECON);
    } 

    /* Release mutual exclusion over the device's device register */
    SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[diskNumber], 0, 0);

    /* If any of the operation was unsuccessful */
    if (status != SUCCESS) {
        /* Set v0 to the negative of the status code to signal an error */
        currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -1 * status;
    } else {
        /* Set v0 to the number of bytes written */
        currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = status;
    }

    /* Return control to the instruction after SYSCALL instruction */
    LDST(&(currentSupportStruct->sup_exceptState[GENERALEXCEPT]));
}

/*
 * 
 * 
 *  
 */
void diskGet(support_t *currentSupportStruct) {
    /* Retrieve the parameters from the exception state */
    memaddr *logicalAddress = (memaddr *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int     diskNumber      = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a2;
    int     sectionNumber   = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a3;

    /* Retrieve the three dimensional parameters */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;
    int maxCylinder = (devRegArea->devreg[diskNumber].d_data1) >> CYLINDERSHIFT;
    int maxHead     = ((devRegArea->devreg[diskNumber].d_data1) & HEADMASK) >> HEADSHIFT ;
    int maxSector   = (devRegArea->devreg[diskNumber].d_data1) & SECTORMASK;
    int maxCount    = maxCylinder * maxHead * maxSector;

    /* Defensive programming: check the virtual address and section number */
    if ((sectionNumber < 0) || ((int) logicalAddress < KUSEG) || (sectionNumber > maxCount)) {
        /* Terminate the U-proc */
        SYSCALL(SYS9CALL, 0, 0, 0);
    }

    /* Compute the DMA buffer address */
    memaddr *dmaBufferAddress = (memaddr *) (DISKSTART + (diskNumber * PAGESIZE));

    /* Gain mutual exclusion over the device's device register */
    SYSCALL(SYS3CALL, (unsigned int) &devSemaphores[diskNumber], 0, 0);

    /* Covert sector number to CHS */
    int cylinder = sectionNumber / (maxHead * maxSector);
    int temp     = sectionNumber % (maxHead * maxSector);
    int head     = temp / maxSector;
    int sector   = temp % maxSector;

    /* Disable interrupts for atomic operations: COMMAND + SYS5 */
    setSTATUS(getSTATUS() & IECOFF);

    /* Place the cylinder number and SEEK command in disk's COMMAND field */
    devRegArea->devreg[diskNumber].d_command = (cylinder << CYLNUMSHIFT) | SEEKCYL;

    /* Issue a SYS5 with the appropriate parameters */
    int status = SYSCALL(SYS5CALL, DISKINT, diskNumber, FALSE);

    /* Re-enable interrupts now that the atomic operation is complete */
    setSTATUS(getSTATUS() | IECON);

    /* If the seek device was successful, continue with the read operation */
    if (status == SUCCESS) {
        /* Write the starting address of the DMA buffer in the device's DATA0 field */
        devRegArea->devreg[diskNumber].d_data0 = (unsigned int) dmaBufferAddress;

        /* Disable interrupts for atomic operations: COMMAND + SYS5 */
        setSTATUS(getSTATUS() & IECOFF);

        /* Place the head number, section number, and READBLK command in disk's COMMAND field */
        devRegArea->devreg[diskNumber].d_command = (head << HEADNUMSHIFT) | (sector << SECTORNUMSHIFT) | DISKREADBLK;

        /* Issue a SYS5 with the appropriate parameters */
        status = SYSCALL(SYS5CALL, DISKINT, diskNumber, FALSE);
    
        /* Re-enable interrupts now that the atomic operation is complete */
        setSTATUS(getSTATUS() | IECON);
    }

    /* Release mutual exclusion over the device's device register */
    SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[diskNumber], 0, 0);

    /* If the disk read operation was successful */
    if (status == SUCCESS) {
        /* Copy the data from the device's DMA buffer -> U-proc's address space */
        int i;
        for (i = 0; i < (PAGESIZE / WORDLEN); i++) {
            logicalAddress[i] = dmaBufferAddress[i];
        }

        /* Set v0 to the number of bytes read */
        currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = status;
    } else {
        /* Set v0 to the negative of the status code to signal an error */
        currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -1 * status;
    }

    /* Return control to the instruction after SYSCALL instruction */
    LDST(&(currentSupportStruct->sup_exceptState[GENERALEXCEPT]));
}

/******************************* FLASH OPERATIONS *****************************/

/*
 * 
 * 
 * 
 */
int flashOperation(support_t *currentSupportStruct, int logicalAddress, int flashNumber, int blockNumber, int operation) {
    /* Pointer to the device register area */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;

    /* Compute the index into the device register array */
    int flashIndex = ((FLASHINT - OFFSET) * DEVPERINT) + flashNumber;

    /* Retrieve the maximum number of blocks for the device */
    int maxBlock = devRegArea->devreg[flashIndex].d_data1;

    /* Defensive programming: Check for virtual address and blocknumber */
    if ((blockNumber < 0) || (blockNumber >= maxBlock) || ((int) logicalAddress < KUSEG)) {
        SYSCALL(SYS9CALL, 0, 0, 0); /* Terminate the U-proc */
    }

    /* Gain mutual exclusive over the device's device register */
    SYSCALL(SYS3CALL, (unsigned int) &devSemaphores[flashIndex], 0, 0);

    /* Write the frame's starting address into device's DATA0 field */
    devRegArea->devreg[flashIndex].d_data0 = logicalAddress;

    /* Disable interrupt so that COMMAND + SYS5 is atomic */
    setSTATUS(getSTATUS() & IECOFF);

    /* If the operation requested a READ operation */
    if (operation == FLASHREAD) {
        /* Set the device's command register to READ and put in the block number */
        devRegArea->devreg[flashIndex].d_command = (blockNumber << BLOCKSHIFT) | READBLK;
    } else {
        /* Set the device's command register to WRITE and put in the block number */
        devRegArea->devreg[flashIndex].d_command = (blockNumber << BLOCKSHIFT) | WRITEBLK;
    }

    /* Wait for the device to complete the operation */
    int status = SYSCALL(SYS5CALL, FLASHINT, flashNumber, FALSE);

    /* Re-enable interrupts now that the atomic operation is complete */
    setSTATUS(getSTATUS() | IECON);

    /* Release the device semaphore */
    SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[flashIndex], 0, 0);

    /* Check the status code to see if an error occurred */
    if (status != READY) {
        /* Status code = negative */
        status = -1 * status;
    } 
    return status;
}


void flashPut(support_t *currentSupportStruct) {
    /* Retrieve the parameters from the exception state */
    memaddr *logicalAddress = (memaddr *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int flashNumber         = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a2;
    int blockNumber         = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a3;

    /* Defensive programming: check the virtual address */
    if ((int) logicalAddress < KUSEG) {
        /* Terminate the U-proc */
        SYSCALL(SYS9CALL, 0, 0, 0);
    }

    /* Calculate the DMA buffer address */
    memaddr *dmaBufferAddress = (memaddr *) (FLASHSTART + (flashNumber * PAGESIZE));

    /* Copy the data from the U-proc's address space -> device's DMA buffer */
    int i;
    for (i = 0; i < (PAGESIZE / WORDLEN); i++) {
        dmaBufferAddress[i] = logicalAddress[i];
    }

    /* Perform the flash operation */
    int status = flashOperation(currentSupportStruct, (int) dmaBufferAddress, flashNumber, blockNumber, FLASHWRITE);

    /* Place the status code into v0 */
    currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = status;

    /* Return control to the instruction after SYSCALL instruction */
    LDST(&(currentSupportStruct->sup_exceptState[GENERALEXCEPT]));
}




void flashGet(support_t *currentSupportStruct) {
    /* Retrieve the parameters from the exception state */
    memaddr *logicalAddress = (memaddr *) currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int flashNumber         = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a2;
    int blockNumber         = currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_a3;

    /* Defensive programming: check the virtual address */
    if ((int) logicalAddress < KUSEG) {
        /* Terminate the U-proc */
        SYSCALL(SYS9CALL, 0, 0, 0);
    }

    /* Calculate the DMA buffer address */
    memaddr *dmaBufferAddress = (memaddr *) (FLASHSTART + (flashNumber * PAGESIZE));

    /* Perform the flash operation */
    int status = flashOperation(currentSupportStruct, (int) dmaBufferAddress, flashNumber, blockNumber, FLASHREAD);

    /* Copy the data from the device's DMA buffer -> U-proc's address space */
    int i;
    for (i = 0; i < (PAGESIZE / WORDLEN); i++) {
        logicalAddress[i] = dmaBufferAddress[i];
    }

    /* Place the status code into v0 before return */
    currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = status;


    /* Return control to the instruction after SYSCALL instruction */
    LDST(&(currentSupportStruct->sup_exceptState[GENERALEXCEPT]));
}





