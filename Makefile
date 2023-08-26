BUILD := ./build
KERNEL_PATH := ./kernel

BUILD_O_FILES := $(BUILD)/mbr.o $(BUILD)/loader.o
BUILD_HD_IMG := $(BUILD)/hd.img

mkdir:
	$(shell mkdir -p $(BUILD))

$(BUILD)/%.o: $(KERNEL_PATH)/%.asm
	nasm $< -o $@

build: mkdir $(BUILD_O_FILES)

install:
	$(shell rm -f $(BUILD_HD_IMG))
	# 创建硬盘镜像文件，-hd指定镜像大小，单位M
	bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $(BUILD_HD_IMG)
	# MBR装在0盘0道1扇区
	dd if=$(word 1, $(BUILD_O_FILES)) of=$(BUILD_HD_IMG) bs=512 seek=0 count=1 conv=notrunc
	# 给loader分配4个扇区
	dd if=$(word 2, $(BUILD_O_FILES)) of=$(BUILD_HD_IMG) bs=512 seek=1 count=4 conv=notrunc

all: build install

bochs:
	bochs -q -f bochsrc

clean:
	$(shell rm -rf $(BUILD))
