#include "list.h"
#include "interrupt.h"

/* init double linked list */
void list_init(struct list* list) {
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

/* 将 elem 插入到 before 前面 */
void list_insert_before (struct list_elem* before, struct list_elem* elem) {
    /* 开关中断保证操作原子性 */
    enum intr_status old_status = intr_disable();

    before->prev->next = elem;
    elem->prev = before->prev;
    elem->next = before;
    before->prev = elem;

    intr_status_set(old_status);
}

/* push element in front of list */
void list_push_front(struct list* plist, struct list_elem* elem) {
    list_insert_before(plist->head.next, elem);
}

/* push element in back of list */
void list_push_back(struct list* plist , struct list_elem* elem) {
    list_insert_before(&plist->tail, elem);
}

/* remove pelem of list */
void list_remove(struct list_elem* pelem) {
    /* 开关中断保证操作原子性 */
    enum intr_status old_status = intr_disable();

    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;

    intr_status_set(old_status);
}

/* pop first element of stack */
struct list_elem* list_pop(struct list* plist) {
    struct list_elem* elem = plist->head.next;
    list_remove(elem);
    return elem;
}

/* if list is empty return true else return false */
bool list_empty(struct list* plist) {
    return (plist->head.next == &plist->tail ? true : false);
}

/* get list length */
uint32_t list_len(struct list* plist) {
    struct list_elem* elem = plist->head.next;
    uint32_t len = 0;
    while(elem != &plist->tail) {
        len++;
        elem = elem->next;
    }
    return len;
}

/* 遍历 plist 列表，寻找符合 func 条件的元素并返回，没有则返回 NULL */
struct list_elem* list_traversal(struct list* plist, list_func func, int arg) {
    if(list_empty(plist)) {
        return NULL;
    }
    struct list_elem* elem = plist->head.next;
    while(elem != &plist->tail) {
        if(func(elem, arg)) {
            return elem;
        }
        elem = elem->next;
    }
    return NULL;
}

/* if obj_elem is exist in plist return true else return false */
bool elem_find(struct list* plist, struct list_elem* obj_elem) {
    struct list_elem* elem = plist->head.next;
    while(elem != &plist->tail) {
        if(elem == obj_elem) {
            return true;
        }
        elem = elem->next;
    }
    return false;
}
