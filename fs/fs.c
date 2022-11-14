#include "fs.h"
#include "ide.h"
#include "dir.h"
#include "super_block.h"
#include "debug.h"
#include "stdio_kernel.h"

extern uint8_t channel_cnt; /* 按硬盘数计算的通道数 */
extern struct ide_channel channels[2]; /* 有两个ide通道 */
extern struct list partition_list; /* 分区队列 */

struct partition* __cur_part; /* 当前操作分区（全局变量） */
/* 
 * @breif 格式化分区，初始化分区的元信息，创建文件系统 
 * @param part 操作的分区
 */
static void partition_format(struct partition* part) {
    /* 0. 计算相关元数据 */
    /* 为了编码简单，一块即为一个扇区 */
    uint32_t boot_sector_sec_cnt = 1;
    uint32_t super_block_sec_cnt = 1;
    uint32_t inode_btmp_sec_cnt = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR); /* 总文件数 / 每个扇区的总位数（字节数 * 8） */
    uint32_t inode_table_sec_cnt = DIV_ROUND_UP(MAX_FILES_PER_PART * sizeof(struct inode), SECTOR_SIZE); /* 总文件数 * inode节点大小（字节数） / 每个扇区的字节数 */

    uint32_t used_sec_cnt = boot_sector_sec_cnt + super_block_sec_cnt + inode_btmp_sec_cnt + inode_table_sec_cnt;
    /* 这一空闲扇区数目还没有减去快位图占用的扇区 */
    uint32_t free_sec_cnt = part->sec_cnt - used_sec_cnt;

    /* 块位图占用的扇区数：先计算出位图需要的总位数，使用位图总位数除以每个扇区的总位数，向上取整即可 */
    uint32_t bck_btmp_sec_cnt;
    /* 这才是真正的空闲扇区数目（实际上会舍弃掉一些位置） */
    uint32_t bck_btmp_bit_len = free_sec_cnt - DIV_ROUND_UP(free_sec_cnt, BITS_PER_SECTOR); 
    bck_btmp_sec_cnt = DIV_ROUND_UP(bck_btmp_bit_len, BITS_PER_SECTOR);

    /* 超级块初始化 */
    struct super_block sb;
    sb.s_magic = SUPER_BLOCK_MAGIC;

    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.lba_base = part->lba_start;

    /* 第 0 块为引导块，第 1 块为超级块，第 2 块则为块位图 */
    sb.bck_btmp_lba_base = sb.lba_base + 2;
    sb.bck_btmp_sec_cnt = bck_btmp_sec_cnt;

    /* inode 位图跟在块位图后面 */
    sb.inode_btmp_lba_base = sb.bck_btmp_lba_base + sb.bck_btmp_sec_cnt;
    sb.inode_btmp_sec_cnt = inode_btmp_sec_cnt;

    /* inode table */
    sb.inode_table_lba_base = sb.inode_btmp_lba_base + sb.inode_btmp_sec_cnt;
    sb.inode_table_sec_cnt = inode_table_sec_cnt;

    sb.data_lba_start = sb.inode_table_lba_base + sb.inode_table_sec_cnt;
    sb.root_inode_no = 0;
    sb.s_dentry_size = sizeof(struct dentry);

    /* print */
    printk("%s info: \n", part->name);
    printk("    magic:0x%x\n    lba_base:0x%x\n    all_sectors:0x%x\n    inode_cnt:0x%x\n"\
        "    bck_btmp_lba_base:0x%x\n    bck_btmp_sec_cnt:0x%x\n    inode_btmp_lba_base:0x%x\n    inode_btmp_sec_cnt:0x%x\n"\
        "    inode_table_lba_base:0x%x\n    inode_table_sec_cnt:0x%x\n    data_lba_start:0x%x\n",
        sb.s_magic, sb.lba_base, sb.sec_cnt, sb.inode_cnt,
        sb.bck_btmp_lba_base, sb.bck_btmp_sec_cnt, sb.inode_btmp_lba_base, sb.inode_btmp_sec_cnt, 
        sb.inode_table_lba_base, sb.inode_table_sec_cnt, sb.data_lba_start) ;
    
    /* 1. 将超级块写入本分区的 1 号扇区（0 号扇区为引导扇区） */
    struct disk* cur_hd = part->my_disk;
    ide_write(cur_hd, part->lba_start + 1, &sb, 1);
    printk("    super_block_lba:0x%x\n", part->lba_start + 1);

    /* 找到数据中占用扇区最大的值 * SECTOR_SIZE (字节) 作为缓冲区大小 */
    uint32_t buf_size = sb.bck_btmp_sec_cnt > sb.inode_btmp_sec_cnt ? sb.bck_btmp_sec_cnt : sb.inode_btmp_sec_cnt;
    buf_size = sb.inode_table_sec_cnt > buf_size ? sb.inode_table_sec_cnt : buf_size;
    buf_size *= SECTOR_SIZE;
    uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

    /* 2. 初始化块位图并写入超级块中的地址
     *  将位图所在的最后一个扇区的多余部分全置为 1 （标识超过实际块数的部分为占用）；
     */
    buf[0] |= 0x01; /* 根目录位置，占位 */
    uint32_t bck_btmp_last_byte = bck_btmp_bit_len / 8;
    uint8_t bck_btmp_last_bit = bck_btmp_bit_len % 8;
    /* 最后一个扇区多余的字节数 */
    uint32_t last_sec_rest_size = SECTOR_SIZE - (bck_btmp_last_byte % SECTOR_SIZE) - 1;

    /* 将位图最后一个字节中最后一位之外的字节全部置为1 */
    while(bck_btmp_last_bit < 8) {
        buf[bck_btmp_last_byte] |= 1 << bck_btmp_last_bit++;
    }
    /* 将位图所能表示范围之外的字节全部置为1 */
    memset(buf + bck_btmp_last_byte + 1, 0xff, last_sec_rest_size);
    /* 将初始化好的内容写入对应扇区 */
    ide_write(cur_hd, sb.bck_btmp_lba_base, buf, sb.bck_btmp_sec_cnt);

    /* 3. 初始化 inode 位图并写入对应扇区 */
    /* 清空缓冲 */
    memset(buf, 0, buf_size);
    /* inode 位图刚好为 1 扇区（4096位） */
    buf[0] |= 0x01; /* 根目录位置，占位 */
    ide_write(cur_hd, sb.inode_btmp_lba_base, buf, sb.inode_btmp_sec_cnt);

    /* 4. 将 inode 数组初始化并写入对应扇区*/
    /* 清空缓冲 */
    memset(buf, 0, buf_size);
    /* 初始化根目录的inode，表的第 0 项 */
    struct inode* idx_node = (struct inode*)buf;
    idx_node->i_size = sb.s_dentry_size * 2; /* '.' & '..' */
    idx_node->i_no = 0;
    /* i_sectors 已经被清除为0 */
    idx_node->i_sectors[0] = sb.data_lba_start;
    ide_write(cur_hd, sb.inode_table_lba_base, buf, sb.inode_table_sec_cnt);

    /* 5. 将根目录写入空闲数据的起始扇区 sb.data_lba_start */
    /* 清空缓冲 */
    memset(buf, 0, buf_size);

    struct dentry* dir_entry = (struct dentry*)buf;
    /* 初始化当前目录 '.' */
    memcpy(dir_entry->d_filename, ".", 1);
    dir_entry->d_inode = 0;
    dir_entry->d_ftype = FT_DIRECTORY;
    dir_entry++;

    /* 初始化父目录 */
    memcpy(dir_entry->d_filename, "..", 2);
    dir_entry->d_inode = 0;
    dir_entry->d_ftype = FT_DIRECTORY;

    ide_write(cur_hd, sb.data_lba_start, buf, 1);

    printk("    root_dir_lba:0x%x\n", sb.data_lba_start);
    printk("%s format done\n", part->name);
    sys_free(buf);
}

