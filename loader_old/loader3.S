%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR
jmp loader_start

; create GDT & internal segment discriptor
GDT_BASE: 
    dd 0x00000000 
	dd 0x00000000

CODE_DESC: 
    dd 0x0000FFFF 
	dd DESC_CODE_HIGH4

DATA_STACK_DESC: 
    dd 0x0000FFFF 
	dd DESC_DATA_HIGH4

VIDEO_DESC: 
	dd 0x80000007 ; limit = (0xbffff - 0xb8000) / 4k = 0x7
    dd DESC_VIDEO_HIGH4 ; dpl = 0

GDT_SIZE    equ     $ - GDT_BASE
GDT_LIMIT   equ     GDT_SIZE - 1

; reserve 60 empty descriptor position
times 60 dq 0

; equal (CODE_DESC - GDT_BASE) / 8 + TI_GDT + RPL0
SELECTOR_CODE equ (0x0001 << 3) + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002 << 3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0

; total_mem_bytes save memory size, unit is bytes
; total_mem_bytes dd 0

; gdt pointer : front 2 bytes is gdt limit, back 4 bytes is gdt start address
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

loadermsg db '2 loader in read.'

; ; 
; ards_buf timer 244 db 0
; ards_nr dw 0

loader_start:
;------------------------------------------------------------
; INT Ox10 功能号： Ox13 功能描述：打印字符串
;------------------------------------------------------------
;输入：
;AH 子功能号＝ 13H
;BH ＝页码
;BL ＝属性（若AL=00H 或01H)
;CX＝字符串长度
; (DH 、DL ）＝坐标｛行、列）
;ES:BP＝字符串地址
;AL＝显示输出方式
; 0——字符串中只含显示字符，其显示属性在BL 中
    ;显示后，光标位置不变
; 1——字符串中只含显示字符，其显示属性在BL 中
    ;显示后，光标位置改变
; 2——字符事中含显示字符和显示属性。显示后，光标位置不变
; 3——字符串中含显示字符和显示属性。显示后，光标位置改变
; 无返回值
    mov sp, LOADER_BASE_ADDR    
    mov bp, loadermsg           ; ES:BP = 字符串地址
    mov cx, 17                  ; CX = 字符长度
    mov ax, 0x1301              ; AH=13,AL=01h
    mov bx, 0x001f              ; 页号为 0（BH=0）蓝底粉红字（BL=1fh）
    mov dx, 0x1800
    int 0x10                    ; 10h 号中断

; ----------------- preparation for protected mode  ----------------- 
; 1 open A20
; 2 load gdt
; 3 set pe of cr0 to 1
; ----------------- open A20  ----------------- 
    in al,0x92
    or al,0000_0010B
    out 0x92,al

; ----------------- load gdt  ----------------- 
    lgdt [gdt_ptr]

; ----------------- set pe of cr0 to 1  ----------------- 
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

    mov eax, SELECTOR_CODE
    ; jmp dword 0x00010008:p_mode_start
    jmp dword SELECTOR_CODE:p_mode_start
    ; dword 修饰偏移地址
    ; 刷新流水线，因为马上就是从 16 位转到 32 位

[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, LOADER_STACK_TOP
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    mov byte [gs:160], 'P'

    ; load kernel
    mov eax, KERNEL_START_SECTOR
    mov ebx, KERNEL_BIN_BASE_ADDR
    mov ecx, KERNEL_SECTOR_COUNT

    ; 创建页目录及页表并初始化页内存位图
    call setup_page

    ; 将描述符表地址和偏移量写入内存 gdt_ptr
    sgdt [gdt_ptr]
    ; gdt_ptr 中前 2 字节为偏移量 后 4 个字节为 GDT 基址
    mov ebx, [gdt_ptr+2]
    ; 将原来的 dgt 描述符中的显存段基址 + 0xc0000000 （3GB~4GB的起始地址）
    ; 显存段为第 3 个段描述符，每个描述符 8 字节，故偏移 0x8 * 3 = 0x18
    ; 段描述符的高 4 字节 的高 1 字节为段基址的 31~24 位
    ; 0xc0000000 中仅最高 1 字节不为 0，以下 or 操作也只会修改最高 1 字节的值
    or dword [ebx + 0x18 + 4], 0xc0000000

    ; 修改 gdt 的基址：加上 0xc0000000
    add dword [gdt_ptr + 2], 0xc0000000

    ; 修改栈指针的基址
    add esp, 0xc0000000

    ; 将 cr3（页目录基址寄存器） 赋值为页目录地址
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax

    ; 打开 cr0 的 pg 位（第 31 位）
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; 开启分页后，重新加载 gdt
    lgdt [gdt_ptr]

    ; 测试代码，正常输出字符 'V' 则成功
    mov byte [gs:162],'V'

    jmp $

; 创建页目录及页表
setup_page:

; 功能：页目录占用的内存空间逐字节清 0
; ecx 使用来控制循环次数的，loop 每次循环会做两件事：
	; 1. [ecx] = [exc]-1
	; 2. [ecx] 是否为0，为0往下执行，不为0跳到 标号所在位置
; esi 当前清空的字节相对于页目录起始位置 PAGE_DIR_TABLE_POS 的偏移
	mov ecx, 4096
	mov esi, 0
.clear_page_dir:
	mov byte [PAGE_DIR_TABLE_POS + esi], 0
	inc esi
	loop .clear_page_dir
	
; create page directory entry
.create_pde:
	mov eax, PAGE_DIR_TABLE_POS
	add eax, 0x1000 ; 0x1000(4kb)偏移到第 0 个页表的位置
	mov ebx, eax
	
	; 所有特权级别都可访问，允许读写，在内存中
	; PG_US_U | PG_RW_W | PG_P = 0x7
	or eax, PG_US_U | PG_RW_W | PG_P
	
	; 现在 eax 中的值为 
	; (PAGE_DIR_TABLE_POS + 0x1000) | PG_US_U | PG_RW_W | PG_P
	; 即第一个页表的地址及其属性
	
	; 为以下几个页表目录项写入属性
	; 第 0 个目录项
	mov [PAGE_DIR_TABLE_POS + 0x0], eax 
	; 第768个目录项，0xc00 以上的目录项用于内核空间
	; 也就是页表的 0xc0000000 ~ 0xffffffff 共1G
	mov [PAGE_DIR_TABLE_POS + 0xc00], eax 
	
	
	; 最后1个目录项,指向页目录表自己的地址
	; 为了之后能动态的操作
	; eax 减去 0x1000 后为
	; (PAGE_DIR_TABLE_POS) | PG_US_U | PG_RW_W | PG_P
	sub eax, 0x1000
	mov [PAGE_DIR_TABLE_POS + 4092], eax
	
	; 1MB 低端内存 / 每页大小 4KB = 256
	; 将 1MB 的低端内存划分为 256 个页
	; 并将地址全部存入第 0 个页表中
	mov ecx, 256
	mov esi, 0
    ; 所有特权级别都可访问，允许读写，在内存中
	mov edx, PG_US_U | PG_RW_W | PG_P
	
; create page table entry
.create_pte:
	; ebx 当前为第 0 个页表的地址
	mov [ebx + esi * 4], edx
	add edx, 0x1000
	inc esi
	loop .create_pte
	
; 将 769 ~ 1022 的页目录项创建出来
; 并固定内核页目录项指向的页表地址
; 这么做的目的是共享内核
; 如果仅创建页目录项而固定769 ~ 1022的内存
; 当程序访问到 4MB 以外的内存之后，创建的新页表信息只会更新在呢一个程序，对其他程序不可见
	mov eax, PAGE_DIR_TABLE_POS
	add eax, 0x2000
	or eax, PG_US_U | PG_RW_W | PG_P
	mov ebx, PAGE_DIR_TABLE_POS
	mov ecx, 254 ; 1022 - 768 个页目录项
	mov esi, 769
	
; create kernel page directory entry
.create_kernel_pde:
	mov [ebx + esi * 4], eax
	inc esi
	add eax, 0x1000
	loop .create_kernel_pde
	ret