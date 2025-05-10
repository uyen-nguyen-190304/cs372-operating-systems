#include "h/tconst.h"
#include "h/print.h"
#include "h/localLibumps.h"

/* Additional test file for phase 4 - calling SYS14-17 */
void main() {
    int i;
    int status;
    int *buffer;

    buffer = (int *)(SEG2 + (30 * PAGESIZE));

    print(WRITETERMINAL, "Additional Testing for Phase 4\n");

    /* Wrote two words to flash 1 at sector 10 */
    buffer[0] = 0xDEADBEEF;
    buffer[1] = 0x12345678;
    status = SYSCALL(FLASH_PUT, (int)buffer, 1, 10);
    if (status != READY) {
        print(WRITETERMINAL, "flashPut error: cannot put on flash 1 sector 10\n");
    } else {
        print(WRITETERMINAL, "flashPut ok: data put on flash 1 sector 10\n");
    }

    /* Read them back from flash 1 */
    buffer[0] = buffer[1] = 0;
    status = SYSCALL(FLASH_GET, (int)buffer, 1, 10);
    if (status != READY) {
        print(WRITETERMINAL, "flashGet error: cannnot read back\n");
    } else if (buffer[0] != 0xDEADBEEF || buffer[1] != 0x12345678) {
        print(WRITETERMINAL, "flashGet error: data mismatched\n");
    } else {
        print(WRITETERMINAL, "flashGet ok: data verified\n");
    }

    /* Write those words to disk 1 at sector 20 */
    status = SYSCALL(DISK_PUT, (int)buffer, 1, 20);
    if (status != READY) {
        print(WRITETERMINAL, "diskPut error: cannot put on disk 1 sector 20\n");
    } else {
        print(WRITETERMINAL, "diskPut ok: data put on disk 1 sector 20\n");
    }

    /* Read them back from disk 1 */
    buffer[0] = buffer[1] = 0;
    status = SYSCALL(DISK_GET, (int)buffer, 1, 20);
    if (status != READY) {
        print(WRITETERMINAL, "diskGet error: cannnot read back\n");
    } else if (buffer[0] != 0xDEADBEEF || buffer[1] != 0x12345678) {
        print(WRITETERMINAL, "diskGet error: data mismatched\n");
    } else {
        print(WRITETERMINAL, "diskGet ok: data verified\n");
    }

    /* Graceful terminate */
    SYSCALL(TERMINATE, 0, 0, 0);

}