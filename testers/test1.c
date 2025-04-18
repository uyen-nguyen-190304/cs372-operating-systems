#include "h/tconst.h"
#include "h/print.h"
#include "h/localLibumps.h"

char buf[32];

/* I'm bored so I will just trigger every support-level SYSCALL */
void main() {
    unsigned int now;
    int r;

    /* SYS10: GET_TOD */
    now = SYSCALL(GET_TOD, 0, 0, 0);
    SYSCALL(WRITETERMINAL, "GET_TOD OK\n", 11, 0);

    /* SYS11: WRITEPRINTER */
    SYSCALL(WRITEPRINTER, "WRITEPRINTER OK\n", 16, 0);

    /* SYS12: WRITETERMINAL */
    SYSCALL(WRITETERMINAL, "WRITETERMINAL OK\n", 17, 0);

    /* SYS13: READTERMINAL */
    SYSCALL(WRITETERMINAL, "Enter text: ", 12, 0);
    r = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);
    buf[r] = '\0';
    SYSCALL(WRITETERMINAL, "You entered: ", 13, 0);
    SYSCALL(WRITETERMINAL, buf,           r,  0);
    SYSCALL(WRITETERMINAL, "\n",           1,  0);

    /* Normal exit */
    SYSCALL(TERMINATE, 0, 0, 0);
}