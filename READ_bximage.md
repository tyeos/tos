# bximage 命令说明：

注：该命令是安装bochs后附带的命令，在系统bin目录下



## 1, 直接创建硬盘命令

```shell
bximage -q -hd=80 -func=create -sectsize=512 -imgmode=flat h80M.img
```
```shell
========================================================================
                                bximage
  Disk Image Creation / Conversion / Resize and Commit Tool for Bochs
         $Id: bximage.cc 14091 2021-01-30 17:37:42Z sshwarts $
========================================================================

Creating hard disk image 'h8.img' with CHS=162/16/63 (sector size = 512)

The following line should appear in your bochsrc:
  ata0-master: type=disk, path="h8.img", mode=flat
```



## 2, 分步创建硬盘


### 2.1, 磁盘操作命令
```shell
bximage
```

```shell
========================================================================
                                bximage
  Disk Image Creation / Conversion / Resize and Commit Tool for Bochs
         $Id: bximage.cc 14091 2021-01-30 17:37:42Z sshwarts $
========================================================================

1. Create new floppy or hard disk image
2. Convert hard disk image to other format (mode)
3. Resize hard disk image
4. Commit 'undoable' redolog to base image
5. Disk image info

0. Quit

Please choose one [0] 1
```

### 2.2, 选择硬盘
    Create image
    
    Do you want to create a floppy disk image or a hard disk image?
    Please type hd or fd. [hd] 

### 2.3, 选择扇区大小
    What kind of image should I create?
    Please type flat, sparse, growing, vpc or vmware4. [flat] 
    
    Choose the size of hard disk sectors.
    Please type 512, 1024 or 4096. [512] 

### 2.4，选择磁盘大小（单位MB）
    Enter the hard disk size in megabytes, between 10 and 8257535
    [10] 80

### 2.5，选择生成文件名
    What should be the name of the image?
    [c.img] hd80M.img
    
    Creating hard disk image 'hd80M.img' with CHS=162/16/63 (sector size = 512)
    
    The following line should appear in your bochsrc:
      ata0-master: type=disk, path="hd80M.img", mode=flat



注：最后返回的 ata0-master 信息即为 bochs 配置文件添加示例。
