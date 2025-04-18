#include "h/tconst.h"
#include "h/print.h"
#include "h/localLibumps.h"

/* Bad Read: Trigger SYS9 via an invalid REAADTERMINAL buffer */
void main() {
    /* Inform the user */
    SYSCALL(WRITETERMINAL, "Attempting invalid READTERMINAL\n", 32, 0);

    /* SYS13: invalid buffer (0x80000000 ∉ KUSEG) should invoke terminateUserProcess */
    SYSCALL(READTERMINAL, 0x80000000, 0, 0);

    /* Should never reach here */
    SYSCALL(WRITETERMINAL, "ERROR: READTERMINAL returned\n", 29, 0);
    SYSCALL(TERMINATE,           0, 0, 0);
}