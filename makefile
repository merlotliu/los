BUILD_DIR 	= ./build
ENTRY_POINT = 0xc0001500

AS			= nasm
CC 			= gcc
LD 			= ld
LIB 		= -I kernel/ -I device/ -I thread/ -I lib/ -I lib/kernel -I lib/user/ 

ASFLAGS 	= -f elf
CFLAGS		= -m32 -Wall $(LIB) -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes
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
				$(BUILD_DIR)/ioqueue.o

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

# lib
$(BUILD_DIR)/string.o: lib/string.c 
	$(CC) $(CFLAGS) $< -o $@

# lib/kernel
$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o: lib/kernel/list.c 
	$(CC) $(CFLAGS) $< -o $@

# thread
$(BUILD_DIR)/thread.o: thread/thread.c 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/sync.o: thread/sync.c 
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

.PHONY : mk_dir hd clean all

mk_dir:
	if[[! -d $(BUILD_DIR)]]; then mkdir $(BUILD_DIR); fi

hd:
	dd if=$(BUILD_DIR)/kernel.bin of=/home/ml/bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -rf ./*

build: $(BUILD_DIR)/kernel.bin

all: mk_dir build hd