#include "dir.h"

extern struct partition* __cur_part; /* 当前操作分区（全局变量） */

struct dir __root_dir; /* 根目录 */

/* 打开根目录 */
void root_dir_open(struct partition* part) {
    __root_dir.d_inode = inode_open(part, part->sb->root_inode_no);
    __root_dir.d_offset = 0;
}

/* 是否为根目录 */
bool is_root_dir(const char* pathname) {
    return (0 == strcmp("/", pathname) || 0 == strcmp("/.", pathname) || 0 == strcmp("/..", pathname));
}

/* 在分区 part 上打开 inode 节点为 inode_no 的目录并返回目录指针 */ 
struct dir* dir_open(struct partition* part, uint32_t inode_no) {
    struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
    pdir->d_inode = inode_open(part, inode_no);
    pdir->d_offset = 0;
    return pdir;
}

/* 在 part 分区的 pdir 目录内寻找 name 的文件或目录，找到则返回 true 并将目录项存入 dir_entry 反之返回false */
bool dentry_search(struct partition* part, struct dir* pdir, const char* name, struct dentry* dir_entry) {
    uint32_t bck_cnt = 140; /* 12 个直接块 + 128 个一级间接块 */
    /* 存放 pdir 文件或目录的所有扇区地址 */
    uint32_t* all_bcks = (uint32_t*)sys_malloc(48 + 512);
    if(all_bcks == NULL) {
        printk("search_dir_entry: sys_malloc for all_blocks failed");
        return false;
    }

    uint32_t bck_idx = 0;
    while(bck_idx < 12) {
        all_bcks[bck_idx] = pdir->d_inode->i_sectors[bck_idx];
        bck_idx++;
    }
    
    if(pdir->d_inode->i_sectors[12] != 0) {
        ide_read(part->my_disk, pdir->d_inode->i_sectors[12], all_bcks + 12, 1);
    }
    
    struct dentry* dentry = (struct dentry*)sys_malloc(SECTOR_SIZE);
    uint32_t dentry_size = part->sb->s_dentry_size;
    uint32_t dentry_cnt = SECTOR_SIZE / dentry_size;

    /* 查找目录项 */
    bck_idx = 0;
    while(bck_idx < bck_cnt) {
        if(all_bcks[bck_idx] != 0) {
            ide_read(part->my_disk, all_bcks[bck_idx], dentry, 1);
            uint32_t dentry_idx = 0;
            while(dentry_idx < dentry_cnt) {
                if(strcmp(name, (dentry + dentry_idx)->d_filename) == 0) {
                    memcpy(dir_entry, (dentry + dentry_idx), dentry_size);
                    sys_free(dentry);
                    sys_free(all_bcks);
                    return true;
                }
                dentry_idx++;
            }
            memset(dentry, 0, SECTOR_SIZE);
        }   
        bck_idx++;
    }

    sys_free(dentry);
    sys_free(all_bcks);
    return false;
}

/* 关闭目录 */
void dir_close(struct dir* dir) {
    /*  根目录不能关闭的原因 ：
        1. 关闭后需要打开的话必须调用 root_dir_open; 
        2. root_dir 所在的内存是低端 1MB 之内（内核），不在堆中，free 会出现问题；
    */
    if(dir == &__root_dir) {
        return;
    }
    inode_close(dir->d_inode);
    sys_free(dir);
}

/* 在内存中初始化目录项 dentry */
void dentry_create(char* filename, uint32_t inode_no, uint8_t ftype, struct dentry* dentry) {
    ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);

    memcpy(dentry->d_filename, filename, strlen(filename));
    dentry->d_inode_no = inode_no;
    dentry->d_ftype = ftype;
}

