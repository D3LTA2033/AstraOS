/* ==========================================================================
 * AstraOS - Capability System Implementation
 * ==========================================================================
 * Enforces least-privilege access control through per-task capability
 * bitmasks. Security-relevant events are logged to the serial port.
 * ========================================================================== */

#include <kernel/capability.h>
#include <kernel/kstring.h>
#include <drivers/serial.h>

/* Audit log uses serial output for persistence across reboots
 * (serial can be captured by the host system or QEMU -serial file:) */

static void audit_write_dec(uint32_t val)
{
    if (val == 0) {
        serial_putchar('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (char)(val % 10);
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--)
        serial_putchar(buf[j]);
}

static void audit_write_hex(uint32_t val)
{
    const char hex[] = "0123456789ABCDEF";
    serial_putchar('0');
    serial_putchar('x');
    for (int i = 28; i >= 0; i -= 4)
        serial_putchar(hex[(val >> i) & 0xF]);
}

void security_audit(const char *subsystem, const char *event,
                    uint32_t task_id, uint32_t detail)
{
    serial_write("[AUDIT] ");
    serial_write(subsystem);
    serial_write(": ");
    serial_write(event);
    serial_write(" (task=");
    audit_write_dec(task_id);
    serial_write(", detail=");
    audit_write_hex(detail);
    serial_write(")\r\n");
}

int cap_grant(uint32_t *target_caps, uint32_t granter_caps, uint32_t new_caps)
{
    /* Granter must have CAP_SYS_ADMIN */
    if (!cap_check(granter_caps, CAP_SYS_ADMIN))
        return -1;

    /* Cannot grant capabilities the granter doesn't hold */
    if ((new_caps & granter_caps) != new_caps)
        return -1;

    *target_caps |= new_caps;
    return 0;
}

int cap_revoke(uint32_t *target_caps, uint32_t revoker_caps,
               uint32_t revoke_caps, bool self)
{
    /* Self-revocation is always allowed (dropping privileges) */
    if (!self && !cap_check(revoker_caps, CAP_SYS_ADMIN))
        return -1;

    *target_caps &= ~revoke_caps;
    return 0;
}

void security_init(void)
{
    serial_write("[AUDIT] Security subsystem initialized\r\n");
    serial_write("[AUDIT] Capability enforcement active\r\n");
}
