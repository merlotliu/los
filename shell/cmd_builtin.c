#include "cmd_builtin.h"
#include "stdio.h"
#include "fs.h"
#include "debug.h"
#include "dir.h"

extern char __final_path[MAX_PATH_LEN];
extern char __io_buf[SECTOR_SIZE]; /* 读缓冲 */

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

void ls_builtin(int argc, char** argv) {
    /* 遍历除命令 ls 之后的参数名 */
    char* pathname = NULL; /* 记录路径参数 */
    bool f_all = false; /* 是否显示隐藏内容 */
    bool f_long_list = false; /* 是否显示详细信息 */
    int idx = 1;
    int opt_idx = 1;
    int path_arg_cnt = 0; /* 除开 ls 指令和可选项的参数个数，最多有一个路径参数 */
    for(; idx < argc; idx++) {
        if(argv[idx][0] == '-') { /* 以 '-' 开头视为可选项，其他的视为路径参数 */
            opt_idx = 1;
            for(; argv[idx][opt_idx] != 0; opt_idx++) {
                // printf("%c\n", argv[idx][opt_idx]);
                switch (argv[idx][opt_idx]) {
                case 'a': {/* 显示全部目录和文件 '.' 开头的为隐藏目录或文件 */
                    f_all = true;
                    break;
                }
                case 'l': { /* 显示详细内容 */
                    f_long_list = true;
                    break;
                }
                default:
                    printf("ls: unknow option %c!\n", argv[idx][opt_idx]);
                    return;
                }
            }
        } else {
            if(++path_arg_cnt > 1) {
                printf("ls: only support 1 argument!\n");
                return;
            }
            /* 提供路径参数则 pathname 为该值 */
            pathname = argv[idx];
        }
    }
    if(pathname != NULL) {
        /* 将获取到的参数转化为绝对路径 */
        path2abs(pathname, __final_path);
    } else if(NULL == getcwd(__final_path, MAX_PATH_LEN)) {/* 没有提供路径，默认获取当前路径 */
        printf("ls: getcwd for default path failed\n");
        return;
    }
    pathname = __final_path;
    // printf("path: %s\n", pathname);
    /* 获取目录项相关属性 */
    struct stat file_stat;
    bzero(&file_stat, sizeof(struct stat));
    if(-1 == stat(pathname, &file_stat)) {
        printf("ls: cannot access %s: No such file or directory\n", pathname);
        return;
    }
    switch (file_stat.st_ftype) {
        case FT_DIRECTORY: {
            int dentry_cnt = 0;
            struct dir* dir = opendir(pathname);
            struct dentry* dentry = NULL;
            rewinddir(dir);
            if(!f_long_list) { /* not long list */
                while((dentry = readdir(dir)) != NULL) {
                    /* 如果不是显示所有内容并且为隐藏目录或文件（'.'开头的名字），则跳过 */
                    if(!f_all && '.' == dentry->d_filename[0]) { 
                        continue;
                    }
                    printf("%s ", dentry->d_filename);
                    dentry_cnt++;
                }
                if(dentry_cnt > 0) {
                    printf("\n");
                }
            } else { /* long list */
                char sub_pathname[MAX_FILE_NAME_LEN] = {0};
                strcpy(sub_pathname, pathname);
                int len = strlen(sub_pathname);
                if(sub_pathname[len - 1] != '/') {
                    sub_pathname[len++] = '/';
                }
                int dir_size = f_all ? file_stat.st_size : file_stat.st_size - 2 * sizeof(struct dentry);
                printf("total: %d\n", dir_size);
                char ftype; /* 文件类型缩写 */
                while((dentry = readdir(dir)) != NULL) {
                    /* 如果不是显示所有内容并且为隐藏目录或文件（'.'开头的名字），则跳过 */
                    if(!f_all && '.' == dentry->d_filename[0]) { 
                        continue;
                    }
                    /* 获取当前目录的属性 */
                    bzero(sub_pathname + len, MAX_FILE_NAME_LEN - len);
                    strcat(sub_pathname, dentry->d_filename);
                    bzero(&file_stat, sizeof(struct stat));
                    if(-1 == stat(sub_pathname, &file_stat)) {
                        printf("ls: cannot access %s: No such file or directory\n", sub_pathname);
                        return;
                    }
                    if(FT_DIRECTORY == file_stat.st_ftype) { 
                        ftype = 'd';
                        strcat(sub_pathname, "/");
                    } else {
                        ftype = '-';
                    }
                    printf("%c %d %d %s\n", ftype, dentry->d_inode_no, file_stat.st_size, sub_pathname + len);
                }
            }
            closedir(dir);
            break;
        }
        case FT_REGULAR: { /* 普通文件 */
            if(!f_long_list) {
                printf("%s\n", pathname);
            } else {
                printf("- %d %d %s/\n", file_stat.st_inode_no, file_stat.st_size, pathname);
            }
            break;   
        }
        default: {
            break;   
        }
    }
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
        } else {
            printf("mkdir: cannot create directory '/': File exists\n") ;
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

/* create file (read & write) */
void touch_builtin(int argc, char** argv) {
    if(argc == 2) {
        path2abs(argv[1], __final_path);
        if(open(__final_path, O_CREAT) != -1) {
            return;
        } else {
            printf("touch: create file '%s' failed.\n", argv[1]);
        }
    } else {
        printf("touch: only support 1 argument!\n") ;
    }
}

/* usage: echo content >> filename. '>>' is append. '>' is override. */
void echo_builtin(int argc, char** argv) {
    if(argc == 4) {
        /* content */
        char* idx_start = argv[1];
        size_t cnt = strlen(argv[1]);
        if(('\'' == *idx_start && '\'' == *(idx_start + cnt - 1)) 
            || ('"' == *idx_start  && '"' == *(idx_start + cnt - 1))) { /* 双引号或单引号包裹 */
            idx_start++;
            cnt -= 2;
        }
        /* '>' or '>>' */
        if(0 == strcmp(">", argv[2])) {

        } else if(0 == strcmp(">>", argv[2])) {

        } else {
            printf("echo: unknow key '%s'", argv[2]);
            return;    
        }
        /* filename */
        path2abs(argv[3], __final_path);

        /* write */
        int fd = open(__final_path, O_WRONLY);
        write(fd, idx_start, cnt);
        close(fd);
    } else {
        printf("usage: echo content >>/> filename\nnote: '>>' is append. '>' is override\n") ;
    }
}

/* usage: cat filename*/
void cat_builtin(int argc, char** argv) {
    if(argc == 2) {
        path2abs(argv[1], __final_path);
        int fd = open(__final_path, O_RDONLY);
        bzero(__io_buf, SECTOR_SIZE);
        read(fd, __io_buf, SECTOR_SIZE);
        write(stdout_no, __io_buf, SECTOR_SIZE);
        printf("\n");
        close(fd);
    } else {
        printf("usage: cat filename\n") ;
    }
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