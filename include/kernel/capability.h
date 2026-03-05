/* ==========================================================================
 * AstraOS - Capability-Based Access Control
 * ==========================================================================
 * Each task carries a 32-bit capability bitmask that determines what
 * operations it is authorized to perform. Capabilities are checked at
 * every security boundary (syscall entry, device access, process creation).
 *
 * Design rationale:
 *   Capabilities are strictly subtractive. A child task can never hold a
 *   capability its parent did not hold at the time of creation. The kernel
 *   (task 0, idle, main) holds CAP_ALL. User tasks receive a restricted
 *   default set. Capabilities can be further restricted but never escalated
 *   without CAP_SYS_ADMIN.
 *
 *   This is simpler than a full capability token system (like seL4) but
 *   provides meaningful least-privilege enforcement for an educational OS.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_CAPABILITY_H
#define _ASTRA_KERNEL_CAPABILITY_H

#include <kernel/types.h>

/* Capability bits — each controls a class of privileged operations */
#define CAP_VFS_READ        (1U << 0)   /* Read files through VFS */
#define CAP_VFS_WRITE       (1U << 1)   /* Write files through VFS */
#define CAP_DEVICE_ACCESS   (1U << 2)   /* Open/read/write device nodes */
#define CAP_PROC_CREATE     (1U << 3)   /* Create child processes */
#define CAP_SYS_ADMIN       (1U << 4)   /* Grant/revoke capabilities */
#define CAP_RAW_IO          (1U << 5)   /* Direct port I/O (future) */
#define CAP_NET             (1U << 6)   /* Network operations (future) */
#define CAP_SIGNAL          (1U << 7)   /* Send signals to other tasks */
#define CAP_MOUNT           (1U << 8)   /* Mount/unmount filesystems */
#define CAP_AUDIT_READ      (1U << 9)   /* Read security audit log */

/* Composite sets */
#define CAP_ALL             0xFFFFFFFF
#define CAP_NONE            0x00000000

/* Default capabilities for user-space tasks:
 * Can read/write files and access devices, but cannot create processes,
 * modify capabilities, or do raw I/O. */
#define CAP_USER_DEFAULT    (CAP_VFS_READ | CAP_VFS_WRITE | CAP_DEVICE_ACCESS | CAP_PROC_CREATE | CAP_SIGNAL)

/* Check if a capability set includes a specific capability.
 * Returns 1 if granted, 0 if denied. */
static inline int cap_check(uint32_t cap_mask, uint32_t required)
{
    return (cap_mask & required) == required;
}

/* Grant capabilities (OR into existing set).
 * The granter must hold CAP_SYS_ADMIN. */
int cap_grant(uint32_t *target_caps, uint32_t granter_caps, uint32_t new_caps);

/* Revoke capabilities (AND-NOT from existing set).
 * A task can always revoke its own capabilities (drop privileges).
 * Revoking another task's capabilities requires CAP_SYS_ADMIN. */
int cap_revoke(uint32_t *target_caps, uint32_t revoker_caps,
               uint32_t revoke_caps, bool self);

/* Log a security event to the audit trail (serial output) */
void security_audit(const char *subsystem, const char *event,
                    uint32_t task_id, uint32_t detail);

/* Initialize the security subsystem */
void security_init(void);

#endif /* _ASTRA_KERNEL_CAPABILITY_H */
