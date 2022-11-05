#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

#define IDT_DESC_CNT 0x21 /* curent interrput handler count */
#define PIC_M_CTRL 0x20 /* master control port */
#define PIC_M_DATA 0x21 /* master data port */
#define PIC_S_CTRL 0xa0 /* slave control port */
#define PIC_S_DATA 0xa1 /* slave data port */

#define EFLAGS_IF 0x00000200
/* pushfl will push eflags register. popl%0 & '=g' will pop eflags's value to EFLAG_VAR */
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAG_VAR))

struct gate_desc {
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint8_t dcount; 

    uint8_t attribute;
    uint16_t func_offset_high_word;
};

static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
static struct gate_desc idt[IDT_DESC_CNT];

/* save exception name */
char* intr_name[IDT_DESC_CNT]; 
/* idt_table is the final handler */
intr_handler idt_table[IDT_DESC_CNT]; 
/* intr_entry_table just a entry */
extern intr_handler intr_entry_table[IDT_DESC_CNT]; 

/* create idt descriptor */
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) {
    p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
    p_gdesc->selector = SELECTOR_K_CODE;
    p_gdesc->dcount = 0;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

/* init idt */
static void idt_desc_init(void) {
    int i;
    for(i = 0; i < IDT_DESC_CNT; i++) {
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }
    put_str("   idt_desc_init done\n");
}

/* init pic 8259A */
static void pic_init(void) {
    /* master */
    outb(PIC_M_CTRL, 0x11); /* ICW1: 0001 0001 */
    outb(PIC_M_DATA, 0x20); /* ICW2: 0010 0000 */
    outb(PIC_M_DATA, 0x04); /* ICW3: 0000 0100 */
    outb(PIC_M_DATA, 0x01); /* ICW4: 0000 0001 */
    /* slave */
    outb(PIC_S_CTRL, 0x11); /* ICW1: 0001 0001 */
    outb(PIC_S_DATA, 0x28); /* ICW2: 0010 1000 */
    outb(PIC_S_DATA, 0x02); /* ICW3: 0000 0010 */
    outb(PIC_S_DATA, 0x01); /* ICW4: 0000 0001 */

    /* open IR0 of master : accept interrupt of alarm */
    outb(PIC_M_DATA, 0xfe); /* master OCW1: 1111 1110 */
    outb(PIC_S_DATA, 0xff); /* slave OCW1: 1111 1111 */

    put_str("   pic_init done\n");
}

static void general_intr_handler(uint8_t vec_nr) {
    if(vec_nr == 0x27 || vec_nr == 0x2f) {
        /* IRQ7 & IRQ15 will produce spurious interrupt, no action */
        /* 0x2f is 8259A's last IRQ (reserved item) */
        return;
    }

    /* 光标置 0，清除一片区域为 0 */
    set_cursor(0);
    int cursor_pos = 0;
    while(cursor_pos < 320) {
        put_char(' ');
        cursor_pos++;
    }

    set_cursor(0);
    put_str("! ! ! ! ! ! ! excetion message begin ! ! ! ! ! ! ! ! \n") ;
    set_cursor(88) ; /* 从第 2 行第 8 个字符开始打印 */
    put_str(intr_name[vec_nr]);

    /*若为Page fault, 将缺失的地址打印出来并悬停 */
    if (vec_nr == 14) { 
        int page_fault_vaddr = 0;
        /* cr2 存放造成 page_fault 地址 */
        asm("movl %%cr2, %0" : "=r" (page_fault_vaddr));

        put_str("\npage fault addr is");
        put_int(page_fault_vaddr);
    }
    put_str("\n ! ! ! ! ! ! ! excetion message end ! ! ! ! ! ! ! ! \n");
    /* 能进入中断处理程序就表示已经处在关中断情况下
        不会出现调度进程的情况。故下面的死循环不会再被中断 */
    while(1);
}

/* init & register normal interrupt handler */
static void exception_init(void) {
    int i;
    for(i = 0; i < IDT_DESC_CNT; i++) {
        /* call [idt_table + %1 * 4] */
        idt_table[i] = general_intr_handler;
        intr_name[i] = "unknown";
    }
    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    // intr_name[l5] 15 is intel reserved item (no used)
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}

/* complete all of interrupt work */
void idt_init(void) {
    put_str("idt_init start\n");
    idt_desc_init(); /* init idt */
    exception_init(); /* init & register normal interrupt handler */
    pic_init(); /* init 8259A */

    /* load idt */
    /* [address(16-47)][limit(0-15)] */
    uint64_t idt_operand = ((uint64_t)((uint32_t)idt) << 16) | (sizeof(idt) - 1);
    asm volatile("lidt %0" : : "m" (idt_operand));
    put_str("idt_init done\n");
}

/* get current interrupt status */
enum intr_status intr_status_get(void) {
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (eflags & EFLAGS_IF) ? INTR_ON : INTR_OFF;
}

/* turn on interrupt & return last interrupt status */
enum intr_status intr_enable(void) {
    enum intr_status old_status;
    if(INTR_ON == intr_status_get()) {
        old_status = INTR_ON;
    } else {
        old_status = INTR_OFF;
        asm volatile("sti"); /* set eflags' IF to 1 */
    }
    return old_status;
}

/* turn off interrupt & return last interrupt status */
enum intr_status intr_disable(void) {
    enum intr_status old_status;
    if(INTR_ON == intr_status_get()) {
        old_status = INTR_ON;
        asm volatile("cli" : : : "memory"); /* close interrupt : set eflags' IF to 0 */
    } else {
        old_status = INTR_OFF;
    }
    return old_status;
}

/* set interrupt status & return last interrupt status */
enum intr_status intr_status_set(enum intr_status status) {
    return (status & INTR_ON) ? intr_enable() : intr_disable();
}

/* register interrupt handler in idt table */
void intr_handler_register(uint8_t vec_no, intr_handler func) {
    /* call handler according interrupt vector number. see kernel/loader.S call [idt_table + %1 * 4] */
    idt_table[vec_no] = func;
    put_int((uint32_t)func);
    put_str(" register success\n");
}