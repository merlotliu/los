#ifndef __USERPROG_FORK_H
#define __USERPROG_FORK_H

#include "thread.h"

/* fork 子进程 内核不可直接调用 */
pid_t sys_fork(void);

#endif /* __USERPROG_FORK_H */
