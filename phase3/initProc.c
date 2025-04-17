/******************************* INITPROC.c ***************************************
 *  
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/04/13
 * 
 ***********************************************************************************/

/*
 ! NOTE: Implement allocate and deallocate (mentioned in the optimization part) 
 */

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

/**************************** SUPPORT LEVEL GLOBAL VARIABLES ****************************/ 

int masterSemaphore;                    /* Semaphore for synchronization */
int devSemaphores[MAXIODEVICES];        /* Semaphore for mutual exclusion */

/******************************* EXTERNAL ELEMENTS *******************************/

void debug1(int a, int b, int c, int d) {
    int i;
    i = 0;
    i++;
}

void test() {
    /*--------------------------------------------------------------*
    * 0. Initialize Local Variables 
    *---------------------------------------------------------------*/
    int pid;                                            /* Process ID */
    state_t initialState;                                       
    static support_t supportStructArray[UPROCMAX + 1];      /* Array of support structures for U-procs */
    int status;

    /*--------------------------------------------------------------*
     * 1. Initialize Phase 3 Data Structure 
    *---------------------------------------------------------------*/
    /* Initialize Swap Pool semaphore & table */
    initSwapStructs();                                 
    
    /* Initialize the semaphore of each (potentially) sharable peripheral I/O device */
    int i;
    for (i = 0; i < MAXDEVICES - 1; i++) {
        devSemaphores[i] = 1;                        /* For mutual exclusion */
    }

    /* Initialize the masterSemaphore */
    masterSemaphore = 0;                               /* For synchronization */

    /* Set PC and s_t9 to start of the .text section (0x8000.00B0) */
    initialState.s_pc = initialState.s_t9 = (memaddr) UPROCTEXTSTART;

    /* Set SP to start of the one-page user-mode stack (0xC000.0000) */
    initialState.s_sp = (memaddr) USERSTACKTOP;

    /* Set Status to user-mode with all interrupts and processor Local Timer Enable */
    initialState.s_status = ALLOFF | USERPON | IEPON | PLTON | IMON;

    /*--------------------------------------------------------------*
    * 2. Initialize and Launch (SYS1) between 1-8 U-Procs
    *---------------------------------------------------------------*/
    /* Loop for UPROCMAX U-Procs to set up the parameters for SYS1 and call SYS1 afterward */
    for (pid = 1; pid <= UPROCMAX; pid++) {
        /*----------------------------------------------------------*
        * a. Set up the initial processor state for the U-Proc
        *-----------------------------------------------------------*/
                       

        /* Set EntryHi.ASID to the process's unique ID */
        initialState.s_entryHI = ALLOFF | KUSEG | (pid << ASIDSHIFT); 

        /*----------------------------------------------------------*
        * b. Set up the support structure for the U-Proc
        *-----------------------------------------------------------*/
        /* Set sup_asid to the process's ASID */
        supportStructArray[pid].sup_asid = pid;

        /* Set the two PC fields: one to TLB handler, one to general exception handler */
        supportStructArray[pid].sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) pager;
        supportStructArray[pid].sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) VMgeneralExceptionHandler;

        /* Set the two Status registers: kernel-mode with all interrupts and Processor Local Timer enabled */
        supportStructArray[pid].sup_exceptContext[PGFAULTEXCEPT].c_status = ALLOFF | IEPON | PLTON | IMON;
        supportStructArray[pid].sup_exceptContext[GENERALEXCEPT].c_status = ALLOFF | IEPON | PLTON | IMON;

        /* Set the two SP fields: End of the two stack spaces allocated in Support Structure */
        supportStructArray[pid].sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) &(supportStructArray[pid].sup_stackTLB[499]);
        supportStructArray[pid].sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &(supportStructArray[pid].sup_stackGen[499]);

        /* Initialize the per-process Page Table */
        int j;
        for (j = 0; j < NUMPAGES; j++) {
            /* Set the VPN and ASID in EntryHI */
            supportStructArray[pid].sup_privatePgTbl[j].pt_entryHI = ALLOFF | (VPNSTART + j) << VPNSHIFT | (pid << ASIDSHIFT);

            /* Set the Dirty Bit in EntryLO */
            supportStructArray[pid].sup_privatePgTbl[j].pt_entryLO = ALLOFF | DIRTYON;
        }

        /* (Re)Set the VPN for Stack Page (last entry) to 0xBFFFF */
        supportStructArray[pid].sup_privatePgTbl[NUMPAGES - 1].pt_entryHI = ALLOFF | (STACKPAGEVPN << VPNSHIFT) | (pid << ASIDSHIFT);

        /*----------------------------------------------------------*
        * c. Call SYS1 to create the U-Proc
        *-----------------------------------------------------------*/
        debug1(0,0,0,0);
        status = SYSCALL(SYS1CALL, (unsigned int) &initialState, (unsigned int) &supportStructArray[pid], 0); 

        if (status != 0) {
            SYSCALL(SYS2CALL, 0, 0, 0);
        }
    }
    /*--------------------------------------------------------------*
    * 3. Repeatedly issue SYS3 on masterSemaphore for UPROCMAX times
    *---------------------------------------------------------------*/
    int k;
    for (k = 0; k < UPROCMAX; k++) {
        SYSCALL(SYS3CALL, (unsigned int) &masterSemaphore, 0, 0); /* P operation */
    }

    /* Terminate the instantiator process */
    SYSCALL(SYS2CALL, 0, 0, 0);
}

/******************************* END OF INITPROC.c *******************************/
