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
     |                               | (0x4FF000 ~ 0x4FFFFF) (PT 767: 4K) | => 页目录的第0x2FF项指向这里
     ·                               ·             ...                    ·
     ·                               ·             ...                    ·
     |                               | (0x400000 ~ 0x400FFF) (PT 512: 4K) | => 页目录的第0x200项指向这里
     ·                               ·             ...                    ·
     ·    user (PT|PDE: 4K * 767)    ·             ...                    ·
     ·                               ·             ...                    ·
     |                               | (0x300000 ~ 0x300FFF) (PT 256: 4K) | => 页目录的第0x100项指向这里
     ·                               ·             ...                    ·
     ·                               ·             ...                    ·
     ·                               ·             ...                    ·
     |                               | (0x200000 ~ 0x200FFF) (PT 1: 4K)   | => 页目录的第1项指向这里
     ----------------------------------------------------------------------
     |                               | (0x1FF000 ~ 0x1FFFFF) (PT 1022: 4K)| => 页目录的第0x3FE项指向这里
     ·                               ·                ...                 ·
     ·   kernel (PT|PDE: 4K * 254)   ·                ...                 ·
     ·                               ·                ...                 ·
     |                               | (0x102000 ~ 0x102FFF) (PT 769: 4K) | => 页目录的第0x301项指向这里
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

 注: 页表(PT)地址和页目录项(PDE)中保持一致即可, 不一定非按上面的物理地址规划, 上图仅作参考示例.
     实际在以下程序中, 只有页目录表(PDT)存放的物理地址(0x100000~0x100FFF)最为关键, 该地址不仅作为页目录表, 还作为最后一个页表使用.
     其次就是第0张页表存放的物理地址(0x101000~0x101FFF), 用其指向低4MB物理内存(实际是为了用低1MB).

     至于其他都可随意, 比如可以将kernel和user的页表存放位置对换一下, 甚至分散到不同的内存区域也无所谓,
     只要最终在页目录表中的表项指向对应页表物理地址即可，页表项中的地址也可动态更改，
     因为已经把页目录表的最后一项的地址指向了自己，所以页目录项的值和每一个页表的值就建立好了对应的虚拟地址映射，即所有值都可以动态更改了。

     实际在下面的程序中，也只有页目录地址和第0个页表的地址是固定的，其他都是动态分配的，即上面有括号的地址都是参考地址，假的。

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
        TLB中存放的就是 虚拟地址的高20位 到 页表项物理地址 的映射关系。
        单条目清理指令示例：invlpg [0xC00FF000]; 中括号中的0xC00FF000为虚拟地址。
 AVL:
     意为Available位，表示可用，即软件，操作系统可用该位，CPU不理会该位的值。

 ---------------------------------------------------------------------------------------------------
*/


#include "../../include/mm.h"
#include "../../include/string.h"
#include "../../include/print.h"

#define PAGE_DIR_PHYSICAL_ADDR 0x100000         // 页目录表物理地址, 低1MB空间之上的第一个字节
#define PAGE_TAB_ITEM0_PHYSICAL_ADDR 0x101000   // 第0个页表物理地址, 页表空间之上的第一个字节


/*
 * 虚拟内存的3GB以下分给用户，高1GB给内核，
 * 内核和用户各自虚拟地址内存空间的低1MB均一一映射到物理地址内存空间的低1MB, 由0页表的前256项映射。
 * 0页表还可以继续映射3MB的空间，这3MB空间可以映射到任意物理地址，但目前保留不用，
 * 如果启用，这3MB的物理空间将可以由用户和内核同时访问，即内核和用户各自虚拟地址内存空间的低1MB~4MB之间的空间最终会指向同一块物理地址。
 * 所以，虚拟地址空间 0x100000~0x3FFFFF(用户) 及 0xC0100000~0xC03FFFFF(内核) 在未来可作为特殊只用，
 * 普通的虚拟地址空间分配将从 0x400000(用户) 或 0xC0400000(内核) 开始。
 */