/* 找到名为 mount_part_name 的分区，将其赋值给 __cur_part 当前操作分区 */
static bool mount_partition(struct list_elem* pelem, void* arg) {
    char* mount_part_name = (char*)arg;
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    if(strcmp(part->name, mount_part_name) == 0) {
        __cur_part = part;
        /* 1 读取分区上的超级块到内存 */
        struct disk* hd = part->my_disk;
        struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
        memset(sb_buf, 0, SECTOR_SIZE);
        ide_read(hd, part->lba_start + 1, sb_buf, 1);

        part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
        if(part->sb == NULL) {
            PANIC("allocate memory failed!");
        }
        /* 复制超级块到 __cur_part->sb 指向的内存 */
        memcpy(part->sb, sb_buf, sizeof(struct super_block));

        /* 2 读入块位图到内存 */
        part->bck_btp.bits = (uint8_t*)sys_malloc(sb_buf->bck_btmp_sec_cnt * SECTOR_SIZE);
        if(part->bck_btp.bits == NULL) {
            PANIC("allocate memory failed!");
        }
        part->bck_btp.btmp_bytes_len = sb_buf->bck_btmp_sec_cnt * SECTOR_SIZE;
        ide_read(hd, sb_buf->bck_btmp_lba_base, part->bck_btp.bits, sb_buf->bck_btmp_sec_cnt);

        /* 3 读入 inode 位图到内存 */
        part->inode_btp.bits = (uint8_t*)sys_malloc(sb_buf->inode_btmp_sec_cnt * SECTOR_SIZE);
        if(part->inode_btp.bits == NULL) {
            PANIC("allocate memory failed!");
        }
        part->inode_btp.btmp_bytes_len = sb_buf->inode_btmp_sec_cnt * SECTOR_SIZE;
        ide_read(hd, sb_buf->inode_btmp_lba_base, part->inode_btp.bits, sb_buf->inode_btmp_sec_cnt);

        list_init(&part->open_inodes);
        printk("mount %s done!\n", part->name);

        return true;
    }   
    return false;
}

