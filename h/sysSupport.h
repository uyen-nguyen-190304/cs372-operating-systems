#ifndef SYSSUPPORT
#define SYSSUPPORT

/************************* VMSUPPORT.h *****************************
 *
 * 
 * 
 * Written by   : Uyen Nguyen
 * Last update  : 2025/04/15
 *
 *****************************************************************/

#include "../h/const.h"
#include "../h/types.h"
 
extern void VMgeneralExceptionHandler(void);                                          /* General exception handler */
extern void VMsyscallExceptionHandler(state_PTR savedState, support_t *currentSupportStruct);  /* SYSCALL exception handler */
extern void VMprogramTrapExceptionHandler(void);                                      /* Program Trap exception handler */

#endif /* SYSSUPPORT */
 