/* 将目录项 dentry 写入父目录 parent_dir 中，io_buf 由主调函数提供 */
bool dentry_sync(struct dir* parent_dir, struct dentry* dentry,void* io_buf) {
    ASSERT(parent_dir != NULL);
    ASSERT(dentry != NULL);
    ASSERT(io_buf != NULL);

    uint8_t bck_idx = 0;
    uint32_t all_bcks[140] = {0};
    while(bck_idx < 12) {
        all_bcks[bck_idx] = parent_dir->d_inode->i_sectors[bck_idx];
        bck_idx++;
    }

    struct dentry* dentry_ptr = (struct dentry*)io_buf;
    int32_t bck_btmp_idx = -1;
    int32_t bck_lba = -1;
    bck_idx = 0;
    while(bck_idx < 140) {
        bck_btmp_idx = -1;
        if(all_bcks[bck_idx] == 0) {
            bck_lba = block_bitmap_alloc(__cur_part);
            if(bck_lba == -1) {
                printk("alloc block bitmap for sync dir_entry failed \n");
                return false;
            }

            bck_btmp_idx = bck_lba - __cur_part->sb->data_lba_start;
            ASSERT(bck_btmp_idx != -1);
            bitmap_sync(__cur_part, bck_btmp_idx, BLOCK_BITMAP);

            bck_btmp_idx = -1;
            if(bck_idx < 12) { /* 直接块 */
                parent_dir->d_inode->i_sectors[bck_idx] = all_bcks[bck_idx] = bck_lba;
            } else if(bck_idx == 12) { /* 一级间接块 */
                /* 该 bck_lba 作为一级间接块表的地址 */
                parent_dir->d_inode->i_sectors[bck_idx] = bck_lba;
                /* 再分配一个作为间接块 0 号 */
                bck_lba = block_bitmap_alloc(__cur_part);
                if(bck_lba == -1) {
                    bck_btmp_idx = parent_dir->d_inode->i_sectors[bck_idx] - __cur_part->sb->data_lba_start;
                    bitmap_set(&__cur_part->bck_btmp, bck_btmp_idx, 0);
                    parent_dir->d_inode->i_sectors[bck_idx] = 0;
                    printk("alloc block bitmap for sync_dir_entry failed\n ") ;
                    return false;
                } else {
                    bck_btmp_idx = bck_lba - __cur_part->sb->data_lba_start;
                    ASSERT(bck_btmp_idx != -1);
                    bitmap_sync(__cur_part, bck_btmp_idx, BLOCK_BITMAP);

                    all_bcks[bck_idx] = bck_lba;
                    ide_write(__cur_part->my_disk, parent_dir->d_inode->i_sectors[bck_idx], all_bcks + bck_idx, 1);
                }
            } else {
                all_bcks[bck_idx] = bck_lba;
                ide_write(__cur_part->my_disk, parent_dir->d_inode->i_sectors[12], all_bcks + 12, 1);
            }
            memset(io_buf, 0, SECTOR_SIZE);
            memcpy(io_buf, dentry, sizeof(struct dentry));
            ide_write(__cur_part->my_disk, all_bcks[bck_idx], io_buf, 1);
            parent_dir->d_inode->i_size += sizeof(struct dentry);
            
            // printk("%d: %d + %d\n", parent_dir->d_inode->i_no, parent_dir->d_inode->i_size, sizeof(struct dentry));
            return true;
        } else {
            /* 该 block 已存在，直接读入内存，然后在里面寻找空目录项 */
            ide_read(__cur_part->my_disk, all_bcks[bck_idx], io_buf, 1);
            uint8_t dentry_idx = 0;
            while(dentry_idx < (SECTOR_SIZE / sizeof(struct dentry))) {
                if(FT_UNKNOWN == (dentry_ptr + dentry_idx)->d_ftype) {
                    memcpy(dentry_ptr + dentry_idx, dentry, sizeof(struct dentry));
                    ide_write(__cur_part->my_disk, all_bcks[bck_idx], io_buf, 1);
                    parent_dir->d_inode->i_size += sizeof(struct dentry);

                    // printk("%d: %d + %d\n", parent_dir->d_inode->i_no, parent_dir->d_inode->i_size, sizeof(struct dentry));
                    return true;
                }
                dentry_idx++;
            }
        }
        bck_idx++;
    }
    printk("directory is full!\n");
    return false;
}

