/* ==========================================================================
 * AstraOS - User-Space Syscall Interface
 * ==========================================================================
 * Shared syscall stubs and basic utilities for all user programs.
 * No kernel headers — this is pure user-space.
 * ========================================================================== */

#ifndef _ASTRA_USYSCALL_H
#define _ASTRA_USYSCALL_H

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  2
#define SYS_YIELD   3
#define SYS_SLEEP   4
#define SYS_OPEN    5
#define SYS_READ    6
#define SYS_CLOSE   7
#define SYS_STAT    8
#define SYS_READDIR 9
#define SYS_SPAWN   10
#define SYS_WAITPID 11
#define SYS_PIPE    12
#define SYS_KILL    13
#define SYS_GETPPID 14
#define SYS_REBOOT  15

static inline unsigned int sc0(unsigned int n)
{
    unsigned int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n));
    return r;
}
static inline unsigned int sc1(unsigned int n, unsigned int a)
{
    unsigned int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n), "b"(a));
    return r;
}
static inline unsigned int sc2(unsigned int n, unsigned int a, unsigned int b)
{
    unsigned int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b));
    return r;
}
static inline unsigned int sc3(unsigned int n, unsigned int a,
                                unsigned int b, unsigned int c)
{
    unsigned int r;
    __asm__ volatile ("int $0x80" : "=a"(r)
                      : "a"(n), "b"(a), "c"(b), "d"(c));
    return r;
}
static inline unsigned int sc4(unsigned int n, unsigned int a,
                                unsigned int b, unsigned int c, unsigned int d)
{
    unsigned int r;
    __asm__ volatile ("int $0x80" : "=a"(r)
                      : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d));
    return r;
}

static inline void puts(const char *s)
{
    unsigned int l = 0;
    while (s[l]) l++;
    sc3(SYS_WRITE, 1, (unsigned int)s, l);
}

static inline void putdec(unsigned int val)
{
    static char _dbuf[12];
    if (val == 0) { sc3(SYS_WRITE, 1, (unsigned int)"0", 1); return; }
    int i = 0;
    char tmp[12];
    while (val > 0) { tmp[i++] = '0' + (char)(val % 10); val /= 10; }
    for (int j = 0; j < i; j++) _dbuf[j] = tmp[i - 1 - j];
    sc3(SYS_WRITE, 1, (unsigned int)_dbuf, (unsigned int)i);
}

static inline void setup_stdio(void)
{
    sc2(SYS_OPEN, (unsigned int)"/dev/console", 0);  /* fd 0 */
    sc2(SYS_OPEN, (unsigned int)"/dev/console", 0);  /* fd 1 */
    sc2(SYS_OPEN, (unsigned int)"/dev/console", 0);  /* fd 2 */
}

#endif /* _ASTRA_USYSCALL_H */
