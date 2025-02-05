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
 * Function  : initPcbs
 * Purpose   : Initialize the pcbFree list to contain all the elements of the
 *             static array of MAXPROC pcbs. This method will be called only
 *             once during data structure initialization.
 * Parameters: None
 */
void initPcbs() {
    // Table of proc entries
    static pcb_t    pcbTable[MAXPROC];

    pcbFree_h = mkEmptyProcQ();
    for (int i = 0; i < MAXPROC; i++) {
        insertProcQ(&pcbFree_h, &pcbTable[i]);
    }
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
pcb_PTR allocPcb() {
    if (pcbFree_h == NULL)
        return NULL;

    pcb_PTR temp = pcbFree_h;
    pcbFree_h = pcbFree_h->p_next;
    
    /* set queue values to NULL */
    temp->p_next = NULL;
    temp->p_prev = NULL;

    /* set tree values to NULL */
    temp->p_prnt  = NULL;
    temp->p_child = NULL;
    temp->p_sib   = NULL;

    /* set semaphore value to NULL*/
    temp->p_semAdd = NULL;

    /* set process status information values to 0 */
    temp->p_time = 0;
    
    /* set support layer values to NULL */
    temp->p_supportStruct = NULL;

    return temp;
}

/*
 * Function  :  freePcb
 * Purpose   :  Insert the element pointed to by p onto the pcbFree list.
 * Parameters: p - pointer to the pcb to be freed
 */
*/
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
    head = (*tp)->p_next;
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

/******************************* PROCESS TREE MANAGEMENT *****************************/

/* Return TRUE if the pcb pointed to by p has no children. Return
FALSE otherwise. */
int emptyChild(pcb_PTR p) 
{
    return (p->p_child == NULL); 
}

/* Make the pcb pointed to by p a child of the pcb pointed to by prnt. */
void insertChild(pcb_PTR prnt, pcb_PTR p) 
{
    if (p == NULL || prnt == NULL) return;

    p->p_prnt = prnt;  // Set parent

    // Insert p at the beginning of the parent's child list
    p->p_sib = prnt->p_child;
    prnt->p_child = p;
}

/* code for doubly linked list*/
// void insertChild(pcb_PTR prnt, pcb_PTR p) 
//{
//     if (p == NULL || prnt == NULL) return;

//     p->p_prnt = prnt;  // Set parent

//     // Insert p at the beginning of the parent's child list
//     p->p_sib = prnt->p_child;
//     p->p_prev_sib = NULL;  // No prev sibling since it's first

//     if (prnt->p_child != NULL) {
//         prnt->p_child->p_prev_sib = p;  // Update prev first child's back
//     }

//     prnt->p_child = p;  // Update parent's 1st child ptr
// }

/* Remove and return the first child of p */
pcb_PTR removeChild(pcb_PTR p) 
{
    // return NULL if initially there were no children of p
    if (p == NULL || p->p_child == NULL) return NULL; 

    pcb_PTR firstChild = p->p_child;
    p->p_child = firstChild->p_sib;  // Update parent's child pointer

    firstChild->p_prnt = NULL; // NULL parent for removed first child
    firstChild->p_sib = NULL; // NULL sibling for removed first child

    return firstChild;
}

// pcb_PTR removeChild(pcb_PTR p) 
//{
//     if (p == NULL || p->p_child == NULL) return NULL; // no p or parent

//     pcb_PTR firstChild = p->p_child;
//     p->p_child = firstChild->p_sib;  // Update parent's child pointer

//     if (p->p_child != NULL) {
//         p->p_child->p_prev_sib = NULL;  // Update new 1st child's back link
//     }

//     firstChild->p_prnt = NULL;
//     firstChild->p_sib = NULL;
//     firstChild->p_prev_sib = NULL;

//     return firstChild;
// }

/* Make the pcb pointed to by p no longer the child of its parent */
pcb_PTR outChild(pcb_PTR p) 
{
    if (p == NULL || p->p_prnt == NULL) return NULL; // return NULL if the pcb pointed to by p has no parent

    pcb_PTR parent = p->p_prnt;
    pcb_PTR curr = parent->p_child;
    pcb_PTR prev = NULL;

    // Traverse to find p in parent's child list
    while (curr != NULL) {
        if (curr == p) 
        {  
            // If p is found in the child list
            if (prev == NULL) 
            { 
                // p is the first child
                parent->p_child = p->p_sib;
            } 
            else 
            {
                // p is in the middle or end
                prev->p_sib = p->p_sib;
            }

            p->p_prnt = NULL;
            p->p_sib = NULL;
            return p;
        }
        prev = curr;
        curr = curr->p_sib;
    }
    
    return NULL;  // p isn't found in the child list
}

// pcb_PTR outChild(pcb_PTR p) 
//{
//     if (p == NULL || p->p_prnt == NULL) return NULL;

//     pcb_PTR parent = p->p_prnt;

//     if (p == parent->p_child) {
//         // p is 1st child, update parent's child pointer
//         parent->p_child = p->p_sib;
//         if (p->p_sib) 
//              p->p_sib->p_prev_sib = NULL;  // Update backward link idk not sure
//     } else {
//         // p is in mid or end, update sibling pointers
//         if (p->p_prev_sib)
//              p->p_prev_sib->p_sib = p->p_sib;
//         if (p->p_sib) 
//              p->p_sib->p_prev_sib = p->p_prev_sib; 
//     }

//     // Disconnect p from its parent and siblings
//     p->p_prnt = NULL;
//     p->p_sib = NULL;
//     p->p_prev_sib = NULL;
    
//     return p;
// }




