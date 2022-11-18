#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "global.h"
#include "thread.h"
#include "fs.h"

enum SYSCALL_NR {
    SYS_GETPID = 0,
    SYS_MALLOC,
    SYS_FREE,
    SYS_WRITE,
    SYS_READ,
    SYS_PUTCHAR,
    SYS_CLEAR,
    SYS_FORK,
    SYS_GETCWD,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_LSEEK,
    SYS_UNLINK,
    SYS_MKDIR,
    SYS_OPENDIR,
    SYS_CLOSEDIR,
    SYS_CHDIR,
    SYS_RMDIR,
    SYS_READDIR,
    SYS_REWINDDIR,
    SYS_STAT,
    SYS_PS
};

/* get current process id */
uint32_t getpid(void);

/* memory allocate */
void *malloc(uint32_t size);

/* free memory that ptr points to */
void free(void* ptr);

/* write() writes  up  to  count  bytes  from  the buffer pointed buf to the file referred to by the file descriptor fd. */
ssize_t write(int fd, const void* buf, size_t count);

/* read() attempts to read up to count bytes from file descriptor fd into the buffer starting at buf. */
ssize_t read(int fd, void* buf, size_t count);

/* print single character */
int putchar(int c);

/* clear screen */
void clear(void);

/* process copy */
pid_t fork(void);

/* get current work directory */
char* getcwd(char* buf, size_t size);

/* open file of pathname with flag */
int open(char* pathname, uint8_t flag);

/* close fd */
int close(int fd);

/* set file seek */
int lseek(int fd, int offset, uint8_t whence);

/* delete file pathname */
int unlink(const char* pathname);

/* create directory pathname */
int mkdir(const char* pathname);

/* open directory name */
struct dir* opendir(const char* name);

/* close dir */
int closedir(struct dir* dir);

/* change dirctory */
int chdir(const char* path);

/* remove directory */
int rmdir(const char* pathname);

/* read directory */
struct dentry* readdir(struct dir* dir);

/* rebwind directory pointer */
void rewinddir(struct dir* dir);

/* get path's attribute to buf */
int stat(const char* pathname, struct stat* buf);

/* list tasks' info */
void ps(void);

#endif /* __LIB_USER_SYSCALL_H */