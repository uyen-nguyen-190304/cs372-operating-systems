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




/******************************* DISK OPERATIONS *****************************/

void diskPut(support_t *currentSupportStruct) {
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

    /* Copy the data from the disk sector -> device's DMA buffer */
    int i;
    for (i = 0; i < PAGESIZE; i++) {
        *(dmaBufferAddress + i) = *(logicalAddress + i);
    }

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
        devRegArea->devreg[diskNumber].d_command = (head << HEADNUMSHIFT) | (sector << SECTORNUMSHIFT) | WRITEBLK;

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
        /* Set v0 to the number of bytes written */
        currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = status;
    }

    /* Release mutual exclusion over the device's device register */
    SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[diskNumber], 0, 0);
}



void diskGet(support_t *currentSupportStruct) {
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
 
    /* Disable interrupts for atomic operations: COMMAND + SYS5 */
    setSTATUS(getSTATUS() & IECOFF);

    /* Place the cylindar number and SEEK command in disk's COMMAND field */
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
        devRegArea->devreg[diskNumber].d_command = (head << HEADNUMSHIFT) | (sector << SECTORNUMSHIFT) | READBLK;

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
        /* Copy the data from the device's DMA buffer -> U-proc's address space */
        int i;
        for (i = 0; i < PAGESIZE; i++) {
            *(logicalAddress + i) = *(dmaBufferAddress + i);
        }

        /* Set v0 to the number of bytes read */
        currentSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = status;
    }

    /* Release mutual exclusion over the device's device register */
    SYSCALL(SYS4CALL, (unsigned int) &devSemaphores[diskNumber], 0, 0);
}