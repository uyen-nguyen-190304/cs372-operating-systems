/******************************* DELAYDAEMON.c ***************************************
 * 
 * [DESCRIPTION LATER]
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/04/25
 * 
 ***********************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/delayDaemon.h"

/******************************* GLOBAL VARIABLES *****************************/

/* Semphore for mutual exclusion on the Active Delay List (ADL) */
HIDDEN int ADLsemaphore = 1;                    

/* Head pointer for the Active Delay List */
HIDDEN delayd_t *delay_h;

/* Head pointer for the free list of delay descriptors */
HIDDEN delayd_t *delayFree_h;

/******************************* ADL UTILITY FUNCTIONS *****************************/

/*
 * Function     : allocateDelayd
 */
delayd_t *allocateDelayd(void) {
    /* Check if the free list is empty */
    if (delayFree_h == NULL) {
        /* If no delay descriptor available, return NULL */
        return NULL;  
    }

    /* Pop the first descriptor from the free list */
    delayd_t *newNode = delayFree_h;
    delayFree_h = delayFree_h->d_next;

    /* Initialize the new delay descriptor */
    newNode->d_next = NULL;
    newNode->d_wakeTime = 0;
    newNode->d_supStruct = NULL;

    /* Return the new delay descriptor */
    return newNode;
}

/*
 * Function     : freeDelayd
 */
void freeDelayd(delayd_t *node) {
    /* Push the node onto the free list */
    node->d_next = delayFree_h;
    delayFree_h = node;
}

/*
 * Function     : insertDelayd
 */
void insertDelayd(delayd_t *newNode) {
    /* Local variables to help with insertion */
    delayd_t *prev = delay_h;
    delayd_t *curr = delay_h->d_next;

    /* Tranverse until we find the correct insertion point */
    while (curr != NULL && newNode->d_wakeTime > curr->d_wakeTime) {
        prev = curr;
        curr = curr->d_next;
    }

    /* Link the new node into the list */
    prev->d_next = newNode;
    newNode->d_next = curr;
}

/******************************* ADL INITIALIZATION *****************************/

/*
 * Function     : initADL
 * Purpose      : Initialize the Active Delay List (ADL) and create the Delay Daemon.
 * Parameters   : None
 * Returns      : None
 */
void initADL(void) {
    /* --------------------------------------------------------------
     * 0. Initialize Local Variables 
     *--------------------------------------------------------------- */
    int status;                                 /* Return code from SYS1 to create the Delay Daemon */
    state_t initialState;                       /* Initial state for the Delay Daemon */
    static delayd_t delayEvents[UPROCMAX + 1];  /* Delay descriptor table (+1 for dummy tail) */

    /* Calculate ramtop to then get the penultimate frame for Delay Daemon's SP */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR; 
    memaddr ramtop = devRegArea->rambase + devRegArea->ramsize;

    /* --------------------------------------------------------------
     * 1. Free List Setup
     *--------------------------------------------------------------- */
    int i;
    for (i = 0; i < UPROCMAX; i++) {
        /* Add each element from the static array to the free list */
        delayEvents[i].d_next = &delayEvents[i + 1];
    }
    /* Last element points to NULL */
    delayEvents[UPROCMAX - 1].d_next = NULL; 

    /* Set the head of the free list */
    delayFree_h = &delayEvents[0];

    /* --------------------------------------------------------------
     * 2. Initialize the Active List dummy tail
     *--------------------------------------------------------------- */
    delay_h              = &delayEvents[UPROCMAX];
    delay_h->d_next      = NULL; 
    delay_h->d_wakeTime  = INFINITE;        /* Dummy sentinel never wake up */
    delay_h->d_supStruct = NULL;

    /* --------------------------------------------------------------
     * 3. Initialize and Lauch (SYS1) the Delay Daemon
     *--------------------------------------------------------------- */
    /* The PC (and s_t9) is set to the delayDaemon function */
    initialState.s_pc = initialState.s_t9 = (memaddr) delayDaemon;

    /* The SP is set to an unused frame at the end of the RAM */
    initialState.s_sp = ramtop - 2 * PAGESIZE;          /* penultimate frame */

    /* The Status register is set to kernel-mode with all interrupts enabled */
    initialState.s_status = ALLOFF | IEPON | IMON | PLTON;

    /* The EntryHi.ASID is set the the kernel ASID: zero */
    initialState.s_entryHI = ALLOFF | (DELAYASID << ASIDSHIFT);

    /* Create the Delay Daemon (Support Structure = NULL) */
    status = SYSCALL(SYS1CALL, (unsigned int) &initialState, NULL, 0);

    /* Check if the Delay Daemon was created successfully */
    if (status != CREATESUCCESS) {
        /* If not, terminate the process */
        SYSCALL(SYS2CALL, 0, 0, 0);
    }
}