/* 在磁盘上搜索文件系统，若没有则格式化分区创建文件系统 */
void filesys_init(void) {
    uint8_t channel_no = 0;
    uint8_t dev_no = 0;
    uint8_t part_idx = 0;

    /* 获取硬盘上的超级块，没有超级块则说明没有文件系统 */
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
    if(sb_buf == NULL) {
        PANIC("allocate memory failed!");
    }
    printk("searching file system ... ... \n");
    while(channel_no < channel_cnt) {
        dev_no = 0;
        while(dev_no < 2) {
            if(dev_no == 0) { /* hd60M.img( OS ) */
                dev_no++;
                continue;
            }
   
            struct disk* hd = channels[channel_no].devices + dev_no;
            struct partition *cur_part = hd->prim_parts;
            uint8_t max_part_cnt = MAX_PARTITION_CNT;
            while(part_idx < max_part_cnt) {
                if(part_idx == 4) { /* logic partition */
                   cur_part = hd->logic_parts;
                }

                if(cur_part->sec_cnt != 0) {
                    memset(sb_buf, 0, SECTOR_SIZE);
                    ide_read(hd, cur_part->lba_start + 1, sb_buf, 1);
                    
                    if(SUPER_BLOCK_MAGIC == sb_buf->s_magic) {
                        /* 已存在文件系统 */
                        printk("%s has file system\n", cur_part->name);
                    } else {
                        printk("formatting %s's partition %s\n", hd->name, cur_part->name);
                        partition_format(cur_part);
                    }
                }
                part_idx++;
                cur_part++;
            }
            dev_no++;
        }
        channel_no++;
    }
    sys_free(sb_buf);

    /* default operation partition */
    char default_part[MAX_FILE_NAME_LEN] = "sdb1";
    /* mount partition */
    list_traversal(&partition_list, mount_partition, (void*)default_part);
}
