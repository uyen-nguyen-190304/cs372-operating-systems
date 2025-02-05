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

/* 
 * Function    : findSemaphore
 * Purpose     : Locate the appropriate position of a given semaphore address
 *               in the ASL. Return a pointer to the previous node in the ASL 
 *               where the semaphore should be located.
 * Parameters  : semAdd - pointer to the semaphore
 */
HIDDEN semd_PTR findSemaphore(int *semAdd) {
    semd_t *current;
    semd_t *previous;
    current = semd_h; 
    previous = NULL;  

    /* Traverse the ASL while semAdd is greater than the current semaphore's address */ 
    while (current->s_semAdd < semAdd) { 
        if (current->s_semAdd == (int*) MAXINT) {
            /* if current is the tail dummy node */
            return previous;
        }
        previous = current; 
        current = current->s_next;
    }
    return previous; /* return a pointer to the semd that precedes current */
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
int insertBlocked(int *semAdd, pcb_PTR p) {
    semd_PTR prev = findSemaphore(semAdd);
    semd_PTR curr = prev->s_next;
    
    if (curr->s_semAdd != semAdd) {
        /* If the semaphore is not currently active */
        semd_PTR newSemd = semdFree_h;

        if (newSemd == NULL) 
            return TRUE;  /* semdFree list is empty */

    /* Remove newSemd from the free list */
    semdFree_h = newSemd->s_next;  

    /* Initialize new semaphore descriptor*/
    newSemd->s_semAdd = semAdd;
    newSemd->s_procQ = mkEmptyProcQ();

    /* Insert p into the process queue of newSemd */
    insertProcQ(&(newSemd->s_procQ), p);
    p->p_semAdd = semAdd;

    /* Insert newSemd into the ASL */
    newSemd->s_next = curr;
    prev->s_next = newSemd;
    } else {
        /* If the semaphore is already in ASL */
        insertProcQ(&(curr->s_procQ), p);
        p->p_semAdd = semAdd;
        return FALSE;
    }
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
    if (prev == NULL || prev->s_next == NULL || prev->s_next->s_semAdd != semAdd) 
        return NULL;    /* Semaphore not found */ 

    semd_PTR current = prev->s_next;
    pcb_PTR removedPcb = removeProcQ(&current->s_procQ);

    if (removedPcb != NULL) 
        removedPcb->p_semAdd = NULL;

    if (emptyProcQ(current->s_procQ)) {
        /* Remove the semaphore from ASL */ 
        prev->s_next = current->s_next;
        current->s_next = semdFree_h;
        semdFree_h = current;
    }

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
    if (p == NULL || p->p_semAdd == NULL) return NULL;  /* Invalid input */ 

    semd_PTR prev = findSemaphore(p->p_semAdd);
    if (prev == NULL || prev->s_next == NULL || prev->s_next->s_semAdd != p->p_semAdd) 
        return NULL;  /* Semaphore not found */ 
    
    semd_PTR current = prev->s_next;
    pcb_PTR removedPcb = outProcQ(&current->s_procQ, p);

    if (removedPcb == NULL) return NULL;  /* PCB not found in the queue */ 

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
pcb_PTR headBlocked(int *semAdd) {
    semd_PTR prev = findSemaphore(semAdd);
    if (prev == NULL || prev->s_next == NULL || prev->s_next->s_semAdd != semAdd) 
        return NULL;    /* Semaphore not found */

    return headProcQ(prev->s_next->s_procQ);
}

/*
 * Function    : initASL
 * Purpose     : Initialize the semdFree list to contain all the elements of the array
 *               static semd_t semdTable[MAXPROC + 2]. This method will be called only
 *               once during data structure initialization.
 * Parameters  : None
 */
void initASL() {
    int i;
    static semd_t semdTable[MAXPROC + 2];

    semdFree_h = NULL;
    for (i = 0; i < MAXPROC + 2; i++) {
        semdTable[i].s_next = semdFree_h;
        semdTable[i].s_procQ = mkEmptyProcQ(); 
        semdFree_h = &semdTable[i];
    }

    /* Initialize the dummy head and tail nodes */ 
    semd_h = &semdTable[MAXPROC];               
    semd_h->s_semAdd = (int *)0;
    semd_h->s_procQ = NULL;
    semd_h->s_next = &semdTable[MAXPROC + 1]; 

    semd_h->s_next->s_semAdd = (int *)MAXINT;
    semd_h->s_next->s_procQ = NULL;
    semd_h->s_next->s_next = NULL;
}
