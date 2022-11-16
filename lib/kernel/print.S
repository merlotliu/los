TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0

[bits 32]
section .data
    put_int_buffer dq 0 ; buffer for converting number to char

section .text

global set_cursor
set_cursor:
    push ebx
    push eax
    push edx
    mov bx, [esp + 16] ; 3个寄存器 + 返回地址

    ; 1 设置高 8 位
    mov dx, 0x03d4 ; 索引寄存器
    mov al, 0x0e ; 用于提供光标位置的高 8 位
    out dx, al
    mov dx, 0x03d5 ; 通过读写 0x03d5 获得或设置光标位置
    mov al, bh
    out dx, al

    ; 2 设置低 8 位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al

    pop edx
    pop eax
    pop ebx
    ret

;----------------------------------------
; put_str 
; brief : print string whose end is '\0'
; input : char *, address of string 
; output : none
;----------------------------------------
global put_str
put_str:
	push ebx
	push ecx
	xor ecx, ecx ; clear ecx
	mov ebx, [esp + 12] ; string address
	
    .goon:
        mov cl, [ebx] ; get char
        cmp cl, 0 ; is or not reach string tail ('\0')
        jz .str_over
        push ecx ; push arg for put_char
        call put_char
        add esp, 4 ; recycle arg space of stack
        inc ebx ; mov to next char
        jmp .goon

    .str_over:
        pop ecx
        pop ebx
        ret
;---------------------------------------- 
; put_char
; brief : print single char
; input : char
; output : none
;----------------------------------------
global put_char
put_char:
    pushad ; 备份 32 位寄存器

    mov ax, SELECTOR_VIDEO  ; 不能直接把立即数送入段寄存器
    mov gs, ax

    ; 获取光标的高 8 位
    mov dx, 0x03d4 ; 索引寄存器
    mov al, 0x0e ; 提供光标位置的高 8 位的端口
    out dx, al
    mov dx, 0x03d5 ; 读写端口 0x3d5 获得或设置光标位置
    in al, dx ; 获取光标位置的高 8 位
    mov ah, al

    ; 获取光标的低 8 位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    in al, dx

    ; 将光标存入 bx
    mov bx, ax
    ; pushad 压入 4*8=32 字节
    ; 再加上 4 字节返回地址
    ; 这个位置就是压入的需要打印的字符
    mov ecx, [esp + 36] 

    ; CRLF  回车换行
    ; CR 0x0d, LF 0x0a
    cmp cl, 0xd
    jz .is_carriage_return
    cmp cl, 0xa
    jz .is_line_feed

    ; BS(backspace)'s ascii is 0x8
    cmp cl, 0x8
    jz .is_backspace

    jmp .put_other

    ; backspace
    .is_backspace:
        dec bx ; 光标向前移动 1 位
        shl bx, 1 ; 光标索引 * 2 为对应字符的位置

        mov byte [gs:bx], 0x20 ; 将待删除的字节补为 0 或空格皆可
        inc bx
        mov byte [gs:bx], 0x07 ; 字符属性，黑屏白字
        shr bx, 1 ; 移动到删除一个字符的位置
        jmp .set_cursor

    ; 显示正常字符
    .put_other:
        shl bx, 1
        mov [gs:bx], cl ; 需要显示的字符
        inc bx
        mov byte [gs:bx], 0x07 ; 字符的属性
        shr bx, 1 ; 恢复老的光标
        inc bx ; 下一个光标位置
        cmp bx, 2000
        jl .set_cursor
                    ; 光标小于 2000，表示未写到末尾，设置新的光标
                    ; 否则超出屏幕字符总数，换行处理

    .is_line_feed: ; LF(\n)
    .is_carriage_return: ; CR(\r) 将光标移至行首
        xor dx, dx ; dx 为被除数高 16 位，清 0，
        mov ax, bx ; ax 为被除数低 16 位，清 0，
        mov si, 80 
                ; 模仿Linux，使用 \n 表示下一行行首
                ; 在此 \n \r 都处理为 \n
        div si 
        sub bx, dx ; dx 为余数

    .is_carriage_return_end: ; 切换到下一行
        add bx, 80 ; 加一行字符
        cmp bx, 2000

    .is_line_feed_end: ; LF(\n) 光标 +80
        jl .set_cursor

    ; 滚屏方法1: 将屏幕的 1-24 行搬运到 0-23 行，24 行用 0 填充
    ; 超出屏幕，开始滚屏
    .roll_screen:
        cld
        mov ecx, 960 
                ; 共 2000-80=1920 字符，共1920*2=3840字节
                ; 一次搬运 4 字节，共 3840/4=960 次
        mov esi, 0xc00b80a0 ; 第1行行首
        mov edi, 0xc00b8000 ; 第0行行首
        rep movsd ; movesd 一次传递双字 4 字节

        mov ebx, 3840 ; 最后一行首字符的第一个字节偏移 1920*2
        mov ecx, 80 
                    ; 一行 80 字符（160字节）
                    ; 每次清空 1 字符（2 字节），一行需要移动 80 次
    .cls:
        mov word [gs:ebx], 0x0720 ; 0x0720 是黑底白字的空格键
        add ebx, 2
        loop .cls
        mov bx, 1920 ; 将光标值重置为 1920，最后一行的首字符
        
    ; 将光标设为 bx 值
    .set_cursor:
    ; 1 设置高 8 位
        mov dx, 0x03d4 ; 索引寄存器
        mov al, 0x0e ; 用于提供光标位置的高 8 位
        out dx, al
        mov dx, 0x03d5 ; 通过读写 0x03d5 获得或设置光标位置
        mov al, bh
        out dx, al

    ; 2 设置低 8 位
        mov dx, 0x03d4
        mov al, 0x0f
        out dx, al
        mov dx, 0x03d5
        mov al, bl
        out dx, al

    .put_char_done:
        popad
        ret

