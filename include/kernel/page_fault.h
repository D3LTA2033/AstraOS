/* ==========================================================================
 * AstraOS - Page Fault Handler
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_PAGE_FAULT_H
#define _ASTRA_KERNEL_PAGE_FAULT_H

/* Register the page fault handler on ISR 14 */
void page_fault_init(void);

#endif /* _ASTRA_KERNEL_PAGE_FAULT_H */
