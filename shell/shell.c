#include "shell.h"
#include "debug.h"
#include "fs.h"
#include "stdio.h"

static char _cmd_line[MAX_CMD_LEN] = {0}; /* 存储输入的命令 */
char __cwd_cache[MAX_PATH_LEN] = {0}; /* 记录当前目录，每次 cd 更新该内容 */

static void readline(char* buf, int count) {
    ASSERT(buf != NULL);
    char* pos = buf;
    while(read(stdin_no, pos, 1) != -1 && (pos - buf) < count) {
        switch (*pos) {
            case '\n':
            case '\r':{
                *pos = 0;
                putchar('\n');
                return;
            }
            case '\b':{
                if(*buf != '\b') { /* 仅能删除本次命令的输入 */
                    pos--; /* 退回到 _cmd_line 的上一个字符 */
                    putchar('\b');
                }
                break;
            }
            default: {
                putchar(*pos);
                pos++;
            }
        }
    }
    putchar('\n');
}

/* 输出命令提示符 */
void print_prompt(void) {
    printf("sakura@localhost:%s$", __cwd_cache);
}

/* 简单的 shell */
void shell(void) {
    while(1) {
        print_prompt();
        memset(_cmd_line, 0, MAX_CMD_LEN);
        readline(_cmd_line, MAX_CMD_LEN);
        if(*_cmd_line == 0) {
            continue;
        }
    }
    PANIC("my_shell: should not be here");
}