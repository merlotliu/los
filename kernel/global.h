#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H

#include "stdint.h"

#define NULL            ((void*)0)
#define UNUSED          __attribute__((unused))
#define PG_SIZE         4096
#define bool            int
#define true            1
#define false           0

/* 向上取整的除法 */
#define DIV_ROUND_UP(X, STEP) ((X + STEP - 1) / (STEP))

/************** GDT Descriptor Attribute [begin] *****************/
#define GDT_DESC_SIZE   8
#define GDT_ADDR_BASE   0xc0000900

#define DESC_G_4K   1
#define DESC_D_32   1
#define DESC_L      0 // 64 位代码标记，此处标记为0 便可
#define DESC_AVL    0 

#define DESC_P      1
#define DESC_DPL_0  0
#define DESC_DPL_1  1
#define DESC_DPL_2  2
#define DESC_DPL_3  3

/* 
code segment & data segement belong to stored segment. 
TSS & Gate Descriptor belong to system segment. 
s is 1 (stored segment). s is 0(system segment);
*/
#define DESC_S_CODE     1
#define DESC_S_DATA     DESC_S_CODE
#define DESC_S_SYS      0

/* 1000 : x=1(executable) c=0(not conforming) r=0(unreadable) a=0(not access) */
#define DESC_TYPE_CODE  8 
/* 0010 : x=0(unexecutable) e=0(extern up) w=1(writable) a=0(not access) */
#define DESC_TYPE_DATA  2
/* 1001 :  */
#define DESC_TYPE_TSS   9

#define GDT_ATTR_HIGH\
    ((DESC_G_4K << 7) + ( DESC_D_32 << 6) + (DESC_L << 5) + (DESC_AVL << 4))

#define GDT_CODE_ATTR_LOW_DPL3\
    ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_CODE << 4) + DESC_TYPE_CODE)
#define GDT_DATA_ATTR_LOW_DPL3\
    ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_DATA << 4) + DESC_TYPE_DATA)

/* GDT descriptor */
typedef struct {
    /* low 32 bits */
    uint16_t limit_low_word;
    uint16_t base_low_word;
    /* high 32 bits */
    uint8_t base_mid_byte; /* [0-7] */
    uint8_t attr_low_byte; /* [8-15] */
    uint8_t limit_high_attr_high; /* [16-23] */
    uint8_t base_high_byte; /* [24-31] */
}gdt_desc_t;

/*************** GDT Descriptor Attribute [end] ******************/

/****************** Selector Attribute [begin] *******************/
#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

#define TI_GDT 0
#define TI_LDT 1

#define SELECTOR_K_CODE     ((1 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA     ((2 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_STACK    SELECTOR_K_DATA
#define SELECTOR_K_GS       ((3 << 3) + (TI_GDT << 2) + RPL0)

#define SELECTOR_U_CODE     ((5 << 3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_DATA     ((6 << 3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_STACK    SELECTOR_U_CODE

/******************** Selector Attribute [end] *******************/

/********************** IDT Attribute [begin] ********************/
#define IDT_DESC_P          1
#define IDT_DESC_DPL0       0
#define IDT_DESC_DPL3       3
#define IDT_DESC_32_TYPE    0xE
#define IDT_DESC_16_TYPE    0x6

#define IDT_DESC_ATTR_DPL0 \
    ((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)
#define IDT_DESC_ATTR_DPL3 \
    ((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)

/********************** IDT Attribute [end] ********************/

/********************** TSS Attribute [begin] ********************/

#define TSS_DESC_D  0

#define TSS_ATTR_HIGH\
    ((DESC_G_4K << 7) + (TSS_DESC_D << 6) + (DESC_L << 5) + (DESC_AVL << 4) + 0x0)
#define TSS_ATTR_LOW\
    ((DESC_P << 7) + (DESC_DPL_0 << 5) + (DESC_S_SYS << 4) + DESC_TYPE_TSS)

#define SELECTOR_TSS ((4 << 3) + (TI_GDT << 2) + RPL0)

/********************** TSS Attribute [end] ********************/

/* eflags regiester */
#define EFLAGS_MBS      (1 << 1) /* 必须设置 */
#define EFLAGS_IF_0     (0)      /* if = 0, 关中断 */
#define EFLAGS_IF_1     (1 << 9) /* if = 1, 开中断 */
#define EFLAGS_IOPL_3   (3 << 12)/* 测试用户程序在非系统调用下进行 IO */
#define EFLAGS_IOPL_0   (0 << 12)

#endif