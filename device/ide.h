#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H

#include "global.h"
#include "sync.h"

/* partition struct */
struct partition {
    uint32_t lba_start; /* 起始扇区 */
    uint32_t sec_cnt; /* 扇区数 */
    struct disk* my_disk; /* 分区所属的硬盘 */
    struct list_elem part_tag; /* 队列中的标记 */
    char name[8]; /* 分区名 */
    struct super_bck* sb; /* 本分区超级块 */
    struct bitmap bck_btp; /* 块位图 */
    struct bitmap inode_btp; /* inode 节点位图 */
    struct list open_inodes; /* 本分区打开的 inode 节点队列 */
};

/* disk struct */
struct disk {
    char name[8]; /* disk name */
    struct ide_channel* my_channel; /* 此硬盘属于哪个 ide 通道 */
    uint8_t dev_no; /* master 0 slave 1 */
    struct partition prim_parts[4]; /* 最多 4 个主分区 */
    struct partition logic_parts[8]; /* 最多允许 8 个逻辑分区（实际上可以无限） */
};

/* ata channel struct */
struct ide_channel {
    char name[8]; /* ata 通道名 */
    uint16_t port_base; /* 通道的起始端口号 */
    uint8_t irq_no; /* IRQ number */
    locker_t locker; /* channel locker */
    bool expecting_intr; /* 等待硬盘中断 */
    sem_t disk_done; /* 阻塞或唤醒驱动程序 */
    struct disk devices[2]; /* 连接主从两个硬盘 */
};

/* 从硬盘 hd 读取从 lba 扇区地址开始的 sec_cnt 个扇区到 buf */
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);

/* 将 buf 中 sec_cnt 个扇区的数据写入到从硬盘 hd 从 lba 扇区地址开始的扇区 */
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);

/* hardware interrupt handler */
void hd_intr_handler(uint8_t irq_no);

/* 硬盘数据结构初始化 */
void ide_init(void);

#endif /* __DEVICE_IDE_H */