/* 读取一个目录项，成功返回 1 个目录项，失败返回 NULL */
struct dentry* dir_read(struct dir* dir) {
    struct inode* dinode = dir->d_inode;
    struct dentry* dentry_table = (struct dentry*)dir->d_buf;

    uint32_t bck_idx = 0;
    uint32_t bck_cnt = 12;
    uint32_t all_bcks[140] = {0};
    while(bck_idx < 12) {
        all_bcks[bck_idx] = dinode->i_sectors[bck_idx];
        bck_idx++;
    }
    if(dinode->i_sectors[12] != 0) {
        ide_read(__cur_part->my_disk, dinode->i_sectors[12], all_bcks + 12, 1);
        bck_cnt = 140;
    }

    uint32_t cur_dentry_pos = 0;
    uint32_t dentry_idx = 0;
    uint32_t dentry_size = __cur_part->sb->s_dentry_size;
    uint32_t dentry_per_sec = SECTOR_SIZE / dentry_size;
    /* 遍历目录 */
    bck_idx = 0;
    while(dir->d_offset < dinode->i_size) {
        if(dir->d_offset >= dinode->i_size) {
            return NULL;
        }

        if(all_bcks[bck_idx] != 0) {
            bzero(dentry_table, SECTOR_SIZE);
            ide_read(__cur_part->my_disk, all_bcks[bck_idx], dentry_table, 1);
            /* 遍历每个扇区的目录项 */
            dentry_idx = 0;
            while(dentry_idx < dentry_per_sec) {
                if((dentry_table + dentry_idx)->d_ftype != FT_UNKNOWN) {
                    if(cur_dentry_pos < dir->d_offset) {
                        cur_dentry_pos += dentry_size;
                        dentry_idx++;
                        continue;
                    }
                    ASSERT(cur_dentry_pos == dir->d_offset);
                    dir->d_offset += dentry_size; /* 下一个返回的目录项地址 */
                    return dentry_table + dentry_idx;
                }
                dentry_idx++;
            }
        }
        bck_idx++;
    }
    return NULL;
}

