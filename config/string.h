#ifndef AIXOS_STRING_H
#define AIXOS_STRING_H
#include <stddef.h>
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
int aixos_snprintf(char *str, size_t size, const char *format, ...);
#endif
