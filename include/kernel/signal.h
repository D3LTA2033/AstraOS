/* ==========================================================================
 * AstraOS - Signal Infrastructure
 * ==========================================================================
 * Simple signal delivery mechanism. Signals cause immediate action on the
 * target task (no user-space signal handlers in this phase).
 *
 * Supported signals:
 *   SIGKILL (9)  — Unconditionally terminate the task
 *   SIGTERM (15) — Request termination (same as SIGKILL for now)
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_SIGNAL_H
#define _ASTRA_KERNEL_SIGNAL_H

#include <kernel/types.h>

#define SIGKILL  9
#define SIGTERM  15

/* Send a signal to a task. Returns 0 on success, -1 on failure. */
int signal_send(uint32_t sender_id, uint32_t target_tid, int sig);

#endif /* _ASTRA_KERNEL_SIGNAL_H */
