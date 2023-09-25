//
// Created by toney on 2023/9/13.
//


#include "../../include/mm.h"
#include "../../include/sys.h"
#include "../../include/print.h"
#include "../../include/bridge/eflags.h"


/*
  bucket管理不同的size对应的freeptr组织结构（根据size等分成N个item）：

             --------------------------------------------------------------------
             |    4K    |    4K    |    4K    |  ···  |     4K     |     4K     |
             --------------------------------------------------------------------
             |  ------  |  ------  |  ------  |  ···  |  --------  |  --------  |
  freeptr -> |  | 16 |  |  |    |  |  |    |  |  ···  |  |      |  |  |      |  |
             |  ------  |  | 32 |  |  |    |  |  ···  |  |      |  |  |      |  |
             |  | 16 |  |  |    |  |  |    |  |  ···  |  |      |  |  |      |  |
             |  ------  |  ------  |  | 64 |  |  ···  |  |      |  |  |      |  |
             |  | 16 |  |  |    |  |  |    |  |  ···  |  | 2048 |  |  |      |  |
             |  ------  |  | 32 |  |  |    |  |  ···  |  |      |  |  |      |  |
             |  | 16 |  |  |    |  |  |    |  |  ···  |  |      |  |  |      |  |
             |  ------  |  ------  |  ------  |  ···  |  |      |  |  |      |  |
             |  | .. |  |  | .. |  |  | .. |  |  ···  |  |      |  |  |      |  |
             |  | .. |  |  | .. |  |  | .. |  |  ···  |  --------  |  | 4096 |  |
             |  | .. |  |  | .. |  |  | .. |  |  ···  |  |      |  |  |      |  |
             |  ------  |  ------  |  ------  |  ···  |  |      |  |  |      |  |
             |  | 16 |  |  |    |  |  |    |  |  ···  |  |      |  |  |      |  |
             |  ------  |  | 32 |  |  |    |  |  ···  |  |      |  |  |      |  |
             |  | 16 |  |  |    |  |  |    |  |  ···  |  | 2048 |  |  |      |  |
             |  ------  |  ------  |  | 64 |  |  ···  |  |      |  |  |      |  |
             |  | 16 |  |  |    |  |  |    |  |  ···  |  |      |  |  |      |  |
             |  ------  |  | 32 |  |  |    |  |  ···  |  |      |  |  |      |  |
             |  | 16 |  |  |    |  |  |    |  |  ···  |  |      |  |  |      |  |
             |  ------  |  ------  |  ------  |  ···  |  --------  |  --------  |
             --------------------------------------------------------------------
  注：对应size分配一次，freeptr指针就指向下一个地址，全部分配完后，再分配就需要重新申请一页内存。
*/
struct bucket_desc {            // 16 bytes
    void *page;                 // 物理页地址（即freeptr对应页的首地址）
    struct bucket_desc *next;   // 下一个bucket地址
    void *freeptr;              // 下一个可供分配的item地址
    unsigned short refcnt;      // 引用计数，释放物理页时要用
    unsigned short bucket_size; // 每个bucket负责一种字节size的分配，且每个bucket管理一个物理页大小的空间
};

/*
 _bucket_dir 及 bucket_dir 结构：

    --------------------
    |            | IDX |          ---------------
    |            ------|          | _bucket_dir |
    |            |     |          ---------------
    |            |  0  | ---16--> | size        |      -----used------      -----used------
    |            |     |          | chain       | ---> | bucket_desc |      | bucket_desc |
    |            |     |          ---------------      ---------------      ---------------
    |            |     |                               | page        |      | page        |
    |            |     |                               | next        | ···> | next        | ---> ignore
    |            |     |                               | freeptr     |      | freeptr     |
    |            |     |                               | refcnt      |      | refcnt      |
    |            |     |                               | bucket_size |      | bucket_size |
    |            |     |                               -----first-----      -----last------
    |            ------|
    |            |     |
    |            |  1  | ---32-->   ···
    |            |     |
    |            ------|
    |            |     |
    | bucket_dir | ··· |  ···       ···
    |            |     |
    |            ------|
    |            |     |          ---------------
    |            |     |          | _bucket_dir |
    |            |     |          ---------------
    |            |  8  | --4096-> | size        |      -----used------      -----used------
    |            |     |          | chain       | ---> | bucket_desc |      | bucket_desc |
    |            |     |          ---------------      ---------------      ---------------
    |            |     |                               | page        |      | page        |
    |            |     |                               | next        | ···> | next        | ---> ignore
    |            |     |                               | freeptr     |      | freeptr     |
    |            |     |                               | refcnt      |      | refcnt      |
    |            |     |                               | bucket_size |      | bucket_size |
    |            |     |                               -----first-----      -----last------
    |            ------|
    |            |     |         ---------------
    |            |     |         | _bucket_dir |
    |            |     |         ---------------
    |            |  9  | ---0--> | size        |
    |            |     |         | chain       | -> 0
    |            |     |         ---------------
    --------------------
*/
struct _bucket_dir {           // 8 bytes
    int size;                  // bucket大小，单位字节
    struct bucket_desc *chain; // 一个bucket目录可以挂多个bucket实例
};

