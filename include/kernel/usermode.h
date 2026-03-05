/* ==========================================================================
 * AstraOS - User Mode (Ring 3) Support
 * ==========================================================================
 * Provides the mechanism to transition from kernel mode (ring 0) to user
 * mode (ring 3). This is done by crafting a fake interrupt return frame
 * on the stack and executing IRET, which loads user-space CS/SS/ESP/EIP.
 *
 * The TSS must have the kernel stack pointer set so that when the user
 * process triggers an interrupt (or syscall), the CPU automatically
 * switches to the kernel stack.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_USERMODE_H
#define _ASTRA_KERNEL_USERMODE_H

#include <kernel/types.h>

/* Initialize user-mode support (allocate user page tables, etc.) */
void usermode_init(void);

/* Create a user-mode task that runs the given code at the given address.
 * code_src:  kernel-space pointer to the user program binary
 * code_size: size of the program in bytes
 * Returns task ID, or 0 on failure. */
uint32_t usermode_task_create(const void *code_src, size_t code_size);

/* Assembly routine: jump to ring 3 with given EIP and ESP */
extern void enter_usermode(uint32_t eip, uint32_t esp);

#endif /* _ASTRA_KERNEL_USERMODE_H */
