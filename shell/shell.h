#ifndef __SHELL_SHELL_H
#define __SHELL_SHELL_H

#define MAX_CMD_LEN 128 /* 最后键入 128 个字符的命令行输入 */
#define MAX_ARG_NR  16 /* 加上命令名最后支持 16 个参数 */

/* 输出命令提示符 */
void print_prompt(void);

/* 简单的 shell */
void shell(void);

#endif /* __SHELL_SHELL_H */
