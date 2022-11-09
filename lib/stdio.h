#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H

#include "global.h"
#include "syscall.h"
#include "string.h"

typedef char* va_list;

/* 将参数 ap 按照 format 格式输出到字符串 str */
uint32_t vsprintf(char* str, const char* format, va_list ap);

/* 而格式化输出字符串 format */
uint32_t printf(const char* format, ...);

#endif /* __LIB_STDIO_H */
