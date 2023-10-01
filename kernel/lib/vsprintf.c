//
// Created by toney on 23-8-31.
// 该文件参考linux内核源码
//

#include "../../include/stdarg.h"
#include "../../include/string.h"

/* we use this so that we can do without the ctype library */
#define is_digit(c) ((c) >= '0' && (c) <= '9')

// 将字符数字转换成整数
// 输入：数字串的二级指针
static int skip_atoi(const char **s) {
    int i = 0;
    while (is_digit(**s))
        i = i * 10 + *((*s)++) - '0';
    return i;
}

#define ZEROPAD 1        /* pad with zero */
#define SIGN    2        /* unsigned/signed long */
#define PLUS    4        /* show plus */
#define SPACE   8        /* space if plus */
#define LEFT    16       /* left justified */
#define SPECIAL 32       /* 0x */
#define SMALL   64       /* use 'abcdef' instead of 'ABCDEF' */

// 除法操作
// 输入：n为被除数，base为除数
// 结果：n为商，函数返回值为余数
#define do_div(n, base) ({ \
int __res; \
__asm__("div %4":"=a" (n),"=d" (__res):"0" (n),"1" (0),"r" (base)); \
__res; })

// 将int数字转换为指定进制的字符串
// 输入：num-待转换的数字，base-进制[2~36]，size-字符串长度，precision-数字长度(精度)，type-类型选项
// 输出：str字符串指针
static char *number(char *str, int num, int base, int size, int precision, int type) {
    char c, sign, tmp[36];
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;

    if (type & SMALL) digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (type & LEFT) type &= ~ZEROPAD;
    if (base < 2 || base > 36)
        return 0;
    c = (type & ZEROPAD) ? '0' : ' ';
    if (type & SIGN && num < 0) {
        sign = '-';
        num = -num;
    } else
        sign = (type & PLUS) ? '+' : ((type & SPACE) ? ' ' : 0);
    if (sign) size--;
    if (type & SPECIAL) {
        if (base == 16) size -= 2;    // 16进制size减2，用于前面的0x
        else if (base == 8) size--;   // 8进制减1，因为前面的0
    }
    i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else
        while (num != 0)
            tmp[i++] = digits[do_div(num, base)];
    if (i > precision) precision = i;   // 若字符个数大于精度值，精度值扩展为字符个数
    size -= precision;
    // 将转换结果放str中，如果类型中没有零填充和左靠齐标志，则在str中先填放剩余宽度值指出的空格数。若需要带符号位，则存入符号
    if (!(type & (ZEROPAD + LEFT)))
        while (size-- > 0)
            *str++ = ' ';
    if (sign)
        *str++ = sign;
    // 如果是特殊转换的处理，8进制和16进制分别填入0/0x/0X
    if (type & SPECIAL) {
        if (base == 8)
            *str++ = '0';
        else if (base == 16) {
            *str++ = '0';
            *str++ = digits[33];    // 'x' or 'X'
        }
    }

    // 如果类型么没左对齐标志，则在剩余的宽度中填充占位字符
    if (!(type & LEFT))
        while (size-- > 0)
            *str++ = c;
    // 若i存有数值num的数字个数，若数字个数小于精度值，则str中放入 精度值-i 个'0'
    while (i < precision--)
        *str++ = '0';
    // 将数值转换好的数字字符填入str中，共i个
    while (i-- > 0)
        *str++ = tmp[i];
    // 若宽度值仍大于零，则表达类型标志中有左靠齐标志，则在剩余宽度中放入空格
    while (size-- > 0)
        *str++ = ' ';
    return str;
}

// 字符串格式化输出，附带可变参数列表
int vsprintf(char *buf, const char *fmt, va_list args) {
    int len;
    int i;
    char *str;        // 存放转换过程中的字符串
    char *s;
    int *ip;

    int flags;        /* flags to number() */

    int field_width;  /* width of output field min. # of digits for integers; max number of chars for from string */
    int precision;
    int qualifier;    /* 'h', 'l', or 'L' for integer fields */

    // 按单字符扫描，到'\0'退出循环
    for (str = buf; *fmt; ++fmt) {
        // 识别'%'作为flag转义符
        if (*fmt != '%') {
            *str++ = *fmt;
            continue;
        }

        /* process flags */
        flags = 0;
        repeat:
        ++fmt;        /* this also skips first '%' */
        switch (*fmt) {
            case '-':
                flags |= LEFT;
                goto repeat;
            case '+':
                flags |= PLUS;
                goto repeat;
            case ' ':
                flags |= SPACE;
                goto repeat;
            case '#':
                flags |= SPECIAL;
                goto repeat;
            case '0':
                flags |= ZEROPAD;
                goto repeat;
        }

        /* get field width */
        field_width = -1;
        if (is_digit(*fmt))
            field_width = skip_atoi(&fmt);
        else if (*fmt == '*') {
            /* it's the next argument */
            field_width = va_arg(args, int); // 使用%*指定域宽度，需要从参数列表取宽度值
            ++fmt; // linux源码没有这一行，是个bug，加上即可，已测试
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        /* get the precision */
        precision = -1;
        if (*fmt == '.') {
            ++fmt;
            if (is_digit(*fmt))
                precision = skip_atoi(&fmt);
            else if (*fmt == '*') {
                /* it's the next argument */
                precision = va_arg(args, int);
                ++fmt; // linux源码没有这一行，是个bug，加上即可，已测试
            }
            if (precision < 0)
                precision = 0;
        }

        /* get the conversion qualifier */
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            qualifier = *fmt;
            ++fmt;
        }

        // 分析转换指示符
        switch (*fmt) {
            case 'c': // 字符
                if (!(flags & LEFT))
                    while (--field_width > 0)
                        *str++ = ' ';
                *str++ = (unsigned char) va_arg(args, int);
                while (--field_width > 0)
                    *str++ = ' ';
                break;

            case 's': // 字符串
                s = va_arg(args, char *);
                len = strlen(s);
                if (precision < 0)
                    precision = len;
                else if (len > precision)
                    len = precision;

                if (!(flags & LEFT))
                    while (len < field_width--)
                        *str++ = ' ';
                for (i = 0; i < len; ++i)
                    *str++ = *s++;
                while (len < field_width--)
                    *str++ = ' ';
                break;


            case 'o': // 8进制数
                str = number(str, va_arg(args, unsigned long), 8,
                             field_width, precision, flags);
                break;

            case 'p': // 指针类型
                if (field_width == -1) {
                    field_width = 8;
                    flags |= ZEROPAD;
                }
                str = number(str,
                             (unsigned long) va_arg(args, void *), 16,
                             field_width, precision, flags);
                break;

            case 'x': // 16进制数，字母小写
                flags |= SMALL;
            case 'X': // 16进制数，字母大写
                str = number(str, va_arg(args, unsigned long), 16,
                             field_width, precision, flags);
                break;

            case 'd': // 带符号整数
            case 'i':
                flags |= SIGN;
            case 'u': // 无符号整数
                str = number(str, va_arg(args, unsigned long), 10,
                             field_width, precision, flags);
                break;

            case 'n': // 把到目前为止转换输出字符数保存到对应参数上
                ip = va_arg(args, int *);
                *ip = (str - buf);
                break;

            default: // 输出百分号
                if (*fmt != '%')
                    *str++ = '%';
                if (*fmt)
                    *str++ = *fmt;
                else
                    --fmt;
                break;
        }
    }
    *str = '\0';        // 字符串结尾字符：'\0'
    return str - buf;     // 返回转换好的长度值
}
