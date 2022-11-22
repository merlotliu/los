BUILD_DIR 	= ./build
ENTRY_POINT = 0xc0001500

AS			= nasm
CC 			= gcc
LD 			= ld
LIB 		= -I kernel/ -I device/ -I fs/ -I shell/ -I thread/ -I userprog/ -I lib/ -I lib/kernel -I lib/user/ 

ASFLAGS 	= -f elf
CFLAGS		= -m32 -Wall $(LIB) -c	-W\
				-fno-builtin\
				-Wstrict-prototypes\
				-Wmissing-prototypes\
				-fno-stack-protector

LDFLAGS		= -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map

OBJS		= 	$(BUILD_DIR)/main.o \
				$(BUILD_DIR)/init.o \
				$(BUILD_DIR)/interrupt.o \
				$(BUILD_DIR)/timer.o \
				$(BUILD_DIR)/kernel.o \
				$(BUILD_DIR)/print.o \
				$(BUILD_DIR)/debug.o \
				$(BUILD_DIR)/string.o \
				$(BUILD_DIR)/bitmap.o \
				$(BUILD_DIR)/memory.o \
				$(BUILD_DIR)/thread.o \
				$(BUILD_DIR)/list.o \
				$(BUILD_DIR)/switch.o \
				$(BUILD_DIR)/sync.o \
				$(BUILD_DIR)/console.o \
				$(BUILD_DIR)/keyboard.o \
				$(BUILD_DIR)/ioqueue.o \
				$(BUILD_DIR)/tss.o \
				$(BUILD_DIR)/process.o \
				$(BUILD_DIR)/syscall_init.o \
				$(BUILD_DIR)/syscall.o \
				$(BUILD_DIR)/stdio.o \
				$(BUILD_DIR)/stdio_kernel.o \
				$(BUILD_DIR)/ide.o \
				$(BUILD_DIR)/fs.o \
				$(BUILD_DIR)/dir.o \
				$(BUILD_DIR)/file.o \
				$(BUILD_DIR)/inode.o \
				$(BUILD_DIR)/fork.o \
				$(BUILD_DIR)/shell.o \
				$(BUILD_DIR)/cmd_builtin.o \
				$(BUILD_DIR)/exec.o

# C
# kernel
$(BUILD_DIR)/main.o: kernel/main.c \
					lib/kernel/print.h lib/stdint.h kernel/init.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o: kernel/init.c \
					kernel/init.h lib/kernel/print.h lib/stdint.h kernel/interrupt.h device/timer.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: kernel/interrupt.c \
						kernel/interrupt.h lib/stdint.h kernel/global.h lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o: kernel/debug.c \
					kernel/debug.h lib/kernel/print.h lib/stdint.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o: kernel/memory.c 
	$(CC) $(CFLAGS) $< -o $@

# device
$(BUILD_DIR)/timer.o: device/timer.c \
					device/timer.h lib/stdint.h lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/console.o: device/console.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o: device/keyboard.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ioqueue.o: device/ioqueue.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ide.o: device/ide.c
	$(CC) $(CFLAGS) $< -o $@

# device
$(BUILD_DIR)/fs.o: fs/fs.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/dir.o: fs/dir.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/file.o: fs/file.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/inode.o : fs/inode.c
	$(CC) $(CFLAGS) $< -o $@

# lib
$(BUILD_DIR)/string.o: lib/string.c 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio.o: lib/stdio.c 
	$(CC) $(CFLAGS) $< -o $@

# lib/kernel
$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o: lib/kernel/list.c 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio_kernel.o: lib/kernel/stdio_kernel.c 
	$(CC) $(CFLAGS) $< -o $@

# lib/user
$(BUILD_DIR)/syscall.o: lib/user/syscall.c
	$(CC) $(CFLAGS) $< -o $@

# thread
$(BUILD_DIR)/thread.o: thread/thread.c 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/sync.o: thread/sync.c 
	$(CC) $(CFLAGS) $< -o $@

# shell
$(BUILD_DIR)/shell.o: shell/shell.c 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/cmd_builtin.o: shell/cmd_builtin.c
	$(CC) $(CFLAGS) $< -o $@

# userprog
$(BUILD_DIR)/tss.o: userprog/tss.c 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/process.o: userprog/process.c 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fork.o: userprog/fork.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall_init.o: userprog/syscall_init.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/exec.o: userprog/exec.c
	$(CC) $(CFLAGS) $< -o $@

# assembly	
# kernel
$(BUILD_DIR)/kernel.o: kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@

# thread
$(BUILD_DIR)/switch.o: thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@

# lib/kernel
$(BUILD_DIR)/print.o: lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@

# link all of object file
$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

.PHONY : mk_dir hd mbr loader clean all

mk_dir:
	if[[! -d $(BUILD_DIR)]]; then mkdir $(BUILD_DIR); fi

hd:
	dd if=$(BUILD_DIR)/kernel.bin of=/home/ml/bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc

mbr:
	$(AS) mbr.S -o $(BUILD_DIR)/mbr.bin -I include/
	dd if=$(BUILD_DIR)/mbr.bin of=/home/ml/bochs/hd60M.img bs=512 count=1 conv=notrunc

loader:
	$(AS) loader.S -o $(BUILD_DIR)/loader.bin -I include/
	dd if=$(BUILD_DIR)/loader.bin of=/home/ml/bochs/hd60M.img bs=512 count=6 seek=2 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -rf ./*

build: $(BUILD_DIR)/kernel.bin

all: mk_dir build hd