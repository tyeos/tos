//
// Created by toney on 23-9-8.
//

/*
 ---------------------------------------------------------------------------------------------------
 101012分页模式：
 ---------------------------------------------------------------------------------------------------
 虚拟地址规划（4GB空间）：
     -----------------------------------------------
     |    0xC0000000 ~ 0xFFFFFFFF (kernel: 1GB)    |
     -----------------------------------------------
     |                                             |
     |       0x0 ~ 0xBFFFFFFF (user: 3GB)          |
     |                                             |
     -----------------------------------------------

 页目录表(总4K) 和 页表(总4M) 规划：
     ----------------------------------------------------------------------
     |                               | [0x4FF000 ~ 0x4FFFFF] (PT 767: 4K) |
     ·                               ·             ...                    ·
     ·                               ·             ...                    ·
     |                               | [0x400000 ~ 0x400FFF] (PT 512: 4K) |
     ·                               ·             ...                    ·
     ·    user (PT|PDE: 4K * 767)    ·             ...                    ·
     ·                               ·             ...                    ·
     |                               | [0x300000 ~ 0x300FFF] (PT 256: 4K) | => 页目录的第0x100项指向这里
     ·                               ·             ...                    ·
     ·                               ·             ...                    ·
     ·                               ·             ...                    ·
     |                               | [0x200000 ~ 0x200FFF] (PT 1: 4K)   | => 页目录的第1项指向这里
     ----------------------------------------------------------------------
     |                               | [0x1FF000 ~ 0x1FFFFF] (PT 1022: 4K)| => 页目录的第0x3FE项指向这里
     ·                               ·                ...                 ·
     ·   kernel (PT|PDE: 4K * 254)   ·                ...                 ·
     ·                               ·                ...                 ·
     |                               | [0x102000 ~ 0x102FFF] (PT 769: 4K) | => 页目录的第0x301项指向这里
     ----------------------------------------------------------------------
     ----------------------------------------------------------------------
     | kernel and user (PT|PDE: 4K)  | 0x101000 ~ 0x101FFF (PDE 0|968: 4K)| => 页目录的第0项和第0x300项同时指向这里, 即用户和内核的第0个页表都指向低4M物理地址
     ----------------------------------------------------------------------
     |                               | 0x100FFC ~ 0x100FFF (PDE 1023: 4B) | => 这里指向自身0x100000地址, 即将PDT自身最为最后一个PDE项
     ·                               ·             ...                    ·
     |                               | 0x100C00 ~ 0x100C03 (PDE 768: 4B)  | => 第0x300项, 这里往上是对应3GB以上的虚拟地址空间
     ·                               ·             ...                    ·
     | 0x100000 ~ 0x100FFF (PDT: 4K) | 0x100800 ~ 0x100803 (PDE 512: 4B)  |
     ·  (Also used for PDE 0: 4K)    ·             ...                    ·
     |                               | 0x100400 ~ 0x100403 (PDE 256: 4B)  |
     ·                               ·             ...                    ·
     |                               | 0x100000 ~ 0x100003 (PDE 0: 4B)    | => CR3 页目录表物理地址 = 0x100000
     ----------------------------------------------------------------------

 注: 页表(PT)地址和页目录项(PDE)中保持一直即可, 不一定非按上面的物理地址规划, 上图仅作参考实例.
     实际在以下程序中, 只有页目录表(PDT)存放的物理地址(0x100000~0x100FFF)最为关键, 该地址不仅作为页目录表, 还作为最后一个页表使用.
     其次就是第0张页表存放的物理地址(0x101000~0x101FFF), 用其指向低4MB物理内存(实际是为了用低1MB).
     至于其他都可随意, 比如可以将kernel和user的页表存放位置对换一下, 甚至分散到不同的内存区域也无所谓, 在页目录表中指向对应页表物理地址即可.

 ---------------------------------------------------------------------------------------------------
 cr3控制寄存器：
     又称为页目录基址寄存器(Page Directory BaseRegister, PDBR)，用于存储页表物理地址：

 结构（4字节）：
      -------------------------------------------------------------------------------------------------------------------------
      | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  | 15  | 14  | 13  | 12  |
      |                                                       页目录表物理地址 12~31位                                            |
      -------------------------------------------------------------------------------------------------------------------------
      -------------------------------------------------------------------------
      | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
      |                                         | PCD | PWT |                 |
      -------------------------------------------------------------------------

 ---------------------------------------------------------------------------------------------------
 段寄存器在不同场景的的定义：
    在实模式下，段寄存器存放的是物理段地址，段地址*16即物理地址（基址）。
    在保护模式下，段寄存器存放的是段选择子，通过段描述符可以得到线性地址：
        未开启分页的情况下，线性地址即为物理地址（基址）。
        开启分页的情况下，线性地址又称为虚拟地址，通过N级页表（N取决于分页模式）找到物理地址（基址）。

 101012模式下的虚拟地址结构：
      -------------------------------------------------------------
      | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  |
      |                页目录项索引（10位，可表示0~1023）              |
      -------------------------------------------------------------
      -------------------------------------------------------------
      | 21  | 20  | 19  | 18  | 17  | 16  | 15  | 14  | 13  | 12  |
      |                页表项索引（10位，可表示0~1023）                |
      -------------------------------------------------------------
      -------------------------------------------------------------------------
      | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
      |                       页内偏移量（12位，寻址范围4K）                       |
      -------------------------------------------------------------------------

 ---------------------------------------------------------------------------------------------------
*/


