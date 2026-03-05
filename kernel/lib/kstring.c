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

int kstrcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (int)(*(const uint8_t *)s1) - (int)(*(const uint8_t *)s2);
}

char *kstrncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}
