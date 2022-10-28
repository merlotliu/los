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

; gdt pointer : front 2 bytes is gdt limit, back 4 bytes is gdt start address
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

loadermsg db '2 loader in read.'


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

    jmp $