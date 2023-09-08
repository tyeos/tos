//
// Created by toney on 23-8-30.
//

#include "../../include/linux/types.h"
#include "../../include/string.h"
#include "../../include/bridge/io.h"
#include "../../include/bridge/sys.h"

/*
    ------------------------------------------------------------------------------------
    VGA寄存器分组：
        Graphics Registers
        Sequencer Registers
        Attribute Controller Registers
        CRT Controller Registers
        Color Registers
        External (General) Registers
    ------------------------------------------------------------------------------------
    CRT Controller Registers:
        --------------------------------------
        |   Sub Register   | Read/Write Port |
        --------------------------------------
        | Address Register |       3x4h      |
        | Data Register    |       3x5h      |
        --------------------------------------
        x的取值取决于Input/Output Address Select字段
        --------------------------------------------------------------------------------
        CRT Controller Data Registers:
            -----------------------------------------
            | index | Register                      |
            -----------------------------------------
            |  ...  | ...                           |
            |  0Ch  | Start Address High Register   |
            |  0Dh  | Start Address Low Register    |
            |  0Eh  | Cursor Location High Register |
            |  0Fh  | Cursor Location Low Register  |
            |  ...  | ...                           |
            -----------------------------------------
    ------------------------------------------------------------------------------------
    External (General) Registers:
        ----------------------------------------------------------
        |   Sub Register                | Read Port | Write Port |
        ----------------------------------------------------------
        | Miscellaneous Output Register |    3CCh   |    3C2h    |
        ----------------------------------------------------------
        --------------------------------------------------------------------------------
        Miscellaneous Output Register:
            -------------------------------------------------------------------------
            |   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
            | VSYNCP | HSYNCP |O/E Page|        |   Clock Select  |RAM En. | I/OAS  |
            -------------------------------------------------------------------------
            I/OAS:
                Input/Output Address Select, 此位用来选择 CRT controller 寄存器的地址。
                当此位为0时，CRT controller 寄存器组的端口地址被设置为3Bxh，
                    即此时 Address Register 和 Data Register 的端口地址实际值为3B4h和3B5h。
                当此位为1时，CRT controller 寄存器组的端口地址被设置为3Dxh，
                    即此时 Address Register 和 Data Register 的端口地址实际值为3D4h和3D5h。
        注：默认情况下，Miscellaneous Output Register 的值为0x67, 即I/OAS位值为1。
    ------------------------------------------------------------------------------------
 */
#define CRT_ADDR_REG 0x3D4 // CRT(6845)索引寄存器
#define CRT_DATA_REG 0x3D5 // CRT(6845)数据寄存器
#define CRT_SCREEN_H 0xC   // 显示内存起始位置 - 高位
#define CRT_SCREEN_L 0xD   // 显示内存起始位置 - 低位
#define CRT_CURSOR_H 0xE   // 光标位置 - 高位
#define CRT_CURSOR_L 0xF   // 光标位置 - 低位

#define MEM_BASE 0xB8000              // 显卡内存起始位置（0xB8000~0xBFFFF, 即32K）
#define MEM_SIZE (4000*2)             // 设置要使用的显卡内存大小（一屏4000字节，约4K，也就是显存最多可加载8屏多一点的数据）
#define MEM_END (MEM_BASE + MEM_SIZE) // 显卡内存结束位置
#define WIDTH 80                      // 屏幕文本列数
#define HEIGHT 25                     // 屏幕文本行数
#define ROW_SIZE (WIDTH * 2)          // 每行字节数
#define SCR_SIZE (ROW_SIZE * HEIGHT)  // 屏幕字节数

#define ASCII_NUL 0x00 // 空字符
#define ASCII_BEL 0x07 // 响铃 \a
#define ASCII_BS 0x08  // 退格 \b
#define ASCII_HT 0x09  // 水平定位符（制表符） \t
#define ASCII_LF 0x0A  // 换行符 \n
#define ASCII_VT 0x0B  // 垂直定位符 \v
#define ASCII_FF 0x0C  // 换页键 \f
#define ASCII_CR 0x0D  // 回车符 \r
#define ASCII_DEL 0x7F // Delete字符

/*
    显卡的显示模式:
        CGA、EGA、VGA。
    CGA的文本模式:
        屏幕分成25行，每行80个字。
        每个字占两字节，低地址字节为字符ASCII码，高地址字符字节为显示属性：
        -------------------------------------------------------------------------------------------------
        | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
        |  K  | BR  | BG  | BB  |  I  | FR  | FG  | FB  |                  字符ASCII码                   |
        -------------------------------------------------------------------------------------------------
        K: 控制字符是否闪烁，1为闪烁
        BR、BG、BB: 背景色对应位的RGB是否填充
        I: 是否高亮，1为高亮
        FR、FG、FB: 前景色（文字）对应位的RGB是否填充
*/
#define CHARACTER_ATTR 0x07 // 字符属性：黑底白字
#define CHARACTER_SPACE 0x0720 // 黑底白字的空格

