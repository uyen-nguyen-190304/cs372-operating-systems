#ifndef SCHEDULER
#define SCHEDULER

extern cpu_t startTOD;
extern cpu_t currentTOD;

extern void copyState();
extern void scheduler();

#endif /* SCHEDULER */