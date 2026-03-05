/* ==========================================================================
 * AstraOS - Kernel String Utilities Implementation
 * ========================================================================== */

#include <kernel/kstring.h>

size_t kstrlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

void *kmemset(void *dest, int val, size_t count)
{
    uint8_t *d = (uint8_t *)dest;
    for (size_t i = 0; i < count; i++)
        d[i] = (uint8_t)val;
    return dest;
}

void *kmemcpy(void *dest, const void *src, size_t count)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < count; i++)
        d[i] = s[i];
    return dest;
}
