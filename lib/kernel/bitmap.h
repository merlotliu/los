#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H

#include "global.h"

#define BITMAP_MASK 1
struct bitmap {
    uint32_t btmp_bytes_len;
    uint8_t* bits;
};

/* 
 * @brief: 将位图清空，所有位置为 0  
 */
void bitmap_init(struct bitmap* btmp);

/* 
 * @brief: 判断 bit_idx 位是否为 1
 */
uint8_t bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);

/* 
 * @brief: 寻找连续个 cnt 空间
 * @return: 成功找到返回空闲位起始索引，失败返回 -1
 */

int bitmap_scan(struct bitmap* btmp, int32_t cnt);
/* 
 * @brief: 将位图 btmp 的 bit_idx 位设置为 value
 */
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);

#endif /* __LIB_KERNEL_BITMAP_H */