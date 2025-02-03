/******************************* PCB.c ***************************************
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

/******************************* GLOBAL VARIABLES *****************************/

/* Head pointer for the PCB free list */
HIDDEN pcb_PTR pcbFree_h;

/* Static array of MAXPROC PCBs */
HIDDEN pcb_t pcbPool[MAXPROC];

/****** ************************* PCB ALLOCATION *****************************/

/* Initialize the PCB free list */
void initPcbs() {
    pcbFree_h = NULL;
    for (int i = 0; i < MAXPROC; i++) {
        freePcb(&pcbPool[i]);
    }
}

/* Allocate a PCB from the free list */
pcb_PTR allocPcb() {
    register pcb_PTR p;

    if (pcbFree_h == NULL)
        return NULL;

    p = pcbFree_h;
    pcbFree_h = pcbFree_h->p_next;
    
    /* Reset PCB fields */
    p->p_next = NULL;
    p->p_prev = NULL;
    p->p_prnt = NULL;
    p->p_child = NULL;
    p->p_sib = NULL;
    p->p_semAdd = NULL;
    p->p_time = 0;
    p->p_supportStruct = NULL;
    
    return p;
}

/* Return a PCB to the free list */
void freePcb(pcb_PTR p) {
    p->p_next = pcbFree_h;
    pcbFree_h = p;
}

/******************************* PROCESS QUEUE MANAGEMENT *****************************/

/* Return an empty process queue */
pcb_PTR mkEmptyProcQ() {
    return NULL;
}

/* Check if a process queue is empty */
int emptyProcQ(pcb_PTR tp) {
    return (tp == NULL);
}

/* Insert a PCB into a process queue */
void insertProcQ(pcb_PTR *tp, pcb_PTR p) {
    if (*tp == NULL) {
        // If queue is empty, initialize first element
        p->p_prev = p;
        p->p_next = p;
    } else {
        // If queue is not empty, insert at tail
        p->p_next = (*tp)->p_next;      // New pcb points to head
        p->p_prev = *tp;                // New pcb points to tail
        (*tp)->p_next = p;              // Tail points to new pcb
        p->p_next->p_prev = p;          // Head points to new pcb
    }
    *tp = p;                            // Update tail pointer
}

/* Remove and return the head of the process queue */
pcb_PTR removeProcQ(pcb_PTR *tp) {
    register pcb_PTR head;

    if (emptyProcQ(*tp))
    // If queue is empty, return NULL
        return NULL;

    // First pcb in queue
    head = (*tp->p_next);
    if (head == *tp) {
        // If queue has only one element
        *tp = NULL;
    } else {
        // If queue has more than one element
        (*tp)->p_next = head->p_next;
        head->p_next->p_prev = *tp;
    }
    head->p_prev = NULL;
    head->p_next = NULL;
    return head;
}

/* Remove a specific PCB from a process queue */
pcb_PTR outProcQ(pcb_PTR *tp, pcb_PTR p) {
    if (*tp == NULL || p == NULL)
        return NULL;                    // Queue is empty or PCB is NULL

    if (p->p_next == p) {
        // Only one element in queue
        *tp = NULL;
    } else {
        // Remove p from queue
        p->p_prev->p_next = p->p_next;
        p->p_next->p_prev = p->p_prev;

        // If p was the tail, update tp
        if (*tp == p) {
            *tp = p->p_prev;
        }
    }
    p->p_next = NULL;
    p->p_prev = NULL;
    return p;
}

/* Return the pointer to the first pcb */
pcb_PTR headProcQ(pcb_PTR tp) {
    return tp->p_next;
}
