/* ==========================================================================
 * AstraOS - Fundamental Kernel Types
 * ==========================================================================
 * Freestanding type definitions. We cannot use <stdint.h> from a hosted
 * environment, but GCC's freestanding <stdint.h> is available. We wrap it
 * here to provide a single import point and add kernel-specific types.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_TYPES_H
#define _ASTRA_KERNEL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Status codes — will expand as subsystems are added */
typedef enum {
    ASTRA_OK    = 0,
    ASTRA_ERROR = -1,
} astra_status_t;

#endif /* _ASTRA_KERNEL_TYPES_H */
