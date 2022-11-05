#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

#include "global.h"

/* 
    将 elem 转化为 tcb ：
    1. 获取 elem 在 tcb中的偏移；
    2. 使用 elem 的地址减去偏移获得 tcb 的起始地址；
    3. 强制转化为 tcb 类型；
    相同地址的不同数据类型，实际上只是取的连续字节不同
*/
#define offset(struct_type, member) (int)(&((struct_type*)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr)\
    (struct_type*)((int)elem_ptr - offset(struct_type, struct_member_name))

/* list element : no data */
struct list_elem {
    struct list_elem* prev;
    struct list_elem* next;
};

struct list {
    /* head is a empty node. head.next is the first element. head.prev is empty. */
    struct list_elem head;
    /* tail is same as head. but tail.next is empty. tail.prev is the last element.*/
    struct list_elem tail;
};

/* callback function */
typedef bool (list_func)(struct list_elem*, int arg);

/* init double linked list */
void list_init(struct list* list);

/* 将 elem 插入到 before 前面 */
void list_insert_before (struct list_elem* before, struct list_elem* elem);

/* push element in front of list */
void list_push_front(struct list* plist, struct list_elem* elem);

/* push element in back of list */
void list_push_back(struct list* plist , struct list_elem* elem);

/* remove pelem of list */
void list_remove(struct list_elem* pelem);

/* pop first element of stack */
struct list_elem* list_pop(struct list* plist);

/* if list is empty return true else return false */
bool list_empty(struct list* plist);

/* get list length */
uint32_t list_len(struct list* plist);

/* 遍历 plist 列表，寻找符合 func 条件的元素并返回，没有则返回 NULL */
struct list_elem* list_traversal(struct list* plist, list_func func, int arg);

/* if obj_elem is exist in plist return true else return false */
bool elem_find(struct list* plist, struct list_elem* obj_elem);

#endif /* __LIB_KERNEL_LIST_H */