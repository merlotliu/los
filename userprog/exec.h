#ifndef __USERPROG_EXEC_H
#define __USERPROG_EXEC_H

/* 使用 path 指向的程序替换当前进程，失败返回 -1，成功无返回值 */
int sys_execv(const char *path, const char *argv[]);

#endif /* __USERPROG_EXEC_H */