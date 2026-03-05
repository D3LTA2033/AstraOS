/* ==========================================================================
 * AstraOS - User-Space Syscall Stubs
 * ==========================================================================
 * Inline assembly wrappers for user programs to invoke system calls via
 * int 0x80. These are designed to be included in user-space code that
 * runs in ring 3.
 *
 * Register convention (matching Linux i386):
 *   EAX = syscall number
 *   EBX = arg1, ECX = arg2, EDX = arg3, ESI = arg4, EDI = arg5
 *   EAX = return value
 * ========================================================================== */

#ifndef _ASTRA_USER_SYSCALL_H
#define _ASTRA_USER_SYSCALL_H

#include <kernel/types.h>

/* Syscall numbers — must match kernel/core/syscall.c */
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  2
#define SYS_YIELD   3
#define SYS_SLEEP   4

static inline uint32_t syscall0(uint32_t num)
{
    uint32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num));
    return ret;
}

static inline uint32_t syscall1(uint32_t num, uint32_t arg1)
{
    uint32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

static inline uint32_t syscall3(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3)
{
    uint32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret)
                      : "a"(num), "b"(a1), "c"(a2), "d"(a3));
    return ret;
}

static inline void user_exit(uint32_t code)
{
    syscall1(SYS_EXIT, code);
}

static inline uint32_t user_write(const char *buf, uint32_t len)
{
    return syscall3(SYS_WRITE, 1, (uint32_t)buf, len);
}

static inline uint32_t user_getpid(void)
{
    return syscall0(SYS_GETPID);
}

static inline void user_yield(void)
{
    syscall0(SYS_YIELD);
}

static inline void user_sleep(uint32_t ticks)
{
    syscall1(SYS_SLEEP, ticks);
}

#endif /* _ASTRA_USER_SYSCALL_H */
