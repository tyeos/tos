# 关于磁盘：

## 盘片、扇区、磁头、磁道、柱面：

    磁盘从上到下有多个盘片。
    每个盘片圆用多条直径等分成多个扇形，每个扇形称为一个扇区。
    每个盘片有上下两个盘面，每个盘面有一个磁头，磁头号由上到下从0编号。
    每个盘片从中心向外分为多个(环行)磁道，磁道号由内到外从0开始。
    同一时刻所有的磁头指向的磁道号始终相同，所以磁道号也叫柱面号。



## 分区方式：

### 柱面分区：

	由多个编号连续的柱面组成，同一个柱面不可分配给多个分区。

### 扇区分区：

	由多个编号连续的扇区组成，同一个扇区不可分配给多个分区。



## 磁盘分区表（DPT, Disk Partition Table) ：

    在硬盘的MBR中有个64字节“固定大小”的数据结构，这就是分区表，分区表中的每个表项就是一个分区的“描述符”，表项大小是16字节，因此64字节的分区表总共可容纳4个表项，即硬盘仅支持4个分区。

### 主分区、扩展分区、子扩展分区：

    硬盘的4个分区中任意一个都可以作为扩展分区（在分区表项中通过属性来判断分区类型）。扩展分区是可选项，有没有都行，但最多只有1个，1个扩展分区在理论上可被划分出任意多的子扩展分区，因此1个扩展分区足够了。注意，这里所说的是理论上支持无限多的划分，由于硬件上的限制，分区数量也变得有限，比如ide硬盘只支持63个分区，scsi硬盘只支持15个分区。硬盘本来没有扩展分区的概念，为了突破4个分区的限制才提出了扩展分区，为了区别这一概念，将剩下的3个区称为主分区。

### 逻辑分区：

    在分区描述符中有个属性是文件系统id,它表示文件系统的类型，为支持更多的分区，硬盘厂商规定，专门增加一种id属性值（id为5), 用来表示该分区可被再次划分出更多的子分区，这就是逻辑分区。逻辑分区只存在于扩展分区，它属于扩展分区的子集。

### 分区中的存储数据：

    要想使用分区，就离不开分区表，逻辑分区也是分区，为了使用它，也需要有元信息来描述它的范围、边界、类型等信息，因此在子扩展分区中也要有分区表来描述这些逻辑分区。
    分区表本身也要在子扩展分区中占磁盘空间，因此实际情况是每个子扩展分区的空间并不是只有逻辑分区，在每个子扩展分区中最开始的扇区（此扇区称为EBR引导扇区）用于存储此子扩展分区中的分区表，
    此扇区中的内容也是前446字节是引导程序，中间64字节是分区表，后2字节是0x55和0xAA,它同MBR引导扇区的结构相同。
    紧随其后的是空闲的一部分扇区，其余剩下的大部分扇区才被用作存储数据的分区，即逻辑分区。因此，子扩展分区包含逻辑分区。

### 引导扇区：

    扩展分区被划分出多个子扩展分区，每个子扩展分区都有自己的分区表，所以子扩展分区在逻辑上相当于单独的硬盘，各分区表在各个子扩展分区最开始的扇区中，该扇区同MBR引导扇区结构相同，由于是经扩展分区划分出来的，所以它们称为EBR,即扩展引导记录。
    MBR只有1个，EBR理论上有无限个，MBR和EBR所在的扇区统称为引导扇区，它们的结构是相同的，MBR中有的EMR中也有。各子扩展分区的结构同整个硬盘结构一样，最开始的扇区都是引导扇区，中间都是空闲一小部分扇区，最后的大片扇区空间作为数据存储的分区。

