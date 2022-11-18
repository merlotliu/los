#include "fs.h"
#include "dir.h"
#include "ide.h"
#include "super_block.h"
#include "debug.h"
#include "stdio_kernel.h"
#include "inode.h"
#include "string.h"
#include "keyboard.h"

extern uint8_t channel_cnt; /* 按硬盘数计算的通道数 */
extern struct ide_channel channels[2]; /* 有两个ide通道 */
extern struct list partition_list; /* 分区队列 */
extern struct file __file_table[MAX_FILE_OPEN]; /* 文件表 */
extern struct dir __root_dir; /* 根目录 */
extern ioqueue_t __kbd_buf; /* keyboard buffer */

struct partition* __cur_part; /* 当前操作分区(全局变量） */

/* 
 * @breif 格式化分区，初始化分区的元信息，创建文件系统 
 * @param part 操作的分区
 */
static void partition_format(struct partition* part) {
    /* 0. 计算相关元数据 */
    /* 为了编码简单，一块即为一个扇区 */
    uint32_t boot_sector_sec_cnt = 1;
    uint32_t super_block_sec_cnt = 1;
    uint32_t inode_btmp_sec_cnt = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR); /* 总文件数 / 每个扇区的总位数(字节数 * 8） */
    uint32_t inode_table_sec_cnt = DIV_ROUND_UP(MAX_FILES_PER_PART * sizeof(struct inode), SECTOR_SIZE); /* 总文件数 * inode节点大小(字节数） / 每个扇区的字节数 */

    uint32_t used_sec_cnt = boot_sector_sec_cnt + super_block_sec_cnt + inode_btmp_sec_cnt + inode_table_sec_cnt;
    /* 这一空闲扇区数目还没有减去快位图占用的扇区 */
    uint32_t free_sec_cnt = part->sec_cnt - used_sec_cnt;

    /* 块位图占用的扇区数：先计算出位图需要的总位数，使用位图总位数除以每个扇区的总位数，向上取整即可 */
    uint32_t bck_btmp_sec_cnt;
    /* 这才是真正的空闲扇区数目(实际上会舍弃掉一些位置） */
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
    
    /* 1. 将超级块写入本分区的 1 号扇区(0 号扇区为引导扇区） */
    struct disk* cur_hd = part->my_disk;
    ide_write(cur_hd, part->lba_start + 1, &sb, 1);
    printk("    super_block_lba:0x%x\n", part->lba_start + 1);

    /* 找到数据中占用扇区最大的值 * SECTOR_SIZE (字节) 作为缓冲区大小 */
    uint32_t buf_size = sb.bck_btmp_sec_cnt > sb.inode_btmp_sec_cnt ? sb.bck_btmp_sec_cnt : sb.inode_btmp_sec_cnt;
    buf_size = sb.inode_table_sec_cnt > buf_size ? sb.inode_table_sec_cnt : buf_size;
    buf_size *= SECTOR_SIZE;
    uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

    /* 2. 初始化块位图并写入超级块中的地址
     *  将位图所在的最后一个扇区的多余部分全置为 1 (标识超过实际块数的部分为占用）；
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
    /* inode 位图刚好为 1 扇区(4096位） */
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
        part->bck_btmp.bits = (uint8_t*)sys_malloc(sb_buf->bck_btmp_sec_cnt * SECTOR_SIZE);
        if(part->bck_btmp.bits == NULL) {
            PANIC("allocate memory failed!");
        }
        part->bck_btmp.btmp_bytes_len = sb_buf->bck_btmp_sec_cnt * SECTOR_SIZE;
        ide_read(hd, sb_buf->bck_btmp_lba_base, part->bck_btmp.bits, sb_buf->bck_btmp_sec_cnt);

        /* 3 读入 inode 位图到内存 */
        part->inode_btmp.bits = (uint8_t*)sys_malloc(sb_buf->inode_btmp_sec_cnt * SECTOR_SIZE);
        if(part->inode_btmp.bits == NULL) {
            PANIC("allocate memory failed!");
        }
        part->inode_btmp.btmp_bytes_len = sb_buf->inode_btmp_sec_cnt * SECTOR_SIZE;
        ide_read(hd, sb_buf->inode_btmp_lba_base, part->inode_btmp.bits, sb_buf->inode_btmp_sec_cnt);

        list_init(&part->open_inodes);
        printk("mount %s done!\n", part->name);

        return true;
    }   
    return false;
}

