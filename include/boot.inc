; ----------------- loader & kernel ----------------- 
; loader base load address
LOADER_BASE_ADDR equ 0x900
; loader start of strored sector
LOADER_START_SECTOR equ 0x2
; loader covered sectors
LOADER_SECTOR_COUNT equ 0x4

; kernel start of strored sector
KERNEL_START_SECTOR equ 0x9
; kernel bin base address
KERNEL_BIN_BASE_ADDR equ 0x70000
; kernel covered sectors
KERNEL_SECTOR_COUNT equ 200
; kernel entry address
KERNEL_ENTRY_POINT equ 0xc0001500

; 定义目录表的物理地址
PAGE_DIR_TABLE_POS equ 0x100000

; ----------------- gdt discriptor attributes ----------------- 
; Granularity(23) : if 1, unit is 4KB 
DESC_G_4K 			equ 	1000_0000_0000_0000_0000_0000b 
; D/B(22) : if 1, 32 bit for offset & operating number
DESC_D_32 			equ 	100_0000_0000_0000_0000_0000b
; L(21) : (reserved bit) if 0, don't set 64 bit code segment 
DESC_L				equ		00_0000_0000_0000_0000_0000b
; AVaiLable(20) : no
DESC_AVL			equ		0_0000_0000_0000_0000_0000b
; segment limit(16~19)
DESC_LIMIT_CODE2	equ		1111_0000_0000_0000_0000b
DESC_LIMIT_DATA2	equ		DESC_LIMIT_CODE2
DESC_LIMIT_VIDEO2	equ		0000_0000_0000_0000b
; Present(15) : if segment in memory, set 1. or else set 0
DESC_P				equ		1000_0000_0000_0000b
; Descriptor Privilege Level(13~14) : CPU 0(high level), user 3 
DESC_DPL_0			equ		000_0000_0000_0000b
DESC_DPL_1			equ		010_0000_0000_0000b
DESC_DPL_2			equ		100_0000_0000_0000b
DESC_DPL_3			equ		110_0000_0000_0000b
; S(12) : if 0, system segment. if 1, data segement(code & data). 
DESC_S_CODE			equ		1_0000_0000_0000b
DESC_S_DATA			equ		DESC_S_CODE
DESC_S_sys			equ		0_0000_0000_0000b
; type(8~11) : with S
; EXecutable(11), Read(10), Conforming(9), Accessed(8)
; code : x=1,c=0,r=0,a=0 EXecutable, no read, no Conforming, no Accessed
DESC_TYPE_CODE		equ		1000_0000_0000b
; data : x=0,e=0,w=1,a=0 no EXecutable, extend to high addr, writeable, no Accessed
DESC_TYPE_DATA		equ		0010_0000_0000b

; segment base address(segment descriptor's 0~7) -> (16~23 of segment base address): 0xb = 1011
DESC_SEG_BASE_VIDEO equ     1011b

; code segment high 4 bytes 
DESC_CODE_HIGH4		equ		(0x00 << 24) + \
DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + \
DESC_LIMIT_CODE2 + DESC_P + DESC_DPL_0 + \
DESC_S_CODE + DESC_TYPE_CODE + 0x00

; data(stack) segment high 4 bytes 
DESC_DATA_HIGH4 	equ 	(0x00 << 24) + \
DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + \
DESC_LIMIT_DATA2 + DESC_P + DESC_DPL_0 + \
DESC_S_DATA + DESC_TYPE_DATA + 0x00

; video segment high 4 bytes 
DESC_VIDEO_HIGH4 	equ 	(0x00 << 24) +\
DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + \
DESC_LIMIT_VIDEO2 + DESC_P + DESC_DPL_0 + \
DESC_S_DATA + DESC_TYPE_DATA + DESC_SEG_BASE_VIDEO + 0x00
                  
; ----------------- selector attribute ----------------- 
; RPL (0~1) : 0、1、2、3
RPL0	equ		00b
RPL1	equ		01b
RPL2	equ		10b
RPL3	equ		11b
; Table Indicator (2): 0 GDT 1 LDT
TI_GDT	equ		000b
TI_LDT	equ		100b

; ----------------- page table attribute ----------------- 
; 存在位，1标识在内存中，0标识当前页不在内存中，触发 pagefault异常
PG_P equ 1b
; 读写位，1标识可读可写，0标识只读不可写
PG_RW_R equ 00b
PG_RW_W equ 10b
; 左边第1位标识 US 值
; 0 : 不能被特权级 3 的任务访问
; 1 : 所有任务都可以访问
PG_US_S equ 000b
PG_US_U equ 100b

; ----------------- kernel attribute ----------------- 
PT_NULL equ 0