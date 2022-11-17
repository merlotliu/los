#include "syscall.h"

#define _syscall0(NUMBER) ({\
    int retval;\
    asm volatile (\
        "int $0x80"\
        : "=a" (retval)\
        : "a" (NUMBER)\
        : "memory"\
    );\
    retval;\
})

#define _syscall1(NUMBER, ARG1) ({\
    int retval;\
    asm volatile (\
        "int $0x80"\
        : "=a" (retval)\
        : "a" (NUMBER), "b" (ARG1)\
        : "memory"\
    );\
    retval;\
})

#define _syscall2(NUMBER, ARG1, ARG2) ({\
    int retval;\
    asm volatile (\
        "int $0x80"\
        : "=a" (retval)\
        : "a" (NUMBER), "b" (ARG1), "c" (ARG2)\
        : "memory"\
    );\
    retval;\
})

#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({\
    int retval;\
    asm volatile (\
        "int $0x80"\
        : "=a" (retval)\
        : "a" (NUMBER), "b" (ARG1), "c" (ARG2), "d" (ARG3)\
        : "memory"\
    );\
    retval;\
})

/* get current process id */
uint32_t getpid(void) {
    return _syscall0(SYS_GETPID);
}

/* memory allocate */
void* malloc(uint32_t size) {
    return (void*)_syscall1(SYS_MALLOC, size);
}

/* free memory that ptr points to */
void free(void* ptr) {
    _syscall1(SYS_FREE, ptr);
}

/* write() writes  up  to  count  bytes  from  the buffer pointed buf to the file referred to by the file descriptor fd. */
ssize_t write(int fd, const void* buf, size_t count) {
    return _syscall3(SYS_WRITE, fd, buf, count);
}

/* read() attempts to read up to count bytes from file descriptor fd into the buffer starting at buf. */
ssize_t read(int fd, void* buf, size_t count) {
    return _syscall3(SYS_READ, fd, buf, count);
}

/* print single character */
int putchar(int c) {
    _syscall1(SYS_PUTCHAR, c);
    return 0;
}

/* clear screen */
void clear(void) {
    _syscall0(SYS_CLEAR);
}

/* process copy */
pid_t fork(void) {
    return (pid_t)_syscall0(SYS_FORK);
}

/* get current work directory */
char* getcwd(char* buf, size_t size) {
    return (char*)_syscall2(SYS_GETCWD, buf, size);
}

/* open file of pathname with flag */
int open(char* pathname, uint8_t flag) {
    return (int)_syscall2(SYS_OPEN, pathname, flag);    
}

/* close fd */
int close(int fd) {
    return (int)_syscall1(SYS_CLOSE, fd);
}

/* set file seek */
int lseek(int fd, int offset, uint8_t whence) {
    return (int)_syscall3(SYS_LSEEK, fd, offset, whence);    
}

/* delete file pathname */
int unlink(const char* pathname) {
    return _syscall1(SYS_UNLINK, pathname);
}

/* create directory pathname */
int mkdir(const char* pathname) {
    return _syscall1(SYS_MKDIR, pathname);
}

/* open directory name */
struct dir* opendir(const char* name) {
    return (struct dir*)_syscall1(SYS_OPENDIR, name);
}

/* close dir */
int closedir(struct dir* dir) {
    return (int)_syscall1(SYS_CLOSEDIR, dir);
}

/* change dirctory */
int chdir(const char* path) {
    return (int)_syscall1(SYS_CHDIR, path);
}

/* remove directory */
int rmdir(const char* pathname) {
    return (int)_syscall1(SYS_RMDIR, pathname);
}
/* read directory */
struct dentry* readdir(struct dir* dir) {
    return (struct dentry*)_syscall1(SYS_READDIR, dir);
}

/* rebwind directory pointer */
void rewinddir(struct dir* dir) {
    _syscall1(SYS_REWINDDIR, dir);
}

/* get path's attribute to buf */
// int stat(const char* path, struct stat* buf) {
//     return (int)_syscall2(SYS_STAT, path, buf);
// }

/* list tasks' info */
void ps(void) {
    _syscall0(SYS_PS);
}
