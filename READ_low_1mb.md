# 实模式下的1MB内存布局

| 起始  | 结束  | 大小           | 用途                                                         |
| ----- | ----- | -------------- | ------------------------------------------------------------ |
| FFFF0 | FFFFF | 16B            | BIOS入口地址，此地址也属于BIOS代码，同样属于顶部的640KB字节。只是为了强调其入口地址才单独贴出来。此处16字节的内容是跳转指令jmp f000:e05b |
| F0000 | FFFEF | 64KB-16B       | 系统BIOS范围是F0000~FFFFF共640KB,为说明入口地址，将最上面的16字节从此处去掉了，所以此处终止地址是OXFFFEF |
| C8000 | EFFFF | 160KB          | 映射硬件适配器的ROM或内存映射式I/O                           |
| C0000 | C7FFF | 32KB           | 显示适配器BIOS                                               |
| B8000 | BFFFF | 32KB           | 用于文本模式显示适配器                                       |
| B0000 | B7FFF | 32KB           | 用于黑白显示适配器                                           |
| A0000 | AFFFF | 64KB           | 用于彩色显示适配器                                           |
| 9FC00 | 9FFFF | IKB            | EBDA(Extended BIOS Data Area)扩展BIOS数据区                  |
| 7E00  | 9FBFF | 622080B约608KB | 可用区域                                                     |
| 7C00  | 7DFF  | 512B           | MBR被BIOS加载到此处，共512字节                               |
| 500   | 7BFF  | 30464B约30KB   | 可用区域                                                     |
| 400   | 4FF   | 256B           | BIOS Data Area(BIOS数据区）                                  |
| 000   | 3FF   | 1KB            | Interrupt Vector Table( IVT, 中断向量表）                    |

---

# 