### OBR引导扇区：

    由于扩展分区采用了链式分区表，理论上可以存在无限个分区表，支持无限个逻辑分区。每一个逻辑分区所在的子扩展分区都有一个与MBR结构相同的EBR,EBR中分区表的第一分区表项用来描述所包含的逻辑分区的元信息，第二分区表项用来描述下一个子扩展分区的地址，第三、四表项未用到。位于EBR中的分区表相当于链表中的结点，第一个分区表项存的是分区数据，第二个分区表项存的是后继分区的指针。
    这两个分区表项都是指向一个分区的起始，起始地址都是个扇区地址，只不过第一个分区表项指向的是该逻辑分区最开始的扇区，此扇区称为操作系统引导扇区，即OBR引导扇区。第二个分区表项指向下一个子扩展分区的EBR引导扇区。
    OBR引导扇区不是EBR引导扇区。EBR不属于分区之内，不属于操作系统管理的范围，而OBR引导扇区位于分区（主分区和逻辑分区）最开始的扇区，属于操作系统管理的范围。

## 分区表项结构：


|偏移量  | 数据宽度  |描述|
|:----|:----|:----|
|0      |  1|活动分区标记，此标记有两种取值，0x80和0，其他值非法。<br />0x80表示活动分区，此分区的引导扇区中包含引导程序，可引导。<br />0表示非活动分区，不可引导。|
|1      |  1|分区起始磁头号|
|2      |  1|分区起始扇区号|
|3      |  1|分区起始柱面号|
|4      |  1|文件系统类型ID, 如0表示不可识别的文件系统，1表示FAT32|
|5      |  1|分区结束磁头号|
|6      |  1|分区结束扇区号|
|7      | 1 |分区结束柱面号|
|8     |  4|分区起始偏移扇区|
|12 | 4 |分区容量扇区数|

### 活动分区:

    活动分区是指引导程序所在的分区，活动分区标记是给MBR或其他需要移交CPU使用权的程序看的，它们通过此位来判断该分区的引导扇区中是否有可执行的程序，也就是引导程序，这个引导程序通常是操作系统内核加载器，故此引导程序通常被称为操作系统引导记录，即OBR(OS Boot Record)。如果MBR发现该分区表项的活动分区标记为0x80,这就表示该分区的引导扇区中有引导程序（这是MBR与分区工具或操作系统约定好的）,MBR就将CPU使用权交给此引导程序，如果此引导程序是操作系统或其加载器，此时操作系统便掌握了CPU使用权，也就是大家平时所说的加载内核。
    这里一直说的“分区引导扇区”是位于分区最开始的扇区，是分区引导程序所在的扇区，由于此引导程序通常都是操作系统内核加载器，故此扇区被称为操作系统引导扇区，也就是OBR所在的扇区，即OBR引导扇区。
    OBR引导扇区并不是EBR或MBR引导扇区，它们虽然都包含引导程序，并且都以0×55和0xaa结束，但它们最大的区别是分区表只包含在MBR和EBR中，OBR中可没有分区表。MBR和EBR所在的扇区不属于分区范围之内，它们是由分区工具创建并管理的，因此不归操作系统管理，操作系统不可以随意往里面写数据，尽管操作系统有能力这样做。而OBR引导扇区是分区中最开始的扇区，归操作系统的文件系统管理，因此操作系统通常往OBR引导扇区中添加内核加载器的代码，供MBR调用以实现操作系统自举，总之OBR引导扇区中绝对不包括分区表。



## 硬盘布局：

    | MBR引导扇区(512B) \
    | 空闲的多个扇区(一般62个扇区) \
    | 主分区及扩展分区所在的空间(一般从第63扇区开始, 即偏移扇区数0x3F, 偏移字节数0x7E00) |

### (MBR/EBR)引导扇区:

    | 引导程序 (0x0~0x1BD, 446B) \
    | 分区表 (0x1BE~0x1FD, 64B) \
    | 魔数 (0x1FE~0x1FF, 0xAA55, 2B) |

### 分区表：

    | 主分区1(16B) \
    | 主分区2(16B) \
    | 主分区3(16B) \
    | 主分区4/总扩展分区(16B) |

### 总扩展分区：

    | 子扩展分区1 \
    | 子扩展分区2 \
    | 子扩展分区3 \
    | 子扩展分区4 |

