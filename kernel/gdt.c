//
// Created by toney on 2023-09-02.
//

#include "../include/print.h"
#include "../include/types.h"
#include "../include/dt.h"
#include "../include/string.h"
#include "../include/sys.h"
#include "../include/mm.h"
#include "../include/task.h"

#define GDT_SIZE 256

/*
 * 在实模式下创建的描述符
 * 参见 loader.asm 中的定义
 */
#define R0_CODE_GDT_ENTRY_INDEX 1   // 代码段索引值
#define R0_DATA_GDT_ENTRY_INDEX 2   // 数据段索引值
#define R0_STACK_GDT_ENTRY_INDEX 3  // 栈段索引值
#define GDT_SCREEN_INDEX 4 // 屏幕段索引值

/*
 * 在此新建的描述符
 */
#define R3_CODE_GDT_ENTRY_INDEX 5 // R3代码段在GDT中的索引值
#define R3_DATA_GDT_ENTRY_INDEX 6 // R3数据段在GDT中的索引值, R3的栈段就不单独创建了，和数据段共用一个即可
#define TSS_GDT_ENTRY_INDEX 7     // TSS段在GDT中的索引值

uint64 gdt[GDT_SIZE] = {0};
dt_ptr gdt_ptr;


/*
 -----------------------------------------------------------------------------------------------------------------------
 TSS为intel为了方便操作系统管理进程而加入的一种结构
 TSS和LDT一样，必须要在GDT中注册才行，这是为了在引用描述符的阶段做安全检查，因此TSS是通过（16位）选择子来访问，
 --------------------------------------------------------------------------
    将tss加载到寄存器TR的指令是ltr, 其指令格式为：
        ltr "16位通用寄存器"或"16位内存单元" ; 16位对应的值就是描述符在GDT中的tss选择子
 -----------------------------------------------------------------------------------------------------------------------
 TSS描述符结构参考 loader.asm 中的段描述符格式，对于高32位中的取值有以下规定：
 --------------------------------------------------------------------------
     D位(D/B): 固定为0
     L位: 固定为0
     S位: TSS描述符属于系统段描述符，所以S位固定位0
     TYPE位: 在S为0的情况下，TYPE的值为10B1
        -------------------------
        | 11  | 10  |  9  |  8  |
        |  1  |  0  |  B  |  1  |
        -------------------------
        B：表示busy位，B位为0时，表示任务不繁忙，B位为1时，表示任务繁忙。
            任务创建未上CPU执行时初始为0，任务处于运行态时置为1，任务正常被换下时置回0，
            如果当前任务嵌套调用了新任务，当前任务并不会置0，而是新老任务TSS的B位都为1，因为新任务属于老任务的分支。
            嵌套任务调用的情况还会影响eflags寄存器中的NT位，这表示任务嵌套（Nest Task)。
            B位由CPU维护。
 -----------------------------------------------------------------------------------------------------------------------
 iretd指令一共有两个功能：
    (1)从中断返回到当前任务的中断前代码处。
    (2)当前任务是被嵌套调用时，它会调用自己TSS中“上一个任务的TSS指针”的任务，也就是返回到上一个任务（iretd可以调用一个任务）。

 当CPU执行iretd指令时，始终要判断eflags中NT位的值。
    如果NT等于1, 表示是从新任务返回到旧任务，即CPU到当前任务TSS的“上一个任务的TSS指针”字段中获取旧任务的TSS,转而去执行旧任务。
    如果NT等于0, 表示要回到当前任务中断前的指令部分。

 -----------------------------------------------------------------------------------------------------------------------


 描述符缓冲器结构：
    | 32位线性基址 | 20位段界限 | 属性 |

 -----------------------------------------------------------------------------------------------------------------------
 */
static tss_t tss;

// 创建（内核态）r0用的选择子：代码段、数据段，栈段
int r0_code_selector = R0_CODE_GDT_ENTRY_INDEX << 3;
int r0_data_selector = R0_DATA_GDT_ENTRY_INDEX << 3;
int r0_stack_selector = R0_STACK_GDT_ENTRY_INDEX << 3;

// 创建（用户态）r3用的选择子：代码段、数据段
int r3_code_selector = R3_CODE_GDT_ENTRY_INDEX << 3 | 0b011; // TI=GDT, RPL=3
int r3_data_selector = R3_DATA_GDT_ENTRY_INDEX << 3 | 0b011; // TI=GDT, RPL=3

// 创建TSS选择子：用于态的切换
int tss_selector = TSS_GDT_ENTRY_INDEX << 3; // TI=GDT, RPL=0