#define KERNEL_MEM_BITMAP_LEN_ADDR 0xE14            // 内存分配对应位图长度的存储地址
#define KERNEL_MEM_BITMAP_ADDR 0x30000              // 内核内存池的位图存储位置
#define KERNEL_VIRTUAL_ALLOC_MEMORY_BASE 0xC0400000 // 内核的虚拟地址从哪里开始分配
#define KERNEL_VIRTUAL_ALLOC_MEMORY_MAX_SIZE (0xFFC00000-KERNEL_VIRTUAL_ALLOC_MEMORY_BASE) // 内核的虚拟地址最多可分配多少字节
#define USER_MEM_BITMAP_LEN_ADDR 0xE18              // 内存分配对应位图长度的存储地址
#define USER_MEM_BITMAP_ADDR 0x38000                // 内核内存池的位图存储位置
#define USER_VIRTUAL_ALLOC_MEMORY_BASE 0x400000     // 用户的虚拟地址从哪里开始分配
#define USER_VIRTUAL_ALLOC_MEMORY_MAX_SIZE (0xC0000000-USER_VIRTUAL_ALLOC_MEMORY_BASE) // 用户的虚拟地址最多可分配多少字节

extern physical_memory_alloc_t g_physical_memory;
virtual_memory_alloc_t g_virtual_memory;

static uint16 pdt_entry_counter[0x400] = {0}; // 记录每个页目录表项已经分配的页表数


static void alloc_virtual_memory_init() {
    // 虚拟内存（内核空间）
    g_virtual_memory.kernel_addr_start = KERNEL_VIRTUAL_ALLOC_MEMORY_BASE;
    g_virtual_memory.kernel_max_available_size =
            g_physical_memory.available_size > KERNEL_VIRTUAL_ALLOC_MEMORY_MAX_SIZE
            ? KERNEL_VIRTUAL_ALLOC_MEMORY_MAX_SIZE : g_physical_memory.available_size;
    g_virtual_memory.kernel_addr_max_end =
            g_virtual_memory.kernel_addr_start + g_virtual_memory.kernel_max_available_size - 1;
    g_virtual_memory.kernel_alloc_cursor = 0;
    g_virtual_memory.kernel_max_pages = g_virtual_memory.kernel_max_available_size >> 12;
    g_virtual_memory.kernel_map_len = (uint32 *) KERNEL_MEM_BITMAP_LEN_ADDR;
    *g_virtual_memory.kernel_map_len = (g_virtual_memory.kernel_max_pages + 7) / 8; // 1个字节可以表示8个物理页的使用情况
    g_virtual_memory.kernel_map = (uint8 *) KERNEL_MEM_BITMAP_ADDR;
    memset(g_virtual_memory.kernel_map, 0, *g_virtual_memory.kernel_map_len); // 清零

    // 虚拟内存（用户空间）
    g_virtual_memory.user_addr_start = USER_VIRTUAL_ALLOC_MEMORY_BASE;
    g_virtual_memory.user_max_available_size =
            g_physical_memory.available_size > USER_VIRTUAL_ALLOC_MEMORY_MAX_SIZE
            ? USER_VIRTUAL_ALLOC_MEMORY_MAX_SIZE : g_physical_memory.available_size;
    g_virtual_memory.user_addr_max_end =
            g_virtual_memory.user_addr_start + g_virtual_memory.user_max_available_size - 1;
    g_virtual_memory.user_alloc_cursor = 0;
    g_virtual_memory.user_max_pages = g_virtual_memory.user_max_available_size >> 12;
    g_virtual_memory.user_map_len = (uint32 *) USER_MEM_BITMAP_LEN_ADDR;
    *g_virtual_memory.user_map_len = (g_virtual_memory.user_max_pages + 7) / 8; // 1个字节可以表示8个物理页的使用情况
    g_virtual_memory.user_map = (uint8 *) USER_MEM_BITMAP_ADDR;
    memset(g_virtual_memory.user_map, 0, *g_virtual_memory.user_map_len); // 清零

    // 虚拟内存（通用）
    g_virtual_memory.pages_used = 0;

    printk("available kernel virtual memory pages %d: [0x%x~0x%x]\n", g_virtual_memory.kernel_max_pages,
           g_virtual_memory.kernel_addr_start, g_virtual_memory.kernel_addr_max_end);
    printk("available user virtual memory pages %d: [0x%x~0x%x]\n", g_virtual_memory.user_max_pages,
           g_virtual_memory.user_addr_start, g_virtual_memory.user_addr_max_end);
}

