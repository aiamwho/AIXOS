#include "config/string.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

int aixos_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    size_t written = 0U;
    int total = 0;
    va_start(args, format);
    while (*format != '\0') {
        char digits[16];
        size_t count = 0U;
        uint32_t value;
        unsigned int base;
        int negative = 0;
        if (*format != '%') {
            if (written + 1U < size) str[written++] = *format;
            total++;
            format++;
            continue;
        }
        format++;
        if (*format == '%') {
            if (written + 1U < size) str[written++] = '%';
            total++;
            format++;
            continue;
        }
        if (*format == 'd') {
            int signed_value = va_arg(args, int);
            negative = signed_value < 0;
            value = negative ? (uint32_t)(-(int64_t)signed_value) :
                               (uint32_t)signed_value;
            base = 10U;
        } else if (*format == 'u') {
            value = va_arg(args, unsigned int);
            base = 10U;
        } else if (*format == 'X') {
            value = va_arg(args, unsigned int);
            base = 16U;
        } else {
            if (written + 1U < size) str[written++] = '%';
            total++;
            continue;
        }
        do {
            unsigned int digit = value % base;
            digits[count++] = (char)(digit < 10U ? '0' + digit :
                                     'A' + digit - 10U);
            value /= base;
        } while (value != 0U);
        if (negative) {
            digits[count++] = '-';
        }
        while (count != 0U) {
            char ch = digits[--count];
            if (written + 1U < size) str[written++] = ch;
            total++;
        }
        format++;
    }
    if (size != 0U) {
        str[written < size ? written : size - 1U] = '\0';
    }
    va_end(args);
    return total;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

size_t strlen(const char *s)
{
    size_t len = 0U;
    while (s[len] != '\0') len++;
    return len;
}
