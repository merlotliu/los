#ifndef __FS_INODE_H
#define __FS_INODE_H

#include "super_block.h"
#include "ide.h"

/* 将 inode 写入到分区 part */
void inode_sync(struct partition* part, struct inode* inode, void* io_buf);

/* 根据 inode 节点号返回对应的 inode 节点地址 */
struct inode* inode_open(struct partition* part, uint32_t inode_no);

/* 减少 inode 引用计数 */
void inode_close(struct inode* inode);

/* 初始化新的 inode 节点 */
void inode_init(uint32_t inode_no, struct inode* new_inode);

/* 将 part 上的 inode_no 对应的 inode 清空 */
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf);

/* 回收 inode 数据块及其本身 */
void inode_release(struct partition* part, uint32_t inode_no);

#endif /* __FS_INODE_H */
