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

/****** ************************* PCB ALLOCATION *****************************/

/*
 * Function  :  freePcb
 * Purpose   :  Insert the element pointed to by p onto the pcbFree list.
 * Parameters:  p - pointer to the pcb to be freed
 */
void freePcb(pcb_PTR p) 
{
    p->p_next = pcbFree_h;
    pcbFree_h = p;
}

/*
 * Function :  allocPcb
 * Purpose  :  Return NULL if the pcbFree list is empty. Otherwise, remove
 *             an element from the pcbFree list, provide initial values for ALL 
 *             of the pcbs fields (i.e. NULL and/or 0) and then return a pointer
 *             to the removed element. pcbs get reused, so it is important that
 *             no previous value persists in a pcb when it is reallocated.
 * Parameters: None
 */
pcb_PTR allocPcb() 
{
    if (pcbFree_h == NULL)
        return NULL;

    pcb_PTR temp = pcbFree_h;
    pcbFree_h = pcbFree_h->p_next;
    
    // Set queue values to NULL
    temp->p_next = NULL;
    temp->p_prev = NULL;

    // Set tree values to NULL
    temp->p_prnt    = NULL;
    temp->p_child   = NULL;
    temp->p_sibNext = NULL;
    temp->p_sibPrev = NULL;

    // Set semaphore value to NULL 
    temp->p_semAdd = NULL;

    // Set process status information values to 0 
    temp->p_time = 0;
    
    // Set support layer values to NULL 
    temp->p_supportStruct = NULL;

    return temp;
}

/*
 * Function  : initPcbs
 * Purpose   : Initialize the pcbFree list to contain all the elements of the
 *             static array of MAXPROC pcbs. This method will be called only
 *             once during data structure initialization.
 * Parameters: None
 */
void initPcbs() 
{
    static pcb_t pcbTable[MAXPROC];

    pcbFree_h = NULL;
    for (int i = 0; i < MAXPROC; i++) {
        pcbTable[i].p_next = pcbFree_h;
        pcbFree_h = &pcbTable[i];
    }
}

/******************************* PROCESS QUEUE MANAGEMENT *****************************/

/*
 * Function  : mkEmptyProcQ
 * Purpose   : This method is used to initialize a variable to be tail pointer to a
 *             process queue. Return a pointer to the tail of an empty process queue; i.e. NULL.
 * Parameters: None
 */
pcb_PTR mkEmptyProcQ() 
{
    return NULL;
}

/*
 * Function  : emptyProcQ
 * Purpose   : Return TRUE is the queue whose tail is pointed to by tp is empty.
 *             Return FALSE otherwise.   
 * Parameters: tp - pointer to the tail of the process queue
 */
int emptyProcQ(pcb_PTR tp) 
{
    return (tp == NULL);
}

/*
 * Function  : insertProcQ
 * Purpose   : Insert the pcb pointed to by p into the process queue whose tail-
 *             pointer is pointed to by tp. Note the double indirection through tp
 *             to allow for the possible updating of the tail pointer as well.
 * Parameters: tp - pointer to the tail of the process queue
 *             p  - pointer to the pcb to be inserted
 */
void insertProcQ(pcb_PTR *tp, pcb_PTR p) 
{
    if (*tp == NULL) {
        // If queue is empty, initialize first element
        p->p_prev = p;
        p->p_next = p;
    } else {
        // If queue is not empty, insert at tail
        p->p_next = (*tp)->p_next;      // New pcb points to head
        (*tp)->p_next->p_prev = p;      // Update old head's previous pointer
        (*tp)->p_next = p;              // Update old tail's next pointer
        p->p_prev = *tp;                // New pcb points to tail
    }
    // Update tail pointer to the new node
    *tp = p;                            
}

/*
 * Function  : removeProcQ
 * Purpose   : Remove the first (i.e. head) element from the process queue whose
 *             tail-pointer is pointed to by tp. Return NULL if the process queue
 *             was initially empty; otherwise return the pointer to the removed ele-
 *             ment. Update the process queue's tail pointer if necessary.
 * Parameters: tp - pointer to the tail of the process queue
 */
pcb_PTR removeProcQ(pcb_PTR *tp) 
{  
    // Check if the queue is empty
    if (emptyProcQ(*tp)) return NULL;

    // Get the first pcb in the queue
    pcb_PTR head = (*tp)->p_next;

    if (head == *tp) {
        // If queue has only one element, remove it and set tail to NULL
        *tp = NULL;
    } else {
        // If queue has multiple elements, update tail pointer
        (*tp)->p_next = head->p_next;   // Tail now points to new head
        head->p_next->p_prev = *tp;     // New head points back to tail
    }
    // Clear links in the removed pcb
    head->p_prev = NULL;
    head->p_next = NULL;

    return head;
}

