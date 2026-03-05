/* ==========================================================================
 * AstraOS - Signal Implementation
 * ==========================================================================
 * For now, all signals result in task termination. Future phases could add
 * user-space signal handlers via a trampoline mechanism.
 * ========================================================================== */

#include <kernel/signal.h>
#include <kernel/task.h>
#include <kernel/capability.h>
#include <drivers/serial.h>

int signal_send(uint32_t sender_id, uint32_t target_tid, int sig)
{
    task_t *target = task_find(target_tid);
    if (!target)
        return -1;

    if (target->state == TASK_DEAD)
        return -1;

    security_audit("SIGNAL", "signal delivered",
                   sender_id, (uint32_t)((target_tid << 8) | (sig & 0xFF)));

    switch (sig) {
    case SIGKILL:
    case SIGTERM:
        target->exit_code = (uint32_t)(128 + sig);
        target->state = TASK_DEAD;

        /* Wake any task waiting on the killed task */
        {
            for (uint32_t i = 1; i <= 64; i++) {
                task_t *t = task_find(i);
                if (t && t->state == TASK_BLOCKED && t->wait_for == target_tid) {
                    t->wait_for = 0;
                    t->state = TASK_READY;
                }
            }
        }
        return 0;

    default:
        return -1;  /* Unknown signal */
    }
}
