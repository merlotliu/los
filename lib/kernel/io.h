#ifndef __LIB_KERNEL_IO_H
#define __LIB_KERNEL_IO_H

#include "stdint.h"

/*
 * @brief: write data to port
 * @param: port is 0~255
 */
static inline void outb(uint16_t port, uint8_t data) {
    /* 'a' is al/ax/eax. 'b' of '%b0' is low 8 bit. so '%b0' is al.
     * 'b' is bl/bx/ebx. 'w' of '%w1' is low 16 bit. so '%w1' is bx.
     * 'N' limit operands to 0-255.
     */
    asm volatile("outb %b0, %w1" : : "a" (data), "Nd" (port));
}

static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt) {
    asm volatile("cld; rep outsw" : "+S" (addr), "+c" (word_cnt) : "d" (port));
}

/*
 * @brief: read data of port
 * @param: port is 0~255
 * @return: data(uint8_t) of port
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    asm volatile("inb %w1, %b0" : "=a" (data) : "Nd" (port));
    return data;
}

static inline void insw(uint16_t port, void* addr, uint32_t word_cnt) {
    asm volatile("cld; rep insw" : "+D" (addr), "+c" (word_cnt) : "d" (port) : "memory");
}

#endif /* __LIB_KERNEL_IO_H */