/******************************* ASL.c ***************************************
 *
 *
 * 
 * 
 * 
 * 
 * 
 *****************************************************************************/

#include "../h/pcb.h"
#include "../h/const.h"
#include "../h/asl.h"

/******************************* GLOBAL VARIABLES *****************************/

/* Head pointer for the active semaphore list */
HIDDEN semd_t *semd_h;

/* Head pointer for the free semaphore list */
HIDDEN semd_t *semdFree_h;

/******************************* HELPER FUNCTIONS *****************************/

HIDDEN semd_PTR findSem(int *semAdd)
{
    semd_PTR prev = semd_h;

    while (prev->s_next != NULL && prev->s_next->s_semAdd < semAdd) {
       prev = prev->s_next;
    }
    return prev;
}

/******************************* SEMAPHORE MANAGEMENT *****************************/

/* 
 * Function     : insertBlocked
 * Purpose      : Insert the pcb pointed to by p at the tail of the process queue as-
 *                sociated with the semaphore whose physical address is semAdd and
 *                set the semaphore address of p to semAdd. If the semaphore is cur-
 *                rently not active, allocate a new descriptor from the semdFree list,
 *                insert it in the ASL, initialize all of the fields, and proceed as above.
 *                If a new semaphore descriptor needs to be allocated and the semdFree
 *                list is empty, return TRUE. In all other cases return FALSE.
 * Parameters   : semAdd - pointer to the semaphore
 *                p      - pointer to the pcb to be inserted
 */
int insertBlocked(int *semAdd, pcb_PTR p)
{
    semd_PTR prev = findSemaphore(semAdd);
    semd_PTR current = prev->s_next;

    if (current->s_semAdd == semAdd) {
        /* Semaphore already exists in ASL */
        insertProcQ(&current->s_procQ, p);
        p->p_semAdd = semAdd;
        return FALSE;
    }

    /* Allocate a new semaphore */
    if (semdFree_h == NULL) return TRUE; /* No free semaphores */

    semd_PTR newSem = semdFree_h;
    semdFree_h = semdFree_h->s_next; /* Remove from free list */

    /* Initialize new semaphore */
    newSem->s_semAdd = semAdd;
    newSem->s_procQ = mkEmptyProcQ();
    insertProcQ(&newSem->s_procQ, p);
    p->p_semAdd = semAdd;

    /* Insert into ASL */
    newSem->s_next = current;
    prev->s_next = newSem;

    return FALSE;
}

/*
 * Function    : removeBlocked
 * Purpose     : Search the ASL for a descriptor of this semaphore. If none is found,
 *               return NULL; otherwise, remove the first (i.e. head) pcb from the pro-
 *               cess queue of the found semaphore descriptor, set that pcb's address
 *               to NULL, and return a pointer to it. If the process queue for this
 *               semaphore becomes empty, remove the semaphore descriptor from the ASL
 *               and return it to the semdFree list.
 * Parameters  : semAdd - pointer to the semaphore
 */
pcb_PTR removeBlocked(int *semAdd) {
    semd_PTR prev = findSemaphore(semAdd);
    semd_PTR current = prev->s_next;

    if (current->s_semAdd != semAdd) return NULL; /* Semaphore not found */

    pcb_PTR removedPcb = removeProcQ(&current->s_procQ);
    if (emptyProcQ(current->s_procQ)) {
        /* Remove the semaphore from ASL if its queue is empty */
        prev->s_next = current->s_next;
        current->s_next = semdFree_h;
        semdFree_h = current; /* Return to free list */
    }

    removedPcb->p_semAdd = NULL;
    return removedPcb;
}

/* 
 * Function    : outBlocked
 * Purpose     : Search the ASL for a descriptor of this semaphore. If none is found,
 *               return NULL; otherwise, remove the first pcb from the process queue
 *               of the found semaphore descriptor, set that pcb's address to NULL, 
 *               and return a pointer to it. If the process queue for this semaphore
 *               becomes empty, remove the semaphore descriptor from the ASL and
 *               return it to the semdFree list.
 * Parameters  : p - pointer to the pcb to be removed
 */
pcb_PTR outBlocked(pcb_PTR p) {
    if (p == NULL || p->p_semAdd == NULL) return NULL; /* Invalid input */

    semd_PTR prev = findSemaphore(p->p_semAdd);
    semd_PTR current = prev->s_next;

    if (current->s_semAdd != p->p_semAdd) return NULL; /* Semaphore not found */

    pcb_PTR removedPcb = outProcQ(&current->s_procQ, p);
    if (emptyProcQ(current->s_procQ)) {
        /* Remove semaphore if its queue is now empty */
        prev->s_next = current->s_next;
        current->s_next = semdFree_h;
        semdFree_h = current;
    }

    return removedPcb;
}

/* 
 * Function    : headBlocked
 * Purpose     : Return a pointer to the pcb that is at the head of the process queue
 *               associated with the semaphore semAdd. Return NULL if semdAdd is 
 *               not found on the ASL or if the process queue associated with semAdd
 *               is empty.
 * Parameters  : semAdd - pointer to the semaphore
 */
pcb_PTR headBlocked(int *semAdd)
{
    semd_PTR prev = findSemd(semAdd);
    semd_PTR curr = prev->s_next;

    if (curr->s_head != semdAdd) return NULL; // Semaphore not found
    return headProcQ(curr->s_procQ);
}

/*
 * Function    : initASL
 * Purpose     : Initialize the semdFree list to contain all the elements of the array
 *               static semd_t semdTable[MAXPROC + 2]. This method will be called only
 *               once during data structure initialization.
 * Parameters  : None
 */
void initASL()
{
    static semd_t semdTable[MAXPROC + 2];
    
    // Initialize the free list
    semdFree_h = NULL;
    for (int i = 0; i < MAXPROC + 2; i++) {
        semdTable[i].s_next = semdFree_h;
        semdFree_h = &semdTable[i];
    }

    // Initialize the dummy nodes (head & tail)
    semd_h = &semdTable[MAXPROC];               // Head dummy node (s_semAdd = 0)
    semd_h->s_semAdd = (int *)0;
    semd_h->s_procQ = NULL;

    semd_h->s_next = &semdTable[MAXPROC + 1];   // Tail dummy node (s_semAdd = MAXINT)
    semd_h->s_next->s_semAdd = (int *)MAXINT;
    semd_h->s_next->s_procQ = NULL;
    semd_h->s_next->s_next = NULL;
}