//
// Created by toney on 23-8-31.
//

#ifndef TOS_STDARG_H
#define TOS_STDARG_H

// variable argument list
typedef char *va_list;

// 指针占用字节数，和cpu位数有关
#define ptr_size sizeof(void*)

// 定义可变参数地址
#define va_start(va_ptr, last_arg) (va_ptr = (char*)&last_arg + ptr_size)
// 得到可变参数的第一个参数值，并将指针指向下一个参数地址
#define va_arg(va_ptr, type) (*(type*)((va_ptr += ptr_size) - ptr_size))
// 释放可变参数，地址置空
#define va_end(va_ptr) (va_ptr = (void *)0)

#endif //TOS_STDARG_H
