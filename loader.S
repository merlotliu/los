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
; total_mem_bytes 在内存中的地址是 0xb03
total_mem_bytes dd 0

; gdt pointer : front 2 bytes is gdt limit, back 4 bytes is gdt start address
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

; loadermsg db '2 loader in read.'

; total_mem_bytes(4) + gdt_ptr(6) + ards_buf(244) + ards_br(2) = 256
ards_buf times 244 db 0
ards_nr dw 0

loader_start:
; int 15h eax = 0000E820h, edx = 534D4150h ('SMAP) get memory layout
	xor ebx, ebx
	mov edx, 0x534D4150
	mov di, ards_buf

	.e820_mem_get_loop:
		mov eax, 0x0000E820
		mov ecx, 20 ; ARDS 地址范围描述符结构大小 20 字节
		int 0x15
		jc .e820_failed_so_try_e801 ; cf 为 1，错误
		add di, cx ; 增加 20 字节指向新的 ARDS 结构位置
		inc word [ards_nr]
		cmp ebx, 0 ; 若 ebx 为 0 且 cf 不为 1，说明 ards 全部返回
		jnz .e820_mem_get_loop

		; 在所有 ards 结构中找到(base_add_low + length_low) 的最大值
		; 即内存容量
		mov cx, [ards_nr]
		mov ebx, ards_buf
		xor edx, edx ; edx 为最大内存容量

	.find_max_mem_area:
		mov eax, [ebx] ; base_add_low
		add eax, [ebx + 8] ; length_low
		add ebx, 20 ; 指向缓冲区的下一个 ARDS 结构
		cmp edx, eax

		jge .next_ards
		mov edx, eax

	.next_ards:
		loop .find_max_mem_area
		jmp .mem_get_ok

	.e820_failed_so_try_e801:
		; 暂未实现

	.mem_get_ok:
		mov [total_mem_bytes], edx

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
    ; mov sp, LOADER_BASE_ADDR    
    ; mov bp, loadermsg           ; ES:BP = 字符串地址
    ; mov cx, 17                  ; CX = 字符长度
    ; mov ax, 0x1301              ; AH=13,AL=01h
    ; mov bx, 0x001f              ; 页号为 0（BH=0）蓝底粉红字（BL=1fh）
    ; mov dx, 0x1800
    ; int 0x10                    ; 10h 号中断

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

    ; mov byte [gs:160], 'P'

    ; load kernel
    mov eax, KERNEL_START_SECTOR
    mov ebx, KERNEL_BIN_BASE_ADDR
    mov ecx, 200

    call rd_disk_m_32

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
    ; mov byte [gs:162],'V'

    jmp SELECTOR_CODE:enter_kernel

enter_kernel:
	call kernel_init
	mov esp, 0xc009f000
	; mov byte [gs:164],'K'

	; 光标位置清 0
	mov bx, 0x0
	call clear_cursor

	jmp KERNEL_ENTRY_POINT

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

kernel_init:
	xor eax, eax
	xor ebx, ebx ; ebx 记录程序头表地址
	xor ecx, ecx ; ecx 记录程序头表中的 program header 数量
	xor edx, edx ; edx 记录 program header 尺寸 即 e_phentsize
	
	; 偏移到 e_phentsize 标识 program header 大小
	mov dx, [KERNEL_BIN_BASE_ADDR + 42] ; 32 bit
	; mov dx, [KERNEL_BIN_BASE_ADDR + 54] ; 64 bit
	; e_phoff
	mov ebx, [KERNEL_BIN_BASE_ADDR + 28] ; 32 bit
	; mov ebx, [KERNEL_BIN_BASE_ADDR + 32] ; 64 bit
	; mov ebx,0x34

	; 偏移到 e_phnum 标识有几个 program header
    add ebx, KERNEL_BIN_BASE_ADDR
	mov cx, [KERNEL_BIN_BASE_ADDR + 44] ; 32 bit
	; mov cx, [KERNEL_BIN_BASE_ADDR + 56] ; 64 bit
	
.each_segment:
	cmp byte [ebx + 0], PT_NULL
	; p_type == PT_NULL 说明该 program header 未使用
	je .PTNULL
	
	; 为函数 memcpy 压入参数，参数从右至左依次压栈
	; 函数原型类似 memcpy(dst, src, size)
	; 偏移到 program header 中 p_filesz所在的位置
	; 即 memcpy 第三个参数 size 8 字节
	push dword [ebx + 16] ; 32 bit
	; push dword [ebx + 32] ; 64 bit
	; 获取 p_offset 的值
	mov eax, [ebx + 4]; 32 bit
	; mov eax, [ebx + 8] ; 64 bit
	; 加上 kernel.bin 被加载的物理地址，eax 为该段的物理地址
	add eax, KERNEL_BIN_BASE_ADDR
	; memcpy 第2个参数 : src
	push eax
    ; memcpy 第1个参数 : dest
	; 偏移 8 个字节位置是 p_vaddr 即目的地址
	push dword [ebx + 8] ; 32 bit
	; push dword [ebx + 16] ; 64 bit
	; 调用 mem_cpy 完成段复制
	call mem_cpy
	; 清理栈中压入的3个参数
	add esp, 12
	
.PTNULL:
	; edx 为 program header 大小 即e_phentsize
	; 此时 ebx 指向下一个 program header
	add ebx, edx
	
	loop .each_segment
	ret
	
; 逐词解拷贝 mem_cpy
mem_cpy:
	cld
	push ebp
	mov ebp, esp
	push ecx
	
	mov edi, [ebp + 8] ; dst
	mov esi, [ebp + 12] ; src
	mov ecx, [ebp + 16] ; size
	rep movsb
	
	; recover env
	pop ecx
	pop ebp
	ret

;-----------------------------------------------------
; 功能 : 读取硬盘 n 个扇区
;-----------------------------------------------------
rd_disk_m_32:
	; eax=LBA扇区号
	; bx=将数据写入的内存地址
	; cx=读入的扇区数
	mov esi,eax						; 备份 eax
	mov di,cx						; 备份 cx
	
; 读写硬盘
; 第1步 : 设置要读取的扇区数
	mov dx,0x1f2
	mov al,cl
	out dx,al						; 读取的扇区数
	
	mov eax,esi						; 恢复 ax

; 第2步 : 将LBA 地址存入 0x1f3 ~ 0x1f6
	; LBA 地址 7 ~ 0 位 写入 LBA low : 0x1f3
	mov dx,0x1f3
	out dx,al
	
	; LBA 地址 15 ~ 8 位 写入 LBA mid : 0x1f4
	mov cl,8
	shr eax,cl
	mov dx,0x1f4
	out dx,al
	
	; LBA 地址 23 ~ 16 位 写入 LBA high : 0x1f5
	shr eax,cl
	mov dx,0x1f5
	out dx,al
	
	shr eax,cl
	and al,0x0f							; lba 第24-27
	or al,0xe0							; 设置 7~4 位为 1110，表示lba模式
	mov dx,0x1f6
	out dx,al
	
; 第3步 : 向0x1f7 端口写入读命令， 0x20
	mov dx,0x1f7
	mov al,0x20
	out dx,al
	
; 第4步 : 检测硬盘状态
.not_ready:
	; 同一端口，写时表示写入命令字，读时表示读入硬盘状态
	nop
	in al,dx
	and al,0x88		; 第3位为1表示硬盘控制器已准备好数据传输
					; 第7位为1表示硬件忙
    cmp al,0x08
    jnz .not_ready	; 若未准备好，继续等

; 第5步 : 从0x1f0端口读数据
	mov ax,di
	mov dx,256
	mul dx
	mov cx,ax
	; di 为要读取的扇区数，一个扇区 512 字节，每次读入一个字
	; di*512 / 2 = di * 256 次
	mov dx,0x1f0
.go_on_read:
	in ax,dx
    mov [ebx],ax
    add ebx,2
    loop .go_on_read
		; 由于在实模式下偏移地址为16位,所以用bx只会访问到0~FFFFh的偏移。
		; loader的栈指针为0x900,bx为指向的数据输出缓冲区,且为16位，
		; 超过0xffff后,bx部分会从0开始,所以当要读取的扇区数过大,待写入的地址超过bx的范围时，
		; 从硬盘上读出的数据会把0x0000~0xffff的覆盖，
		; 造成栈被破坏,所以ret返回时,返回地址被破坏了,已经不是之前正确的地址,
		; 故程序出会错,不知道会跑到哪里去。
		; 所以改为ebx代替bx指向缓冲区,这样生成的机器码前面会有0x66和0x67来反转。
		; 0X66用于反转默认的操作数大小! 0X67用于反转默认的寻址方式.
		; cpu处于16位模式时,会理所当然的认为操作数和寻址都是16位,处于32位模式时,
		; 也会认为要执行的指令是32位.
		; 当我们在其中任意模式下用了另外模式的寻址方式或操作数大小(姑且认为16位模式用16位字节操作数，
		; 32位模式下用32字节的操作数)时,编译器会在指令前帮我们加上0x66或0x67，
		; 临时改变当前cpu模式到另外的模式下.
		; 假设当前运行在16位模式,遇到0X66时,操作数大小变为32位.
		; 假设当前运行在32位模式,遇到0X66时,操作数大小变为16位.
		; 假设当前运行在16位模式,遇到0X67时,寻址方式变为32位寻址
		; 假设当前运行在32位模式,遇到0X67时,寻址方式变为16位寻址.
	ret
;-----------------------------------------------------
; 读取硬盘结束
;-----------------------------------------------------

clear_cursor:
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
	
	ret