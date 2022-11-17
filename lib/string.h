#ifndef __LIB_STRING_H
#define __LIB_STRING_H

#include "stdint.h"

#define NULL ((void*)0)

/* 
 * @brief: 将 dst_ 起始的 size 个字节置为 value 
 */
void memset(void* dst_, uint8_t value, uint32_t size);

/* 
 * @brief: 将 dst_ 起始的 size 个字节置为 0 
 */
void bzero(void* buf, size_t size);

/* 
 * @brief: 将 src_ 起始的 size 个字节拷贝到 dst_ 
 */
void memcpy(void* dst_, const void* src_, uint32_t size);

/* 
 * @brief: 比较 s1_ 和 s2_ 开头的 size 个字节
 * @return: 
 *      0  : s1_ == s2_
 *      1  : s1_ > s2_
 *      -1 : s1_ < s2_
 */
uint8_t memcmp(const void* s1_, const void* s2_, uint32_t size);

/* 
 * @brief: 将字符串 src_ 拷贝到 dst_ 
 * @return: 返回拷贝后的地址
 */
char* strcpy(char* dst_, const char* src_);

/* 
 * @brief: 计算字符串长度
 */
uint32_t strlen(const char* str);

/* 
 * @brief: 比较字符串 s1_ 和 s2_ 
 * @return: 
 *      0  : s1_ == s2_
 *      1  : s1_ > s2_
 *      -1 : s1_ < s2_
 */
uint8_t strcmp(const char* s1, const char* s2);

/* 
 * @brief: 返回字符 ch 在 str 首次出现的地址
 */
char* strchr(const char* str, const uint8_t ch);

/* 
 * @brief: 返回字符 ch 在 str 最后一次出现的地址
 */
char* strrchr(const char* str, const uint8_t ch);

/* 
 * @brief: 将字符串 src_ 拼接到 dst_
 * @return: 拼接后字符串地址
 */
char* strcat(char* dst_, const char* src_);

/* 
 * @brief: 计算字符串 str 中 ch 字符出现的次数
 * @return: 返回 ch 的次数
 */
uint32_t strchrs(const char* str, uint8_t ch);

/* 
 * @brief: 根据分隔符 delim 对字符串 s 分割，save_ptr 保存下一次分割的起始位置指针
 * @return: 返回分割符分割的第一个字符串
 */
char* strtok_r(char* s, const char delim, char **saveptr);

#endif /* __LIB_STRING_H */