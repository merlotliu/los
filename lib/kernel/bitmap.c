#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "debug.h"

/* 
 * @brief: 将位图清空，所有位置为 0  
 */
void bitmap_init(struct bitmap* btmp) {
    ASSERT(btmp != NULL);
    memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

/* 
 * @brief: 判断 bit_idx 位是否为 1
 */
uint8_t bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx) {
    ASSERT(btmp != NULL);
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;
    return ((BITMAP_MASK << bit_odd) & btmp->bits[byte_idx]);
}

/* 
 * @brief: 寻找连续个 cnt 空间
 * @return: 成功找到返回空闲位起始索引，失败返回 -1
 */
int bitmap_scan(struct bitmap* btmp, int32_t cnt) {
    ASSERT(btmp != NULL);
    /* 1. 先找到有空闲位的字节 */
    uint32_t byte_idx = 0;
    while((byte_idx < btmp->btmp_bytes_len && 0xff == btmp->bits[byte_idx])) {
        byte_idx++;
    }
    ASSERT(byte_idx < btmp->btmp_bytes_len);
    if(byte_idx == btmp->btmp_bytes_len) {
        return -1;
    }
    /* 2. 找到字节中空闲位开始索引 */
    uint32_t bit_local_idx = 0; /* 单个字节内部索引 */
    while((uint8_t)(BITMAP_MASK << bit_local_idx) & btmp->bits[byte_idx]) {
        bit_local_idx++;
    }
    int bit_idx_start = byte_idx * 8 + bit_local_idx;

    /* 3. 是否有连续 cnt 个空闲位 */
    if(cnt == 1) {
        return bit_idx_start;
    }
    int bit_left_cnt = (btmp->btmp_bytes_len * 8 - bit_idx_start);
    if(cnt > bit_left_cnt) {
        return -1;
    }

    uint32_t next_bit = bit_idx_start + 1;
    int count = 1;
    bit_idx_start = -1;
    /*  
        next_bit：
        0 : 连续空闲位数 ++，
        1 : count 清0，如果剩余位数不足，返回 -1 ，否则重新查找连续的 0
    */
    while(bit_left_cnt--) {
        if(bitmap_scan_test(btmp, next_bit)) {
            count = 0;
            if(cnt >= bit_left_cnt) {
                break;
            }
            continue;
        } else {
            count++;
        }
        if(count == cnt) {
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }
    return bit_idx_start;
}

/* 
 * @brief: 将位图 btmp 的 bit_idx 位设置为 value
 */
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value) {
    ASSERT(btmp != NULL);
    ASSERT((value == 0) || (value == 1));
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;
    if(value) { /* set to 1 */
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    } else {
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
    }
}
