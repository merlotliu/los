#ifndef __FS_FS_H
#define __FS_FS_H

#include "global.h"
#include "file.h"

#define MAX_FILES_PER_PART  4096        /* 每个分区最大的文件数 */
#define BITS_PER_SECTOR     4096        /* 每扇区的位数 512*8 */
#define SECTOR_SIZE         512         /* 扇区字节大小 */
#define BLOCK_SIZE          SECTOR_SIZE /* 块字节大小 */

#define MAX_PATH_LEN        512         /* 路径最大长度 */

/* file type */
enum file_type {
    FT_UNKNOWN = 0, /* unknown file type */
    FT_REGULAR, /* regular file */
    FT_DIRECTORY /* directory */
};

/* 打开文件选项 */
enum oflags {
    O_RDONLY, /* 只读 */
    O_WRONLY, /* 只写 */
    O_RDWR, /* 读写 */
    O_CREAT = 4 /* 创建 */
};

/* 查找文件的路径 */
struct path_search_record {
    char searched_path[MAX_PATH_LEN]; /* 父路径 */
    struct dir* parent_dir; /* 文件或目录的直接父目录 */
    enum file_type p_ftype; /* 找到的文件类型 */
};

/* 文件属性结构体 */
struct stat {
    uint32_t st_inode_no; /* inode number */
    uint32_t st_size; /* file size or contain file size */
    enum file_type st_ftype; /* file tyepe */
};

/* 根据 '/' 解析路径名存储在 name, str 之后的路径 */
char* path_parse(char* pathname, char* name);

/* 返回路径深度，如 '/a/b/c' 和 '///a/b//c' 返回的都是 3 */
uint32_t path_depth(char* pathname);

/* 创建文件，成功则返回文件描述符，失败返回-1 */
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flags);

/* 把buf 中的count 个字节写入file, 成功则返回写入的字节数，失败则返回 -1 */
ssize_t file_write(struct file* file, const void* buf, size_t count);

/* 从 file 连续读取 count 个字节到 buf, 成功则返回读到的字节数，失败则返回 -1 */
ssize_t file_read(struct file* file, void* buf, size_t count);

/* 成功打开或创建文件后，返回文件描述符，否则返回 -1 */
int32_t sys_open(const char* pathname, uint8_t flags);

/* 关闭文件描述符 fd 指向的文件，成功返回 0，否则返回 -1 */
int32_t sys_close(int32_t fd);

/* 将 buf 中连续 count 字节写入 fd，成功返回写入的字节数，失败返回 -1 */
ssize_t sys_write(int fd, const void* buf, size_t count);

/* 从 buf 中连续读入 count 字节到 fd，成功返回读入的字节数，失败返回 -1 */
ssize_t sys_read(int fd, void* buf, size_t count);

/* 在磁盘上搜索文件系统，若没有则格式化分区创建文件系统 */
void filesys_init(void);

/* 创建目录 pathname，成功返回 0，失败返回 -1 */
int sys_mkdir(const char* pathname);

/* 把当前工作路径绝对路径写入 buf，size 是 buf 的大小，当 buf 为NULL时，由操作系统分配空间，失效返回 NULL */
char* sys_getcwd(char* buf, size_t size);

/* 更改当前工作目录为绝对路径 path，成功返回 0，失败返回 -1 */
int sys_chdir(const char* path);

/* 在 stat 中填充 pathname 的文件属性，成功返回 0，失败返回 -1 */
int sys_stat(const char* pathname, struct stat* buf);

#endif /* __FS_FS_H */