static uint screen; // 当前显示器开始的内存位置
static uint cursor; // 当前光标的内存位置
static uint x, y;   // 当前光标的坐标，x范围0~79，y范围0~24

// 更新光标位置，光标位置从0开始（对应内存地址为0xB8000），位置+1，对应内存地址+2
static void move_cursor() {
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    outb(CRT_DATA_REG, ((cursor - MEM_BASE) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    outb(CRT_DATA_REG, ((cursor - MEM_BASE) >> 1) & 0xff); // 光标在屏幕的位置=屏幕占用字节数/2
}

// 更新屏幕显示位置（将指定位置在屏幕中置顶显示）
static void move_screen() {
    outb(CRT_ADDR_REG, CRT_SCREEN_H);
    outb(CRT_DATA_REG, ((screen - MEM_BASE) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_SCREEN_L);
    outb(CRT_DATA_REG, ((screen - MEM_BASE) >> 1) & 0xff);
}

void console_clear() {
    screen = MEM_BASE;
    cursor = MEM_BASE;
    x = y = 0;
    move_cursor();
    move_screen();
    uint16 *ptr = (uint16 *) MEM_BASE;
    while (ptr < MEM_END) {
        *ptr++ = CHARACTER_SPACE;
    }
}

// 滚动到顶部
void scroll_to_top() {
    screen = MEM_BASE;
    move_screen();
}

// 滚动到光标处（当光标超出当前显示屏幕下方时，将光标对齐到屏幕最后一行）
void scroll_to_cursor() {
    if (cursor >= screen + SCR_SIZE) {
        screen = cursor - 2 * x - ROW_SIZE * (HEIGHT - 1);
        move_screen();
    }
}

// 屏幕上翻阅一行（无条件上翻，仅控制屏幕，判断边界）
static void scroll_up() {
    if (screen + ROW_SIZE <= MEM_END - SCR_SIZE) {
        screen += ROW_SIZE;
        move_screen();
    }
}

// 屏幕下翻阅一行（无条件下翻，仅控制屏幕，判断边界）
static void scroll_down() {
    if (screen - ROW_SIZE >= MEM_BASE) {
        screen -= ROW_SIZE;
        move_screen();
    }
}

// check cursor fix overflow, 检查光标是否溢出，如果溢出，则向上迁移一行数据
static void cf_overflow() {
    if (cursor >= MEM_END) {
        memcpy((void *) MEM_BASE, (void *) (MEM_BASE + ROW_SIZE), MEM_SIZE - ROW_SIZE);
        cursor -= ROW_SIZE; // 恢复cursor
        uint16 *ptr = (uint16 *) cursor;
        for (size_t i = x; i < WIDTH; i++) {
            *ptr++ = CHARACTER_SPACE;
        }
    }
}

// 换行（只控制光标，不控制屏幕）
static void command_lf() {
    cursor += ROW_SIZE;
    if (y < HEIGHT - 1) {
        y++;
    }
}

// 回车（只控制光标，不控制屏幕）
static void command_cr() {
    cursor -= (x << 1);
    x = 0;
}

// 退格（回删字符）
static void command_bs() {
    if (x) {
        x--;
        cursor -= 2;
        *(uint16 *) cursor = CHARACTER_SPACE;
    }
}

// 删除（将制指定光标位置的字符清除）
static void command_del() {
    *(uint16 *) cursor = CHARACTER_SPACE;
}

void console_write(char *buf, uint32 count) {
    CLI

    char c;
    char *ptr = (char *) cursor;
    while (count--) {
        c = *buf++;
        switch (c) {
            case ASCII_NUL:
            case ASCII_BEL:
            case ASCII_HT:
            case ASCII_VT:
                break;

            case ASCII_BS:
                command_bs();
                break;
            case ASCII_DEL:
                command_del();
                break;

            case ASCII_LF:
            case ASCII_CR:
            case ASCII_FF:
                // 无条件新启一行
                command_lf();
                command_cr();
                cf_overflow();
                scroll_to_cursor();
                ptr = (char *) cursor;
                break;

            default:
                // 在光标处写入字符
                *ptr++ = c;
                *ptr++ = CHARACTER_ATTR;
                // 移动光标到下一个位置
                cursor += 2;
                x++;
                // 处理换行
                if (x >= WIDTH) {
                    x -= WIDTH;
                    cf_overflow();
                    scroll_to_cursor();
                    ptr = (char *) cursor;
                }
                break;
        }
    }
    move_cursor();

    STI
}