// 一共9种类型的bucket目录，最大支持申请4096字节，即一个物理页大小
struct _bucket_dir bucket_dir[] = {
        {16,   (struct bucket_desc *) 0},
        {32,   (struct bucket_desc *) 0},
        {64,   (struct bucket_desc *) 0},
        {128,  (struct bucket_desc *) 0},
        {256,  (struct bucket_desc *) 0},
        {512,  (struct bucket_desc *) 0},
        {1024, (struct bucket_desc *) 0},
        {2048, (struct bucket_desc *) 0},
        {4096, (struct bucket_desc *) 0},
        {0,    (struct bucket_desc *) 0}
};   /* End of list marker */

/*
 free_bucket_desc组织结构：

               free_bucket_desc
                              |
                              ↓
  -----used------      -----free------      -----free------      -----free------
  | bucket_desc |      | bucket_desc |      | bucket_desc |      | bucket_desc |
  ---------------      ---------------      ---------------      ---------------
  | page        |      | page        |      | page        |      | page        |
  | next        | ···> | next        | ---> | next        | ···> | next        | ---> 0
  | freeptr     |      | freeptr     |      | freeptr     |      | freeptr     |
  | refcnt      |      | refcnt      |      | refcnt      |      | refcnt      |
  | bucket_size |      | bucket_size |      | bucket_size |      | bucket_size |
  -----old-------      -----first-----      ----second-----      -----last------
*/

/*
 * This contains a linked list of free bucket descriptor blocks
 */
struct bucket_desc *free_bucket_desc = (struct bucket_desc *) 0;

/*
 * This routine initializes a bucket description page.
 */
static inline void init_bucket_desc() {
    struct bucket_desc *bdesc, *first;
    int i;

    first = bdesc = (struct bucket_desc *) alloc_kernel_page();
    if (!bdesc) return;

    // 一个物理页可以存放 4096>>4 = 256 个bucket描述符结构项
    for (i = PAGE_SIZE / sizeof(struct bucket_desc); i > 1; i--) {
        bdesc->next = bdesc + 1; // 将bucket连成链表
        bdesc++;
    }

    /*
     * This is done last, to avoid race conditions in case
     * get_free_page() sleeps and this routine gets called again....
     */
    bdesc->next = free_bucket_desc;
    free_bucket_desc = first;
}

/*
 * 该方法一旦使用，则至少一个物理页不会被释放，其用于待分配的bucket链表缓存：free_bucket_desc
 */
