#include "bootpack.h"

void keywin_off( struct SHEET *key_win )
{
	change_wtitle8( key_win, 0 );
	
	if( ( key_win->flags & 0x20 ) != 0 ) {
		fifo32_put( &key_win->task->fifo, 3 ); // 主控台游標為off
	}		
	return;
}

void keywin_on( struct SHEET *key_win )
{
	change_wtitle8( key_win, 1 );
	if ((key_win->flags & 0x20) != 0 ) {
		fifo32_put( &key_win->task->fifo, 2 ); // 主控台游標為on
	}
	return;
}

struct TASK *open_constask( struct SHEET *sht, unsigned int memtotal )
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = task_alloc();
	int *cons_fifo = (int *) memman_alloc_4k(memman, 128 * 4);
	task->cons_stack = memman_alloc_4k( memman, 64 * 1024 );
	task->tss.esp = task->cons_stack + 64 * 1024 - 12;
	task->tss.eip = (int) &console_task;
	task->tss.es = 1 * 8;
	task->tss.cs = 2 * 8;
	task->tss.ss = 1 * 8;
	task->tss.ds = 1 * 8;
	task->tss.fs = 1 * 8;
	task->tss.gs = 1 * 8;
	*((int *) (task->tss.esp + 4)) = (int) sht;
	*((int *) (task->tss.esp + 8)) = memtotal;
	task_run(task, 2, 2); /* level=2, priority=2 */
	fifo32_init(&task->fifo, 128, cons_fifo, task);
	return task;
}

struct SHEET *open_console(struct SHTCTL *shtctl, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHEET *sht = sheet_alloc(shtctl);
	unsigned char *buf = (unsigned char *) memman_alloc_4k(memman, 256 * 165);
	sheet_setbuf(sht, buf, 256, 165, -1); // 沒有透明色
	make_window8(buf, 256, 165, "console", 0);
	make_textbox8(sht, 8, 28, 240, 128, COL8_000000);
	sht->task = open_constask( sht, memtotal );
	sht->flags |= 0x20;
	return sht;
}

void close_constask( struct TASK *task )
{
	struct MEMMAN *memman = ( struct MEMMAN* ) MEMMAN_ADDR;
	task_sleep( task );
	memman_free_4k( memman, task->cons_stack, 64 * 1024 );
	memman_free_4k( memman, ( int ) task->fifo.buf, 128 * 4 );
	task->flags = 0; // 替代task_free( task )
	return;
}

void close_console( struct SHEET *sht )
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = sht->task;
	memman_free_4k( memman, ( int ) sht->buf, 256 * 156 );
	sheet_free(sht);
	close_constask( task );
	return;
}

