#include "ide.h"
#include "stdio_kernel.h"
#include "debug.h"
#include "io.h"
#include "timer.h"
#include "interrupt.h"

/* 硬盘数量 */
#define HD_CNT (*((uint8_t*)(0x475)))

#define HD_FS_TYPE_EXTEND       0x05    /* 扩展分区 */

/* 定义硬盘个寄存器的端口号 */
#define reg_data(channel)       (channel->port_base + 0)
#define reg_error(channel)      (channel->port_base + 1)
#define reg_sect_cnt(channel)   (channel->port_base + 2)
#define reg_lba_l(channel)      (channel->port_base + 3)
#define reg_lba_m(channel)      (channel->port_base + 4)
#define reg_lba_h(channel)      (channel->port_base + 5)
#define reg_dev(channel)        (channel->port_base + 6)
#define reg_status(channel)     (channel->port_base + 7)
#define reg_cmd(channel)        (reg_status(channel))
#define reg_alt_status(channel) (channel->port_base + 0x206) 
#define reg_ctl(channel)        (reg_alt_status(channel))

#define BIT_STAT_BSY            0x80    /* 硬盘忙 */
#define BIT_STAT_DRQ            0x40    /* 驱动器准备好 */

/* reg_alt_status 寄存器的一些关键位 */
#define BIT_ALT_STAT_BSY        0x80    /* 硬盘忙 */
#define BIT_ALT_STAT_DRDY       0x40    /* 驱动器准备好 */
#define BIT_ALT_STAT_DRQ        0x8     /* 数据传输准备好了 */

/* device 寄存器的一些关键位 */
#define BIT_DEV_MBS             0xa0    /* 第7位和第5位固定为1 */
#define BIT_DEV_LBA             0x40
#define BIT_DEV_DEV             0x10

/* 一些硬盘操作的指令 */
#define CMD_IDENTIFY            0xec    /* identify指令 */
#define CMD_READ_SECTOR         0x20    /* 读扇区指令 */
#define CMD_WRITE_SECTOR        0x30    /* 写扇区指令 */

/* 定义可读写的最大扇区数， 调试用的 */
#define MAX_LBA_CNT             ((80*1024*1024/512) - 1) /* 只支持80MB 硬盘 */

uint8_t channel_cnt; /* 按硬盘数计算的通道数 */
struct ide_channel channels[2]; /* 有两个ide通道 */

/* 用于记录总扩展分区的起始 lba，初始为0，partition_scan 时以此为标记 */
uint32_t ext_lba_base = 0;

/* 用来记录硬盘主分区和逻辑分区的下标 */
uint8_t primary_no = 0;
uint8_t logic_no = 0;

/* 分区队列 */
struct list partition_list; 

/* 分区表项 */
struct partition_table_entry {
    uint8_t bootable; /* 是否可引导 */
    uint8_t head_start; /* 起始磁头号 */
    uint8_t sec_start; /* 起始扇区号 */
    uint8_t chs_start; /* 起始柱面号 */
    uint8_t fs_type; /* 分区类型 */
    uint8_t head_end; /* 结束磁头号 */ 
    uint8_t sec_end; /* 结束扇区号 */
    uint8_t chs_end; /* 结束柱面号 */

    uint32_t lba_start; /* 本分区起始扇区的 lba 地址 */
    uint32_t sec_cnt; /* 本分区扇区数目 */
} __attribute__((packed)); 

/* mbr 或 ebr 所在的引导扇区 */
struct boot_sector {
    uint8_t other[446];
    /* 分区表有4项共64字节 */
    struct partition_table_entry partition_table[4];
    uint16_t signature; /* end flag : 0x55,0xaa */
} __attribute__((packed));

/* 为通道选择读写的硬盘 */
static void select_disk(struct disk* hd) {
    uint8_t dev_operand = BIT_DEV_MBS | BIT_DEV_LBA;
    if(hd->dev_no == 1) { /* slave */
        dev_operand |= BIT_DEV_DEV;
    }
    outb(reg_dev(hd->my_channel), dev_operand);
}