/*
 ---------------------------------------------------------------------------------------------------
 页目录项（PDE，Page Directory Entry，4字节）：
      -------------------------------------------------------------------------------------------------------------------------
      | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  | 15  | 14  | 13  | 12  |
      |                                                      页表物理页地址 12~31位                                              |
      -------------------------------------------------------------------------------------------------------------------------
      -------------------------------------------------------------------------
      | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
      |       AVL       |  G  |  0  |  D  |  A  | PCD | PWT |  US |  RW |  P  |
      -------------------------------------------------------------------------

 页表项（PTE，Page Table Entry，4字节）：
      -------------------------------------------------------------------------------------------------------------------------
      | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  | 15  | 14  | 13  | 12  |
      |                                                         物理页地址 12~31位                                              |
      -------------------------------------------------------------------------------------------------------------------------
      -------------------------------------------------------------------------
      | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
      |       AVL       |  G  | PAT |  D  |  A  | PCD | PWT |  US |  RW |  P  |
      -------------------------------------------------------------------------

 P：
     Present,意为存在位。若为1表示该页存在于物理内存中，若为0表示该表不在物理内存中。
     操作系统的页式虚拟内存管理便是通过P位和相应的pagefault异常来实现的。
 RW：
     Read/Write,意为读写位。若为1表示可读可写，若为0表示可读不可写。
 US：
     User/Supervisor,意为普通用户/超级用户位。
     若为1时，表示处于User级，任意级别(0、1、2、3)特权的程序都可以访问该页。
     若为O,表示处于Supervisor级，特权级别为3的程序不允许访问该页，该页只允许特权级别为0、1、2的程序可以访问。
 PWT：
     Page-level Write-Through,意为页级通写位，也称页级写透位。
     若为1表示此项采用通写方式表示该页不仅是普通内存，还是高速缓存。此项和高速缓存有关，
     “通写”是高速缓存的一种工作方式，本位用来间接决定是否用此方式改善该页的访问效率。这里用0即可。
 PCD：
     Page-level Cache Disable,意为页级高速缓存禁止位。若为1表示该页启用高速缓存，为0表示禁止将该页缓存。这里将其置为0。
 A：
     Accessed,意为访问位。若为1表示该页被CPU访问过啦，所以该位是由CPU设置的。
     页目录项和页表项中的A位也可以用来记录某一内存页的使用频率（操作系统定期将该位清0，统计一段时间内变成1的次数)，
     从而当内存不足时，可以将使用频率较低的页面换出到外存（如硬盘），
     同时将页目录项或页表项的P位置0，下次访问该页引起pagefault异常时，中断处理程序将硬盘上的页再次换入，同时将P位置1。
 D:
     Dirty,意为脏页位。当CPU对一个页面执行写操作时，就会设置对应页表项的D位为1。
     此项仅针对页表项有效，并不会修改页目录项中的D位。
 PAT:
     Page Attribute Table,意为页属性表位，能够在页面一级的粒度上设置内存属性。比较复杂，将此位置0即可。
 G:
     Global,意为全局位。由于内存地址转换也是颇费周折，先得拆分虚拟地址，然后又要查页目录，又要查页表的，
     所以为了提高获取物理地址的速度，将虚拟地址与物理地址转换结果存储在TLB(Translation Lookaside Buffer)中。
     此G位用来指定该页是否为全局页，为1表示是全局页，为0表示不是全局页。
     若为全局页，该页将在高速缓存TLB中一直保存，给出虚拟地址直接就出物理地址，无需那三步骤转换。
     由于TLB容量比较小（一般速度较快的存储设备容量都比较小)，所以这里面就存放使用频率较高的页面。
     清空TLB有两种方式，一是用invlpg指令针对单独虚拟地址条目清理，或者是重新加载cr3寄存器，这将直接清空TLB。
 AVL:
     意为Available位，表示可用，即软件，操作系统可用该位，CPU不理会该位的值。

 ---------------------------------------------------------------------------------------------------
*/


