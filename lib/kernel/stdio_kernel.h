#ifndef __LIB_KERNEL_STDIO_KERNEL_H
#define __LIB_KERNEL_STDIO_KERNEL_H

#include "stdio.h"
#include "console.h"

/* 内核格式化输出 */
void printk(const char* format, ...);

#endif /* __LIB_KERNEL_STDIO_KERNEL_H */