/* 向硬盘监控器写入起始扇区地址及读取的扇区数 */
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt) {
    ASSERT(lba <= MAX_LBA_CNT);
    struct ide_channel* channel = hd->my_channel;

    /*  1. 写入需要读取的扇区数；
        2. 写入 lba 地址，即扇区号：
            2.1 先写入低 8 位；
            2.1 写入 8-15 位；
            2.3 再写入 16-23 位；
            2.4 最后写入 24-27 位;
            共 28 位；*/
    outb(reg_sect_cnt(channel), sec_cnt);

    outb(reg_lba_l(channel), lba);
    outb(reg_lba_m(channel), lba >> 8);
    outb(reg_lba_h(channel), lba >> 16);

    /* 24-27位存在于 device 寄存器的 0-3 位，需重新为 device 赋值 */
    uint8_t data = BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0x00) | lba >> 24;
    outb(reg_dev(channel), data);
}

/* 向通道channel写入指令cmd */
static void out_cmd(struct ide_channel* channel, uint8_t cmd) {
    /* 发送指令后置为 true，表示等待中断返回 */
    channel->expecting_intr = true;
    outb(reg_cmd(channel), cmd);
}

/* 等待 30s */
static bool busy_wait(struct disk* hd) {
    struct ide_channel* channel = hd->my_channel;
    uint16_t time_limit = 30 * 1000;
    while((time_limit -= 10)) {
        if((inb(reg_status(channel)) & BIT_STAT_BSY)) {
            /* busy */
            mtime_sleep(10); 
        } else {
            return ((inb(reg_status(channel) & BIT_STAT_DRQ)));
        }
    }
    return false;
}

/* 硬盘读入 sec_cnt 个扇区的数据到 buf */
static void read_sectors(struct disk* hd, void* buf, uint8_t sec_cnt) {
    uint32_t size_in_bytes;
    /* sec_cnt 为0表示 1 0000 0000 即 256 个扇区 */
    size_in_bytes = sec_cnt == 0 ? 256 * 512 : sec_cnt * 512;
    insw(reg_data(hd->my_channel), buf, size_in_bytes / 2);
}

/* 将 buf 中的数据写入到 hd 的 sec_cnt 个扇区 */
static void write_sectors(struct disk* hd, void* buf, uint8_t sec_cnt) {
    uint32_t size_in_bytes;
    size_in_bytes = sec_cnt == 0 ? 256 * 512 : sec_cnt * 512;
    outsw(reg_data(hd->my_channel), buf, size_in_bytes / 2);
}

/* 将 src 中 len 个相邻字节（0和1，2和3...）交换位置后存入 dest */
static void swap_pairs_bytes(const char* src, char* dest, uint32_t len) {
    uint8_t idx;
    for(idx = 0; idx < len; idx += 2) {
        dest[idx + 1] = *src++;
        dest[idx] = *src++;
    }
    dest[idx] = '\0';
}

/* 向硬盘发送 identify 命令并获取硬盘参数信息 */
static void identify_disk(struct disk* hd) {
    /*  
        1. 选择硬盘 hd；
        2. 像硬盘发送 CMD_IDENTIFY 指令；
        3. 使用信号量 disk_done 阻塞自己；
        4. 被唤醒后，判断硬盘状态，成功则获取硬盘信息；
        5. 输出硬盘信息，由于是根据字（2字节）读入，且为小端字节序，需要将相邻字节交换得到合适的内容；
    */
    char id_info[512];
    select_disk(hd); /* 为通道选择硬盘 */
    out_cmd(hd->my_channel, CMD_IDENTIFY); /* 向通道写入指令 */
    sem_wait(&hd->my_channel->disk_done); /* 阻塞自己等待硬盘处理完成后唤醒 */
    
    if(!busy_wait(hd)) { /* error */
        // char error[64];
        // sprintf(error, "%s identify failed!!!!!\n", hd->name);
        // PANIC(error);
    }

    read_sectors(hd, id_info, 1);/* 读取硬盘信息 */
    char buf [64];
    uint8_t sn_start = 10 * 2, sn_len = 20, md_start = 27 * 2, md_len = 40;
    swap_pairs_bytes (&id_info[sn_start], buf, sn_len);
    printk("disk %s info:\n     SN: %s\n", hd->name, buf);
    
    memset(buf, 0, sizeof(buf));
    swap_pairs_bytes(&id_info[md_start], buf, md_len);
    printk("    MODULE : %s\n ", buf);
    
    uint32_t sectors = *(uint32_t*)&id_info [60 * 2] ;
    printk("    SECTORS : %d\n", sectors);
    printk("    CAPACITY : %dMB\n", sectors* 512 / 1024 / 1024);
}

