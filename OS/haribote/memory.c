#include "bootpack.h"

unsigned int memtest( unsigned int start, unsigned int end )
{
	char flg486 = 0;
	unsigned int eflg, cr0, i;
	eflg = io_load_eflags(); // 確認是386還是486以後
	eflg |= EFLAGS_AC_BIT; // AC bit = 1
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if( (eflg & EFLAGS_AC_BIT ) != 0 ) { // 在386就算是設成AC=1也會自動傳回0
		flg486 = 1;
	}

	eflg &= ~EFLAGS_AC_BIT; // AC bit = 0
	io_store_eflags(eflg);

	if ( flg486 != 0 ) {
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; // 禁止快取功能
		store_cr0(cr0);
	}

	i = memtest_sub(start, end);

	if( flg486 != 0 ) {
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; // 允許快取功能
		store_cr0(cr0);
	}
	return i;
}

void memman_init(struct MEMMAN *man)
{
	man->frees = 0;			// 閒置資訊的個數
	man->maxfrees = 0;		// 狀況觀察用: frees的最大值
	man->lostsize = 0;		// 釋放失敗合計的大小
	man->losts = 0;			// 釋放失敗的次數
	return;
}

unsigned int memman_total(struct MEMMAN *man)
// 報告閒置空間的合計大小
{
	unsigned int i, t = 0;
	for (i = 0; i < man->frees; i++) {
		t += man->free[i].size;
	}
	return t;
}

unsigned int memman_alloc(struct MEMMAN *man, unsigned int size)
/* 配置 */
{
	unsigned int i, a;
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].size >= size) {
			/* 發現了充分的空間 */
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0) {
				/* free[i]已經沒空間了，後面的資訊向前搬移 */
				man->frees--;
				for (; i < man->frees; i++) {
					man->free[i] = man->free[i + 1]; /* 代入結構 */
				}
			}
			return a;
		}
	}
	return 0; // 沒有閒置空間
}

int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size)
/* 釋放 */
{
	int i, j;
	/* 考慮到彙整得容易度，希望free[]最好以addr的順序排列
	因此先決定在哪置入 */
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].addr > addr) {
			break;
		}
	}
	/* free[i - 1].addr < addr < free[i].addr */
	if (i > 0) {
		/* 在前面的位置有空間可以彙整 */
		if (man->free[i - 1].addr + man->free[i - 1].size == addr) {
			man->free[i - 1].size += size;
			if (i < man->frees) {
				/* 後面也有空間可以彙整 */
				if (addr + size == man->free[i].addr) {
					man->free[i - 1].size += man->free[i].size;
					/* man->free[i]刪除 */
					/* free[i]向前搬移 */
					man->frees--;
					for (; i < man->frees; i++) {
						man->free[i] = man->free[i + 1];
					}
				}
			}
			return 0; /* 成功結束作業 */
		}
	}
	/* 無法與前面彙整 */
	if (i < man->frees) {
		/* 後面可以彙整 */
		if (addr + size == man->free[i].addr) {
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0; /* 成功結束作業 */
		}
	}
	/* 前面與後面都無法彙整 */
	if (man->frees < MEMMAN_FREES) {
		// 將比free[i]還後面的向後移位，以製作出閒置空間
		for (j = man->frees; j > i; j--) {
			man->free[j] = man->free[j - 1];
		}
		man->frees++;
		if (man->maxfrees < man->frees) {
			man->maxfrees = man->frees; // 更新最大值
		}
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0;
	}
	/* 無法向後移 */
	man->losts++;
	man->lostsize += size;
	return -1; // 失敗結束作業
}

unsigned int memman_alloc_4k( struct MEMMAN *man, unsigned int size ) // 將size補成4k的倍數
{
	unsigned int a;
	size = ( size + 0xfff ) & 0xfffff000;
	a = memman_alloc( man, size );
	return a;
}

int memman_free_4k( struct MEMMAN *man, unsigned int addr, unsigned int size )
{
	int i;
	size = ( size + 0xfff ) & 0xfffff000;
	i = memman_free( man, addr, size );
	return i;	
}