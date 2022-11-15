#include "dir.h"

extern struct partition* __cur_part; /* 当前操作分区（全局变量） */

struct dir __root_dir; /* 根目录 */

/* 打开根目录 */
void root_dir_open(struct partition* part) {
    __root_dir.d_inode = inode_open(part, part->sb->root_inode_no);
    __root_dir.d_offset = 0;
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
    dentry->d_inode = inode_no;
    dentry->d_ftype = ftype;    
}

/* 将目录项 dentry 写入父目录 parent_dir 中，io_buf 由主调函数提供 */
bool dentry_sync(struct dir* parent_dir, struct dentry* dentry,void* io_buf) {
    
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
                    bitmap_set(&__cur_part->bck_btp, bck_btmp_idx, 0);
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