;----------------------------------------
; put_int 
; brief : print 32 bits 16 based integer
;----------------------------------------
; input : 32 bits 16 based integer
; output : 32 bits 16 based integer string
global put_int
put_int:
	pushad
	mov ebp, esp
	mov eax, [ebp + 4 * 9]
	mov edx, eax
	mov ebx, put_int_buffer
	mov	edi, 7
	mov ecx, 8
	
    .16based_4bits:
    	and edx, 0x0000000f
    	cmp edx, 9
    	jg .is_a2f
    	add edx, '0'
    	jmp .store
    	
    	.is_a2f:
    		sub edx, 10
    		add edx, 'a'
            
        .store:
        	mov [ebx + edi], dl
        	dec edi
        	shr eax, 4
        	mov edx, eax
        	loop .16based_4bits
    
    .ready_to_print:
    	inc edi
        .skip_prefix_0:
            cmp edi, 8
            je .full0
            .go_on_skip:
                mov cl, [put_int_buffer + edi]
                inc edi
                cmp cl, '0'
                je .skip_prefix_0
                dec edi
                jmp .put_each_num
            .full0:
                mov cl, '0'
    .put_each_num:
		push ecx
		call put_char
		add esp, 4
		inc edi
		mov cl, [put_int_buffer + edi]
		cmp edi, 8
		jl .put_each_num
		popad
		ret

;----------------------------------------
; cls_screen
; brief : clear screen
;----------------------------------------
global cls_screen
cls_screen:
    pushad
    ;由于用户程序的cpl 为3 ，显存段的dpl 为O,
    ;故用于显存段的选择子gs 在低于自己特权的环境中为0,
    ;导致用户程序再次进入中断后， gs 为O,
    ;故直接在put _str 中每次都为gs 赋值
    mov ax, SELECTOR_VIDEO ;不能直接把立即数送入gs ，需由ax 中转
    mov gs, ax
    mov ebx, 0
    mov ecx, 80*25
    
    .cls:
        mov word [gs:ebx], 0x0720 ; 0x0720 是黑底白字的空格键
        add ebx, 2
        loop .cls
        mov ebx, 0

    .set_cursor:
    ; 1 设置高 8 位
        mov dx, 0x03d4 ; 索引寄存器
        mov al, 0x0e ; 用于提供光标位置的高 8 位
        out dx, al
        mov dx, 0x03d5 ; 通过读写 0x03d5 获得或设置光标位置
        mov al, bh
        out dx, al

    ; 2 设置低 8 位
        mov dx, 0x03d4
        mov al, 0x0f
        out dx, al
        mov dx, 0x03d5
        mov al, bl
        out dx, al
    ; end
    popad
    ret