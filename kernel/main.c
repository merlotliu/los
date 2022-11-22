#include "print.h"
#include "init.h"
#include "memory.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "syscall_init.h"
#include "stdio.h"
#include "fs.h"
#include "shell.h"
#include "stdio_kernel.h"

/* init process */
void init(void);

extern struct ide_channel channels[2]; /* 有两个ide通道 */

int main(void) {
    put_str("I am kernel\n");
    init_all();

    /*************写入应用程序*************/ 
    uint32_t file_size = 4604;
    uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512) ;
    struct disk* sda = &channels[0].devices[0];
    void* prog_buf = sys_malloc(file_size) ;
    ide_read(sda, 300, prog_buf, sec_cnt) ;
    int32_t fd = sys_open("/proc", O_CREAT | O_RDWR);
    int ret = 0;
    if(fd != -1) {
        ret = sys_write(fd, prog_buf, file_size);
        if(ret == -1) {
            printk("file write error! \n") ;
            while (1) ;
        }
    }
    close(fd);
    sys_free(prog_buf);
    /************写入应用程序结束*************/
    cls_screen();
    
    print_prompt();
    intr_enable(); /* open interrupt */
    
    while(1) {
        // console_put_str("Main ");
    }
    return 0;   
}

/* init process */
void init(void) {
    uint32_t ret_pid = fork();
    if(ret_pid) { /* 父进程 */
        while(1);
        // printf("i am father, my pid is %d, child pid is %d\n", getpid() , ret_pid) ;
    } else {
        shell();
        // printf("i am child, my pid is %d, ret pid is %d\n", getpid() , ret_pid) ;
    }
    PANIC("init: should not be here");
}