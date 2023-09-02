BUILD := ./build

MBR_PATH := mbr
BRIDGE_PATH := bridge
KERNEL_PATH := kernel
CHR_DRV_PATH := $(KERNEL_PATH)/chr_drv

BUILD_MBR_O_FILES := $(BUILD)/$(MBR_PATH)/mbr.o $(BUILD)/$(MBR_PATH)/loader.o
BUILD_BRIDGE_O_FILES := $(BUILD)/$(BRIDGE_PATH)/head.o $(BUILD)/$(BRIDGE_PATH)/io.o $(BUILD)/$(BRIDGE_PATH)/interrupt.o
BUILD_KERNEL_O_FILES := $(BUILD)/$(KERNEL_PATH)/main.o $(BUILD)/$(KERNEL_PATH)/string.o \
	$(BUILD)/$(KERNEL_PATH)/vsprintf.o $(BUILD)/$(KERNEL_PATH)/printk.o \
	$(BUILD)/$(KERNEL_PATH)/gdt.o $(BUILD)/$(KERNEL_PATH)/idt.o \
	$(BUILD)/$(KERNEL_PATH)/pic_handler.o \
	$(BUILD)/$(CHR_DRV_PATH)/console.o $(BUILD)/$(CHR_DRV_PATH)/keyboard.o $(BUILD)/$(CHR_DRV_PATH)/clock.o

BUILD_KERNEL_ELF := $(BUILD)/kernel.elf
BUILD_KERNEL_BIN := $(BUILD)/kernel.bin
BUILD_KERNEL_MAP := $(BUILD)/kernel.map

BUILD_HD_IMG := $(BUILD)/hd.img


# 由于自己写的内核很多东西都没有，比如标准库的打印函数，这里把相关内容都屏蔽掉
CFLAGS := -m32 					# 32位程序
CFLAGS += -masm=intel
CFLAGS += -fno-builtin			# 不需要gcc内置函数
CFLAGS += -nostdinc				# 不需要标准头文件
CFLAGS += -fno-pic				# 不需要位置无关的代码  position independent code
CFLAGS += -fno-pie				# 不需要位置无关的可执行程序 position independent executable
CFLAGS += -nostdlib				# 不需要标准库
CFLAGS += -fno-stack-protector	# 不需要栈保护
CFLAGS := $(strip $(CFLAGS))

# -g参数在nasm和gcc中，都是用于生成调试信息以供Debug处理的参数选项
DEBUG := -g

mkdir:
	$(shell mkdir -p $(BUILD)/$(MBR_PATH))
	$(shell mkdir -p $(BUILD)/$(BRIDGE_PATH))
	$(shell mkdir -p $(BUILD)/$(CHR_DRV_PATH))

$(BUILD)/$(KERNEL_PATH)/%.o: $(KERNEL_PATH)/%.c
	gcc $(CFLAGS) $(DEBUG) -c $< -o $@

$(BUILD)/$(BRIDGE_PATH)/%.o: $(BRIDGE_PATH)/%.asm
	# 运行在linux下，所以采用elf格式，加-g可生成调试符号，要和C程序一起打包
	nasm -f elf32 $(DEBUG) $< -o $@

$(BUILD)/$(MBR_PATH)/%.o: $(MBR_PATH)/%.asm
	nasm $< -o $@


kernel: $(BUILD_BRIDGE_O_FILES) $(BUILD_KERNEL_O_FILES)
	$(shell rm -f $(BUILD_KERNEL_ELF) $(BUILD_KERNEL_MAP) $(BUILD_KERNEL_BIN))
	# 生成的elf文件可用于linux下的调试
	# 注：在CLion中添加RemoteDebug时，Symbol file 填写该生成文件的路径
	ld -m elf_i386 $^ -o $(BUILD_KERNEL_ELF) -Ttext 0x1500
	# 查看代码段
	readelf -S $(BUILD_KERNEL_ELF) -W
	# 导出符号地址
	nm $(BUILD_KERNEL_ELF) | sort > $(BUILD_KERNEL_MAP)
	# 生成二进制文件，用于打包到镜像中
	objcopy -O binary $(BUILD_KERNEL_ELF) $(BUILD_KERNEL_BIN)

build: mkdir $(BUILD_MBR_O_FILES) kernel

install:
	$(shell rm -f $(BUILD_HD_IMG))
	# 创建硬盘镜像文件，-hd指定镜像大小，单位M
	bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $(BUILD_HD_IMG)
	# MBR装在0盘0道1扇区
	dd if=$(word 1, $(BUILD_MBR_O_FILES)) of=$(BUILD_HD_IMG) bs=512 seek=0 count=1 conv=notrunc
	# 给loader分配4个扇区
	dd if=$(word 2, $(BUILD_MBR_O_FILES)) of=$(BUILD_HD_IMG) bs=512 seek=1 count=4 conv=notrunc
	# 给system分配60个扇区
	dd if=$(BUILD_KERNEL_BIN) of=$(BUILD_HD_IMG) bs=512 seek=5 count=60 conv=notrunc

all: build install

bochs: all
	bochs -q -f bochsrc

qemu: all
	# -m megs: Sets guest startup RAM size to megs megabytes
	# -boot c: first hard disk
	qemu-system-i386 -m 32M -boot c -drive file=$(BUILD_HD_IMG),format=raw,index=0,media=disk

qemu_gdb: all
	# -S: Do not start CPU at startup (you must type 'c' in the monitor).
	# -s: Shorthand for -gdb tcp::1234, i.e. open a gdbserver on TCP port 1234.
	# 注：在CLion中添加RemoteDebug时，'target remote' args 填写 127.0.0.1:1234
	qemu-system-x86_64 -m 32M -S -s -drive file=$(BUILD_HD_IMG),format=raw,index=0,media=disk

clean:
	$(shell rm -rf $(BUILD))
