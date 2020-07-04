#include "bootpack.h"

struct TIMERCTL timerctl;

void init_pit( void ) 
{
    int i;
	struct TIMER *t;
    io_out8( PIT_CTRL, 0x34 );
    io_out8( PIT_CNT0, 0x9c );
    io_out8( PIT_CNT0, 0x2e );
    timerctl.count = 0;
    for( i = 0; i < MAX_TIMER; i++ ) {
        timerctl.timer0[i].flags = 0; // No use
    }
	t = timer_alloc(); // 分配一個
	t->timeout = 0xffffffff;
	t->flags = TIMER_FLAGS_USING;
	t->next = 0; // 最後面
	timerctl.t0 = t; // 只有哨兵而已，所以在最前面
	timerctl.next_time = 0xffffffff; // 只有哨兵而已，所以以哨兵的時間為逾時時間
    return;
}

struct TIMER* timer_alloc( void )
{
    int i;
    for( i = 0; i < MAX_TIMER; i++ ) {
        if( timerctl.timer0[i].flags == 0 ) {
            timerctl.timer0[i].flags = TIMER_FLAGS_ALLOC;
			timerctl.timer0[i].flags2 = 0;
            return &timerctl.timer0[i];
        }
    }
    return 0; // 沒找到
}

void timer_free( struct TIMER *timer )
{
    timer->flags = 0;
    return;
}

void timer_init( struct TIMER *timer, struct FIFO32 *fifo, int data )
{
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_settime( struct TIMER *timer, unsigned int timeout )
{
    int e;
	struct TIMER *t, *s;
	timer->timeout = timeout + timerctl.count;
	timer->flags = TIMER_FLAGS_USING;
	e = io_load_eflags();
	io_cli();
	t = timerctl.t0;
	if (timer->timeout <= t->timeout) {
		// 放入到最前面的情況
		timerctl.t0 = timer;
		timer->next = t; // 下一個是t
		timerctl.next_time = timer->timeout;
		io_store_eflags(e);
		return;
	}
	// 尋找放入哪裡才好
	for (;;) {
		s = t;
		t = t->next;
		if (timer->timeout <= t->timeout) {
			// 放入s和t之間的情況
			s->next = timer; // s下一個是timer
			timer->next = t; // timer下一個是t
			io_store_eflags(e);
			return;
		}
	}
	return;
}

void inthandler20( int *esp )
{
	char ts = 0;
	struct TIMER *timer;
	io_out8(PIC0_OCW2, 0x60);	// 接受IRQ-00完成，通知PIC
	timerctl.count++;
	if (timerctl.next_time > timerctl.count) {
		return;
	}

	timer = timerctl.t0; // 首先將最前面的地址帶入timer
	for (;;) {
		// 因為timers的計時器全部都是運作中，所以不確認flags
		if (timer->timeout > timerctl.count) {
			break;
		}
		// 逾時
		timer->flags = TIMER_FLAGS_ALLOC;
		if( timer != task_timer ) {
			fifo32_put(timer->fifo, timer->data);
		} else {
			ts = 1; // mt_timer已經逾時了
		}	
		timer = timer->next;
	}
	// 剛好有i個的計時器逾時，剩下的挪移
	timerctl.t0 = timer;
	timerctl.next_time = timer->timeout;
	if( ts != 0 ) {
		task_switch();
	}
	return;
}

int timer_cancel( struct TIMER *timer )
{
	int e;
	struct TIMER *t;
	e = io_load_eflags();
	io_cli(); // 設定時，要禁止計時器得狀態發生變化
	if( timer->flags == TIMER_FLAGS_USING ) { // 是否要進行取消作業
		if( timer == timerctl.t0 ) {
			// 第一個計時器的取消處理
			t = timer->next;
			timerctl.t0 = t;
			timerctl.next_time = t->timeout;
		} else {
			// 第一個計數器以外的取消處理
			// 尋找timer的前一個計數器
			t = timerctl.t0;
			for(;;) {
				if( t->next == timer ) {
					break;
				}
				t = t->next;
			}
			/* 將timer的上一個計數器物件所指向的下一個指向的下一個計數器
			，改成指向timer的下一個計數器 */
			t->next = timer->next; 
		}
		timer->flags = TIMER_FLAGS_ALLOC;
		io_store_eflags(e);
		return 1; // 取消處理成功
	} 
	io_store_eflags(e);
	return 0;
}

void timer_cancelall( struct FIFO32 *fifo )
{
	int e, i;
	struct TIMER *t;
	e = io_load_eflags();
	io_cli(); // 設定作業下，禁止計時器的狀態有所變化
	for( i = 0; i < MAX_TIMER; i++ ) {
		t = &timerctl.timer0[i];
		if( t->flags != 0 && t->flags2 != 0 && t->fifo == fifo ) {
			timer_cancel(t);
			timer_free(t);
		}
	}
	io_store_eflags(e);
	return;
}