/* 扫描硬盘 hd 中地址为 ext_lba 的扇区中所有的分区 */
static void partition_scan(struct disk* hd, uint32_t ext_lba) {
    /* 读取扇区地址为 ext_lba 的一个扇区，存放在 bs，因为需要递归调用，为避免栈溢出，在堆中申请空间 */
    /* 不管是哪个分区，第一个扇区总是 mbr或ebr存储分区表 */
    struct boot_sector* bs = (struct boot_sector*)sys_malloc(sizeof(struct boot_sector));
    ide_read(hd, ext_lba, bs, 1);
    
    /* 最多 4 个主分区 */
    uint8_t part_idx = 0;
    struct partition_table_entry* partition_table = bs->partition_table;
    while(part_idx++ < 4) {
        if(partition_table->fs_type == HD_FS_TYPE_EXTEND) { /* 扩展分区 */
            if(ext_lba_base == 0) { /* 第一次读取引导块 */
                /* 记录子扩展分区的绝对扇区 lba 地址 */
                ext_lba_base = partition_table->lba_start;
                partition_scan(hd, partition_table->lba_start);
            } else {
                /* 逻辑分区的绝对扇区 lba 地址 = 子扩展分区的绝对扇区 lba 地址 + 逻辑分区偏移扇区 */
                partition_scan(hd, ext_lba_base + partition_table->lba_start);
            }
        } else if(partition_table->fs_type) { /* 0 表示无效分区 */
            if(ext_lba == 0) { /* 主分区 */
                hd->prim_parts[primary_no].lba_start = ext_lba + partition_table->lba_start;
                hd->prim_parts[primary_no].sec_cnt = partition_table->sec_cnt;
                hd->prim_parts[primary_no].my_disk = hd;
                sprintf(hd->prim_parts[primary_no].name, "%s%d", hd->name, primary_no + 1);
                list_push_back(&partition_list, &hd->prim_parts[primary_no].part_tag);
                
                primary_no++;
                ASSERT(primary_no < 4);
            } else {
                hd->logic_parts[logic_no].lba_start = ext_lba + partition_table->lba_start;
                hd->logic_parts[logic_no].sec_cnt = partition_table->sec_cnt;
                hd->logic_parts[logic_no].my_disk = hd;
                sprintf(hd->logic_parts[logic_no].name, "%s%d", hd->name, logic_no + 5);
                list_push_back(&partition_list, &hd->logic_parts[logic_no].part_tag);
                
                logic_no++;
                if(logic_no >= MAX_PARTITION_LOGIC_CNT) { /* 仅支持 8 个逻辑分区，避免数组越界 */
                    return;
                }
            }
        }
        partition_table++;
    }
    sys_free(bs);
}

/* 打印分区信息 */
static bool print_partition_info(struct list_elem* pelem, void* arg UNUSED) {
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    printk("    %s lba_start:0x%x, sec_cnt:0x%x\n", part->name, part->lba_start, part->sec_cnt);
    return false;
}

/* 从硬盘 hd 读取从 lba 扇区地址开始的 sec_cnt 个扇区到 buf */
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
    /* 该操作需要加锁，以保证一次只操作同意通道上的一块硬盘 */
    ASSERT(lba <= MAX_LBA_CNT);
    ASSERT(sec_cnt > 0);
    locker_lock(&hd->my_channel->locker);
    
    /*  1. 选择操作的硬盘；
        2. 选择待读入的扇区数和起始的扇区号；
        3. 向 reg_cmd 寄存器写入读扇区的指令， 并阻塞当前线程；
        4. 检测硬盘是否可读，失败则弹出错误信息
        5. 把数据从硬盘的缓冲区读出；
    */
    select_disk(hd);
    uint32_t secs_op = 0; /* 本次读取的扇区总数 */
    uint32_t secs_done = 0; /* 已经处理的扇区总数 */
    /* sec_cnt 大于 256 时需要多次操作（一次最多操作 256 个扇区） */
    while(secs_done < sec_cnt) {
        /* 剩余的扇区数大于 256 则本次读取 256，否则读取剩下所有扇区 */
        secs_op = secs_done + 256 <= sec_cnt ? 256 : sec_cnt - secs_done;

        select_sector(hd, lba + secs_done, secs_op);

        out_cmd(hd->my_channel, CMD_READ_SECTOR);
        sem_wait(&hd->my_channel->disk_done);

        if(busy_wait(hd)) { /* 成功 */
            read_sectors(hd, (void*)((uint32_t)buf + (secs_done * 512)), secs_op);
            secs_done += secs_op;
        } else { /* 失败 */
            char error[64];
            sprintf(error, "%s read sector %d failed!!!\n", hd->name, lba);
            PANIC(error);
        }
    }
    locker_unlock(&hd->my_channel->locker);
}

