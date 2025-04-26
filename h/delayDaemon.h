#ifndef DELAYDAEMON_H
#define DELAYDAEMON_H

#include "../h/const.h"
#include "../h/types.h"

void initADL(void);

/* The Daemon process itself (infinite loop) */
void delayDaemon(void);

#endif /* DELAYDAEMON_H */