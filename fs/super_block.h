#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H

#include "global.h"
#include "list.h"

#define SUPER_BLOCK_SIZE        512
#define SUPER_BLOCK_ATTR_SIZE   sizeof(uint32_t)
#define SUPER_BLOCK_ATTR_CNT    13
#define SUPER_BLOCK_PADDING_LEN (SUPER_BLOCK_SIZE - SUPER_BLOCK_ATTR_CNT * SUPER_BLOCK_ATTR_SIZE)

#define SUPER_BLOCK_MAGIC       0x20221113
/* 超级块 */
struct super_block {
    uint32_t s_magic; /* 标识文件系统类型，支持多文件系统的操作系统根据此标志执行不同的操作 */

    uint32_t sec_cnt; /* 本分区扇区总数 */
    uint32_t inode_cnt; /* 本分区 inode 总数 */
    uint32_t lba_base; /* 起始的lba地址 */

    uint32_t bck_btmp_lba_base; /* 块位图起始的lba扇区地址 */
    uint32_t bck_btmp_sec_cnt; /* 块位图占用的扇区总数 */

    uint32_t inode_btmp_lba_base; /* inode结点位图起始的lba扇区地址 */
    uint32_t inode_btmp_sec_cnt; /* inode结点位图位图占用的扇区总数 */

    uint32_t inode_table_lba_base; /* inode结点表起始的lba扇区地址 */
    uint32_t inode_table_sec_cnt; /* inode结点表占用的扇区总数 */

    uint32_t data_lba_start; /* 数据区开始的第一个扇区号 */
    uint32_t root_inode_no; /* 根目录所在的 inode 节点号 */
    uint32_t s_dentry_size; /* 目录项大小 */

    uint8_t padding[SUPER_BLOCK_PADDING_LEN];/* 填充字节 */
} __attribute__ ((packed)); /* 512 bytes */

/* inode struct */
struct inode {
    uint32_t i_no; /* inode number */
    uint32_t i_size; /* file size or sum of directory entry (uint byte) */
    uint32_t i_count; /* 文件被打开的次数（引用计数） */
    bool i_write; /* 写文件时进程独享 */

    uint32_t i_sectors[13]; /* 0-11 直接块， 12 为1级指针 */
    struct list_elem i_tag; 
};


#endif /* __FS_SUPER_BLOCK_H */