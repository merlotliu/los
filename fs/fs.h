#ifndef __FS_FS_H
#define __FS_FS_H

#define MAX_FILES_PER_PART  4096        /* 每个分区最大的文件数 */
#define BITS_PER_SECTOR     4096        /* 每扇区的位数 512*8 */
#define SECTOR_SIZE         512         /* 扇区字节大小 */
#define BLOCK_SIZE          SECTOR_SIZE /* 块字节大小 */

/* file type */
enum file_type {
    FT_UNKNOWN = 0, /* unknown file type */
    FT_REGULAR, /* regular file */
    FT_DIRECTORY /* directory */
};

/* 在磁盘上搜索文件系统，若没有则格式化分区创建文件系统 */
void filesys_init(void);

#endif /* __FS_FS_H */