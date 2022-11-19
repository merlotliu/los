#include "shell.h"
#include "debug.h"
#include "fs.h"
#include "stdio.h"
#include "cmd_builtin.h"

static char _cmd_line[MAX_CMD_LEN] = {0}; /* 存储输入的命令 */
char __cwd_cache[MAX_PATH_LEN] = {0}; /* 记录当前目录，每次 cd 更新该内容 */
char __final_path[MAX_PATH_LEN] = {0}; /*  */
char __io_buf[SECTOR_SIZE] = {0}; /* 读缓冲 */

char* argv[MAX_ARG_NR]; /* 命令字符串列表 */
int argc = -1; /* 参数个数 */

static void readline(char* buf, int count) {
    ASSERT(buf != NULL);
    char* pos = buf;
    while(read(stdin_no, pos, 1) != -1 && (pos - buf) < count) {
        switch (*pos) {
            case ('l' - 'a'): { /* ctrl + l : clear screen & reserve current input */
                *pos = 0;
                clear();
                print_prompt();
                printf("%s", buf);
                break;
            }
            case ('u' - 'a'): {
                while(pos != buf) {
                    *(pos--) = 0;
                    putchar('\b');
                }
                break;
            }
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
    printf("\nreadline: can't find enter_key in the cmd_line, max num of char is 128\n");
}

/* 根据 delim 将 cmd_str 分割成一个个字符串存放在 argv 中 */
static int cmd_parse(char* cmd_str, char** argv, const char delim) {
    int cnt = 0;
    char* str = NULL;
    char* sp = NULL;
    argv[cnt++] = strtok_r(cmd_str, delim, &sp);
    while(sp != NULL) {
        str = strtok_r(NULL, delim, &sp);
        if(str == NULL) {
            break;
        }
        argv[cnt] = str;
        if(++cnt >= MAX_ARG_NR) {
            return -1;
        }
    }
    // printf("%d\n",cnt);
    return cnt;
}

/* 输出命令提示符 */
void print_prompt(void) {
    printf("sakura@localhost:%s$ ", __cwd_cache);
}

/* 简单的 shell */
void shell(void) {
    __cwd_cache[0] = '/';
    while(1) {
        print_prompt();
        memset(_cmd_line, 0, MAX_CMD_LEN);
        readline(_cmd_line, MAX_CMD_LEN);
        if(*_cmd_line == 0) {
            continue;
        }

        argc = cmd_parse(_cmd_line, argv, ' ');
        if(argc == -1) {
            printf("num of arguments exceed %d\n", MAX_ARG_NR);
            continue;
        }

        if(strcmp("ps", argv[0]) == 0) {
            ps_builtin(argc, argv);
        } else if(strcmp("ls", argv[0]) == 0) {
            ls_builtin(argc, argv);
        } else if(strcmp("pwd", argv[0]) == 0) {
            pwd_builtin(argc, argv);
        } else if(strcmp("clear", argv[0]) == 0) {
            clear_builtin(argc, argv);
        } else if(strcmp("mkdir", argv[0]) == 0) {
            mkdir_builtin(argc, argv);
        } else if(strcmp("cd", argv[0]) == 0) {
            if(cd_builtin(argc, argv) != NULL) {
                strcpy(__cwd_cache, __final_path);
            }
        } else if(strcmp("rmdir", argv[0]) == 0) {
            rmdir_builtin(argc, argv);
        } else if(strcmp("touch", argv[0]) == 0) {
            touch_builtin(argc, argv);
        } else if(strcmp("echo", argv[0]) == 0) {
            echo_builtin(argc, argv);
        } else if(strcmp("cat", argv[0]) == 0) {
            cat_builtin(argc, argv);
        } else if(strcmp("rm", argv[0]) == 0) {
            rm_builtin(argc, argv);
        } else {
            printf("can't recognize command '%s", argv[0]);
            int32_t i;
            for(i = 1; i < argc; i++) {
                printf(" %s", argv[i]);
            }
            printf("\n");
        }
    }
    PANIC("my_shell: should not be here");
}