void virtual_memory_init() {
    alloc_virtual_memory_init();

    // 页目录表 (Page Directory Table) , 大小为4KB (0x100000 ~ 0x100FFF)
    int *pdt = (int *) PAGE_DIR_PHYSICAL_ADDR;
    memset(pdt, 0, 1 << 12); // 清零

    /*
     *  0x300表示第768个页表占用的目录项，0x300~0x3FF的目录项用于内核空间
     *  即页表的0xC0000000~0xFFFFFFFF共计1G属于内核, 0x0~0xBFFFFFFF共计3G属于用户进程
     */

    // 页表 (Page Table) ，大小为4KB (0x101000 ~ 0x101FFF), 一个页表项 (Page Table Entry) 对应一个真实的4KB物理地址空间
    int *pt = (int *) PAGE_TAB_ITEM0_PHYSICAL_ADDR;
    // 页目录项 (Page Directory Entry)，其中存放页表信息
    pdt[0] = (int) pt | 0b111; // 页属性，US=User, RW=1, P=1

    // 将内核的首个目录项(页目录第768项)也指向首个页表，目的是想在操作系统高3GB以上的虚拟地址上访问到低1MB物理地址
    // 这样虚拟地址0xC0000000~0xC03FFFFF的内存地址即指向了低4MB空间之内的物理地址(实际只用1M)
    pdt[0x300] = pdt[0];

    // 创建页表项：每个页目录项(pde)对应一个页表，每个页表有1024个页表项，一个页表对应4MB物理地址空间
    // 这里仅创建前512个, 即映射到低2MB物理地址空间（0x0~0xFFFFF），剩余3MB(0x100000~0x3FFFFF)在该页表中保留
    for (int i = 0; i < 0x100; i++) {
        pt[i] = (i << 12) | 0b111; // 页属性，US=User, RW=1, P=1
    }

    pdt_entry_counter[0] = 0x100;
    pdt_entry_counter[0x300] = 0x100;

    /*
     * 将最后一个页目录项指向指向页目录表自己的地址，目的是为了以后动态操作页表:
     *     虚拟地址高10位指定为0x3FF(第1023项), 中间10位则可指定访问页目录表中的哪一项, 低12位即可指定对应页表的4KB物理地址空间
     *     即使用虚拟地址 0xFFC00000~0xFFC00FFF 可以映射到物理地址 0x00101000~0x00101FFF
     *     使用虚拟地址 0xFFC01000~0xFFC01FFF 可以映射到物理地址 0x00102000~0x00102FFF ... 以此类推
     */
    pdt[0x3FF] = (int) pdt | 0b111; // 页属性，US=User, RW=1, P=1

    pdt_entry_counter[0x3FF] = 0x400;

    /*
     * 按规划，至此，虚拟内存到物理内存的映射关系应为：
     *     0x00000000~0x000FFFFF => 0x00000000~0x000FFFFF : 第0个页目录项(即用户空间的第0个页表)
     *     0xC0000000~0xC00FFFFF => 0x00000000~0x000FFFFF : 第768个页目录项(即内核空间的第0个页表)
     *     0xFFC00000~0xFFC00FFF => 0x00101000~0x00101FFF : 第1023个页目录项(内核最后1个页表)的第0个页表项
     *     0xFFF00000~0xFFF00FFF => 0x00101000~0x00101FFF : 第1023个页目录项(内核最后1个页表)的第768个页表项
     *     0xFFFFF000~0xFFFFFFFF => 0x00100000~0x00100FFF : 第1023个页目录项(内核最后1个页表)的第1023个页表项
     */

    /*
     * 所以，按现在的规划:
     *     既可以将 虚拟地址(用户及内核)低4MB空间 和 物理地址低4MB空间 一一映射，
     *     又可以通过虚拟地址获取其对应的物理地址，
     *     还可以通过虚拟地址修改所有的页目录项及页表项内容。
     -------------------------------------------------------------------------------------------------------------------
        标准示例：
  physical addr: (4KB: 0x100000~0x100FFF)        (4KB: 0x... ~ 0x...+0xFFF)       (4KB: 0x.... ~ 0x....+0xFFF)
                     -------------                     -------------                     ------------
                     | PDE ...   |                     | PTE ...   |                     |          |
                     | PDE ...   |                     | PTE ...   |                     |          |
                     | PDE ...   |                     | PTE ...   |                     | physical |
                     | PDE 0xXXX | →--------+          | PTE 0xXXX | →--------+          |   page   |
                     | PDE ...   |      (1) |          | PTE ...   |      (2) |          |          |
                     | PDE ...   |          ↓          | PTE ...   |          ↓          |          |
                     -------------          +--------→ -------------          +--------→ ------------
                           ↑
   virtual addr: 0xFFFFF000~0xFFFFFFFF

     -------------------------------------------------------------------------------------------------------------------
        特殊情况1（页目录第0项和第768项示例）：
  physical addr: (4KB: 0x100000~0x100FFF)          (4KB: 0x101000~0x101FFF)           (4KB: 0x0 ~ 0x3FFFFF)
                     -------------                     -------------                     ------------
                     | PDE ...   |                     | PTE ...   |                     |          |
                     | PDE ...   |                     | PTE ...   |                     |          |
                     | PDE 0x300 | →--------+          | PTE ...   |                     | physical |
                     | PDE ...   |          |          | PTE 0xXXX | →--------+          |   page   |
                     | PDE ...   |      (1) |          | PTE ...   |      (2) |          |          |
                     | PDE 0x000 | →--------↓          | PTE ...   |          ↓          |          |
                     -------------          +--------→ -------------          +--------→ ------------
                           ↑                                ↑                                  ↑
         virtual addr: 0xFFFFF000~0xFFFFFFFF                ↑        virtual addr: 0x0~0xFFFFF or 0xC0000000~0xC00FFFFF
                               virtual addr: 0xFFC00000~0xFFC00FFF or 0xFFF00000~0xFFF00FFF

     -------------------------------------------------------------------------------------------------------------------
        特殊情况2（页目录第1023项示例）：
    physical addr:       (4KB: 0x100000~0x100FFF)              (4KB: 0x... ~ 0x...+0xFFF)
                        -------------------------                    -------------
                 +----← | PDE 0x3FF & PTE ...   |                    |           |
                 |      | PDE ...   & PTE ...   |                    |           |
                 |      | PDE ...   & PTE ...   |                    |  physical |
             (1) |      | PDE ...   & PTE 0xXXX | →--------+         |    page   |
                 |      | PDE ...   & PTE ...   |      (2) |         | (PT 0xXXX)|
                 |      | PDE ...   & PTE ...   |          ↓         |           |
                 +----→ -------------------------          +-------→ -------------
                                  ↑                                        ↑
                virtual addr: 0xFFFFF000~0xFFFFFFFF                        ↑
                                     virtual addr: 0xFFC00000+0xXXX<<12 ~ 0xFFC00FFF+0xXXX<<12, 0xXXX∈[0~767]∪[769~1022]

     -------------------------------------------------------------------------------------------------------------------
        特殊情况2的特殊情况1（页目录第1023项对应页表的第0项和第768项示例）：
   physical addr:       (4KB: 0x100000~0x100FFF)             (4KB: 0x101000 ~ 0x101FFF)
                        -------------------------                    -------------
                 +----← | PDE 0x3FF & PTE ...   |                    |           |
                 |      | PDE ...   & PTE ...   |                    |           |
                 |      | PDE ...   & PTE 0x300 | →--------+         |  physical |
             (1) |      | PDE ...   & PTE ...   |          |         |    page   |
                 |      | PDE ...   & PTE ...   |      (2) |         |   (PT 0)  |
                 |      | PDE ...   & PTE 0x000 | →--------↓         |           |
                 +----→ -------------------------          +-------→ -------------
                                  ↑                                         ↑
                virtual addr: 0xFFFFF000~0xFFFFFFFF                         ↑
                                               virtual addr: 0xFFC00000~0xFFC00FFF or 0xFFF00000~0xFFF00FFF

     -------------------------------------------------------------------------------------------------------------------
        特殊情况2的特殊情况2（页目录第1023项对应页表的第1023项示例）：
            physical addr:  (4KB: 0x100000~0x100FFF)
                        ------------------------------------
                 +----← | PDE 0x3FF &    ..    & PTE 0x3FF | →----+
                 |      | PDE ...   &   (PD)   & PTE ...   |      |
                 |      | PDE ...   & physical & PTE ...   |      |
             (1) |      | PDE ...   &   page   & PTE ...   |      | (2)
                 |      | PDE ...   &  (PT 0)  & PTE ...   |      |
                 |      | PDE ...   &    ..    & PTE ...   |      |
                 +----→ ------------------------------------ ←----+
                                          ↑
                        virtual addr: 0xFFFFF000~0xFFFFFFFF

     -------------------------------------------------------------------------------------------------------------------
     */


    /*
     * 其他页表在需要时再动态创建（页目录项也在页表存在时再初始化），其中包括:
     *      高1GB的内核虚拟地址空间：第0x301~0x3FE项, 共254个
     *      低3GB的用户虚拟空间：第0x1~0x2FF项, 共767个
     *
     * 通过虚拟地址0xFFFFF000~0xFFFFFFFF即可修改页目录项指向的页表地址，
     * 通过0xFFC00000~0xFFFFFFFC可以修改全部1024个页表的所有页表项指向的具体物理地址。
     */

    set_cr3((uint) pdt);
    enable_page();
}

