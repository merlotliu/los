; 主引导程序
;-----------------------------------------------------
%include "boot.inc"
SECTION MBR vstart=0x7c00 ; 起始地址编译为 0x7c00
    mov ax,cs   ; ax为通用寄存器,ds/es/fs/gs为段寄存器
    mov ds,ax   ; CPU 不能直接从立即数到段寄存器，需要通过其他寄存器中转
    mov es,ax   ; 这里使用 ax 转化
    mov ss,ax
    mov fs,ax
    mov sp,0x7c00 ; 初始化栈指针（栈是向低地址生长的）
			      ; 0x7c00 向上是MBR的代码，向下为可使用的安全区域，作为栈使用
    mov ax,0xb800 ; 0xb800 是显卡文本模式的段基址
    mov gs,ax

; 清屏 : 利用 0x06 号功能，上卷全部行，则可清屏
;-----------------------------------------------------
; INT 0x10  功能号:0x06  功能描述 : 上卷窗口
;-----------------------------------------------------
; 输入 : 
; AH    功能号 = 0x06
; AL    =   上卷的行数（如果为0，表示全部）
; BH    =   上卷行属性
; (CL,CH) = 窗口左上角的 (X,Y)位置
; (DL,DH) = 窗口右下角的 (X,Y)位置
; 无返回值:
    mov ax, 0600h
    mov bx, 0700h
    mov cx, 0       ; 左上角 : (0, 0)
    mov dx, 184fh  	; 右下角 : (80, 25)
                    ; VGA 文本模式下，一行只能容纳 80 个字符，共 25 行
                    ; 下标从 0 开始，故 0x18=24, 0x4f=79
    int 10h         ; int 0x10
    
; 输出字符串 : 1 MBR
    ; byte 表示只读取一个字节
    ; 0xA4 即为 1010 0100 
    ; 第一个1表示闪烁，第2个表示绿背景，第三个表示红字
	mov byte [gs:0x00],'1'
    mov byte [gs:0x01],0xA4 
    
    mov byte [gs:0x02],' '
    mov byte [gs:0x03],0xA4
    
    mov byte [gs:0x04],'M'
    mov byte [gs:0x05],0xA4
    
    mov byte [gs:0x06],'B'
    mov byte [gs:0x07],0xA4
    
    mov byte [gs:0x08],'R'
    mov byte [gs:0x09],0xA4

	mov eax, LOADER_START_SECTOR	; 起始扇区 lba 地址（宏）
	mov bx,LOADER_BASE_ADDR			; 写入地址
	mov cx,LOADER_SECTOR_COUNT		; 待写入扇区数
	call rd_disk_m_16				; 读取程序的起始部分（一个扇区）
	
	jmp LOADER_BASE_ADDR
	
;-----------------------------------------------------
; 功能 : 读取硬盘 n 个扇区
;-----------------------------------------------------
rd_disk_m_16:
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
	and al,0x88		; 第4位为1表示硬盘控制器已准备好数据传输
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
    mov [bx],ax
    add bx,2
    loop .go_on_read
	ret
;-----------------------------------------------------
; 读取硬盘结束
;-----------------------------------------------------
	
;-----------------------------------------------------
; 字符填充
; 510 字节减去上面通过($-$$)得到的偏移量，其结果便是本扇区
; 内的剩余量，也就是要填充的字节数。
;-----------------------------------------------------
    times 510-($-$$) db 0
;-----------------------------------------------------
; 保证最后两个字节为 0x55, 0xaa
;-----------------------------------------------------
    db 0x55,0xaa