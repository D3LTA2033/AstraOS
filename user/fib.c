/* ==========================================================================
 * AstraOS - Fibonacci User Program
 * ==========================================================================
 * Computes and prints the first 20 Fibonacci numbers, then exits.
 * ========================================================================== */

#include "usyscall.h"

void user_main(void)
{
    setup_stdio();

    puts("Fibonacci sequence:\n");

    unsigned int a = 0, b = 1;
    for (int i = 0; i < 20; i++) {
        puts("  fib(");
        putdec((unsigned int)i);
        puts(") = ");
        putdec(a);
        puts("\n");
        unsigned int next = a + b;
        a = b;
        b = next;
    }

    sc1(SYS_EXIT, 0);
}
