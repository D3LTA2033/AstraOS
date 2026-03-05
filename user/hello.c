/* ==========================================================================
 * AstraOS - Hello World User Program
 * ==========================================================================
 * Simple test program that prints a greeting and exits.
 * ========================================================================== */

#include "usyscall.h"

void user_main(void)
{
    setup_stdio();

    puts("Hello from AstraOS!\n");
    puts("PID: ");
    putdec(sc0(SYS_GETPID));
    puts("\n");

    sc1(SYS_EXIT, 0);
}
