//
// Created by toney on 23-8-30.
//

#ifndef TOS_STRING_H
#define TOS_STRING_H

#include "../include/types.h"

char *strcpy(char *dest, const char *src);

char *strcat(char *dest, const char *src);

size_t strlen(const char *str);

int strcmp(const char *lhs, const char *rhs);

char *strchr(const char *str, char ch);

char *strrchr(const char *str, char ch);

int memcmp(const void *lhs, const void *rhs, size_t count);

void *memset(void *dest, char ch, size_t count);

void *memcpy(void *dest, const void *src, size_t count);

void *memchr(const void *ptr, int ch, size_t count);

#endif //TOS_STRING_H