static void set_gdt_tss_entry() {
    printk("init tss...\n");

    /*
     * TSS只能且必须在GDT中注册描述符，tr寄存器中存储的是tss的选择子。
     *
     * 仿照Linux的做法，为每个cpu创建一个tss，在各个CPU上所有的任务共享一个TSS，各CPU的TR寄存器保存各CPU上的TSS，
     * 在用ltr指令加载tss后，该tr寄存器用于指向同一个tss，之后也不会在重新加载tss。
     * 在进程切换（到低特权级）时，只需要把tss中的ss0和esp0更新为新任务（低特权级）的内核栈（回到高特权级）的段地址及栈指针。
     *
     * 目前的程序设计中的特权级使用也和Linux一致，仅使用r0（内核）和r3（用户）。
     *
     * CPU在首次发生态的切换时，会自动读取并载入tss，并将tss描述符TYPE中的B位置1，
     * 还会将当前切换前任务的tss写入到“上一个任务的TSS指针”字段中，
     * 所以说，无论是从用户态使用 int 0x80 发送中断指令到内核态，还是使用正常中断返回，都不需要管tss。
     *
     * tss无需人工干预，由CPU维护，但首次还是要初始化的。
     */
    memset(&tss, 0, sizeof(tss_t));
    tss.ss0 = r0_stack_selector;
    tss.iobase = sizeof(tss_t); // IO位图的偏移地址大于等于TSS大小减一时，表示没有IO位图

    global_descriptor *item = (global_descriptor *) &gdt[TSS_GDT_ENTRY_INDEX];
    uint32 base = (uint32) &tss;
    uint32 limit = sizeof(tss_t) - 1;

    item->base_low = base & 0xffffff;
    item->base_high = (base >> 24) & 0xff;
    item->limit_low = limit & 0xffff;
    item->limit_high = (limit >> 16) & 0xf;
    item->segment = 0;     // 系统段
    item->granularity = 0; // 粒度为字节
    item->big = 0;         // D位固定为0
    item->long_mode = 0;   // L位固定为0
    item->present = 1;     // 在内存中
    item->DPL = 0;         // 用于任务门或调用门
    item->type = 0b1001;   // 32位tss为0b10B1，TYPE中的B位初始为0表示未上CPU运行
}

void update_tss_esp(uint32 esp0) {
    /*
     * 任务切换就是改变TR的指向，CPU自动将当前寄存器组的值（快照）写入TR指向的TSS,
     * 同时将新任务TSS中的各寄存器的值载入CPU中对应的寄存器，从而实现了任务切换。
     *
     * 所以首次从内核态进入用户态时，要将当前内核态的栈地址更新到tss中，
     * 这个栈同时也就是用户进程从用户态切回（通过 int 0x80 中断）到内核态后所使用的栈。
     *
     * 目前的设计内核栈段是固定的，即ss0不用更新，仅更新esp0即可。
     *
     * 只有初始化时调用一次即可，之后都无需再调用
     */
    tss.esp0 = esp0;

}

/*
 * 设置全局描述符表项
 *    gdt_index: 范围 [GDT_REAL_MODE_USED_SIZE ~ GDT_SIZE)
 *    type: 内存段类型，只用低四位
 *    dpl: 描述符特权级，只用低两位
 */
static void set_gdt_entry(int gdt_index, char type, char dpl) {
    global_descriptor *item = (global_descriptor *) &gdt[gdt_index];

    item->base_low = 0;
    item->base_high = 0;        // base = 0
    item->limit_low = 0xffff;
    item->limit_high = 0xf;     // limit = 0xFFFFF, 表示4GB空间

    item->segment = 1;          // 非系统段
    item->type = type & 0xf;    // 取低四位
    item->DPL = dpl & 0b11;     // 取低两位

    item->present = 1;
    item->available = 0;
    item->long_mode = 0;
    item->big = 1;
    item->granularity = 1;
}

/*
 * 修复在实模式下创建的GDT项, 使其符合虚拟地址分页模式规划
 */
static void gdt_virtual_model_fix() {
    /*
     * 开启虚拟模式后，对于低1MB的空间地址，有以下映射关系：
     *      0x00000000~0x000FFFFF => 0x00000000~0x000FFFFF
     *      0xC0000000~0xC00FFFFF => 0x00000000~0x000FFFFF
     * 即内核使用3GB以上的低1MB和直接使用低1MB最终操作的物理地址都是一样的，
     * 所以这里只修改屏幕段，证明虚拟地址和映射关系都已经生效即可。
     *
     * 其他段描述符的段基址和段界限暂时不改，
     *      原因之一是因为如果内核代码需要虚拟地址映射，那么qemu下的gdb调试程序地址也都要改成虚拟地址，不然断点会有问题，
     *      而这个并不是重点，且改不改对后面也没什么影响，既然已经支持调试，所以就不花精力去搞调试环境了。
     */

    // 屏幕段, 基址从 0xb8000 改为 0xc00b8000
    global_descriptor *p = (global_descriptor *) &gdt[GDT_SCREEN_INDEX];
    p->base_high = 0xc0;
}

void gdt_init() {
    printk("init gdt ...\n");

    // 取出GDT
    __asm__ volatile ("sgdt gdt_ptr;");
    memcpy(&gdt, (void *) gdt_ptr.base, gdt_ptr.limit);

    // 创建r3用的代码段和数据段描述符
    set_gdt_entry(R3_CODE_GDT_ENTRY_INDEX, 0b1000, 0b11); // 代码段，只执行，DPL=3
    set_gdt_entry(R3_DATA_GDT_ENTRY_INDEX, 0b0010, 0b11); // 数据段，只读，DPL=3

    // 创建tss描述符
    set_gdt_tss_entry();

    // 虚拟地址模式处理
    gdt_virtual_model_fix();

    // 重设GDT
    gdt_ptr.base = (int) &gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;
    __asm__ volatile ("lgdt gdt_ptr;");

    // 将tss加载到tr寄存器
    asm volatile("ltr ax;"::"a"(tss_selector));
}