/*
 * 提供通过虚拟地址访问页表项的方法
 *      通过虚拟地址获取其对应的页表项
 */
static uint32 *get_pte_vaddr(void *vpage_addr) {
    /*
     -------------------------------------------------------------------------------------------------------------------
     * 要构建的目标虚拟地址 (构建规则参考:特殊情况2)：
     ----------------------------------------------------------------------
        -------------------------------------------------------------
        | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  |
        |                          0x3FF                            |
        -------------------------------------------------------------
        -------------------------------------------------------------
        | 21  | 20  | 19  | 18  | 17  | 16  | 15  | 14  | 13  | 12  |
        |                          pteIndex                         |
        -------------------------------------------------------------
        -------------------------------------------------------------------------
        | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
        |                          offset                           |  0  |  0  |
        -------------------------------------------------------------------------
     -------------------------------------------------------------------------------------------------------------------
     * 传入的虚拟地址（一般为虚拟页地址，即低12位均为0）
     ----------------------------------------------------------------------
        -------------------------------------------------------------
        | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  |
        |         作为目标地址的pteIndex，通过这里可以拿到pt的4KB空间      |
        -------------------------------------------------------------
        -------------------------------------------------------------
        | 21  | 20  | 19  | 18  | 17  | 16  | 15  | 14  | 13  | 12  |
        |    作为目标地址的offset的高10位，即将4KB空间分为1024份，每份4字节  |
        -------------------------------------------------------------
        -------------------------------------------------------------------------
        | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
        |                              保留不用                                   |
        -------------------------------------------------------------------------
     -------------------------------------------------------------------------------------------------------------------
     */

    // 最终返回长度为4字节的虚拟访问地址，这4字节对应的就是页表项的物理地址
    uint32 *pte_virtual_addr = (uint32 *) (0xFFC00000 | // 目标地址高10位即pdeIndex固定为0x3FF
                                           // 传入地址的 高10位和中10位 分别作为目标地址的 pteIndex和offset高10位
                                           ((((uint32) vpage_addr & 0xFFFFF000) >> 10)));
    return pte_virtual_addr;
}