void HariMain(void)
{
	static char keytable0[0x80] = {
		0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 0,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',   0,   '\\', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0x5c, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0x5c, 0,  0
	};
	static char keytable1[0x80] = {
		0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,   '|', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   '_', 0,   0,   0,   0,   0,   0,   0,   0,   0,   '|', 0,   0
	};
	struct BOOTINFO *binfo = ADR_BOOTINFO;
	char s[40];
	unsigned int memtotal;
	int i, j, x, y;
	int mx, my, mmx = -1, mmy = -1, mmx2 = 0, new_mx = -1, new_my = 0, new_wx = 0x7fffffff, new_wy = 0;
	int fifobuf[128], keycmd_buf[32], *cons_fifo[2];
	int  key_shift = 0, key_leds = (binfo->leds >> 4 ) & 7, keycmd_wait = -1;
	struct FIFO32 keycmd;
	struct FIFO32 fifo;
	struct MOUSE_DEC mdec;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHTCTL *shtctl;
	struct SHEET *sht_back, *sht_mouse, *sht = 0, *key_win;
	unsigned char *buf_back, buf_mouse[64], *buf_cons[2];
	extern struct TASKCTL *taskctl;
	struct TASK *task_a , *task, *task_cons[2];
	struct SEGMENT_DESCRIPTOR *gdt = ( struct SEGMENT_DESCRIPTOR* ) ADR_GDT;
	struct CONSOLE *cons;
	int *fat;
	unsigned char  *nihongo;
	struct FILEINFO *finfo;
	extern char hankaku[4096];

	init_gdtidt();
	init_pic();
	fifo32_init( &keycmd, 32, keycmd_buf, 0 );
	fifo32_init( &fifo, 128, fifobuf, 0 );
	*((int*) 0x0fec) = (int) &fifo;
	init_keyboard( &fifo, 256 );
	enable_mouse( &fifo, 512, &mdec );
	io_sti(); // 設定IF(旗標暫存器裡的中斷旗標)，設定完後cpu開始接受外部中斷
	init_pit(); // 初始化計時器
	io_out8(PIC0_IMR, 0xf8); // (11111000) IRQ1 -> PIC1, IRQ2-> 0x21 PS/2 鍵盤
	io_out8(PIC1_IMR, 0xef); // (11101111) IRQ4 -> 0x2c PS/2 滑鼠

	memtotal = memtest(0x00400000, 0xbfffffff); // 32MB 程式執行區
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff 64KB
	memman_free(memman, 0x00400000, memtotal - 0x00400000);

	init_palette(); // 設定調色盤
	shtctl = shtctl_init( memman, binfo->vram, binfo->scrnx, binfo->scrny );
	*((int*) 0x0fe4) = (int) shtctl;
	task_a = task_init(memman);
	fifo.task = task_a;
	task_run(task_a, 1, 2);

	/* sht_back */
	sht_back = sheet_alloc(shtctl);
	buf_back = (unsigned char*) memman_alloc_4k( memman, binfo->scrnx * binfo->scrny );
	sheet_setbuf( sht_back, buf_back, binfo->scrnx, binfo->scrny, -1 ); // 無透明色
	init_screen( buf_back, binfo->scrnx, binfo->scrny );

	/* sht_cons */
	key_win = open_console( shtctl, memtotal );

	/* sht_mouse */
	sht_mouse = sheet_alloc(shtctl);
	sheet_setbuf( sht_mouse, buf_mouse, 8, 8, 99 ); // 透明色編號99
	init_mouse_cursor8(buf_mouse, 99 ); // 背景色99
	mx = (binfo->scrnx - 8) / 2; // 置於螢幕中央
	my = (binfo->scrny - 18 - 8) / 2;

	sheet_slide( sht_back, 0, 0 );
	sheet_slide( key_win, 32, 4 );
	sheet_slide( sht_mouse, mx, my );
	sheet_updown( sht_back, 0 );
	sheet_updown( key_win, 1 );
	sheet_updown( sht_mouse, 2 );

	keywin_on( key_win );

	// 為了不和最初的鍵盤狀態不一致，先行設定
	fifo32_put( &keycmd, KEYCMD_LED );
	fifo32_put( &keycmd, key_leds );

	// 讀取nihongo.fnt
	nihongo = (unsigned char*) memman_alloc_4k( memman, 16 * 256 + 32 * 94 *47 );
	fat = ( int* ) memman_alloc_4k( memman, 4 * 2880 );
	file_readfat( fat, (unsigned char*) (ADR_DISKIMG + 0x000200) );
	finfo = file_search( "nihongo.fnt", (struct FILEINFO*)(ADR_DISKIMG+0x002600),224);
	if( finfo != 0 ) {
		file_loadfile( finfo->clustno, finfo->size, nihongo, fat, (char*)(ADR_DISKIMG+0x003e00));
	} else {
		for( i = 0; i < 16 * 256; i++ ) {
			nihongo[i] = hankaku[i]; // 因為沒有字型所以要複製半行的部分
		}

		for( i = 16 * 256; i < 16*256+32*94*47;i++) {
			nihongo[i] = 0xff; // 因為沒有字形所以用0xff填入全形的部分
		} 
	}

	*((int*) 0x0fe8) = (int) nihongo;
	memman_free_4k( memman, (int) fat, 4*2880);

    for(;;) {
		if (fifo32_status(&keycmd) > 0 && keycmd_wait < 0) {
			// 如果需要傳送資料給鍵盤控制的話，進行傳送
			keycmd_wait = fifo32_get(&keycmd);
			wait_KBC_sendready();
			io_out8(PORT_KEYDAT, keycmd_wait);
		}
        io_cli();
		if( fifo32_status(&fifo) == 0 ) {
			// 因已經清空了FIFO，所以如果有留存下來描繪內容，即可執行
			if (new_mx >= 0) {
				io_sti();
				sheet_slide(sht_mouse, new_mx, new_my);
				new_mx = -1;
			} else if (new_wx != 0x7fffffff) {
				io_sti();
				sheet_slide(sht, new_wx, new_wy);
				new_wx = 0x7fffffff;
			} else {
				task_sleep(task_a);
				io_sti();
			}
		} else {
			i = fifo32_get(&fifo);
			io_sti();
			if( key_win != 0 && key_win->flags == 0 ) { // 輸入視窗被關閉
				if( shtctl->top == 1 ) { 
					key_win = 0;
				} else {
					key_win = shtctl->sheets[shtctl->top - 1];
					keywin_on( key_win );
				}
			}

			if( 256 <= i && i <= 511 ) {
				if (i < 256 + 0x80 ) { // 將鍵盤代碼轉換為文字代碼
					if( key_shift == 0 ) {
						s[0] = keytable0[ i - 256 ];
					} else {
						s[0] = keytable1[ i - 256 ];
					}
				} else {
					s[0] = 0;
				}
				if( 'A' <= s[0] && s[0] <= 'Z'  ) { // 一般文字字元
					if (((key_leds & 4) == 0 && key_shift == 0) ||
							((key_leds & 4) != 0 && key_shift != 0)) {
						s[0] += 0x20;	// 轉換成小寫
					}
				}
				if( s[0] != 0 && key_win != 0 ) {
					// 往主控台
					fifo32_put( &key_win->task->fifo, s[0] + 256 );
				}

				if ( i == 256 + 0x57 && shtctl->top > 2 ) { // F11
					sheet_updown( shtctl->sheets[1], shtctl->top-1 );
				}

				if( i == 256 + 0x0e ) { // Backspace
					fifo32_put( &key_win->task->fifo, 8 + 256 );
				}

				if( i == 256 + 0x1c ) { // Enter
					fifo32_put( &key_win->task->fifo, 10 + 256 );
				}

				if( i == 256 + 0x0f && key_win != 0 ) { // TAB
					keywin_off( key_win );
					j = key_win->height - 1;
					if( j == 0 ) {
						j = shtctl->top - 1;
					}
					key_win = shtctl->sheets[j];
					keywin_on( key_win );
				}

				if( i == 256 + 0x3b && key_shift != 0 && key_win != 0 ){
					// Shift + F1
					task = key_win->task;
					if( task != 0 && task->tss.ss0 != 0 ) {
						cons_putstr0( task->cons, "\nBreak(key) :\n");
						io_cli();
						task->tss.eax = (int) &( task->tss.esp0);
						task->tss.eip = (int) asm_end_app;
						io_sti();
						task_run( task, -1, 0 ); // 喚醒
					}
				}

				if( i == 256 + 0x3c && key_shift != 0  ){
					// Shift + F2
					if( key_win != 0 ) {
						keywin_off( key_win );
					}
					key_win = open_console( shtctl, memtotal );
					sheet_slide( key_win, 32, 4 );
					sheet_updown( key_win, shtctl->top );
					keywin_on( key_win );
				}

				if (i == 256 + 0x2a) {	/* 左邊shift ON */
					key_shift |= 1;
				}
				if (i == 256 + 0x36) {	/* 右邊shift ON */
					key_shift |= 2;
				}
				if (i == 256 + 0xaa) {	/* 左邊shift  OFF */
					key_shift &= ~1;
				}
				if (i == 256 + 0xb6) {	/* 右邊shift  OFF */
					key_shift &= ~2;
				}
				if (i == 256 + 0xba) {	/* CapsLock */
					key_leds ^= 4;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0x45) {	/* NumLock */
					key_leds ^= 2;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0x46) {	/* ScrollLock */
					key_leds ^= 1;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0xfa) {	// 鍵盤資料順利取得
					keycmd_wait = -1;
				}
				if (i == 256 + 0xfe) {	// 鍵盤資料沒有取得
					wait_KBC_sendready();
					io_out8(PORT_KEYDAT, keycmd_wait);
				}
			} else if( 512 <= i && i <= 767 ) {  // 滑鼠資料
				if( mouse_decode( &mdec, i - 512 ) != 0 ) {
					mx += mdec.x;
					my += mdec.y;
					new_mx = mx;
					new_my = my;
					if( mx < 0 )
						mx = 0;
					if( my < 0 )
						my = 0;
					if( mx > binfo->scrnx - 1 )
						mx = binfo->scrnx - 1;
					if( my > binfo->scrny - 1 )
						my = binfo->scrny - 1;
					sheet_slide( sht_mouse, mx, my ); // 包含sheet_refresh
					if( ( mdec.btn & 0x01 ) != 0 ) { // 點按左鍵
						if( mmx < 0 ) { // 一般模式
							// 用滑鼠依序從上方開始尋找至底的視窗
							for( j = shtctl->top - 1; j > 0; j-- ) {
								sht = shtctl->sheets[j];
								x = mx - sht->vx0;
								y = my - sht->vy0;
								if( 0 <= x && x < sht->bxsize && 0 <= y && y < sht->bysize ) {
									if( sht->buf[ y * sht->bxsize + x] != sht->col_inv ) {
										sheet_updown( sht, shtctl->top - 1 );
										if( sht != key_win ) {
											keywin_off( key_win );
											key_win = sht;
											keywin_on( key_win );
										}
										if( sht->bxsize - 21 <= x && x < sht->bxsize - 5 && 5 <= y && y < 19 ) {
											if( (sht->flags & 0x10 ) != 0 ) { // 是否為應用程式所產生出來的視窗
												task = sht->task;
												cons_putstr0( task->cons, "\nBreak(mouse) :\n");
												io_cli();
												task->tss.eax = (int) &(task->tss.esp0);
												task->tss.eip = (int) asm_end_app;
												io_sti();
												task_run( task, -1, 0 );
											} else { // 主控台
												task = sht->task;
												io_cli();
												fifo32_put( &task->fifo, 4 );
												io_sti();
											}
										}

										if( 3 <= x && x < sht->bxsize - 3 && 3 <= y && y < 21 ) {
											mmx = mx; // 切換至視窗移動模式
											mmy = my;
											mmx2 = sht->vx0;
											new_wy = sht->vy0;
										}
										break;
									}
								}
							}
						} else {
							// 視窗移動模式
							x = mx - mmx;
							y = my - mmy;
							new_wx = ( mmx2 + x + 2 ) & ~3;
							new_wy = new_wy + y;
							sheet_slide( sht, (mmx2 + x + 2 ) & ~3, sht->vy0 + y );
							mmy = my;
						}
					} else {
						mmx = -1; // 回復至一般模式
						if( new_wx != 0x7fffffff ) {
							sheet_slide( sht, new_wx, new_wy ); // 做暫時的確認
							new_wx = 0x7fffffff;
						}
					}
				}
			} else if( 768 <=i && i <= 1023 ) { // 主控台結束處理
				close_console( shtctl->sheets0 + ( i - 768 ));
			} else if( 1024 <= i && i <= 2023 ) { // 沒有視窗的主控台
				close_constask( taskctl->tasks0 + (i - 1024));
			}
		}
    }
}
