#ifndef __SHELL_CMD_BUILTIN_H
#define __SHELL_CMD_BUILTIN_H

#include "global.h"

/* ps builtin cmd */
void ps_builtin(int agrc, char** argv);

void ls_builtin(int argc, char** argv UNUSED) ;

void pwd_builtin(int argc, char** argv UNUSED) ;

void clear_builtin(int argc, char** argv UNUSED) ;

int mkdir_builtin(int argc, char** argv) ;

/* 将 path 处理成绝对路径 */
void path2abs(const char* path, char* abs_path);

/* change dirctory, 返回切换后的路径，失败返回 NULL */
char* cd_builtin(int argc, char** argv);

int rmdir_builtin(int argc, char** argv UNUSED) ;

/* create file (read & write) */
void touch_builtin(int argc, char** argv);

/* usage: echo content >> filename. '>>' is append. '>' is override. */
void echo_builtin(int argc, char** argv);

/* usage: cat filename*/
void cat_builtin(int argc, char** argv);

int rm_builtin(int argc, char** argv UNUSED) ;
#endif /* __SHELL_CMD_BUILTIN_H */