/*
 * 提供通过虚拟地址访问页目录项的方法:
 *      通过虚拟地址获取其对应的页目录项
 */
static uint32 *get_pde_vaddr(void *vpage_addr) {
    /*
     -------------------------------------------------------------------------------------------------------------------
     * 要构建的目标虚拟地址 (构建规则参考:特殊情况2的特殊情况2)：
     ----------------------------------------------------------------------
        -------------------------------------------------------------------------------------------------------------------------
        | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  | 15  | 14  | 13  | 12  |
        |                                                          0xFFFFF                                                      |
        -------------------------------------------------------------------------------------------------------------------------
        -------------------------------------------------------------------------
        | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
        |                          offset                           |  0  |  0  |
        -------------------------------------------------------------------------
     -------------------------------------------------------------------------------------------------------------------
     * 传入的虚拟地址（一般为虚拟页地址，即低12位均为0）
     ----------------------------------------------------------------------
        -------------------------------------------------------------
        | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  |
        |  作为目标地址的offset的高10位，即将4KB空间分为1024份，每份4字节    |
        -------------------------------------------------------------
        -------------------------------------------------------------
        | 21  | 20  | 19  | 18  | 17  | 16  | 15  | 14  | 13  | 12  |
        |                           保留不用                          |
        -------------------------------------------------------------
        -------------------------------------------------------------------------
        | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
        |                              保留不用                                   |
        -------------------------------------------------------------------------
     -------------------------------------------------------------------------------------------------------------------
     */
    // 最终返回长度为4字节的虚拟访问地址，这4字节对应的就是页目录项的物理地址
    uint32 *pde_virtual_addr = (uint32 *) (0xFFFFF000 | ((uint32) vpage_addr & 0xFFC00000) >> 20);
    return pde_virtual_addr;
}


