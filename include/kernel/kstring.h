/* ==========================================================================
 * AstraOS - Kernel String Utilities
 * ==========================================================================
 * Freestanding replacements for common string functions.
 * We cannot link against libc, so we provide our own.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_KSTRING_H
#define _ASTRA_KERNEL_KSTRING_H

#include <kernel/types.h>

size_t  kstrlen(const char *str);
void   *kmemset(void *dest, int val, size_t count);
void   *kmemcpy(void *dest, const void *src, size_t count);
int     kstrcmp(const char *s1, const char *s2);
char   *kstrncpy(char *dest, const char *src, size_t n);

#endif /* _ASTRA_KERNEL_KSTRING_H */
