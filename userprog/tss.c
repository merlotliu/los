#include "tss.h"
#include "print.h"

/* GDT base is 0xc0000900 */
#define TSS_ADDR_BASE (GDT_ADDR_BASE + 4 * GDT_DESC_SIZE)

/* tss struct */
typedef struct {
    uint32_t backlink;
    uint32_t* esp0;
    uint32_t ss0;
    uint32_t* espl;
    uint32_t ssl;
    uint32_t* esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t (*eip) (void) ;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint32_t trace;
    uint32_t io_base;
}tss_t;
static tss_t tss;

/* update esp0 of tss to pthread's stack(level 0) */
void tss_update_esp(struct task_ctl_blk* pthread) {
    tss.esp0 = (uint32_t*)((uint32_t)pthread + PG_SIZE);
}

/* create GDT Descriptor */
static gdt_desc_t gdt_desc_make(uint32_t* desc_addr, uint32_t limit, uint8_t attr_low, uint8_t attr_high) {
    uint32_t desc_base = (uint32_t)desc_addr;

    gdt_desc_t desc;
    desc.limit_low_word = limit & 0x0000ffff;
    desc.base_low_word = desc_base & 0x0000ffff;
    desc.base_mid_byte = ((desc_base & 0x00ff0000) >> 16);
    desc.attr_low_byte = (uint8_t)(attr_low);
    desc.limit_high_attr_high = (((limit & 0x000f0000) >> 16) + (uint8_t)(attr_high));
    desc.base_high_byte = desc_base >> 24;

    return desc;
}

/* create tss in GDT & reload gdt */
void tss_init(void) {
    put_str("tss_init start\n");
    uint32_t tss_size = sizeof(tss);
    memset(&tss, 0, tss_size);
    tss.ss0 = SELECTOR_K_STACK; /* level 0 stack */
    tss.io_base = tss_size; /* no io bitmap */
    
    /* GDT base address is 0x900. tss is the 4.(0x900 + 0x20) */
    /* dpl0's TSS */
    *((gdt_desc_t *)TSS_ADDR_BASE) = gdt_desc_make((uint32_t*)&tss, tss_size - 1, TSS_ATTR_LOW, TSS_ATTR_HIGH);
    /* dpl3's user data & code segment */
    *((gdt_desc_t *)(TSS_ADDR_BASE + GDT_DESC_SIZE)) = gdt_desc_make((uint32_t*)0, 0xfffff, GDT_CODE_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
    *((gdt_desc_t *)(TSS_ADDR_BASE + 2 * GDT_DESC_SIZE))  = gdt_desc_make((uint32_t*)0, 0xfffff, GDT_DATA_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
    
    /* gdt [16 bit's limit][32 bit's segment address] */
    uint64_t gdt_operand = ((GDT_DESC_SIZE * 7 - 1) | ((uint64_t)(uint32_t)GDT_ADDR_BASE << 16));
    /* reload gdt */
    asm volatile("lgdt %0": : "m" (gdt_operand));
    asm volatile("ltr %w0": : "r" (SELECTOR_TSS));
    put_str("tss_init end\n");
}