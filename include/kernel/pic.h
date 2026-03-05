/* ==========================================================================
 * AstraOS - 8259 PIC (Programmable Interrupt Controller)
 * ==========================================================================
 * The x86 PC has two cascaded 8259 PICs:
 *   PIC1 (master): IRQ 0-7
 *   PIC2 (slave):  IRQ 8-15, cascaded through IRQ2 of PIC1
 *
 * BIOS maps IRQ 0-7 to vectors 0x08-0x0F, which collide with CPU exceptions.
 * We remap them to vectors 0x20-0x2F (32-47).
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_PIC_H
#define _ASTRA_KERNEL_PIC_H

#include <kernel/types.h>

/* IRQ base vectors after remapping */
#define PIC1_OFFSET 0x20    /* IRQ 0-7  -> vectors 32-39 */
#define PIC2_OFFSET 0x28    /* IRQ 8-15 -> vectors 40-47 */

/* Initialize and remap both PICs */
void pic_init(void);

/* Send End-of-Interrupt signal */
void pic_send_eoi(uint8_t irq);

/* Mask (disable) a specific IRQ line */
void pic_set_mask(uint8_t irq);

/* Unmask (enable) a specific IRQ line */
void pic_clear_mask(uint8_t irq);

#endif /* _ASTRA_KERNEL_PIC_H */
