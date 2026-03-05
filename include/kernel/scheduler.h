/* ==========================================================================
 * AstraOS - Scheduler
 * ==========================================================================
 * Round-robin preemptive scheduler.
 *
 * Design:
 *   - Tasks are kept in a circular linked list (ready queue)
 *   - PIT timer fires every 10ms; after SCHED_QUANTUM ticks, we switch
 *   - sched_yield() allows voluntary yielding
 *   - Idle task runs when no other task is ready (halts CPU to save power)
 *   - Dead tasks are cleaned up lazily during scheduling
 *
 * The scheduler is invoked from:
 *   1. PIT timer callback (preemptive)
 *   2. sched_yield() (cooperative)
 *   3. task_exit() (current task terminates)
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_SCHEDULER_H
#define _ASTRA_KERNEL_SCHEDULER_H

#include <kernel/types.h>

/* Time slice in PIT ticks (10ms each). 5 ticks = 50ms quantum. */
#define SCHED_QUANTUM 5

/* Initialize the scheduler (creates idle task, sets up timer hook) */
void scheduler_init(void);

/* Trigger a schedule — pick next task and context-switch if needed */
void schedule(void);

/* Voluntarily yield the current time slice */
void sched_yield(void);

/* Get the number of active tasks */
uint32_t sched_task_count(void);

#endif /* _ASTRA_KERNEL_SCHEDULER_H */
