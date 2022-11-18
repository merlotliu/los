#include "cmd_builtin.h"
#include "stdio.h"
#include "fs.h"
#include "debug.h"

extern char __final_path[MAX_PATH_LEN];

/* 去掉路径中的 '.' 和 '..' */
static void path_wash(char* old_abs_path, char* new_abs_path) {
    ASSERT('/' == *old_abs_path);

    char name[MAX_PATH_LEN] = {0};
    char* sp;
    const char* dir_str = strtok_r(old_abs_path, '/', &sp);
    char* parent_dir;
    if(NULL == dir_str) { /* 仅有一个或多个分隔符，则返回 NULL */
        *new_abs_path = '/';
        return ;
    } else { // ////aa/aa/../../.././bb/bb////
        name[0] = '/';
        parent_dir = name + 1;
        if(strcmp(".", dir_str) != 0 && strcmp("..", dir_str) != 0) {
            strcat(name, dir_str);
        }
        while(sp != NULL) {
            dir_str = strtok_r(NULL, '/', &sp);
            if(NULL == dir_str) {
                break;
            } else if(strcmp("..", dir_str) == 0) { /* 找到最后一个 '/'，清空这一位置之后的内容 */
                parent_dir = strrchr(name, '/');
                parent_dir = parent_dir == name ? name + 1 : parent_dir;
                memset(parent_dir, 0, strlen(name));
            } else if(strcmp(".", dir_str) == 0) {
                /* do nothing */
            } else {
                if(strcmp(name, "/") != 0) { /* 只剩下根目录，不添加 '/' */
                    strcat(name, "/");
                }
                strcat(name, dir_str);
            }
        }
        if(strcmp(name, "/") != 0) {
            strcat(name, "/");
        }
        strcpy(new_abs_path, name);
    }
}

/* 将 path 处理成绝对路径 */
void path2abs(const char* path, char* abs_path) {
    char buf[MAX_PATH_LEN] = {0};
    if(path[0] != '/') {
        bzero(buf, MAX_PATH_LEN);
        if(getcwd(buf, MAX_PATH_LEN) != NULL) {
            // printf("%s\n", buf);
            if(strcmp(buf, "/") != 0) {
               strcat(buf, "/");
            }
        }
    }
    strcat(buf, path);
    path_wash(buf, abs_path);
}

/* ps builtin cmd */
void ps_builtin(int argc, char** argv UNUSED) {
    if(argc != 1) {
        printf("ps: no argument support!\n");
        return;
    }
    ps();
}

void ls_builtin(int argc UNUSED, char** argv UNUSED) {
    printf("ls\n");
}

void pwd_builtin(int argc, char** argv UNUSED) {
    if(argc != 1) {
        printf("clear: no argument support!\n");
        return;
    }
    if(NULL == getcwd(__final_path, MAX_PATH_LEN)) {
        printf("pwd: get current work directory failed.\n");
        return;
    } 
    printf("%s\n", __final_path);
}

void clear_builtin(int argc, char** argv UNUSED) {
    if(argc != 1) {
        printf("clear: no argument support!\n");
        return;
    }
    clear();
}

int mkdir_builtin(int argc, char** argv) {
    /* 仅支持 命令 + 路径 2 个参数：先生成绝对路径，然后在绝对路径不为'/'根目录的情况下，创建目录 */    
    if(argc == 2) {
        path2abs(argv[1], __final_path);
        if(strcmp("/", __final_path) != 0) {
            if(mkdir(__final_path) == 0) {
                return 0;
            } else {
                printf("mkdir: create directory %s failed.\n", argv[1]);
            }
        }
    } else {
        printf("mkdir: only support 1 argument!\n") ;
    }
    return -1;
}

/* change dirctory, 返回切换后的路径，失败返回 NULL */
char* cd_builtin(int argc, char** argv) {
    /* 仅支持一个参数或无参数：
        1. 1个参数，参数为切换到的目录；
        2. 无参数，即切换到根目录；
     */
    if(argc > 2) {
        printf("cd: only support 1 argument!\n");
        return NULL;
    }
    if(argc == 1) {
        bzero(__final_path, MAX_PATH_LEN);
        *__final_path = '/';
    } else {
        // sprintf(argv[1], "////aa/aa/../../../.././././bb/bb///cc//.//cd/.."); // test
        path2abs(argv[1], __final_path);
    }
    // printf("%s\n", __final_path); // test
    if(chdir(__final_path) == -1) {
        printf("cd: no such directory %s\n", __final_path) ;
        return NULL;
    }
    return __final_path;
}

int rmdir_builtin(int argc UNUSED, char** argv UNUSED) {
    if(argc == 2) {
        path2abs(argv[1], __final_path);
        if(strcmp("/", __final_path) != 0) { /* 不能删除根目录 */
            if(rmdir(__final_path) == 0) {
                return 0;
            } else {
                printf("rmdir: remove directory %s failed.\n", argv[1]);
            }
        }
    } else {
        printf("rmdir: only support 1 argument!\n") ;
    }
    return -1;
}

int rm_builtin(int argc UNUSED, char** argv UNUSED) {
    if(argc == 2) {
        path2abs(argv[1], __final_path);
        if(strcmp("/", __final_path) != 0) { /* 不能删除根目录 */
            if(unlink(__final_path) == 0) {
                return 0;
            } else {
                printf("rm: remove directory %s failed.\n", argv[1]);
            }
        }
    } else {
        printf("rm: only support 1 argument!\n") ;
    }
    return -1;
}