/* 将 buf 中 sec_cnt 个扇区的数据写入到从硬盘 hd 从 lba 扇区地址开始的扇区 */
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
    ASSERT(lba <= MAX_LBA_CNT);
    ASSERT(sec_cnt > 0);
    locker_lock(&hd->my_channel->locker);
    select_disk(hd);
    uint32_t secs_op = 0;
    uint32_t secs_done = 0;
    while(secs_done < sec_cnt) {
        secs_op = secs_done + 256 <= sec_cnt ? 256 : sec_cnt - secs_done;
        select_sector(hd, lba + secs_done, secs_op);
        out_cmd(hd->my_channel, CMD_WRITE_SECTOR);
        if(busy_wait(hd)) { /* 成功 */
            write_sectors(hd, (void*)((uint32_t)buf + (secs_done * 512)), secs_op);
            sem_wait(&hd->my_channel->disk_done);
            secs_done += secs_op;
        } else { /* 失败 */
            char error[64];
            sprintf(error, "%s write sector %d failed!!!\n", hd->name, lba);
            PANIC(error);
        }
    }
    locker_unlock(&hd->my_channel->locker);
}

/* hardware interrupt handler */
void hd_intr_handler(uint8_t irq_no) {
    ASSERT(irq_no == 0x2e || irq_no == 0x2f);

    struct ide_channel* channel = channels + (irq_no - 0x2e);
    ASSERT(channel->irq_no == irq_no);

    if(channel->expecting_intr) {
        channel->expecting_intr = false;
        sem_post(&channel->disk_done);
        
        /* 读取状态寄存器使硬盘认为此次终端已被处理，从而继续执行新的读写 */
        inb(reg_status(channel));
    }
}

/* 硬盘数据结构初始化 */
void ide_init(void) {
    printk("ide_init start\n");
    list_init(&partition_list);

    /* 获取硬盘数量及通道数 */
    ASSERT(HD_CNT > 0);
    channel_cnt = DIV_ROUND_UP(HD_CNT, 2);

    /* 处理每个通道的硬盘 */
    struct ide_channel* channel;
    uint8_t channel_no = 0;
    uint8_t dev_no = 0;

    while(channel_no < channel_cnt) {
        channel = &channels[channel_no];
        sprintf(channel->name, "ide%d", channel_no);

        /* 为每个 ide 通道初始化端口基址及中断向量 */
        switch (channel_no) {
            case 0: {
                /* ide0 的起始端口号为 0x1f0 */
                channel->port_base = 0x1f0;
                channel->irq_no = 0x2e; /* ide0 的中断向量号 */
                break;
            }
            case 1: {
                /* ide1 的起始端口号为 0x170 */
                channel->port_base = 0x170;
                channel->irq_no = 0x2f; /* ide1 的中断向量号 */
                break;
            }
            default: {
                break;
            }
        }
        channel->expecting_intr = false;
        locker_init(&channel->locker);
        /* 初始化为 0，目的是向硬盘控制器请求数据后，硬盘驱动信号量阻塞线程，硬盘完成后发出中断，唤醒线程 */
        sem_init(&channel->disk_done, 0);
        /* register hd interrupt handler */
        put_str("   ");
        intr_handler_register(channel->irq_no, hd_intr_handler);
        
        /* 获取2个硬盘的参数及分区信息 */
        dev_no = 0;
        while (dev_no < 2) {
            struct disk* hd = channel->devices + dev_no;
            hd->my_channel = channel;
            hd->dev_no = dev_no;
            sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
            identify_disk(hd);
            if(dev_no != 0) { /* 内核本身的硬盘(hd60M.img)不做处理 */
                partition_scan(hd, 0); /* 扫描硬盘分区 */
            }
            primary_no = 0;
            logic_no = 0;
            dev_no++;
        }
        dev_no = 0;
        channel_no++;
    }
    
    printk("all partition info\n");
    list_traversal(&partition_list, print_partition_info, NULL);

    printk("ide_init done\n");
}
