#ifndef DEVICESUPPORTDMA
#define DEVICESUPPORTDMA

/************************* DEVICESUPPORTDMA.h *****************************
 *
 * Written by   : Uyen Nguyen
 * Last update  : 2025/05/09
 *
 *****************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/initial.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/* Function declarations */
extern void flashOperation(support_t *currentSupportStruct, int *logicalAddress, int flashNumber, int blockNumber, int operation);

#endif /* DEVICESUPPORTDMA */