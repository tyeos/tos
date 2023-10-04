//
// Created by toney on 23-8-30.
//

#include "../../include/types.h"

/**
 * 字符串复制
 * @param dest 复制到哪儿去
 * @param src 从哪复制
 * @return 复制完成的目标字符串地址
 */
char *strcpy(char *dest, const char *src) {
    char *ptr = dest;
    while (true) {
        *ptr++ = *src;
        if (*src++ == EOS) return dest;
    }
}

/**
 * 字符串拼接
 * @param dest 拼接到谁后面去
 * @param src 把谁拼接到后面去
 * @return 拼接完成的字符串地址
 */
char *strcat(char *dest, const char *src) {
    char *ptr = dest;
    while (*ptr != EOS) ptr++;
    while (true) {
        *ptr++ = *src;
        if (*src++ == EOS) return dest;
    }
}

/**
 * 计算字符串长度
 * @param str 要计算的字符串
 * @return 计算出的长度
 */
size_t strlen(const char *str) {
    char *ptr = (char *) str;
    while (*ptr != EOS) ptr++;
    return ptr - str;
}

/**
 * 比较字符串
 * @param lhs 位于比较符左边的字符串
 * @param rhs 位于比较符右边的字符串
 * @return 若相等返回0, 若左边大于右边返回1, 若左边小于右边返回-1
 */
int strcmp(const char *lhs, const char *rhs) {
    while (*lhs == *rhs && *lhs != EOS && *rhs != EOS) {
        lhs++;
        rhs++;
    }
    return *lhs < *rhs ? -1 : *lhs > *rhs;
}

/**
 * 正序查找在字符串中的某个字符（从该字符处向后截取字符串）
 * @param str 在哪个字符串中查找
 * @param ch 查找的字符
 * @return 字符在字符串中的位置 对应的地址
 */
char *strchr(const char *str, char ch) {
    char *ptr = (char *) str;
    while (true) {
        if (*ptr == ch) return ptr;
        if (*ptr++ == EOS) return NULL;
    }
}

/**
 * 逆序查找在字符串中的某个字符（从该字符处向后截取字符串）
 * @param str 在哪个字符串中查找
 * @param ch 查找的字符
 * @return 字符在字符串中的位置 对应的地址
 */
char *strrchr(const char *str, char ch) {
    char *last = NULL;
    char *ptr = (char *) str;
    while (true) {
        if (*ptr == ch) last = ptr;
        if (*ptr++ == EOS) return last;
    }
}

/**
 * 内存字符串比较
 * @param lhs 位于比较符左边的字符串
 * @param rhs 位于比较符右边的字符串
 * @param count 比较多少个字符
 * @return 若相等返回0, 若左边大于右边返回1, 若左边小于右边返回-1
 */
int memcmp(const void *lhs, const void *rhs, size_t count) {
    char *lptr = (char *) lhs;
    char *rptr = (char *) rhs;
    while (*lptr == *rptr && count-- > 0) {
        lptr++;
        rptr++;
    }
    return *lptr < *rptr ? -1 : *lptr > *rptr;
}

/**
 * 给对应内存地址数据设置字符
 * @param dest 目标地址
 * @param ch 要设置的字符
 * @param count 设置多少个
 * @return 目标内存地址
 */
void *memset(void *dest, char ch, size_t count) {
    char *ptr = dest;
    while (count--) *ptr++ = ch;
    return dest;
}

/**
 * 内存复制/拷贝
 * @param dest 复制到哪儿去
 * @param src 从哪儿复制
 * @param count 复制多少个字符
 * @return 目标地址
 */
void *memcpy(void *dest, const void *src, size_t count) {
    char *ptr = dest;
    while (count--) *ptr++ = *((char *) (src++));
    return dest;
}

/**
 * 正序查找在字符串中的某个字符（从该字符处向后截取字符串）
 * @param str 在哪个字符串中查找
 * @param ch 查找的字符
 * @param count 最多查找多少个字符
 * @return 字符在字符串中的位置 对应的地址
 */
void *memchr(const void *str, int ch, size_t count) {
    char *ptr = (char *) str;
    while (count--) {
        if (*ptr == ch) return (void *) ptr;
        ptr++;
    }
    return NULL;
}

