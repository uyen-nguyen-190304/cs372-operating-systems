/******************************* INITPROC.c ***************************************
 *  
 * This module implements the Phase 3 initialization process for the Pandos kernel.
 * It initializes the Swap Pool structures (table & semaphore), device semaphores,
 * and master semaphore. It constructs and configures the initial processor state 
 * and support structures (exception contexts and private page table) for up to 
 * UPROCMAX user processes, then invokes SYS1 to spawn each U-Proc. After creation,
 * it performs SYS3 (P) on the masterSemaphore UPROCMAX times to synchronize, and
 * finally issues SYS2 to terminate (halt) the system.
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

/**************************** SUPPORT LEVEL GLOBAL VARIABLES ****************************/ 

int masterSemaphore;                    /* Master semaphore for synchronizing U-Procs */
int devSemaphores[MAXIODEVICES];        /* Semaphore for mutual exclusion on each I/O device */

/******************************* EXTERNAL ELEMENTS *******************************/

/*
 * Function     :   test()
 * Purpose      :   Carry out Phase 3 initialization: setup Swap Pool Table and semaphores,
 *                  build initial processor states and support structures for each U-Proc,
 *                  create processes via SYS1, synchronize via SYS3, and halt via SYS2.
 * Parameters   :   None
 * Returns      :   None 
 */
void test() {
    /* --------------------------------------------------------------
     * 0. Initialize Local Variables 
     *--------------------------------------------------------------- */
    int pid;                                                /* U-Proc identifier (1..UPROCMAX) */
    int status;                                             /* Return code from SYS1 */
    state_t initialState;                                   /* Initial state template for new U-Proc */    
    static support_t supportStructArray[UPROCMAX + 1];      /* Support structure for each U-Proc */

    /* --------------------------------------------------------------
     * 1. Initialize Phase 3 Data Structure 
     * --------------------------------------------------------------- */
    /* Initialize Swap Pool table and its semaphore */
    initSwapStructs();                                      /* Defined in vmSupport.c */   
    
    /* Initialize the Active Delay List */
    initADL();                                             /* Defined in delayDaemon.c */
    
    /* Initialize each (potentially) sharable peripheral I/O device semaphore */
    int i;
    for (i = 0; i < MAXIODEVICES; i++) {
        devSemaphores[i] = 1;                               /* For mutual exclusion */
    }

    /* Initialize the masterSemaphore */
    masterSemaphore = 0;                                    /* For synchronization */

    /* --------------------------------------------------------------
     * 2. Setup the Initial Processor State Template for each U-Proc
     * --------------------------------------------------------------- */
    /* Set PC and s_t9 to start of the .text section (0x8000.00B0) */
    initialState.s_pc = initialState.s_t9 = (memaddr) UPROCTEXTSTART;

    /* Set SP to start of the one-page user-mode stack (0xC000.0000) */
    initialState.s_sp = (memaddr) USERSTACKTOP;

    /* Set Status to user-mode with all interrupts and processor Local Timer Enable */
    initialState.s_status = ALLOFF | USERPON | IEPON | PLTON | IMON;

    /* --------------------------------------------------------------
     * 3. Initialize and Launch (SYS1) between 1-8 U-Procs
     * --------------------------------------------------------------- */
    /* Loop for UPROCMAX U-Procs to set up the parameters for SYS1 and call SYS1 afterward */
    for (pid = 1; pid <= UPROCMAX; pid++) {
        /* ----------------------------------------------------------
         * a. Set EntryHi.ASID to the process's unique ID
         * ----------------------------------------------------------- */
        initialState.s_entryHI = ALLOFF | KUSEG | (pid << ASIDSHIFT); 

        /* ----------------------------------------------------------
         * b. Set up the support structure for the U-Proc
         * ----------------------------------------------------------- */
        /* Set sup_asid to the process's ASID */
        supportStructArray[pid].sup_asid = pid;

        /* Set the two PC fields: one to TLB handler, one to general exception handler */
        supportStructArray[pid].sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) pager;
        supportStructArray[pid].sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) VMgeneralExceptionHandler;

        /* Set the two Status registers: kernel-mode with all interrupts and Processor Local Timer enabled */
        supportStructArray[pid].sup_exceptContext[PGFAULTEXCEPT].c_status = ALLOFF | IEPON | PLTON | IMON;
        supportStructArray[pid].sup_exceptContext[GENERALEXCEPT].c_status = ALLOFF | IEPON | PLTON | IMON;

        /* Set the two SP fields: End of the two stack spaces allocated in Support Structure */
        supportStructArray[pid].sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) &(supportStructArray[pid].sup_stackTLB[STACKTOP]);
        supportStructArray[pid].sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &(supportStructArray[pid].sup_stackGen[STACKTOP]);

        /* ----------------------------------------------------------
         * c. Initialize the per-process Page Table
         * ----------------------------------------------------------- */
        int j;
        for (j = 0; j < NUMPAGES; j++) {
            /* Set the VPN and ASID in EntryHI */
            supportStructArray[pid].sup_privatePgTbl[j].pt_entryHI = ALLOFF | (VPNSTART + j) << VPNSHIFT | (pid << ASIDSHIFT);

            /* Set the Dirty Bit in EntryLO */
            supportStructArray[pid].sup_privatePgTbl[j].pt_entryLO = ALLOFF | DIRTYON;
        }

        /* (Re)Set the VPN for Stack Page (last entry) to 0xBFFFF */
        supportStructArray[pid].sup_privatePgTbl[NUMPAGES - 1].pt_entryHI = ALLOFF | (STACKPAGEVPN << VPNSHIFT) | (pid << ASIDSHIFT);

        /* ----------------------------------------------------------
         * d. Invoke SYS1 to create the U-Proc
         * ----------------------------------------------------------- */
        status = SYSCALL(SYS1CALL, (unsigned int) &initialState, (unsigned int) &(supportStructArray[pid]), 0); 

        /* Check the status after creation */
        if (status != CREATESUCCESS) {
            /* If creation fails, terminate immediately (sorry) */
            SYSCALL(SYS2CALL, 0, 0, 0);
        }
    }
    /* --------------------------------------------------------------
     * 4. Synchronize with U-Procs via masterSemaphore (SYS3)
     * --------------------------------------------------------------- */
    int k;
    for (k = 0; k < UPROCMAX; k++) {
        /* Repeated issue SYS3 on masterSemaphore for UPROCMAX times */
        SYSCALL(SYS3CALL, (unsigned int) &masterSemaphore, 0, 0); 
    }

    /* --------------------------------------------------------------
     * 4. After the loop, test() concludes by issuing a SYS2 -> HALT
     *---------------------------------------------------------------*/
    SYSCALL(SYS2CALL, 0, 0, 0);         /* Farewell */
}

/******************************* END OF INITPROC.c *******************************/