/******************************* DELAY DAEMON *****************************/

void delayDaemon(void) {
    /* Local variable to store current time */
    cpu_t currentTime;

    while (TRUE) {
        /* Execute a SYS7: wait for the next 100 milisecond */
        SYSCALL(SYS7CALL, DELAYTIME, 0, 0); 

        /* Obtain mutual exclusion over the ADL */
        mutex(&ADLsemaphore, TRUE);

        /* Get the current time */
        STCK(currentTime);

        /* Loop thorugh the ADL active list to find node whose wake up time has passed */
        while ((delay_h != NULL) && (delay_h->d_wakeTime <= currentTime)) {
            /* Perform a SYS4 on that U-proc's private semaphore */
            SYSCALL(SYS4CALL, (unsigned int) &(delay_h->d_supStruct->sup_privateSemaphore), 0, 0);

            /* Deallocate the delay descriptor node and return it to the free list */
            delayd_t *expired = delay_h;
            delay_h = delay_h->d_next;
            freeDelayd(expired);
        }

        /* Release mutual exclusion over the ADL */
        mutex(&ADLsemaphore, FALSE);
    }
}

/******************************* SYS18 IMPLEMENTATION *****************************/

void delay(support_t *currentSupportStruct) {
    /* --------------------------------------------------------------
     * 0. Local Variables Declaration
     *--------------------------------------------------------------- */
    int delayTime;                      /* Delay time in milliseconds */
    int status;                         /* Return code from SYS1 to create the Delay Daemon */
    cpu_t currentTime;                  /* Current time in microseconds */
    delayd_t *delayd;                   /* Pointer to the delay descriptor */

    /* --------------------------------------------------------------
     * 1. Get the delay time from the support structure
     *--------------------------------------------------------------- */
    delayTime = currentSupportStruct->sup_exceptState[DELAYTIME].s_a0;

    /* Check for the validity of the */
    if (delayTime < 0) {
        /* Be brutal: SYS9 on bad argument */
        SYSCALL(SYS9CALL, currentSupportStruct, 0, 0);
    }

    /* --------------------------------------------------------------
     * 2. Obtain mutual exclusion over the ADL
     *--------------------------------------------------------------- */
    SYSCALL(SYS3CALL, (unsigned int) &ADLsemaphore, 0, 0);  

    /* --------------------------------------------------------------
     * 3. Allocate a delay descriptor from the free list
     *--------------------------------------------------------------- */
    /* Allocate a delay_event descriptor node from the free list */
    delayd = allocateDelayd();

    if (delayd == NULL) { 
        /* First, release mutual exclusion over the ADL */
        SYSCALL(SYS4CALL, (unsigned int) &ADLsemaphore, 0, 0);

        /* If no delay descriptor is available, be brutal: SYS9 on bad argument */
        SYSCALL(SYS9CALL, currentSupportStruct, 0, 0);
    }

    /* Else, populate the delayed node */
    stck(currentTime);
    delayd->d_wakeTime  = currentTime + (delayTime * UNITCONVERT); /* Convert to microseconds */
    delayd->d_supStruct = currentSupportStruct;

    /* Insert it into its proper location on the active list */
    insertDelayd(delayd);

    /* --------------------------------------------------------------
     * 4. Release mutual exclusion over the ADL
     *--------------------------------------------------------------- */
    /* Disable interrupts so that */
    setSTATUS(getSTATUS() & IECOFF);

    /* Release the ADL semaphore */
    SYSCALL(SYS4CALL, (unsigned int) &ADLsemaphore, 0, 0);

    /* P on U-proc's private semaphore */
    SYSCALL(SYS3CALL, (unsigned int) &(currentSupportStruct->sup_privateSemaphore), 0, 0);

    /* Re-enable interrupts now that the atomic operation is complete */
    setSTATUS(getSTATUS() | IECON);

    /* When woken, restore process context and return to user */
    LDST(&(currentSupportStruct->sup_exceptState[GENERALEXCEPT]));
    } 


/******************************* END OF DELAYDAEMON.c *******************************/