/* 删除 parent_dir 中的 inode_no 对应的目录项 */
bool dentry_delete(struct partition* part, struct dir* parent_dir, uint32_t inode_no, void* io_buf) {
    ASSERT(io_buf != NULL);

    uint8_t bck_idx = 0;
    uint8_t bck_cnt = 12;
    uint32_t all_bcks[140] = {0};
    while(bck_idx < 12) {
        all_bcks[bck_idx] = parent_dir->d_inode->i_sectors[bck_idx];
        bck_idx++;
    }
    if(parent_dir->d_inode->i_sectors[12] != 0) {
        ide_read(__cur_part->my_disk, parent_dir->d_inode->i_sectors[12], all_bcks + 12, 1);
        bck_cnt = 140;
    }
    bool is_dir_first_bck = false;
    struct dentry* dentry_table = (struct dentry*)io_buf;
    struct dentry* dentry_obj = NULL;
    uint32_t dentry_idx = 0;
    uint32_t dentry_cnt = 0;
    for(bck_idx = 0; bck_idx < bck_cnt; bck_idx++) {
        if(all_bcks[0] != 0) {
            /* 读取当前块（一个扇区）的内容 */
            bzero(io_buf, SECTOR_SIZE);
            ide_read(__cur_part->my_disk, all_bcks[0], io_buf, 1);

            for(dentry_idx = 0; dentry_idx < (SECTOR_SIZE / sizeof(struct dentry)); dentry_idx++) {
                if(FT_UNKNOWN != (dentry_table + dentry_idx)->d_ftype) {
                    if(0 == strcmp(".", (dentry_table + dentry_idx)->d_filename)) {
                        is_dir_first_bck = true;
                    } else if (strcmp(".", (dentry_table + dentry_idx)->d_filename) != 0
                        && strcmp("..", (dentry_table + dentry_idx)->d_filename) != 0) {
                        dentry_cnt++;
                        if((dentry_table + dentry_idx)->d_inode_no == inode_no) {
                            ASSERT(dentry_obj == NULL);
                            dentry_obj = (dentry_table + dentry_idx);
                        }
                    }
                }
            }   

            if(NULL == dentry_obj) {/* 去下一个扇区继续寻找目录项 */
                continue;
            }

            /* 找到目录项后，清除该目录项，并判断是否回收该扇区 */
            ASSERT(dentry_cnt >= 1);

            if(1 == dentry_cnt && !is_dir_first_bck) {
                uint32_t bck_btmp_idx = all_bcks[bck_idx] - part->sb->data_lba_start;
                bitmap_set(&part->bck_btmp, bck_btmp_idx, BLOCK_BITMAP);
                if(bck_idx < 12) {
                    parent_dir->d_inode->i_sectors[bck_idx] = 0;
                } else {
                    uint32_t ext_bck_cnt = 0;
                    uint32_t ext_bck_idx = 12;
                    while(ext_bck_idx < 140) {
                        if(all_bcks[ext_bck_idx++] != 0) {
                            ext_bck_cnt++;
                        }
                    }
                    ASSERT(ext_bck_cnt >= 1);

                    if(ext_bck_cnt > 1) {
                        /* 包含多个间接块，仅擦除当前块 */
                        all_bcks[bck_idx] = 0;
                        ide_write(part->my_disk, parent_dir->d_inode->i_sectors[12], all_bcks + 12, 1);
                    } else {
                        /* 仅包含一个间接块，擦除当前块，且擦除间接索引表地址 */
                        bck_btmp_idx = parent_dir->d_inode->i_sectors[12] - part->sb->data_lba_start;
                        bitmap_set(&part->bck_btmp, bck_btmp_idx, 0);
                        bitmap_sync(__cur_part, bck_btmp_idx, BLOCK_BITMAP);

                        parent_dir->d_inode->i_sectors[12] = 0;
                    }
                }
            } else {
                /* 仅清楚当前目录项 */
                bzero(dentry_obj, sizeof(struct dentry));
                ide_write(part->my_disk, all_bcks[bck_idx], io_buf, 1);
            }

            /* 更新到硬盘 */
            ASSERT(parent_dir->d_inode->i_size >= part->sb->s_dentry_size);
            parent_dir->d_inode->i_size -= part->sb->s_dentry_size;
            bzero(io_buf, SECTOR_SIZE * 2);
            inode_sync(part, parent_dir->d_inode, io_buf);

            return true;
        }
    }
    return false;
}

/* 判断目录是否为空(1 空 0 非空)： 仅含有 . 和 .. 两个目录（那么inode所指向的数据大小应该是两个目录项的大小） */
int dir_is_empty(struct dir* dir) {
    return (dir->d_inode->i_size == __cur_part->sb->s_dentry_size * 2 ? 1 : 0);
}

/* 在父目录 parent_dir 中删除 child_dir */
int dir_remove(struct dir* parent_dir, struct dir* child_dir) {
    //![1] 保证只有第 0 扇区之外的扇区没有数据
    uint8_t bck_idx = 1;
    while(bck_idx < 13) {
        ASSERT(child_dir->d_inode->i_sectors[bck_idx++] == 0);
    }//[1]
    
    /* 以下才是删除的相关代码 */
    void* io_buf = sys_malloc(SECTOR_SIZE * 2);
    if(NULL == io_buf) {
        printk("dir remove: malloc for io_buf failed\n");
        return -1;
    }

    dentry_delete(__cur_part, parent_dir, child_dir->d_inode->i_no, io_buf);
    inode_release(__cur_part, child_dir->d_inode->i_no);

    sys_free(io_buf);
    return 0;
}
