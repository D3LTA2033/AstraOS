/* ==========================================================================
 * AstraOS - User-Space Test Program
 * ==========================================================================
 * This program runs in ring 3 and communicates with the kernel exclusively
 * through system calls (int 0x80). It cannot access kernel memory.
 * ========================================================================== */

/* Inline syscall stubs — no libc, no kernel headers at runtime */

static inline unsigned int syscall0(unsigned int num)
{
    unsigned int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num));
    return ret;
}

static inline unsigned int syscall1(unsigned int num, unsigned int arg1)
{
    unsigned int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

static inline unsigned int syscall3(unsigned int num, unsigned int a1,
                                     unsigned int a2, unsigned int a3)
{
    unsigned int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret)
                      : "a"(num), "b"(a1), "c"(a2), "d"(a3));
    return ret;
}

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  2
#define SYS_YIELD   3
#define SYS_SLEEP   4

static void write_str(const char *s)
{
    unsigned int len = 0;
    while (s[len]) len++;
    syscall3(SYS_WRITE, 1, (unsigned int)s, len);
}

static char num_buf[12];

static void write_dec(unsigned int val)
{
    int i = 0;
    if (val == 0) {
        num_buf[0] = '0';
        syscall3(SYS_WRITE, 1, (unsigned int)num_buf, 1);
        return;
    }
    char tmp[12];
    while (val > 0) {
        tmp[i++] = '0' + (char)(val % 10);
        val /= 10;
    }
    for (int j = 0; j < i; j++)
        num_buf[j] = tmp[i - 1 - j];
    syscall3(SYS_WRITE, 1, (unsigned int)num_buf, (unsigned int)i);
}

void user_main(void)
{
    unsigned int pid = syscall0(SYS_GETPID);

    write_str("[USER] Hello from ring 3! PID=");
    write_dec(pid);
    write_str("\n");

    for (int i = 0; i < 3; i++) {
        write_str("[USER] Iteration ");
        write_dec((unsigned int)i);
        write_str(" (syscall working)\n");
        syscall1(SYS_SLEEP, 50);    /* Sleep ~500ms */
    }

    write_str("[USER] Exiting via SYS_EXIT\n");
    syscall1(SYS_EXIT, 0);

    /* Should never reach here */
    while (1) {}
}