/* 
 * Function  : outProcQ
 * Purpose   : Remove the pcb pointed to by p from the process queue whose tail-
 *             pointer is pointed to by tp. Update the process queue's tail pointer if
 *             necessary. If the desired entry is not in the indicated queue (an error 
 *             condition), return NULL; otherwise, return p. Note that p can point
 *             to any element in the process queue
 * Parameters: tp - pointer to the tail of the process queue
 *             p  - pointer to the pcb to be removed
 */
pcb_PTR outProcQ(pcb_PTR *tp, pcb_PTR p) 
{
    // Check for NULL pointers
    if (*tp == NULL || p == NULL) return NULL;               

    // Check if p exists in the queue
    pcb_PTR curr = (*tp);
    do {
        if (curr == p) 
        break;
        curr = curr->p_next;
    } while (curr != (*tp)->p_next);

    if (curr != p) return NULL;  // p was not found in the queue

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
    // Clear links in the removed pcb
    p->p_next = NULL;
    p->p_prev = NULL;

    return p;
}

/*
 * Function  : headProcQ
 * Purpose   : Return a pointer to the first pcb from the process queue whose tail
 *             is pointed to by tp. Do not remove this pcb from the process queue.
 *             Return NULL if the process queue is empty.ACK
 * Parameters: tp - pointer to the tail of the process queue
 */
pcb_PTR headProcQ(pcb_PTR tp) 
{
    return (tp == NULL) ? NULL : tp->p_next;
}

/******************************* PROCESS TREE MANAGEMENT *****************************/

/*
 * Function  : emptyChild
 * Purpose   : Return TRUE if the pcb pointed to by p has no children. 
 *             Return FALSE otherwise.
 * Parameters: p - pointer to the pcb
 */
int emptyChild(pcb_PTR p) 
{
    return (p->p_child == NULL); 
}

/*
 * Function  : insertChild
 * Purpose   : Make the pcb pointed to by p a child of the pcb pointed to by prnt.
 * Parameters: prnt - pointer to the parent pcb
 *             p    - pointer to the child pcb 
 */
void insertChild(pcb_PTR prnt, pcb_PTR p) 
{
    if (p == NULL || prnt == NULL) return;

    // Set new parent
    p->p_prnt = prnt;

    if (emptyChild(prnt)) {
        // If parent has no children
        prnt->p_child = p;
        p->p_sibNext = NULL;
        p->p_sibPrev = NULL;
    } else {
        // If parent has at least one child already
        p->p_sibNext = prnt->p_child;
        p->p_sibPrev = NULL;
        prnt->p_child->p_sibPrev = p;
        prnt->p_child = p;
    }
}

/*
 * Function  : removeChild
 * Purpose   : Make the first child of the pcb pointed to by p no longer a child of p.
 *             Return NULL if initially there were no children of p. 
 *             Otherwise, return a pointer to this removed first child pcb.
 * Parameters: p - pointer to the parent pcb
 */
pcb_PTR removeChild(pcb_PTR p) 
{
    // Return NULL if initially there were no children of p or p is NULL
    if (p == NULL || p->p_child == NULL) return NULL; 

    // Store the first child
    pcb_PTR firstChild = p->p_child;

    if (firstChild->p_sibNext == NULL) {
        // If p has only one child
        p->p_child = NULL;
    } else {
        // If p has multiple children
        p->p_child = firstChild->p_sibNext;
        p->p_child->p_sibPrev = NULL;
    }
    // Clear links in the removed pcb
    firstChild->p_prnt = NULL;
    firstChild->p_sibNext = NULL; 
    firstChild->p_sibPrev = NULL;

    return firstChild;
}

/*
 * Function     : outChild
 * Purpose      : Make the pcb pointed to by p no longer the child of its parent.
 *                If the pcb pointed to by p has no parent, return NULL. Otherwise, return p.
 *                Note that the element pointed to by p need not be the first child of its parent.
 * Parameters   : p - pointer to the pcb
 */
pcb_PTR outChild(pcb_PTR p) 
{
    // Return NULL if the pcb pointed to by p has no parent
    if (p == NULL || p->p_prnt == NULL) 
        return NULL; 

    pcb_PTR parent = p->p_prnt;

    if (parent->p_child == p) {
        // If p is the first child, use removeChild
        return removeChild(parent);
    } else {
        // If p is not the first child
        if (p->p_sibNext == NULL) {
            // If p is the last child
            p->p_sibPrev->p_sibNext = NULL;
        } else {
            // If p is in the middle of its parent's children
            p->p_sibPrev->p_sibNext = p->p_sibNext;
            p->p_sibNext->p_sibPrev = p->p_sibPrev;
        }
    }
    // Clear links in the removed pcb
    p->p_prnt = NULL;
    p->p_sibNext = NULL;
    p->p_sibPrev = NULL;

    return p;
}
