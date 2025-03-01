#ifndef EXCEPTIONS
#define EXCEPTIONS

#include "const.h"      
#include "types.h"     
#include "pcb.h"      
#include "asl.h"        
#include "scheduler.h"  
#include "interrupts.h"
#include "initial.h"   

extern void programTrapExceptionHandler();
extern void TLBExceptionHandler();
extern void syscallExceptionHandler();
extern void uTLB_RefillHandler();

#endif /* EXCEPTIONS */