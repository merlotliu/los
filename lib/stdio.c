#include "stdio.h"

static char buf[1024];

/* ap point to first arg v */
#define va_start(ap, v) (ap = (va_list)&v)
/* ap point to next arg */
#define va_arg(ap, t) (*((t*)(ap += 4)))
/* clear ap */
#define va_end(ap) (ap = NULL)

/* integer to ascii */
static void itoa(uint32_t val, char** buf, uint8_t base) {
    uint32_t m = val % base;
    val = val / base;
    if(val) {
        itoa(val, buf, base);
    }
    /* 处理余数 */
    *((*buf)++) = m < 10 ? m + '0' : m - 10 + 'a';
}

/* 将参数 ap 按照 format 格式输出到字符串 str */
uint32_t vsprintf(char* str, const char* format, va_list ap) {
    char* pdst = str;
    const char* psrc = format;
    while(*psrc) {
        if(*psrc != '%') {
            *pdst++ = *psrc;
        } else {
            switch (*(++psrc)) {
                case 'c': { /* char */
                    *pdst++ = va_arg(ap, char);
                    break;
                }
                case 'd': { /* 十进制 */
                    int arg = va_arg(ap, int);
                    if(arg < 0) {
                        arg = -arg;
                        *pdst++ = '-';
                    }
                    itoa(arg, &pdst, 10);
                    break;
                }
                case 's': { /* 字符串 */
                    char *arg = va_arg(ap, char*);
                    strcpy(pdst, arg);
                    pdst += strlen(arg);
                    break;
                }
                case 'x': { /* 十六进制 */
                    itoa(va_arg(ap, int), &pdst, 16);
                    break;
                }
                default: {
                    break;
                }
            };
        }
        psrc++;
    }
    return strlen(str);
}

/* write format to buf */
uint32_t sprintf(char *buf, const char* format, ...) {
    va_list args;
    uint32_t retval;
    va_start(args, format);
    retval = vsprintf(buf, format, args);
    va_end(args);
    return retval;
}

/* 而格式化输出字符串 format */
uint32_t printf(const char* format, ...) {
    memset(buf, 0, sizeof(buf));

    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);
    return write(buf);
}
