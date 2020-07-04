#include "bootpack.h"

void io_hlt(void);
void io_cli(void);
void io_out8(int port, int data);
int io_load_eflags(void);
void io_store_eflags(int eflags);

void init_palette(void)
{
	static unsigned char table_rgb[16 * 3] = {
		0x00, 0x00, 0x00,	/*  0:黑色 */
		0xff, 0x00, 0x00,	/*  1:亮紅色 */
		0x00, 0xff, 0x00,	/*  2:亮綠色 */
		0xff, 0xff, 0x00,	/*  3:亮黃色 */
		0x00, 0x00, 0xff,	/*  4:亮藍色 */
		0xff, 0x00, 0xff,	/*  5:亮紫色 */
		0x00, 0xff, 0xff,	/*  6:亮水色 */
		0xff, 0xff, 0xff,	/*  7:白色 */
		0xc6, 0xc6, 0xc6,	/*  8:亮灰色 */
		0x84, 0x00, 0x00,	/*  9:暗紅色 */
		0x00, 0x84, 0x00,	/* 10:暗綠色 */
		0x84, 0x84, 0x00,	/* 11:暗黃色 */
		0x00, 0x00, 0x84,	/* 12:暗藍色 */
		0x84, 0x00, 0x84,	/* 13:暗紫色 */
		0x00, 0x84, 0x84,	/* 14:暗水色 */
		0x84, 0x84, 0x84	/* 15:暗灰色 */
	};
	unsigned char table2[ 216 * 3 ];
	int r, g, b;
	set_palette(0, 15, table_rgb);
	for( b = 0; b < 6; b++ ) {
		for( g = 0; g < 6; g++ ) {
			for( r = 0; r < 6; r++ ) {
				table2[ ( r + g * 6  + b * 36 ) * 3 + 0 ] = r * 51;
				table2[ ( r + g * 6  + b * 36 ) * 3 + 1 ] = g * 51;
				table2[ ( r + g * 6  + b * 36 ) * 3 + 2 ] = b * 51;
			}
		}
	}
	set_palette( 16, 231, table2 );
	return;
}

void set_palette(int start, int end, unsigned char *rgb)
{
	int i, eflags;
	eflags = io_load_eflags();	// 紀錄允許插入旗標的值
	io_cli(); 					// 將允許旗標設為0以禁止插入
	io_out8(0x03c8, start);
	for (i = start; i <= end; i++) {
		io_out8(0x03c9, rgb[0] / 4);
		io_out8(0x03c9, rgb[1] / 4);
		io_out8(0x03c9, rgb[2] / 4);
		rgb += 3;
	}
	io_store_eflags(eflags);	// 還原插入許可旗標
	return;
}

void boxfill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1)
{
	int x, y;
	for (y = y0; y <= y1; y++) {
		for (x = x0; x <= x1; x++)
			vram[y * xsize + x] = c;
	}
	return;
}

void init_screen( char* vram, int x, int y ) 
{
	boxfill8(vram, x, COL8_008484,  0,     0,      x -  1, y - 29);
	boxfill8(vram, x, COL8_C6C6C6,  0,     y - 28, x -  1, y - 28);
	boxfill8(vram, x, COL8_FFFFFF,  0,     y - 27, x -  1, y - 27);
	boxfill8(vram, x, COL8_C6C6C6,  0,     y - 26, x -  1, y -  1);

	boxfill8(vram, x, COL8_FFFFFF,  3,     y - 24, 59,     y - 24);
	boxfill8(vram, x, COL8_FFFFFF,  2,     y - 24,  2,     y -  4);
	boxfill8(vram, x, COL8_848484,  3,     y -  4, 59,     y -  4);
	boxfill8(vram, x, COL8_848484, 59,     y - 23, 59,     y -  5);
	boxfill8(vram, x, COL8_000000,  2,     y -  3, 59,     y -  3);
	boxfill8(vram, x, COL8_000000, 60,     y - 24, 60,     y -  3);

	boxfill8(vram, x, COL8_848484, x - 47, y - 24, x -  4, y - 24);
	boxfill8(vram, x, COL8_848484, x - 47, y - 23, x - 47, y -  4);
	boxfill8(vram, x, COL8_FFFFFF, x - 47, y -  3, x -  4, y -  3);
	boxfill8(vram, x, COL8_FFFFFF, x -  3, y - 24, x -  3, y -  3);
	return;
}

void putfont8( char *vram, int xsize, int x, int y, char c, char *font ) {
	int i, j;
	char *p, d; // data
	for( i = 0; i < 16; i++ ) {
		p = vram + ( y + i ) * xsize + x;
		d = font[i];
		for( j = 0; j < 8; j++, d <<=1 ) 
			if( (d & 0x80) != 0 ) { p[j] = c; };
	}
}

void putfonts8_asc( char *vram, int xsize, int x, int y, char c, unsigned char *s ) {
	extern char hankaku[4096]; // type face
	struct TASK *task = task_now();
	char *nihongo = (char*) *((int*) 0x0fe8);

	if( task->langmode == 0 ) {
		for(; *s != 0x00; s++ ) {
			putfont8( vram, xsize, x, y, c, hankaku + *s + 16 );
			x += 8;
		}
	}

	if( task->langmode == 1 ) {
		for(; *s != 0x00; s++) {
			putfont8( vram, xsize, x, y, c, nihongo + *s * 16 );
			x += 8;
		}
	}
	return;
}

void init_mouse_cursor8(char *mouse, char bc)
{
	static char cursor[8][8] = {
		"******..",
		"*O*O*...",
		"**O*....",
		"*O*O*...",
		"**.*O*..",
		"*...*O*.",
		".....*O*",
		"......*.",
	};
	int x, y;

	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x++) {
			if (cursor[y][x] == '*') {
				mouse[y * 8 + x] = COL8_000000;
			}
			if (cursor[y][x] == 'O') {
				mouse[y * 8 + x] = COL8_FFFFFF;
			}
			if (cursor[y][x] == '.') {
				mouse[y * 8 + x] = bc;
			}
		}
	}
	return;
}

void putblock8_8(char *vram, int vxsize, int pxsize,
	int pysize, int px0, int py0, char *buf, int bxsize)
{
	int x, y;
	for (y = 0; y < pysize; y++) {
		for (x = 0; x < pxsize; x++) {
			vram[(py0 + y) * vxsize + (px0 + x)] = buf[y * bxsize + x];
		}
	}
	return;
}

void putfonts8_asc_sht( struct SHEET* sht, int x, int y, int c, int b, char *s, int l )
{
	boxfill8( sht->buf, sht->bxsize, b, x, y, x + l * 8 - 1, y + 15 );
	putfonts8_asc( sht->buf, sht->bxsize, x, y, c, s );
	sheet_refresh( sht, x, y, x + l * 8, y + 16);
	return;
}