/*
 * 分配逻辑虚拟页
 * 注：该地址必须挂物理页才可使用
 */
static void *_alloc_virtual_page(enum pool_flags pf) {
    if (pf == PF_KERNEL) {
        if (!g_virtual_memory.kernel_max_pages) {
            printk("[%s] no memory available!\n", __FUNCTION__);
            return NULL;
        }
        if (g_virtual_memory.pages_used >= g_physical_memory.pages_total) { // 分配的虚拟页总数不能大于物理页总数
            printk("[%s] memory used up!\n", __FUNCTION__);
            return NULL;
        }
        // 查找次数最多为总页数
        for (uint cursor, index, i = 0; i < g_virtual_memory.kernel_max_pages; i++) {
            cursor = (g_virtual_memory.kernel_alloc_cursor + i) % g_virtual_memory.kernel_max_pages;
            index = cursor / 8;
            if (g_virtual_memory.kernel_map[index] & (1 << cursor % 8)) {
                continue; // 已分配
            }
            g_virtual_memory.kernel_map[index] |= (1 << cursor % 8);
            g_virtual_memory.pages_used++;
            g_virtual_memory.kernel_alloc_cursor = cursor + 1;
            void *p = (void *) (g_virtual_memory.kernel_addr_start + (cursor << 12));
            printk("[%s] alloc page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
                   g_virtual_memory.kernel_map[index], g_virtual_memory.pages_used);
            return p;
        }
    } else {
        if (!g_virtual_memory.user_max_pages) {
            printk("[%s] no memory available!\n", __FUNCTION__);
            return NULL;
        }
        if (g_virtual_memory.pages_used >= g_physical_memory.pages_total) { // 分配的虚拟页总数不能大于物理页总数
            printk("[%s] memory used up!\n", __FUNCTION__);
            return NULL;
        }
        // 查找次数最多为总页数
        for (uint cursor, index, i = 0; i < g_virtual_memory.user_max_pages; i++) {
            cursor = (g_virtual_memory.user_alloc_cursor + i) % g_virtual_memory.user_max_pages;
            index = cursor / 8;
            if (g_virtual_memory.user_map[index] & (1 << cursor % 8)) {
                continue; // 已分配
            }
            g_virtual_memory.user_map[index] |= (1 << cursor % 8);
            g_virtual_memory.pages_used++;
            g_virtual_memory.user_alloc_cursor = cursor + 1;
            void *p = (void *) (g_virtual_memory.user_addr_start + (cursor << 12));
            printk("[%s] alloc page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
                   g_virtual_memory.user_map[index], g_virtual_memory.pages_used);
            return p;
        }
    }

    // 前面已经进行空间预测, 逻辑不会走到这里
    printk("[%s] memory error!\n", __FUNCTION__);
    return NULL;
}

/*
 * 释放逻辑虚拟页
 * 注：需要提前释挂载的物理页
 */
