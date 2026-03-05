/* ==========================================================================
 * AstraOS - System Call Interface
 * ==========================================================================
 * System calls are the controlled gateway between user-space (ring 3) and
 * kernel-space (ring 0). User programs invoke `int 0x80` with the syscall
 * number in EAX and arguments in EBX, ECX, EDX, ESI, EDI.
 *
 * This follows the Linux convention for register mapping, which makes
 * it familiar and well-documented. The return value is placed in EAX.
 *
 * Syscall numbers are defined here and must remain stable once published.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_SYSCALL_H
#define _ASTRA_KERNEL_SYSCALL_H

#include <kernel/types.h>
#include <kernel/idt.h>

/* Syscall interrupt vector */
#define SYSCALL_VECTOR  0x80

/* Syscall numbers */
#define SYS_EXIT        0
#define SYS_WRITE       1
#define SYS_GETPID      2
#define SYS_YIELD       3
#define SYS_SLEEP       4

#define SYSCALL_COUNT   5

/* Syscall handler signature: args from registers, return value in EAX */
typedef uint32_t (*syscall_fn_t)(uint32_t ebx, uint32_t ecx, uint32_t edx,
                                  uint32_t esi, uint32_t edi);

/* Initialize the syscall interface (register int 0x80 handler) */
void syscall_init(void);

#endif /* _ASTRA_KERNEL_SYSCALL_H */
