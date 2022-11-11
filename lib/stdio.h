#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H

#include "global.h"
#include "syscall.h"
#include "string.h"

typedef char* va_list;

/* ap point to first arg v */
#define va_start(ap, v) (ap = (va_list)&v)
/* ap point to next arg */
#define va_arg(ap, t) (*((t*)(ap += 4)))
/* clear ap */
#define va_end(ap) (ap = NULL)

/* 将参数 ap 按照 format 格式输出到字符串 str */
uint32_t vsprintf(char* str, const char* format, va_list ap);

/* write format to buf */
uint32_t sprintf(char *buf, const char* format, ...);

/* 而格式化输出字符串 format */
uint32_t printf(const char* format, ...);

#endif /* __LIB_STDIO_H */
