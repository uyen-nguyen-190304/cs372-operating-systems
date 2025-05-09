/******************************* DELAYDAEMON.c ***************************************
 * 
 * The Delay Daemon provides timed suspension for user processes. When a process invokes 
 * the `delay(milliseconds)` syscall (SYS18), it will be enqueded onto an Active Delay 
 * List (ADL) sorted by the wake up time and then blocked on its private semaphore. 
 * The Delay Daemon will periodically wake every 100 milliseconds to check the ADL, 
 * unblock any processes whose delay time has expired, and recylces their descriptors
 * back to the free list. The ADL is protected by a semaphore to ensure mutual exclusion
 * between the Delay Daemon and any user processes that may be modifying the list.
 * 
 * Written by  : Uyen Nguyen
 * Last update : 2025/05/09
 * 
 ***********************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/sysSupport.h"
#include "../h/delayDaemon.h"
#include "/usr/include/umps3/umps/libumps.h"

/******************************* GLOBAL VARIABLES *****************************/

/* Semphore for mutual exclusion on the Active Delay List (ADL) */
HIDDEN int ADLsemaphore;                    

/* Head pointer for the Active Delay List */
HIDDEN delayd_t *delayd_h;

/* Head pointer for the free list of delay descriptors */
HIDDEN delayd_t *delaydFree_h;

/* Static table of delay descriptors */
HIDDEN delayd_t delayEvents[UPROCMAX + 2];  /* +2 for dummy head & tail */ 

/******************************* ADL UTILITY FUNCTIONS *****************************/

/*
 * Function     :   allocateDelayd
 * Purpose      :   Allocate a delay descriptor from the free list. It will pop the first
 *                  descriptor from the free list (delaydFree_h) and initialize it.
 *                  The new descriptor will be initialized with a wake time of 0 and
 *                  a support structure pointer of NULL.
 * Parameters   :   None
 * Returns      :   Pointer to the allocated delay descriptor, 
 *                  or NULL if no free descriptors are available.
 */
HIDDEN delayd_t *allocateDelayd(void) {
    /* Check if the free list is empty */
    if (delaydFree_h == NULL) {
        /* If no delay descriptor available, return NULL */
        return NULL;  
    }

    /* Else, pop the first descriptor from the free list */
    delayd_t *newNode = delaydFree_h;
    delaydFree_h = delaydFree_h->d_next;        /* Advance the free head */

    /* Initialize the new delay descriptor */
    newNode->d_next      = NULL;
    newNode->d_wakeTime  = 0;
    newNode->d_supStruct = NULL;

    /* Return the new delay descriptor */
    return newNode;
}

/*
 * Function     :   freeDelayd
 * Purpose      :   Free a delay descriptor by pushing it back onto the free list.
 * Parameters   :   node - pointer to the delay descriptor to be freed
 * Returns      :   None
 */
HIDDEN void freeDelayd(delayd_t *node) {
    /* Push the node back onto the free list */
    node->d_next = delaydFree_h;
    delaydFree_h = node;                        /* Update the head accordingly */
}

/*
 * Function     :   insertDelayd
 * Purpose      :   Insert a delay descriptor into the Active Delay List (ADL) in sorted order.
 *                  The ADL is sorted by wake time in descending order.
 * Parameters   :   newNode - pointer to the delay descriptor to be inserted
 * Returns      :   None
 */
