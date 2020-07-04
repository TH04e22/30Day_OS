/* int.c */
#include "bootpack.h"

#define PORT_KEYDAT 0x0060

void init_pic(void)
/* PIC的初始化 */
{
	io_out8(PIC0_IMR,  0xff  ); // 設定階段，PIC0所有中斷都不接受
	io_out8(PIC1_IMR,  0xff  ); // 設定階段，PIC1所有中斷都不接受

	io_out8(PIC0_ICW1, 0x11  ); // 邊緣觸發模式
	io_out8(PIC0_ICW2, 0x20  ); // IRQ0-7接受INT20-27
	io_out8(PIC0_ICW3, 1 << 2); // PIC1是以IRQ2做連結
	io_out8(PIC0_ICW4, 0x01  ); // 無緩衝模式

	io_out8(PIC1_ICW1, 0x11  ); // 邊緣觸發模式
	io_out8(PIC1_ICW2, 0x28  ); // IRQ8-15接受INT28-2f
	io_out8(PIC1_ICW3, 2     ); // PIC1是以IRQ2做連結 
	io_out8(PIC1_ICW4, 0x01  ); // 無緩衝模式

	io_out8(PIC0_IMR,  0xfb  ); // 11111011 除pic1以外完全禁止
	io_out8(PIC1_IMR,  0xff  ); // 11111111 PIC1所有中斷都不接受

	return;
}

void inthandler27(int *esp)
{
	io_out8(PIC0_OCW2, 0x67);
	return;
}

