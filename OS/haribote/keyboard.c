#include "bootpack.h"

struct FIFO32 *keyfifo;
int keydata0;

void wait_KBC_sendready(void)
{
	// 鍵盤控制器進入等待發送資料的狀態
	for(;;) {
		if( (io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY ) == 0 ) {
			break;
		}
	}
	return;
}

void init_keyboard( struct FIFO32 *fifo, int data0 )
{
	// 記住寫入目的地的FIFO緩衝區
	keyfifo = fifo;
	keydata0 = data0;
	// 鍵盤控制器的初始化
	wait_KBC_sendready();
	io_out8( PORT_KEYCMD, KEYCMD_WRITE_MODE );
	wait_KBC_sendready();
	io_out8( PORT_KEYDAT, KBC_MODE );
	return;
}

void inthandler21( int *esp )
/* PS/2鍵盤來中斷 */
{
	int data;
	io_out8(PIC0_OCW2, 0x61 ); // IRQ-01受理完畢，通知PIC, 0x61 = 0x60 + IRQ編號
	data = io_in8(PORT_KEYDAT);
	fifo32_put(keyfifo, data + keydata0 );
	return;
}