#include "apilib.h"

void HariMain(void)
{
    char *buf;
    int win, i, x, y;
    api_initmalloc();
    buf = api_malloc( 160 * 100 );
    win = api_openwin( buf, 160, 100, -1, "walk" );
    api_boxfilwin( win, 4, 24, 155, 95, 0 /* 黑色 */ );
    x = 76;
    y = 56;
    api_putstrwin( win, x, y, 3, 1, "*" /* 黃色 */);
    for(;;) {
        i = api_getkey(1);
        api_putstrwin( win, x, y, 0, 1, "*"); // 用黑色清除
        if (i == '4' && x >   4) { x -= 8; }
		if (i == '6' && x < 148) { x += 8; }
		if (i == '8' && y >  24) { y -= 8; }
		if (i == '2' && y <  80) { y += 8; }
		if (i == 0x0a) { break; } /* Enter結束 */
		api_putstrwin(win, x, y, 3 /* 黃色 */, 1, "*");
    }
    api_closewin( win );
    api_end();
}