void *kmalloc(size_t len) {
    struct _bucket_dir *bdir;
    struct bucket_desc *bdesc;
    void *retval;

    /*
     * First we search the bucket_dir to find the right bucket change
     * for this request.
     */
    for (bdir = bucket_dir; bdir->size; bdir++) if (bdir->size >= len) break;
    if (!bdir->size) {
        printk("kmalloc called with impossibly large argument (%d)\n", len);
        return NULL;
    }

    /*
     * Now we search for a bucket descriptor which has free space
     */
    bool iflag = check_close_if();  /* Avoid race conditions */
    for (bdesc = bdir->chain; bdesc; bdesc = bdesc->next) if (bdesc->freeptr) break;

    /*
     * If we didn't find a bucket with free space, then we'll allocate a new one.
     */
    if (!bdesc) { // 对应size的bucket没有可分配的空间了
        char *cp;
        int i;

        if (!free_bucket_desc) init_bucket_desc(); // 新建一批bucket

        bdesc = free_bucket_desc;                                                       // 分配bucket
        free_bucket_desc = bdesc->next;                                                 // 下一个待分配的bucket
        bdesc->refcnt = 0;                                                              // 计数初始化
        bdesc->bucket_size = bdir->size;                                                // 让该bucket以后用于size字节的分配
        bdesc->page = bdesc->freeptr = (void *) (cp = (char *) alloc_kernel_page());    // 申请内存用于分配给调用者

        if (!cp) {
            check_recover_if(iflag);
            return NULL; // 没有申请到可用物理页
        }

        /* Set up the chain of free objects */
        for (i = PAGE_SIZE / bdir->size; i > 1; i--) {  // 将申请的内存页按size分成N等份，每份(item)占用size字节
            *((char **) cp) = cp + bdir->size;          // 从item低地址处取一个指针大小的空间(32位下为4字节)，用其存储下一个item的内存地址
            cp += bdir->size;                           // 指针指向下一个item的地址
        }
        *((char **) cp) = 0;    // 将最后一个item的低地址处的指针变量置零，即将指针赋值为NULL

        // 将新建的bucket挂添加到bucket目录的chain首个位置
        bdesc->next = bdir->chain; /* OK, link it in! */
        bdir->chain = bdesc;
    }

    retval = (void *) bdesc->freeptr;       // 可分配的内存地址
    bdesc->freeptr = *((void **) retval);   // 从低地址处取一个指针变量值，即下一个item的内存地址
    bdesc->refcnt++;                        // 分配一次，对应bucket计数+1

    // 如果之前响应中断, 则恢复
    check_recover_if(iflag); /* OK, we're safe again */

    return (retval);
}

/*
 * Here is the free routine.  If you know the size of the object that you
 * are freeing, then kmfree_s() will use that information to speed up the
 * search for the bucket descriptor.
 *
 * We will #define a macro so that "kmfree(x)" is becomes "kmfree_s(x, 0)"
 */
void kmfree_s(void *obj, int size) {
    void *page;
    struct _bucket_dir *bdir;
    struct bucket_desc *bdesc, *prev;
    bdesc = prev = 0;

    /* Calculate what page this object lives in */
    page = (void *) ((unsigned long) obj & 0xfffff000);
    /* Now search the buckets looking for that page */
    for (bdir = bucket_dir; bdir->size; bdir++) {
        prev = 0;
        /* If size is zero then this conditional is always false */
        if (bdir->size < size) continue;
        // 从对应size的bucket目录的链表中找到对应管理该page的bucket
        for (bdesc = bdir->chain; bdesc; bdesc = bdesc->next) {
            if (bdesc->page == page) goto found;
            prev = bdesc; // 将bucket目录的链表中，位于当前bucket之前一个bucket也暂存
        }
    }
    return;

    found:
    {
        bool iflag = check_close_if();

        *((void **) obj) = bdesc->freeptr; // 将下一个待分配的item地址存到当前要释放的item低地址指针处
        bdesc->freeptr = obj;              // 下次优先分配当前准备释放的item
        bdesc->refcnt--;                   // 引用计数-1

        // 引用计数归零，则释放当前bucket
        if (bdesc->refcnt == 0) {
            /*
             * We need to make sure that prev is still accurate. It may not be, if someone rudely interrupted us....
             */
            // 在bucket目录的链表中，前一个bucket(prev)位于当前准备释放的bucket(bdesc)之前
            // 这里先保证前一个bucket的next指针指向当前bucket地址
            if ((prev && (prev->next != bdesc)) || (!prev && (bdir->chain != bdesc)))
                for (prev = bdir->chain; prev; prev = prev->next) if (prev->next == bdesc) break;

            if (prev) prev->next = bdesc->next; // 将当前bucket释放，上一个bucket的next指针指向当前bucket的next
            else {
                if (bdir->chain != bdesc) {
                    printk("bdir err: 0x%x -> 0x%x\n", bdir->chain, bdesc);
                    check_recover_if(iflag);
                    return; // 这里理论上是相等的
                }
                bdir->chain = bdesc->next;        // 如果当前bucket位于首位，则直接将下一个bucket地址作为chain链表地址
            }
            free_kernel_page(bdesc->page); // 释放物理页
            // 将当前释放的bucket归还到free_bucket_desc链表的的首位，下次优先分配
            bdesc->next = free_bucket_desc;
            free_bucket_desc = bdesc;
        }

        // 如果之前响应中断, 则恢复
        check_recover_if(iflag);
    }
}