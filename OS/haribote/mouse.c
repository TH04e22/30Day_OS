#include "bootpack.h"

struct FIFO32 *mousefifo;
int mousedata0;

void enable_mouse( struct FIFO32 *fifo ,int data0 ,struct MOUSE_DEC *mdec )
{
	// 記憶住寫入目的地的FIFO緩衝區
	mousefifo = fifo;
	mousedata0 = data0;
	// 滑鼠有效狀態
	wait_KBC_sendready();
	io_out8( PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_KBC_sendready();
	io_out8( PORT_KEYDAT, MOUSECMD_ENABLE ); // 若是順利，會送出ACK(0xfa)
	mdec->phase = 0; // 等待滑鼠0xfa階段
	return;
}

int mouse_decode( struct MOUSE_DEC *mdec, unsigned char dat )
{
	switch ( mdec->phase )
	{
		case 0:
			// 等待滑鼠的0xfa階段
			if( dat == 0xfa )
				mdec->phase = 1;
			return 0;
		case 1:
			// 等待滑鼠的第1個位元組階段
			if( ( dat & 0xc8 ) == 0x08) {
				// 是正確的第1個位元組
				mdec->buf[0] = dat;
				mdec->phase = 2;
			}
			return 0;
		case 2:
			// 等待滑鼠的第2個位元組階段
			mdec->buf[1] = dat;
			mdec->phase = 3;
			return 0; 
		case 3:
			// 等待滑鼠的第3個位元組階段
			mdec->buf[2] = dat;
			mdec->phase = 1;
			mdec->btn = mdec->buf[0] & 0x07;
			mdec->x = mdec->buf[1];
			mdec->y = mdec->buf[2];
			if( (mdec->buf[0] & 0x10 ) != 0 )
				mdec->x |= 0xffffff00;
			if( (mdec->buf[0] & 0x20 ) != 0 )
				mdec->y |= 0xffffff00;
			mdec->y = -mdec->y; // 以滑鼠來說，y方向的符號是與畫面相反
			return 1;
	}
	return -1;
}

void inthandler2c(int *esp)
/* PS/2滑鼠來中斷 */
{
	int data;
	io_out8( PIC1_OCW2, 0x64 ); // IRQ-12受理完畢，通知PIC1
	io_out8( PIC0_OCW2, 0x62 ); // IRQ-02受理完畢，通知PIC0
	data = io_in8( PORT_KEYDAT );
	fifo32_put( mousefifo, data + mousedata0 );
	return;
}