#include "../../include/mm.h"
#include "../../include/string.h"

#define PAGE_ATTR_US_RW_P 0b111 // 页属性，US=1, RW=1, P=1


void virtual_memory_init() {

    // 按规划申请的起始地址应为0x100000（即低1MB空间之上的第一个字节）
    // 页目录表 (Page Directory Table) , 大小为4KB (0x100000 ~ 0x100FFF)
    int *pdt = (int *) alloc_page();
    memset(pdt, 0, 1 << 12); // 清零

    /*
     *  0x300表示第768个页表占用的目录项，0x300~0x3FF的目录项用于内核空间
     *  即页表的0xC0000000~0xFFFFFFFF共计1G属于内核, 0x0~0xBFFFFFFF共计3G属于用户进程
     */

    // 页表 (Page Table) ，大小为4KB (0x101000 ~ 0x101FFF), 一个页表项 (Page Table Entry) 对应一个真实的4KB物理地址空间
    int *pt = (int *) alloc_page();
    // 页目录项 (Page Directory Entry)，其中存放页表信息
    pdt[0] = (int) pt | PAGE_ATTR_US_RW_P;

    // 将内核的首个目录项也指向首个页表，目的是想在操作系统高3GB以上的虚拟地址上访问到低1MB物理地址
    // 这样虚拟地址0xC0000000~0xC03FFFFF的内存地址即指向了低4MB空间之内的物理地址(实际只用1M)
    pdt[0x300] = pdt[0];

    // 创建页表项：每个页目录项(pde)对应一个页表，每个页表有1024个页表项，一个页表对应4MB物理地址空间
    // 这里仅创建前256个, 即映射到低1MB物理地址空间（0x0~0xFFFFF），剩余3MB(0x100000~0x3FFFFF)在该页表中保留不用
    for (int i = 0; i < 0x100; i++) {
        pt[i] = (i << 12) | PAGE_ATTR_US_RW_P;
    }

    // 将最后一个页目录项指向指向页目录表自己的地址，目的是为了以后动态操作页表
    pdt[0x3FF] = (int) pdt | PAGE_ATTR_US_RW_P;

    /*
     * 按规划，至此，虚拟内存到物理内存的映射关系应为：
     *    0x00000000~0x000FFFFF => 0x00000000~0x000FFFFF : 第0个页目录项(即用户空间的第0个页表)
     *    0xC0000000~0xC00FFFFF => 0x00000000~0x000FFFFF : 第768个页目录项(即内核空间的第0个页表)
     *    0xFFC00000~0xFFC00FFF => 0x00101000~0x00101FFF : 第1023个页目录项(内核最后1个页表)的第0个页表项
     *    0xFFF00000~0xFFF00FFF => 0x00101000~0x00101FFF : 第1023个页目录项(内核最后1个页表)的第768个页表项
     *    0xFFFFF000~0xFFFFFFFF => 0x00100000~0x00100FFF : 第1023个页目录项(内核最后1个页表)的第1023个页表项
     */

    // 创建页目录项（高1GB的内核虚拟地址空间）, 即第0x301~0x3FE项, 共254个
    for (int i = 0x301; i < 0x3FF; i++) {
        // 注: 每一个页表项都要对应4kB的物理空间, 即一个页表对应4MB的物理空间, 所以一个标准页表应该申请 4KB + 4MB 的物理空间

        // 分配页表地址
        int *pt = (int *) alloc_page(); // 0x301页表的物理地址0x102000
        // 更新页目录项中存放页表信息
        pdt[i] = (int) pt | PAGE_ATTR_US_RW_P;

        // 申请 4KB * 1024 大小的空间, 即一个页表对应的4MB物理地址空间
        // 每个页目录项(pde)对应一个页表，每个页表有1024个页表项(pte)
        void *pte_pages[0x400] = {0};
        alloc_pages(0x400, pte_pages); // 0x301页表项存储数据的物理地址范围0x103000~0x502FFF

        // 分配页表项地址
        for (int j = 0; j < 0x400; ++j) {
            // 每个页表项对应一个4K大小的物理地址，每个目录项(页表)可表示的4M大小物理地址
            pt[j] = (int) pte_pages[j] | PAGE_ATTR_US_RW_P;
        }

        break; // 此行为临时代码, 先只创建一个额外的页表测试, 其他的先不管
    }

    set_cr3((uint) pdt);
    enable_page();
}