### 子扩展分区（在逻辑上相当于单独的硬盘）:

    | EBR引导扇区(512B) \
    | 空闲扇区(一般62个扇区) \
    | 逻辑分区(用作存储数据的分区，一般从第63扇区开始, 具体作用取决于EBR引导扇区中的分区表配置) |

### 分区表项(16B)：

    | 分区标记(1B) \
    | 分区起始磁头号(1B) \
    | 分区起始扇区号(1B) \
    | 分区起始柱面号(1B) \
    | 分区文件系统类型ID(1B) \
    | 分区结束磁头号(1B) \
    | 分区结束扇区号(1B) \
    | 分区结束柱面号(4B) \
    | 分区起始偏移扇区(4B) |



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



### 2.5.1 说明

	CHS=162/16/63：
	
	162 为柱面数
	16 为磁头数
	62 为每磁道扇区数	
	
	生成的硬盘占用字节大小：162*16*63*512 = 83607552 bytes
	
	ata0-master：
	
	bochs 配置文件添加示例



# fdisk 命令说明

注：以下仅为命令操作演示示例，非本项目实际使用参数



## 磁盘分区命令
```shell
$ fdisk ./hd80M.img

Welcome to fdisk (util-linux 2.27.1).
Changes will remain in memory only, until you decide to write them.
Be careful before using the write command.

Device does not contain a recognized partition table.
Created a new DOS disklabel with disk identifier 0x7b1773e5.
```



## 查看帮助

```shell
Command (m for help): m

Help:

  DOS (MBR)
   a   toggle a bootable flag
   b   edit nested BSD disklabel
   c   toggle the dos compatibility flag

  Generic
   d   delete a partition
   F   list free unpartitioned space
   l   list known partition types
   n   add a new partition
   p   print the partition table
   t   change a partition type
   v   verify the partition table
   i   print information about a partition

  Misc
   m   print this menu
   u   change display/entry units
   x   extra functionality (experts only)

  Script
   I   load disk layout from sfdisk script file
   O   dump disk layout to sfdisk script file

  Save & Exit
   w   write table to disk and exit
   q   quit without saving changes

  Create a new label
   g   create a new empty GPT partition table
   G   create a new empty SGI (IRIX) partition table
   o   create a new empty DOS partition table
   s   create a new empty Sun partition table
```

## 查看分区基本信息

```shell
Command (m for help): p
Disk ./hd80M.img: 79.8 MiB, 83607552 bytes, 163296 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x7b1773e5
```

## 创建DOS分区表

Command (m for help): o
Created a new DOS disklabel with disk identifier 0xdb7336bd.

## 创建分区

```shell
Command (m for help): n
Partition type
   p   primary (0 primary, 0 extended, 4 free)
   e   extended (container for logical partitions)
```

### 创建主分区

```shell
Select (default p): p
```

### 选择主分区号

```shell
Partition number (1-4, default 1): 
```

### 选择主分区开始扇区

```shell
First sector (2048-163295, default 2048): 
```

### 设置主分区 结束扇区/分区大小

```shell
Last sector, +sectors or +size{K,M,G,T,P} (2048-163295, default 163295): +20M

Created a new partition 1 of type 'Linux' and of size 20 MiB.
```



### 查看主分区信息

```shell
Command (m for help): p
Disk ./hd80M.img: 79.8 MiB, 83607552 bytes, 163296 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x7b1773e5

Device       Boot Start   End Sectors Size Id Type
./hd80M.img1       2048 43007   40960  20M 83 Linux
```



## 继续创建分区

```shell
Command (m for help): n
Partition type
   p   primary (1 primary, 0 extended, 3 free)
   e   extended (container for logical partitions)
```

### 创建扩展分区

```shell
Select (default p): e
```

### 选择扩展分区号

```shell
Partition number (2-4, default 2): 4
```



### 选择主分区开始扇区

```shell
First sector (43008-163295, default 43008): 
```

### 设置扩展分区 结束扇区/分区大小

