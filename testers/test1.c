#include "h/tconst.h"
#include "h/print.h"
#include "h/localLibumps.h"

/* I'm bored so I will just trigger every support-level SYSCALL */
void main() {
    unsigned int time1, time2;
    char buf[32];
    int status;

    /* SYS10: GET_TOD */
    time1 = SYSCALL(GET_TOD, 0, 0, 0);
    print(WRITETERMINAL, "GET_TOD started\n");

    /* SYS11: WRITEPRINTER */
    print(WRITETERMINAL, "WRITEPRINTER OK\n");

    /* SYS12: WRITETERMINAL */
    print(WRITETERMINAL, "WRITETERMINAL OK\n");

    /* SYS13: READTERMINAL */
	print(WRITETERMINAL, "Terminal Read Test starts\n");
	print(WRITETERMINAL, "Enter a string: ");

    status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);
    buf[status] = EOS;

    print(WRITETERMINAL, "\nYou entered: ");
    print(WRITETERMINAL, &buf[0]);
    print(WRITETERMINAL, "\nWRITETERMINAL OK\n");

    /* SYS10 (again): So now that the GET_TOD should work */
    time2 = SYSCALL(GET_TOD, 0, 0, 0);
    if (time2 < time1) {
        print(WRITETERMINAL, "Something went horribly wrong if this printed out...\n");
    } else {
        print(WRITETERMINAL, "GET_TOD OK\n");
    }

    /* Normal exit */
    SYSCALL(TERMINATE, 0, 0, 0);
}