HIDDEN void insertDelayd(delayd_t *newNode) {
    /* Local pointers to help with insertion */
    delayd_t *prev = delayd_h;
    delayd_t *curr = delayd_h->d_next;

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
 * Function     :   initADL
 * Purpose      :   Initialize the Active Delay List (ADL) and set up the Delay Daemon.
 *                  This function sets up the free list of delay descriptors, initializes
 *                  the dummy head and tail of the ADL, and creates the Delay Daemon process.
 * Parameters   :   None
 * Returns      :   None
 */
void initADL(void) {
    /* Local variable: initial state for Delay Daemon */
    state_t initialState;                       

    /* Calculate ramtop to then get the penultimate frame for Delay Daemon's SP */
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR; 
    memaddr ramtop = devRegArea->rambase + devRegArea->ramsize;

    /* Initialize the ADL semaphore for mutual exclusion */
    ADLsemaphore = 1;

    /* Build the free list of delay descriptors */
    int i;
    for (i = 1; i < UPROCMAX; i++) {
        delayEvents[i].d_next = &(delayEvents[i + 1]);
    }
    delayEvents[UPROCMAX].d_next = NULL; /* Last node in the free list */
    delaydFree_h = &delayEvents[1]; /* The first node in the free list */

    /* Set up the ADL dummy head (node 0) */
    delayd_h = &delayEvents[0];
    delayd_h->d_wakeTime    = 0;
    delayd_h->d_supStruct   = NULL;
    delayd_h->d_next = &(delayEvents[UPROCMAX + 1]);    /* Point to the dummy tail */

    /* Set up the ADL dummy tail (node UPROCMAX + 1) */
    delayd_h->d_next->d_wakeTime  = INFINITE;           /* This will never wake */
    delayd_h->d_next->d_supStruct = NULL;   
    delayd_h->d_next->d_next      = NULL;               /* No next node */                  

    /* Initialize Delay Daemon's initial state */
    initialState.s_pc = initialState.s_t9 = (memaddr) delayDaemon;
    initialState.s_sp = ramtop - PAGESIZE;                      /* penultimate frame */
    initialState.s_status = ALLOFF | IEPON | IMON | PLTON;      /* kernel mode, all interrupts enabled */
    initialState.s_entryHI = ALLOFF | (DELAYASID << ASIDSHIFT); /* Set ASID to 0 */

    /* Create the Delay Daemon (Support Structure = NULL) */
    int status = SYSCALL(SYS1CALL, (unsigned int) &initialState, (unsigned int) NULL, 0);

    /* Check if the Delay Daemon was created successfully */
    if (status != CREATESUCCESS) {
        SYSCALL(SYS9CALL, 0, 0, 0); /* Terminate the process */
    }
}

/******************************* DELAY DAEMON *****************************/

/*
 * Function     :   delayDaemon
 * Purpose      :   The Delay Daemon process. It periodically wakes up every 100 milliseconds
 *                  to check the Active Delay List (ADL) for any processes that need to be woken.
 *                  It will unblock those processes and recycle their delay descriptors back
 *                  to the free list.
 * Parameters   :   None
 * Returns      :   None 
 */
void delayDaemon(void) {
    /* Local variable to store current time */
    cpu_t currentTime;

    while (TRUE) {
        /* Execute SYS7: Sleep for 100ms */
        SYSCALL(SYS7CALL, 0, 0, 0); 

        /* Obtain mutual exclusion over the ADL */
        SYSCALL(SYS3CALL, (unsigned int) &ADLsemaphore, 0, 0);

        /* Get the current time */
        STCK(currentTime);

        /* Loop through the ADL to find node whose wake up time has passed */
        delayd_t *curr = delayd_h->d_next;   
        while ((curr!= NULL) && (curr->d_wakeTime <= currentTime)) {
            /* Wake sleeping process by V on its private semaphore */
            SYSCALL(SYS4CALL, (unsigned int) &(curr->d_supStruct->sup_privateSemaphore), 0, 0);

            /* Remove the descriptor from the ADL */
            delayd_h->d_next = curr->d_next; 
            
            /* Recylce the descriptor back to the free pool */
            freeDelayd(curr);              

            /* Move to next descriptor to continue to check */
            curr = delayd_h->d_next;        
        }
        /* Release mutual exclusion over the ADL */
        SYSCALL(SYS4CALL, (unsigned int) &ADLsemaphore, 0, 0);
    }
}

/******************************* SYS18 IMPLEMENTATION *****************************/

/*
    * Function     :   delay
    * Purpose      :   The implementation of the delay syscall (SYS18). This function will
    *                  allocate a delay descriptor, insert it into the Active Delay List (ADL),
    *                  and block the process on its private semaphore until woken by the Delay Daemon.
    * Parameters   :   currentSupportStruct - pointer to the support structure of the calling process
    * Returns      :   None 
 */
void delay(support_t *currentSupportStruct) {
    /* Local variable to store current time */
    cpu_t currentTime;

    /* Extract delay duration (ms) from register a0 */
    int delayTime = currentSupportStruct->sup_exceptState[DELAYTIME].s_a0;

    /* Defensive programming: Ensure that the delayTime is nonnegative*/
    if (delayTime < 0) {
        /* Invalid argument: Call SYS9 on the process */
        SYSCALL(SYS9CALL, 0, 0, 0);
    }

    /* Obtain mutual exclusion over the ADL */
    SYSCALL(SYS3CALL, (unsigned int) &ADLsemaphore, 0, 0);  

    /* Allocate a delay descriptor from the free list */
    delayd_t *delayd  = allocateDelayd();

    /* Check if the allocation was successful */
    if (delayd == NULL) { 
        /* If not, first release the lock on ADL semaphore */
        SYSCALL(SYS4CALL, (unsigned int) &ADLsemaphore, 0, 0);

        /* Then, call SYS9 to terminate the process */
        SYSCALL(SYS9CALL, 0, 0, 0);
    }

    /* Else, populate the delayed node with correct wakeTime and supportStruct*/
    STCK(currentTime);
    delayd->d_wakeTime  = currentTime + ((cpu_t) delayTime * UNITCONVERT); /* Convert to microseconds */
    delayd->d_supStruct = currentSupportStruct;

    /* Insert it into its proper location on the active list */
    insertDelayd(delayd);

    /* Disable interrupts so that we can V on ADL semaphore and P on the proc's private semaphore atomically */
    setSTATUS(getSTATUS() & IECOFF);

    /* Release the ADL semaphore */
    SYSCALL(SYS4CALL, (unsigned int) &ADLsemaphore, 0, 0);

    /* Block process on its private semaphore until woken by daemon */
    SYSCALL(SYS3CALL, (unsigned int) &(currentSupportStruct->sup_privateSemaphore), 0, 0);

    /* Re-enable interrupts now that the atomic operations are complete */
    setSTATUS(getSTATUS() | IECON);

    /* Restore the process's state to the one saved in the support structure */
    LDST(&(currentSupportStruct->sup_exceptState[GENERALEXCEPT]));
} 


/******************************* END OF DELAYDAEMON.c *******************************/
