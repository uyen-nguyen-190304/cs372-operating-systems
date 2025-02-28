#ifndef INITIAL
#define INITIAL


#include "../h/const.h"
#include "../h/types.h"

extern int processCount;
extern int softBlockCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currentProcess;
extern int deviceSemaphores[MAXDEVICES];

#endif /* INITIAL */