/* 根据 '/' 解析路径名存储在 name, str 之后的路径 */
char* path_parse(char* pathname, char* name) {
    if(pathname == NULL || *pathname == 0) {
        return NULL;
    }
    while(*pathname != 0 && *pathname == '/') pathname++; /* 跳过开头连续的 '/' 即根目录 */
    char* ptr = pathname;
    while(*ptr != 0 && *ptr != '/') {
        *name++ = *ptr++;
    }
    ASSERT((uint8_t)(ptr - pathname) <= MAX_FILE_NAME_LEN);
    return ptr;
}

/* 返回路径深度，如 '/a/b/c' 和 '///a/b//c' 返回的都是 3 */
uint32_t path_depth(char* pathname) {
    ASSERT(pathname != NULL);
    uint32_t depth = 0;
    char name[MAX_FILE_NAME_LEN];
    char* sub_path = path_parse(pathname, name);
    while(*name != 0) {
        depth++;
        bzero(name, MAX_FILE_NAME_LEN);
        if(sub_path != NULL) {
            sub_path = path_parse(sub_path, name);
        }
    }
    return depth;
}

/* 搜索文件 pathname，找到则返回其 inode 号，否则返回 -1 */
static int file_search(const char* pathname, struct path_search_record* searched_record) {
    /* 根目录 */
    if(strcmp("/", pathname) == 0 || strcmp("/.", pathname) == 0 || strcmp("/..", pathname) == 0) {
        searched_record->parent_dir = &__root_dir;
        searched_record->p_ftype = FT_DIRECTORY;
        searched_record->searched_path[0] = 0;
        return 0;
    }

    uint32_t path_len = strlen(pathname);
    /* 保证路径为绝对路径 */
    ASSERT(*pathname == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
    struct dir* parent_dir = &__root_dir;
    struct dentry dir_entry;

    searched_record->parent_dir = parent_dir;
    searched_record->p_ftype = FT_UNKNOWN;
    uint32_t parent_inode_no = 0; /* 父目录 inode */
    
    char name[MAX_FILE_NAME_LEN] = {0};
    char* sub_path = (char*)pathname;
    sub_path = path_parse(sub_path, name);
    while(*name != 0) {
        ASSERT(strlen(searched_record->searched_path) < MAX_PATH_LEN);
        /* 拼接父目录 */
        strcat(searched_record->searched_path, "/");
        strcat(searched_record->searched_path, name);

        /* __cur_part 分区的 parent_dir 目录中查找文件或目录 name */
        if(dentry_search(__cur_part, parent_dir, name, &dir_entry)) {
            memset(name, 0, MAX_FILE_NAME_LEN);
            if(sub_path != NULL) {
                sub_path = path_parse(sub_path, name);
            }
            switch(dir_entry.d_ftype) {
                case FT_DIRECTORY: { /* 目录 */
                    parent_inode_no = parent_dir->d_inode->i_no;
                    dir_close(parent_dir);
                    parent_dir = dir_open(__cur_part, dir_entry.d_inode);
                    searched_record->parent_dir = parent_dir;
                    break;
                }
                case FT_REGULAR: { /* 文件 */
                    searched_record->p_ftype = FT_REGULAR;
                    return dir_entry.d_inode;
                }
                default: {
                    break;
                }
            }
        } else {
            return -1;
        }
    }
    dir_close(searched_record->parent_dir);
    
    searched_record->parent_dir = dir_open(__cur_part, parent_inode_no);
    searched_record->p_ftype = FT_DIRECTORY;
    return dir_entry.d_inode;
}

/* 创建文件，成功则返回文件描述符，失败返回-1 */
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flags) {
    uint8_t rollback_step = 0;
    
    /* inode 位图找到空位 */
    int32_t inode_no = inode_bitmap_alloc(__cur_part);
    if(inode_no == -1) {
        printk("in file_creat: allocate inode failed\n");
        return -1;
    }
    
    struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
    if(new_file_inode == NULL) {
        printk("file_create: sys_malloc for inode failded\n");
        rollback_step = 1;
        goto rollback;
    }
    inode_init(inode_no, new_file_inode);

    /* 文件表中找到空位 */
    int fd_idx = get_free_slot_in_global();
    if(fd_idx == -1) {
        printk("exceed max open files\n");
        rollback_step = 2;
        goto rollback;
    }

    __file_table[fd_idx].fd_inode = new_file_inode;
    __file_table[fd_idx].fd_offset = 0;
    __file_table[fd_idx].fd_flag = flags;
    __file_table[fd_idx].fd_inode->i_write = false;

    /* 创建目录项 */
    struct dentry new_dentry;
    memset(&new_dentry, 0, sizeof(struct dentry));
    dentry_create(filename, inode_no, FT_REGULAR, &new_dentry);
    
    /* 同步数据到硬盘，在 parent_dir 中安装目录项 new_dentry */
    uint8_t* io_buf = (uint8_t*)sys_malloc(1024);
    if(!dentry_sync(parent_dir, &new_dentry, io_buf)) {
        printk ("sync dir_entry to disk failed\n");
        rollback_step = 3;
        goto rollback;
    }
    
    /* 同步父目录的 inode */
    memset(io_buf, 0, 1024);
    inode_sync(__cur_part, parent_dir->d_inode, io_buf);

    /* 新创建的 inode */
    memset(io_buf, 0, 1024);
    inode_sync(__cur_part, new_file_inode, io_buf);
    
    /* inode 位图信息 */
    bitmap_sync(__cur_part, inode_no, INODE_BITMAP);
    
    /* 将创建的文件 inode 添加到缓冲队列 */
    list_push_back(&__cur_part->open_inodes, &new_file_inode->i_tag);
    new_file_inode->i_count = 1;

    sys_free(io_buf);
    return pcb_fd_install(fd_idx);

/* 回滚 */
rollback:
    switch (rollback_step) {
        case 3: {
            memset(&__file_table[fd_idx], 0, sizeof(struct file));
        }
        case 2: {
            sys_free(new_file_inode);
        }
        case 1: {
            bitmap_set(&__cur_part->inode_btmp, inode_no, 0);
            break;
        }
        default: {
            break;
        }
    }
    sys_free(io_buf);
    return -1;
}