```shell
Last sector, +sectors or +size{K,M,G,T,P} (43008-163295, default 163295): +30M

Created a new partition 4 of type 'Extended' and of size 30 MiB.
```



### 查看当前分区信息

```shell
Command (m for help): p
Disk ./hd80M.img: 79.8 MiB, 83607552 bytes, 163296 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x7b1773e5

Device       Boot Start    End Sectors Size Id Type
./hd80M.img1       2048  43007   40960  20M 83 Linux
./hd80M.img4      43008 104447   61440  30M  5 Extended
```

## 继续创建分区

```shell
Command (m for help): n
Partition type
   p   primary (1 primary, 1 extended, 2 free)
   l   logical (numbered from 5)
```

### 创建逻辑分区

```shell
Select (default p): l

Adding logical partition 5
```

### 选择逻辑分区开始扇区

```shell
First sector (45056-104447, default 45056): 
```

### 设置逻辑分区 结束扇区/分区大小

```shell
Last sector, +sectors or +size{K,M,G,T,P} (45056-104447, default 104447): +10M

Created a new partition 5 of type 'Linux' and of size 10 MiB.
```



### 查看当前分区信息

```shell
Command (m for help): p
Disk ./hd80M.img: 79.8 MiB, 83607552 bytes, 163296 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x7b1773e5

Device       Boot Start    End Sectors Size Id Type
./hd80M.img1       2048  43007   40960  20M 83 Linux
./hd80M.img4      43008 104447   61440  30M  5 Extended
./hd80M.img5      45056  65535   20480  10M 83 Linux
```



## 查看所有已知分区类型

```shell
Command (m for help): l

 0  Empty           24  NEC DOS         81  Minix / old Lin bf  Solaris        
 1  FAT12           27  Hidden NTFS Win 82  Linux swap / So c1  DRDOS/sec (FAT-
 2  XENIX root      39  Plan 9          83  Linux           c4  DRDOS/sec (FAT-
 3  XENIX usr       3c  PartitionMagic  84  OS/2 hidden or  c6  DRDOS/sec (FAT-
 4  FAT16 <32M      40  Venix 80286     85  Linux extended  c7  Syrinx         
 5  Extended        41  PPC PReP Boot   86  NTFS volume set da  Non-FS data    
 6  FAT16           42  SFS             87  NTFS volume set db  CP/M / CTOS / .
 7  HPFS/NTFS/exFAT 4d  QNX4.x          88  Linux plaintext de  Dell Utility   
 8  AIX             4e  QNX4.x 2nd part 8e  Linux LVM       df  BootIt         
 9  AIX bootable    4f  QNX4.x 3rd part 93  Amoeba          e1  DOS access     
 a  OS/2 Boot Manag 50  OnTrack DM      94  Amoeba BBT      e3  DOS R/O        
 b  W95 FAT32       51  OnTrack DM6 Aux 9f  BSD/OS          e4  SpeedStor      
 c  W95 FAT32 (LBA) 52  CP/M            a0  IBM Thinkpad hi ea  Rufus alignment
 e  W95 FAT16 (LBA) 53  OnTrack DM6 Aux a5  FreeBSD         eb  BeOS fs        
 f  W95 Ext'd (LBA) 54  OnTrackDM6      a6  OpenBSD         ee  GPT            
10  OPUS            55  EZ-Drive        a7  NeXTSTEP        ef  EFI (FAT-12/16/
11  Hidden FAT12    56  Golden Bow      a8  Darwin UFS      f0  Linux/PA-RISC b
12  Compaq diagnost 5c  Priam Edisk     a9  NetBSD          f1  SpeedStor      
14  Hidden FAT16 <3 61  SpeedStor       ab  Darwin boot     f4  SpeedStor      
16  Hidden FAT16    63  GNU HURD or Sys af  HFS / HFS+      f2  DOS secondary  
17  Hidden HPFS/NTF 64  Novell Netware  b7  BSDI fs         fb  VMware VMFS    
18  AST SmartSleep  65  Novell Netware  b8  BSDI swap       fc  VMware VMKCORE 
1b  Hidden W95 FAT3 70  DiskSecure Mult bb  Boot Wizard hid fd  Linux raid auto
1c  Hidden W95 FAT3 75  PC/IX           bc  Acronis FAT32 L fe  LANstep        
1e  Hidden W95 FAT1 80  Old Minix       be  Solaris boot    ff  BBT            
```

