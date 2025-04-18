#ifndef SYSSUPPORT
#define SYSSUPPORT

/************************* SYSSUPORT.h *****************************
 *
 * This header declares the Phase 3 VM support exception handlers for the 
 * Pandos kernel. It provides the declaration for VMgeneralExceptionHandler()
 * to catch all general exceptions, VMsyscallExceptionHandler() to dispatch 
 * SYS9-13 to corresponding handlers, and VMprogramTrapExceptionHandler()
 * to handle program trap faults by terminating the process
 * 
 * 
 * Written by   : Uyen Nguyen
 * Last update  : 2025/04/17
 *
 *****************************************************************/

#include "../h/const.h"
#include "../h/types.h"
 
/* Function declarations */
extern void VMgeneralExceptionHandler(void);                                          /* General exception handler */
extern void VMsyscallExceptionHandler(state_PTR savedState, support_t *currentSupportStruct);  /* SYSCALL exception handler */
extern void VMprogramTrapExceptionHandler(support_t *currentSupportStruct);                                      /* Program Trap exception handler */

#endif /* SYSSUPPORT */
 