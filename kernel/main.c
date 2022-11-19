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

/* init process */
void init(void);

void k_thread_a(void*);
void k_thread_b(void*);
void k_thread_aa(void*);
void k_thread_bb(void*);
void u_prog_a(void);
void u_prog_b(void);

int prog_a_pid = 0, prog_b_pid = 0;

int main(void) {
    put_str("I am kernel\n");
    init_all();
    cls_screen();
    print_prompt();
    
    intr_enable(); /* open interrupt */
    // ASSERT(intr_status_get() == INTR_ON);

    while(1) {
        // console_put_str("Main ");
    }
    return 0;   

    /* thread */
    // process_execute(u_prog_a, "user_prog_a");
    // process_execute(u_prog_b, "user_prog_b");
    // thread_start("k_thread_a", THREAD_PRIORITY_DEFAULT, k_thread_aa, "    KThrdA");
    // thread_start("k_thread_b", THREAD_PRIORITY_DEFAULT, k_thread_bb, "    KThrdB");
    // uint32_t fd = sys_open("/file1", O_RDWR);
    // printf("open fd : %d\n", fd);
    
    // sys_write(fd, "hello world!\n", 13);

    // sys_close(fd);
    // printf("close fd : %d\n", fd);
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

void k_thread_a(void* arg UNUSED) {
    void* addr1 = sys_malloc(256 ) ;
    void* addr2 = sys_malloc(255 ) ;
    void* addr3 = sys_malloc(254);
    console_put_str (" thread_a malloc addr:0x") ;
    console_put_int ((int) addr1) ;
    console_put_char ( ',');
    console_put_int ((int) addr2); 
    console_put_char(','); 
    console_put_int ((int) addr3); 
    console_put_char ('\n'); 
    int cpu_delay = 1000000; 
    while(cpu_delay-- > 0); 
    sys_free(addr1); 
    sys_free(addr2); 
    sys_free(addr3); 
    while(1); 
}   

void k_thread_b(void* arg UNUSED) {
    void* addr1 = sys_malloc(256 ) ;
    void* addr2 = sys_malloc(255 ) ;
    void* addr3 = sys_malloc(254);
    console_put_str (" thread_a malloc addr:0x") ;
    console_put_int ((int) addr1) ;
    console_put_char ( ',');
    console_put_int ((int) addr2); 
    console_put_char(','); 
    console_put_int ((int) addr3); 
    console_put_char ('\n'); 
    int cpu_delay = 1000000; 
    while(cpu_delay-- > 0); 
    sys_free(addr1); 
    sys_free(addr2); 
    sys_free(addr3); 
    while(1); 
}   

void k_thread_aa(void* arg UNUSED) {
    void* addr1;
    void* addr2;
    void* addr3;
    void* addr4;
    void* addr5;
    void* addr6;
    void* addr7;
    console_put_str(" thread_a start\n");
    int max = 1000;
    while (max-- > 0) {
        int size = 128;
        addr1 = sys_malloc(size);
        size *= 2;
        addr2 = sys_malloc(size);
        size *= 2;
        addr3 = sys_malloc(size);

        sys_free(addr1);
        addr4 = sys_malloc (size);
        size <<= 7;
        addr5 = sys_malloc(size);
        addr6 = sys_malloc(size);

        sys_free(addr5);
        size *= 2;
        addr7 = sys_malloc(size);

        sys_free(addr6);
        sys_free(addr7);
        sys_free(addr2);
        sys_free(addr3);
        sys_free(addr4);
    }
    console_put_str(" thread_a end\n");
    while (1);
    // char* param = arg;
    // console_put_str(param);

    // void* addr = sys_malloc(33);
    // console_put_str(" sys_malloc(33), addr is 0x");
    // console_put_int((int)addr);
    // console_put_char('\n');
    // while(1);
}

void k_thread_bb(void* arg UNUSED) {
    // char* param = arg;
    void* addr1;
    void* addr2;
    void* addr3;
    void* addr4;
    void* addr5;
    void* addr6;
    void* addr7;
    void* addr8;
    void* addr9;
    int max = 1000;
    console_put_str(" thread_b start\n");
    while (max-- > 0) {
        int size = 9;
        // int size = 2048;
        addr1 = sys_malloc (size);
        size *= 2;
        addr2 = sys_malloc (size);
        size *= 2;
        sys_free (addr2);
        addr3 = sys_malloc (size);

        sys_free (addr1);
        addr4 = sys_malloc (size);
        addr5 = sys_malloc (size);
        addr6 = sys_malloc (size);

        sys_free (addr5);
        size *= 2;
        addr7 = sys_malloc (size);

        sys_free (addr6);
        sys_free (addr7);
        sys_free (addr3);
        sys_free (addr4);

        size <<= 3;

        addr1 = sys_malloc (size);
        addr2 = sys_malloc (size);
        addr3 = sys_malloc (size);
        addr4 = sys_malloc (size);
        addr5 = sys_malloc (size);
        addr6 = sys_malloc (size);
        addr7 = sys_malloc (size);
        addr8 = sys_malloc (size);
        addr9 = sys_malloc (size);

        sys_free (addr1);
        sys_free (addr2);
        sys_free (addr3);
        sys_free (addr4);
        sys_free (addr5);
        sys_free (addr6);
        sys_free (addr7);
        sys_free (addr8);
        sys_free (addr9);
    }
    console_put_str (" thread_b end\n");
    while(1);
    
    // before sys_malloc
    // char* param = arg;
    // console_put_str(param);

    // void* addr = sys_malloc(63);
    // console_put_str(" sys_malloc(63), addr is 0x");
    // console_put_int((int)addr);
    // console_put_char('\n');
    // while(1);
}

void u_prog_a(void) {
    void* addr1 = malloc(256);
    void* addr2 = malloc(255);
    void* addr3 = malloc(254);
    printf(" prog_a malloc addr: 0x%x, 0x%x, 0x%x\n", (int) addr1, (int) addr2, (int) addr3);

    int cpu_delay = 1000000;
    while(cpu_delay-- > 0) ;
    free(addr1) ;
    free(addr2);
    free(addr3);

    // char* name = "UProcA";
    // printf("    %s PID:%d%c", name, getpid(), '\n');
    while(1);
}

void u_prog_b(void) {
    void* addr1 = malloc(256);
    void* addr2 = malloc(255);
    void* addr3 = malloc(254);
    printf(" prog_b malloc addr: 0x%x, 0x%x, 0x%x\n", (int) addr1, (int) addr2, (int) addr3);

    int cpu_delay = 1000000;
    while(cpu_delay-- > 0) ;
    free(addr1) ;
    free(addr2);
    free(addr3);

    // char* name = "UProcB";
    // printf("    %s PID:%d%c", name, getpid(), '\n');
    while(1);
}