static void _free_virtual_page(enum pool_flags pf, void *p) {
    if (pf == PF_KERNEL) {
        // 内核逻辑
        if ((uint) p < g_virtual_memory.kernel_addr_start || (uint) p > g_virtual_memory.kernel_addr_max_end) {
            printk("[%s] invalid kernel address: 0x%x\n", __FUNCTION__, p);
            return;
        }

        int cursor = (int) (p - g_virtual_memory.kernel_addr_start) >> 12;
        int index = cursor / 8;
        g_virtual_memory.kernel_map[index] &= ~(1 << cursor % 8);
        g_virtual_memory.pages_used--;

        printk("[%s] free kernel page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
               g_virtual_memory.kernel_map[index], g_virtual_memory.pages_used);
        return;
    }
    // 用户逻辑
    if ((uint) p < g_virtual_memory.user_addr_start || (uint) p > g_virtual_memory.user_addr_max_end) {
        printk("[%s] invalid user address: 0x%x\n", __FUNCTION__, p);
        return;
    }

    int cursor = (int) (p - g_virtual_memory.user_addr_start) >> 12;
    int index = cursor / 8;
    g_virtual_memory.user_map[index] &= ~(1 << cursor % 8);
    g_virtual_memory.pages_used--;

    printk("[%s] free user page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
           g_virtual_memory.user_map[index], g_virtual_memory.pages_used);
}


/*
 * 申请虚拟页，并挂载物理页
 * 注：physical_page 需为可用的物理地址，大小为4KB
 */
void *alloc_virtual_page(enum pool_flags pf, void *binding_physical_page) {
    // 申请虚拟地址
    void *virtual_page = _alloc_virtual_page(pf);
    // 获取页目录项的访问指针
    uint32 *pde_vaddr = get_pde_vaddr(virtual_page);
    /*
     * 检查P位：
     *      如果为1，表示页表可用。
     *      如果为0，需先申请页表地址，并挂载，将位置1。
     */
    if (!(*pde_vaddr & 0b1)) {
        void *paddr = alloc_physical_page(); // 物理地址
        *pde_vaddr = (uint32) paddr | 0b111; // 页属性，US=User, RW=1, P=1
        uint32 pte_index = (uint32) virtual_page >> 22;
        pdt_entry_counter[pte_index]++;
        printk("[%s] create pde [0x%x -> 0x%x]\n", __FUNCTION__, pte_index, paddr);
    }

    // 获取访问页表项的指针
    uint32 *pte_vaddr = get_pte_vaddr(virtual_page);
    // 修改页目表项的值
    if (pf == PF_KERNEL) {
        *pte_vaddr = (uint32) binding_physical_page | 0b011; // 页属性，US=Supervisor, RW=1, P=1
    } else {
        *pte_vaddr = (uint32) binding_physical_page | 0b111; // 页属性，US=User, RW=1, P=1
    }
    printk("[%s] bound [0x%x -> 0x%x]\n", __FUNCTION__, virtual_page, binding_physical_page);
    return virtual_page;
}

/*
 * 取消挂载物理页，并释放虚拟页
 * 注：将返回被取消挂载的物理页
 */
void *free_virtual_page(enum pool_flags pf, void *virtual_page) {

    // 获取访问页表项的指针
    uint32 *pte_vaddr = get_pte_vaddr(virtual_page);
    // 释放虚拟页
    void *unbinding_physical_page = (void *) (*pte_vaddr & 0xFFFFF000); // 将其映射的物理页地址返回
    *pte_vaddr = 0; // 取消关联

    // 检查如果页表项完都释放了，则对应页表也释放

    uint32 pte_index = (uint32) virtual_page >> 22;
    if (pdt_entry_counter[pte_index] <= 1) {
        pdt_entry_counter[pte_index]--;
        // 获取页目录项的访问指针
        uint32 *pde_vaddr = get_pde_vaddr(virtual_page);
        // 获取页表地址
        uint32 paddr = *pde_vaddr & 0xFFFFF000;
        // 取消页目录项和页表的关联
        *pde_vaddr = 0;
        // 释放页表地址
        free_physical_page((void *) paddr);
    }

    // 释放虚拟地址
    _free_virtual_page(pf, virtual_page);
    return unbinding_physical_page;
}

