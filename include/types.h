//
// Created by toney on 23-8-30.
//

#ifndef TOS_TYPES_H
#define TOS_TYPES_H


#define EOS '\0'            // End Of String
#define NULL ((void *)0)    // null pointer
#define ERR_IDX (-1)        // error index, 如果是unsigned类型, 相当于最大值

typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

typedef unsigned int uint;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

typedef unsigned int size_t;

#define bool uint8
#define true 1
#define false 0

#endif //TOS_TYPES_H