/* 把buf 中的count 个字节写入file, 成功则返回写入的字节数，失败则返回 -1 */
ssize_t file_write(struct file* file, const void* buf, size_t count) {
    if((file->fd_inode->i_size + count) > (BLOCK_SIZE * 140)) {
        printk("exceed max file_size 71680 bytes, write file failed\n");
        return -1;
    }   
    uint8_t* io_buf = sys_malloc(SECTOR_SIZE);
    if(io_buf == NULL) {
        printk("file_write: sys_malloc for io_buf failed\n");
        return -1;
    }
    uint32_t* all_bcks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
    if(all_bcks == NULL) {
        printk("file_write: sys_malloc for all_blocks failed\n");
        return -1;
    }

    int32_t bck_lba = -1; /* 块lba地址 */
    uint32_t bck_idx = 0; /* 块索引 */
    /* 如果是第一次写文件，先分配一个块 */
    if(file->fd_inode->i_sectors[0] == 0) {
        bck_lba = block_bitmap_alloc(__cur_part);
        if(bck_lba == 1) {
            printk("file_writ-e: block_bi tmap_alloc failed\n");
            return -1;
        }
        file->fd_inode->i_sectors[0] = bck_lba;
        bitmap_sync(__cur_part, bck_lba - __cur_part->sb->data_lba_start, BLOCK_BITMAP);
    }
    /* 文件写入之前已占据的块数 */
    uint32_t file_used_bcks = file->fd_inode->i_size / BLOCK_SIZE + 1;
    uint32_t file_will_use_bcks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
    ASSERT(file_will_use_bcks <= 140);
    uint32_t add_bcks = file_will_use_bcks - file_used_bcks; /* 需要增加的块 */

    if(add_bcks == 0) { /* 不需要增加块 */
        if(file_will_use_bcks < 13) { /* 数据量在 12 块之内(0-11） */
            all_bcks[file_used_bcks - 1] = file->fd_inode->i_sectors[file_used_bcks - 1];
        } else {
            /* 读入间接块地址 */
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            ide_read(__cur_part->my_disk, file->fd_inode->i_sectors[12], all_bcks + 12, 1);
        }
    } else { /* 需要增加块 */
        if(file_will_use_bcks < 13) {
            /* 情况1：12个直接块够用 */
            bck_idx = file_used_bcks - 1;
            *(all_bcks + bck_idx) = *(file->fd_inode->i_sectors + bck_idx);
            
            bck_idx = file_used_bcks;
            while(bck_idx < file_will_use_bcks) {
                bck_lba = block_bitmap_alloc(__cur_part);
                if(bck_lba == -1) {
                    printk("file_write: block_bitmap alloc for situation 1 failed\n");
                    return -1;
                }
                /* 确保扇区地址尚未分配 */
                ASSERT(file->fd_inode->i_sectors[bck_idx] == 0);
                file->fd_inode->i_sectors[bck_idx] = all_bcks[bck_idx] = bck_lba;
                bitmap_sync(__cur_part, bck_lba - __cur_part->sb->data_lba_start, BLOCK_BITMAP);
                bck_idx++;
            }
        } else if(file_used_bcks < 13 && file_will_use_bcks > 12) {
            /* 情况2：旧数据在 12 个直接块以内，新数据将使用间接块 */
            bck_idx = file_used_bcks - 1;
            all_bcks[bck_idx] = file->fd_inode->i_sectors[bck_idx];
            /* 创建 1 级间接块 */
            bck_lba = block_bitmap_alloc(__cur_part);
            if(bck_lba == -1) {
                printk ("file_write: block bitmap_alloc for situation 2 failed\n");
                return -1;
            }
            ASSERT(file->fd_inode->i_sectors[12] == 0);
            file->fd_inode->i_sectors[12] = bck_lba;
            
            bck_idx = file_used_bcks;
            while(bck_idx < file_will_use_bcks) {
                bck_lba = block_bitmap_alloc(__cur_part);
                if(bck_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 2 failed\n");
                    return -1;
                }
                if(bck_idx < 12) {
                    ASSERT(file->fd_inode->i_sectors[bck_idx] == 0);
                    file->fd_inode->i_sectors[bck_idx] = bck_lba;
                }
                all_bcks[bck_idx] = bck_lba;
                bitmap_sync(__cur_part, bck_lba - __cur_part->sb->data_lba_start, BLOCK_BITMAP);
                bck_idx++;
            }
            ide_write(__cur_part->my_disk, file->fd_inode->i_sectors[12], all_bcks + 12, 1);
        } else if(file_used_bcks > 12) {
            /* 情况3：旧数据已经使用过间接块 */
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            /* 读入所有间接块内容到 all_bcks */
            ide_read(__cur_part->my_disk, file->fd_inode->i_sectors[12], all_bcks + 12, 1);
            bck_idx = file_used_bcks;
            while(bck_idx < file_will_use_bcks) {
                bck_lba = block_bitmap_alloc(__cur_part);
                if(bck_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 3 failed\n");
                    return -1;
                }
                all_bcks[bck_idx] = bck_lba;
                bitmap_sync(__cur_part, bck_lba - __cur_part->sb->data_lba_start, BLOCK_BITMAP);
                bck_idx++;
            }
            ide_write(__cur_part->my_disk, file->fd_inode->i_sectors[12], all_bcks + 12, 1);
        }
    }
    
    /* 写入数据 */
    bool first_write_bck = true; /* 剩余空间块标识 */
    
    uint32_t bytes_written = 0; /* 已写入数据大小 */
    uint32_t bytes_rest = count; /* 剩余数据大小 */
    uint32_t sec_lba; /* 扇区地址 */
    uint32_t sec_bytes_off; /* 扇区内字节偏移量 */
    uint32_t sec_bytes_rest; /* 扇区内剩余字节量 */
    uint32_t chunk_size; /* 每次写入硬盘的数据块的大小 */

    uint8_t* src = (uint8_t*)buf;
    while(bytes_written < count) {
        memset(io_buf, 0, BLOCK_SIZE);
        sec_lba = all_bcks[file->fd_inode->i_size / BLOCK_SIZE];
        sec_bytes_off = file->fd_inode->i_size % BLOCK_SIZE;
        sec_bytes_rest = BLOCK_SIZE - sec_bytes_off;

        chunk_size = bytes_rest < sec_bytes_rest ? bytes_rest : sec_bytes_rest;
        if(first_write_bck) {
            ide_read(__cur_part->my_disk, sec_lba, io_buf, 1);
            first_write_bck = false;
        }
        memcpy(io_buf + sec_bytes_off, src, chunk_size);
        ide_write(__cur_part->my_disk, sec_lba, io_buf, 1);
        printk("file write at lba 0x%x\n", sec_lba); // 调试，完成后去掉

        src += chunk_size;
        file->fd_inode->i_size += chunk_size;
        file->fd_offset += chunk_size;
        
        bytes_written += chunk_size;
        bytes_rest -= chunk_size;
    }
    inode_sync(__cur_part, file->fd_inode, io_buf);
    
    sys_free(all_bcks);
    sys_free(io_buf);
    return -1;
}

