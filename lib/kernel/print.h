#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H

#include "stdint.h"

/* set cursor pos */
void set_cursor(int32_t pos);
/* print single character */
void put_char(uint8_t char_asci);
/* print string */
void put_str(char *message);
/* print 32 bits 16 based integer */
void put_int(uint32_t num);

#endif /* __LIB_KERNEL_PRINT_H */