### 修改分区类型

```shell
Command (m for help): t
```

### 修改逻辑分区类型

```shell
Partition number (1,4,5, default 5): 5
```

### 查看支持设置的所有类型

```shell
Partition type (type L to list all types): L

 0  Empty           24  NEC DOS         81  Minix / old Lin bf  Solaris        
 1  FAT12           27  Hidden NTFS Win 82  Linux swap / So c1  DRDOS/sec (FAT-
 2  XENIX root      39  Plan 9          83  Linux           c4  DRDOS/sec (FAT-
 3  XENIX usr       3c  PartitionMagic  84  OS/2 hidden or  c6  DRDOS/sec (FAT-
 4  FAT16 <32M      40  Venix 80286     85  Linux extended  c7  Syrinx         
 5  Extended        41  PPC PReP Boot   86  NTFS volume set da  Non-FS data    
 6  FAT16           42  SFS             87  NTFS volume set db  CP/M / CTOS / .
 7  HPFS/NTFS/exFAT 4d  QNX4.x          88  Linux plaintext de  Dell Utility   
 8  AIX             4e  QNX4.x 2nd part 8e  Linux LVM       df  BootIt         
 9  AIX bootable    4f  QNX4.x 3rd part 93  Amoeba          e1  DOS access     
 a  OS/2 Boot Manag 50  OnTrack DM      94  Amoeba BBT      e3  DOS R/O        
 b  W95 FAT32       51  OnTrack DM6 Aux 9f  BSD/OS          e4  SpeedStor      
 c  W95 FAT32 (LBA) 52  CP/M            a0  IBM Thinkpad hi ea  Rufus alignment
 e  W95 FAT16 (LBA) 53  OnTrack DM6 Aux a5  FreeBSD         eb  BeOS fs        
 f  W95 Ext'd (LBA) 54  OnTrackDM6      a6  OpenBSD         ee  GPT            
10  OPUS            55  EZ-Drive        a7  NeXTSTEP        ef  EFI (FAT-12/16/
11  Hidden FAT12    56  Golden Bow      a8  Darwin UFS      f0  Linux/PA-RISC b
12  Compaq diagnost 5c  Priam Edisk     a9  NetBSD          f1  SpeedStor      
14  Hidden FAT16 <3 61  SpeedStor       ab  Darwin boot     f4  SpeedStor      
16  Hidden FAT16    63  GNU HURD or Sys af  HFS / HFS+      f2  DOS secondary  
17  Hidden HPFS/NTF 64  Novell Netware  b7  BSDI fs         fb  VMware VMFS    
18  AST SmartSleep  65  Novell Netware  b8  BSDI swap       fc  VMware VMKCORE 
1b  Hidden W95 FAT3 70  DiskSecure Mult bb  Boot Wizard hid fd  Linux raid auto
1c  Hidden W95 FAT3 75  PC/IX           bc  Acronis FAT32 L fe  LANstep        
1e  Hidden W95 FAT1 80  Old Minix       be  Solaris boot    ff  BBT            
```

### 随便设置一个不存在的类型

```shell
Partition type (type L to list all types): 66

Changed type of partition 'Linux' to 'unknown'.
```

### 查看分区信息

```shell
Command (m for help): p
Disk ./hd80M.img: 79.8 MiB, 83607552 bytes, 163296 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x7b1773e5

Device       Boot Start    End Sectors Size Id Type
./hd80M.img1       2048  43007   40960  20M 83 Linux
./hd80M.img4      43008 104447   61440  30M  5 Extended
./hd80M.img5      45056  65535   20480  10M 66 unknown
```

## 保存退出

```shell
Command (m for help): w
The partition table has been altered.
Syncing disks.
```



