#include "h/tconst.h"
#include "h/print.h"
#include "h/localLibumps.h"

/* Bad Read: Trigger SYS9 via an invalid REAADTERMINAL buffer */
void main() {
    /* Inform the user */
    print(WRITETERMINAL, "Attempting invalid READTERMINAL\n");
    print(WRITETERMINAL, "Expecting termination of the process\n");

    /* SYS13: invalid buffer (0x20000000 < KUSEG) should invoke terminateUserProcess */
    SYSCALL(READTERMINAL, 0x20000000, 0, 0);

    /* Should never reach here */
    print(WRITETERMINAL, "ERROR: READTERMINAL returned\n");
    SYSCALL(TERMINATE, 0, 0, 0);
}