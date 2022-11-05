#include "string.h"
#include "global.h"
#include "debug.h"

/* 
 * @brief: 将 dst_ 起始的 size 个字节置为 value 
 */
void memset(void* dst_, uint8_t value, uint32_t size) {
    ASSERT(dst_ != NULL);
    uint8_t* dst = (uint8_t*)dst_;
    while(size--) {
        *dst++ = value;
    }
}

/* 
 * @brief: 将 src_ 起始的 size 个字节拷贝到 dst_ 
 */
void memcpy(void* dst_, const void* src_, uint32_t size) {
    ASSERT(dst_ != NULL && src_ != NULL);
    uint8_t* dst = (uint8_t*)dst_;
    const uint8_t* src = (const uint8_t*)src_;
    while(size--) {
        *dst++ = *src++;
    }
}

/* 
 * @brief: 比较 s1_ 和 s2_ 开头的 size 个字节
 * @return: 
 *      0  : s1_ == s2_
 *      1  : s1_ > s2_
 *      -1 : s1_ < s2_
 */
uint8_t memcmp(const void* s1_, const void* s2_, uint32_t size) {
    ASSERT(s1_ != NULL || s2_ != NULL);
    const char* s1 = (const char*)s1_;
    const char* s2 = (const char*)s2_;
    while(size--) {
        if(*s1 == *s2) {
            return *s1 > *s2 ? 1 : -1;
        }
        s1++;
        s2++;
    }
    return 0;
}

/* 
 * @brief: 将字符串 src_ 拷贝到 dst_ 
 * @return: 返回拷贝后的地址
 */
char* strcpy(char* dst_, const char* src_) {
    ASSERT(dst_ != NULL && src_ != NULL);
    while((*dst_++ = *src_++));
    return dst_;
}

/* 
 * @brief: 计算字符串长度
 */
uint32_t strlen(const char* str) {
    ASSERT(str != NULL);
    const char* ptr = str;
    while(*ptr++);
    return (ptr - str - 1);
}

/* 
 * @brief: 比较字符串 s1_ 和 s2_ 
 * @return: 
 *      0  : s1_ == s2_
 *      1  : s1_ > s2_
 *      -1 : s1_ < s2_
 */
uint8_t strcmp(const char* s1, const char* s2) {
    ASSERT(s1 != NULL || s2 != NULL);
    while(*s1 != 0 && *s1++ == *s2++);
    return *s1 < *s2 ? -1 : *s1 > *s2;
}

/* 
 * @brief: 返回字符 ch 在 str 首次出现的地址
 */
char* strchr(const char* str, const uint8_t ch) {
    ASSERT(str != NULL);
    while(*str != 0 && *str != ch) str++;
    return *str == ch ? (char*)str : NULL;
}

/* 
 * @brief: 返回字符 ch 在 str 最后一次出现的地址
 */
char* strrchr(const char* str, const uint8_t ch) {
    ASSERT(str != NULL);
    const char* last_ch_addr = NULL;
    while(*str != 0) {
        if(ch == *str++) {
            last_ch_addr = str;
        }
    }
    return (char*)last_ch_addr;
}

/* 
 * @brief: 将字符串 src_ 拼接到 dst_
 * @return: 拼接后字符串地址
 */
char* strcat(char* dst_, const char* src_) {
    ASSERT(dst_ != NULL && src_ != NULL);
    char* ptr = dst_;
    while(*ptr != 0) {
        ptr++;
    }
    while((*ptr++ = *src_++));
    return dst_;
}

/* 
 * @brief: 计算字符串 str 中 ch 字符出现的次数
 * @return: 返回 ch 的次数
 */
uint32_t strchrs(const char* str, uint8_t ch) {
    ASSERT(str != NULL);
    uint32_t ch_cnt = 0;
    while(*str != 0) {
        if(ch == *str) {
            ch_cnt++;
        }
    }
    return ch_cnt;
}