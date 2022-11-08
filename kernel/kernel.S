[bits 32]
%define ERROR_CODE nop 
%define ZERO push 0

extern idt_table

section .data
    global intr_entry_table

intr_entry_table:
    %macro VECTOR 2
    section .text
        intr%1entry:
            %2
            push ds
            push es
            push fs
            push gs
            pushad ; push 32 bit register : EAX ECX EDX EBX ESP EBP ESI EDI EAX

            ; slave interrupt : not only send EOI to slave, but also send to master
            mov al, 0x20
            out 0xa0, al
            out 0x20, al

            ; call interrupt handler of idt_table
            push %1 ; interrupt vector number
            call [idt_table + %1 * 4]
            jmp intr_exit

    section .data
        dd intr%1entry
    
    %endmacro

section .text
    global intr_exit

intr_exit:
    ; recover context
    add esp, 4 ; skip interrupt vector number
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4 ; skip error_code
    iretd

; general int
VECTOR 0x00, ZERO
VECTOR 0x01, ZERO
VECTOR 0x02, ZERO
VECTOR 0x03, ZERO
VECTOR 0x04, ZERO
VECTOR 0x05, ZERO
VECTOR 0x06, ZERO
VECTOR 0x07, ZERO
VECTOR 0x08, ERROR_CODE
VECTOR 0x09, ZERO
VECTOR 0x0a, ERROR_CODE
VECTOR 0x0b, ERROR_CODE
VECTOR 0x0c, ERROR_CODE
VECTOR 0x0d, ERROR_CODE
VECTOR 0x0e, ERROR_CODE
VECTOR 0x0f, ZERO
VECTOR 0x10, ZERO
VECTOR 0x11, ERROR_CODE
VECTOR 0x12, ZERO
VECTOR 0x13, ZERO
VECTOR 0x14, ZERO
VECTOR 0x15, ZERO
VECTOR 0x16, ZERO
VECTOR 0x17, ZERO
VECTOR 0x18, ZERO
VECTOR 0x19, ZERO
VECTOR 0x1a, ZERO
VECTOR 0x1b, ZERO
VECTOR 0x1c, ZERO
VECTOR 0x1d, ZERO
VECTOR 0x1e, ZERO
VECTOR 0x1f, ZERO
VECTOR 0x20, ZERO ; clock interrupt entry
VECTOR 0x21, ZERO ; keyboard interrupt entry
VECTOR 0x22, ZERO ; 级联
VECTOR 0x23, ZERO ; 串口2对应的入口
VECTOR 0x24, ZERO ; 串口1对应的入口
VECTOR 0x25, ZERO ; 并口2对应的入口
VECTOR 0x26, ZERO ; 软盘对应的入口
VECTOR 0x27, ZERO ; 并口1对应的入口
VECTOR 0x28, ZERO ; 实时时钟对应的入口
VECTOR 0x29, ZERO ; 重定向
VECTOR 0x2a, ZERO ; reserve
VECTOR 0x2b, ZERO ; reserve
VECTOR 0x2c, ZERO ; ps/2 鼠标
VECTOR 0x2d, ZERO ; fpu 浮点单元异常
VECTOR 0x2e, ZERO ; hard disk
VECTOR 0x2f, ZERO ; reserve

; ################# 0x80 int ##################
[bits 32]
extern syscall_table
section .text
    global syscall_handler
syscall_handler:
    ; 1. 保存上下文
    push 0 ; 统一格式 error no

    push ds
    push ds
    push es
    push gs
    pushad

    push 0x80 ; 统一格式 vector no
    
    ; 2. 为系统调用子功能传入参数
    push edx 
    push ecx 
    push ebx 

    ; 3. 调用子功能处理函数
    call [syscall_table + eax*4]
    add esp, 12

    ; 4. 将 call 调用后的返回值存入当前内核栈中 eax 的位置
    ; eax 在 pushad 的第一个压栈 加上 push 0x80 一共需要越过 8 * 4 字节
    ; eax 存放的是 int 0x80 的返回值
    ; 也就是 eax 给栈内的 eax 赋值，intr_exit 栈内 eax 出栈再给 eax 赋值
    ; 根据我们的 _syscall 宏函数最后 eax 会赋值给返回值返回
    mov [esp + 8 * 4], eax
    jmp intr_exit