/* 从 file 连续读取 count 个字节到 buf, 成功则返回读到的字节数，失败则返回 -1 */
ssize_t file_read(struct file* file, void* buf, size_t count) {

}

/* 成功打开或创建文件后，返回文件描述符，否则返回 -1 */
int32_t sys_open(const char* pathname, uint8_t flags) {
    if('/' == pathname[strlen(pathname) - 1]) { /* 目录 */
        printk("can't open a directory %s\n", pathname);
        return -1;
    }
    ASSERT(flags <= 7);
    int32_t fd = -1;

    /*  1. 检查文件是否存在，如果中间有目录未找到则报错；
        2. 在最后一级：
            如果没有找到文件，O_CREAT的情况下则创建文件，其他情况报错 
            如果找到文件，O_CREAT的情况下报错，其他情况打开文件
    */
    
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    
    int inode_no = file_search(pathname, &searched_record);
    bool found = inode_no != -1 ? true : false;
    if(FT_DIRECTORY == searched_record.p_ftype) {
        printk("can't open a directory with open(), user opendir() to instead\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }
    
    uint32_t pathname_depth = path_depth((char*)pathname);
    uint32_t path_searched_depth = path_depth(searched_record.searched_path);
    if(pathname_depth != path_searched_depth) {
        /* 某个中间目录不存在 */
        printk("cannot access %s: Not a directory, subpath %s is't exist\n", 
            pathname, searched_record.searched_path);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    if(found && (flags & O_CREAT)) { /* 需要创建的文件已存在 */
        printk("'%s' is already exist!\n", pathname);
        dir_close(searched_record.parent_dir) ;
        return -1;
    } else if(!found && !(flags & O_CREAT)) { /* 没有该文件且不创建 */
        printk ("in path %s, file %s is't exist\n",
            searched_record.searched_path,
            (strrchr(searched_record.searched_path,'/') + 1));
        dir_close(searched_record.parent_dir);
        return -1;
    }

    switch (flags) {
        case O_CREAT: {
            printk("creating file '%s' \n", pathname);
            fd = file_create(searched_record.parent_dir, (strrchr(pathname,'/') + 1), flags);
            dir_close(searched_record.parent_dir);
        }
        default: { /* O_RDONLY O_WRPNLY O_RDWR */
            fd = file_open(inode_no, flags);
            break;
        }
    }
    return fd;
}

/* 将文件描述符转化为文件表的下标 */
static uint32_t fd_local2global(uint32_t localfd) {
    struct task_struct* cur_thread = thread_running();
    int32_t globalfd = cur_thread->fd_table[localfd];
    ASSERT(globalfd >= 0 && globalfd < MAX_FILE_OPEN);
    return (uint32_t)globalfd;
}

/* 关闭文件描述符 fd 指向的文件，成功返回 0，否则返回 -1 */
int32_t sys_close(int32_t fd) {
    int32_t ret = -1;
    if(fd > 2) {
        uint32_t gfd = fd_local2global(fd);
        ret = file_close(__file_table + gfd);
        thread_running()->fd_table[fd] = -1;
    }
    return ret;
}

/* 将 buf 中连续 count 字节写入 fd，成功返回写入的字节数，失败返回 -1 */
ssize_t sys_write(int fd, const void* buf, size_t count) {
    if(fd < 0) {
        printk("sys_write: fd %d error\n", fd);
        return -1;
    }
    if(fd == stdout_no) {
        char* str = (char*)buf;
        console_put_str(str);
        return count;
    }
    /* 通过 fd 找到全局文件表中的文件 */
    int gfd = fd_local2global(fd);
    struct file* wr_file = __file_table + gfd;
    if(O_RDWR & wr_file->fd_flag || O_WRONLY & wr_file->fd_flag) {
        return file_write(wr_file, buf, count);
    } else {
        console_put_str("sys_write: not allowed to write file without flag O_RDWR or O_WRONLY\n") ;
        return -1;
    }
}

/* 从 buf 中连续读入 count 字节到 fd，成功返回读入的字节数，失败返回 -1 */
ssize_t sys_read(int fd, void* buf, size_t count) {
    ASSERT(buf != NULL);
    int32_t ret = -1;
    if(fd < 0 || stdout_no == fd || stderr_no == fd) {
        printk ("sys_read: fd error\n");
    } else if(stdin_no == fd) {
        char* buffer = (char*)buf;
        uint32_t bytes_read = 0;
        while(bytes_read < count) {
            *buffer++ = ioq_getchar(&__kbd_buf);
            bytes_read++;
        }
        ret = bytes_read == 0 ? -1 : (ssize_t)bytes_read;
    } else {
        ret = file_read(__file_table + fd_local2global(fd), buf, count);
    }
    return ret;
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

    /* open root directory */
    root_dir_open(__cur_part);
    /* init file table */
    uint32_t fd_idx = 0;
    while(fd_idx < MAX_FILE_OPEN) {
        __file_table[fd_idx++].fd_inode = NULL;
    }
}

/* 获取父目录 inode 编号 */
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr, void* io_buf) {
    struct inode* child_dir_inode = inode_open(__cur_part, child_inode_nr);
    uint32_t bck_lba = child_dir_inode->i_sectors[0];
    inode_close(child_dir_inode);
    ide_read(__cur_part->my_disk, bck_lba, io_buf, 1);

    /* 第 0 个目录项为 '.'，第 2 个目录项为 '..' */
    struct dentry* dentry = ((struct dentry*)io_buf);
    ASSERT(dentry[1].d_inode < 4096 && dentry[1].d_ftype == FT_DIRECTORY);
    return dentry[1].d_inode;
}

/* 在inode 编号为 parent_inode_nr 的目录下查找 inode 编号为 child_inode_nr 的子目录名，将名称存入 name，成功返回 0，失败返回 -1 */
static int get_child_dir_name(uint32_t parent_inode_nr, uint32_t child_inode_nr, char* name, void* io_buf) {
    struct inode* parent_dir_inode = inode_open(__cur_part, parent_inode_nr);
    uint32_t all_bcks[140] = {0};
    uint8_t bck_idx = 0;
    uint8_t bck_cnt = 12;
    while(bck_idx < 12) {
        all_bcks[bck_idx] = parent_dir_inode->i_sectors[bck_idx];
        bck_idx++;
    }
    if(parent_dir_inode->i_sectors[12] != 0) {
        ide_read(__cur_part->my_disk, parent_dir_inode->i_sectors[12], all_bcks + 12, 1);
        bck_cnt = 140;
    }
    inode_close(parent_dir_inode);
    
    bck_idx = 0;
    struct dentry* dentry = (struct dentry*)io_buf;
    uint32_t dentry_size_per_bck = (BLOCK_SIZE / __cur_part->sb->s_dentry_size);
    while(bck_idx < bck_cnt) {
        if(all_bcks[bck_idx] != 0) {
            ide_read(__cur_part->my_disk, all_bcks[bck_idx], io_buf, 1);
            uint32_t dentry_idx = 0;
            while(dentry_idx < dentry_size_per_bck) {
                // printk("%d %d %s\n", bck_idx, dentry->d_inode, dentry->d_filename);
                if(dentry->d_inode == child_inode_nr) {
                    strcat(name, "/");
                    strcat(name, dentry->d_filename);
                    return 0;
                }
                dentry++;
                dentry_idx++;
            }
        }
        bck_idx++;
    }
    return -1;
}

/* 创建目录 pathname，成功返回 0，失败返回 -1 */
int sys_mkdir(const char* pathname) {
    /*  1. 确认待创建的目录是否存在；
        2. 为新目录创建 inode；
        3. 为新目录分配 1 个块存储该目录中的目录项；
        4. 为新目录中创建两个目录项 '.' 和 '..' 这是每个目录都存在的目录项；
        5. 在新目录的父目录中添加新目录的目录项；
        6. 将以上资源变更同步到硬盘；
    */
    uint8_t rollback_step = 0; /* 用于失败时的回滚操作 */
    void* io_buf = sys_malloc(SECTOR_SIZE * 2);
    if(NULL == io_buf) {
        printk("sys_mkdir : sys_malloc for io_buf failed\n");
        return -1;
    }

    struct path_search_record searched_record;
    bzero(&searched_record, sizeof(struct path_search_record));
    int inode_no = file_search(pathname, &searched_record);
    if(inode_no != -1) { /* 存在同名目录或文件 */
        printk("sys mkdir: file or directory %s exist!\n", pathname);
        rollback_step = 1;
        goto rollback;
    } else {
        /* 判断待创建的目录的上级目录都存在 */
        uint32_t d1 = path_depth((char*)pathname);
        printk("%d\n", d1);
        uint32_t d2 = path_depth(searched_record.searched_path);
        printk("%d\n", d2);
        if(d1 != d2) { 
            /* 中间有目录不存在 */
            printk("sys_mkdir : cannot access %s: Not a directory, subpath %s is't exist\n", pathname, searched_record.searched_path);
            rollback_step = 1;
            goto rollback;
        }
        
        /* 以下开始创建目录 */
        /* 先获取最后一级，即创建目录的名字 */
        char* dirname = strrchr(searched_record.searched_path, '/') + 1;

        inode_no = inode_bitmap_alloc(__cur_part);
        if(-1 == inode_no) {
            printk("sys_mkdir: allocate inode failed\n");
            rollback_step = 1;
            goto rollback;
        }

        struct inode new_dir_inode;
        inode_init(inode_no, &new_dir_inode);

        /* 分配一个块用于写入 '.' 和 "." */
        int bck_lba = block_bitmap_alloc(__cur_part);
        if(-1 == bck_lba) {
            printk("sys_mkdir: block bitmap_alloc for create directory failed\n");
            rollback_step = 2;
            goto rollback;
        }
        /* 同步位图到硬盘 */
        new_dir_inode.i_sectors[0] = bck_lba;
        bitmap_sync(__cur_part, bck_lba - __cur_part->sb->data_lba_start, BLOCK_BITMAP);
        /* 将 '.' 和 ".." 写入目录 */
        bzero(io_buf, SECTOR_SIZE * 2);
        struct dentry* dentry = (struct dentry*)io_buf;
        /* 初始化 '.' */
        strcpy(dentry->d_filename, ".");
        dentry->d_ftype = FT_DIRECTORY;
        dentry->d_inode = inode_no;
        /* 初始化 '..' */
        dentry++;
        strcpy(dentry->d_filename, "..");
        dentry->d_ftype = FT_DIRECTORY;
        dentry->d_inode = searched_record.parent_dir->d_inode->i_no;
        
        ide_write(__cur_part->my_disk, bck_lba, io_buf, 1);
        new_dir_inode.i_size = __cur_part->sb->s_dentry_size * 2;

        /* 在父目录中添加自己的目录项 */
        struct dentry new_dentry;
        bzero(&new_dentry, sizeof(struct dentry));
        dentry_create(dirname, inode_no, FT_DIRECTORY, &new_dentry);
        bzero(io_buf, SECTOR_SIZE * 2);
        if(false == dentry_sync(searched_record.parent_dir, &new_dentry, io_buf)) {
            printk("sys_mkdir: sync_dir_entry to disk failed!\n") ;
            rollback_step = 2;
            goto rollback;
        }
        /* 将父目录 inode 同步到硬盘 */
        bzero(io_buf, SECTOR_SIZE * 2);
        inode_sync(__cur_part, searched_record.parent_dir->d_inode, io_buf);
        /* 同步新 inode 到硬盘 */
        bzero(io_buf, SECTOR_SIZE * 2);
        inode_sync(__cur_part, &new_dir_inode, io_buf);
        /* 同步 inode 位图*/
        bitmap_sync(__cur_part, inode_no, INODE_BITMAP);
    }
    dir_close(searched_record.parent_dir);
    sys_free(io_buf);
    printk("sys_mkdir: create %s success\n", pathname) ;
    return 0;

/* 失败返回前的回滚 */
rollback:
    switch (rollback_step) {
        case 2: {
            /* inode 位图恢复 */
            bitmap_set(&__cur_part->inode_btmp, inode_no, 0);
        }
        case 1: {
            dir_close(searched_record.parent_dir);
            break;
        }
    }
    sys_free(io_buf);
    return -1;
}

/* 删除空目录，成功返回 0，失败返回 -1 */
int sys_rmdir(const char* pathname) {
    /* 查找待删除文件 */
    struct path_search_record searched_record;
    bzero(&searched_record, sizeof(struct path_search_record));
    int inode_no = file_search(pathname, &searched_record);
    int ret = -1;
    if(-1 == inode_no) {
        printk("In %s, sub path %s not exist\n", pathname, searched_record.searched_path);
    } else {
        switch (searched_record.p_ftype) {
            case FT_REGULAR: {
                printk("%s is regular file!\n", pathname);
                break;
            }
            case FT_DIRECTORY: {
                struct dir* dir = dir_open(__cur_part, inode_no);
                if(dir_is_empty(dir) == 0) {
                    printk("dir %s is not empty, it is not allowed to delete a nonempty directory!\n", pathname);
                } else {
                    if(dir_remove(searched_record.parent_dir, dir) == 0) {
                        ret = 0;
                    }
                }
                dir_close(dir);
                break;
            }
            default: {
                break;
            }
        }
    }
    dir_close(searched_record.parent_dir);
    return ret;
}

/* 把当前工作路径绝对路径写入 buf，size 是 buf 的大小，当 buf 为NULL时，由操作系统分配空间，失效返回 NULL */
char* sys_getcwd(char* buf, size_t size) {
    ASSERT(buf != NULL);

    void* io_buf = sys_malloc(SECTOR_SIZE);
    if(io_buf == NULL) {
        return NULL;
    }

    int32_t child_inode_nr = thread_running()->cwd_inode_nr;
    ASSERT(child_inode_nr >= 0 && child_inode_nr < 4096) ;
    bzero(buf, size);
    if(child_inode_nr == 0) { /* 根目录 */
        *buf = '/';
        return buf;
    }
    /* 从下往上找服务路，直到根目录 */
    char rpath[MAX_PATH_LEN] = {0}; /* reverse path be like '/a/bc', the right order is '/bc/a  */
    int32_t parent_inode_nr;
    while(child_inode_nr != 0) {
        parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr, io_buf);
        // printk("%d %d\n", child_inode_nr, parent_inode_nr);
        if(-1 == get_child_dir_name(parent_inode_nr, child_inode_nr, rpath, io_buf)) {
            sys_free(io_buf);
            return NULL;
        }
        child_inode_nr = parent_inode_nr;
    }
    // printk("%s\n", rpath);
    /* 反转路径 ： 每次找到最后一个 '/', 将后面的字符串拷贝到缓冲区，并将 '/'的位置置为 '\0'，直到找不到 '/' */
    char* rslash = NULL;
    while((rslash = strrchr(rpath, '/')) != NULL) {
        strcat(buf, rslash);
        *rslash = '\0';
    }

    sys_free(io_buf);
    return buf;
} 

/* 更改当前工作目录为绝对路径 path，成功返回 0，失败返回 -1 */
int sys_chdir(const char* path) {
    int ret = -1;
    struct path_search_record searched_record;
    bzero(&searched_record, sizeof(struct path_search_record));
    int inode_no = file_search(path, &searched_record);
    if(inode_no != -1) {
        if(FT_DIRECTORY == searched_record.p_ftype) {
            thread_running()->cwd_inode_nr = inode_no;
            ret = 0;
        } else {
            printk("sys_chdir : %s is regular file or other!\n", path);
        }
    }
    dir_close(searched_record.parent_dir);
    return ret;
}

/* 在 stat 中填充 pathname 的文件属性，成功返回 0，失败返回 -1 */
int sys_stat(const char* pathname, struct stat* buf) {
    /* 根目录 */
    if(1) {
        buf->st_inode_no = 0;
        buf->st_ftype = FT_DIRECTORY;
        buf->st_size = __root_dir.d_inode->i_size;
        return 0;
    }

    /* 找到文件 inode 号，获取对应信息 */
    struct path_search_record searched_record;
    bzero(&searched_record, sizeof(struct path_search_record));
    int inode_no = file_search(pathname, &searched_record);
    int ret = -1;
    if(-1 == inode_no) {
        printk("sys_stat : %s not found\n", pathname);
    } else {
        struct inode* inode = inode_open(__cur_part, inode_no);
        buf->st_size = inode->i_size;
        inode_close(inode);

        buf->st_ftype = searched_record.p_ftype;
        buf->st_inode_no = inode_no;
        ret = 0;
    }
    dir_close(searched_record.parent_dir);
    return ret;
}
