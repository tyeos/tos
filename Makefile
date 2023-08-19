BUILD := ./build
KERNEL_PATH := ./kernel

MBR_O := mbr.o
LOADER_O := loader.o

HD_IMG := hd.img

asm: ${BUILD}/${MBR_O} ${BUILD}/${LOADER_O}
${BUILD}/%.o: ${KERNEL_PATH}/%.asm
	$(shell mkdir -p ${BUILD})
	nasm $< -o $@

img:
	$(shell rm -f ${BUILD}/${HD_IMG})
	# 创建硬盘镜像文件，-hd指定镜像大小，单位M
	bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat ${BUILD}/${HD_IMG}
	# MBR装在0盘0道1扇区
	dd if=${BUILD}/${MBR_O} of=${BUILD}/${HD_IMG} bs=512 seek=0 count=1 conv=notrunc
	# 给loader分配4个扇区
	dd if=${BUILD}/${LOADER_O} of=${BUILD}/${HD_IMG} bs=512 seek=1 count=4 conv=notrunc

bochs:
	bochs -q -f bochsrc

clean:
	$(shell rm -rf ${BUILD})
