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

/******************************* PCB ALLOCATION *****************************/

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



