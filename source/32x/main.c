/* marsonly.c */

#include "mars.h"


void setup_gray_palette()
{
	unsigned short* cram = (unsigned short *)&MARS_CRAM;

	if ((MARS_SYS_INTMSK & MARS_SH2_ACCESS_VDP) == 0)
		return;

	for (int i = 0; i < 256; i++) 
	{
		unsigned short b1 = (i & 0x1f) << 10;
		unsigned short g1 = (i & 0x1f) << 5;
		unsigned short r1 = (i & 0x1f) << 0;
		cram[i] = r1 | g1 | b1;
	}
}

void clear_framebuffer()
{
	volatile int* p, * p_end;

	p = (int*)(&MARS_FRAMEBUFFER + 0x100);
	p_end = (int*)p + 320 / 4 * mars_framebuffer_height;
	do {
		*p = 0;
	} while (++p < p_end);
}

void draw_square(int x, int y)
{
	volatile unsigned char* framebuffer = (unsigned char*)(&MARS_FRAMEBUFFER + 0x100);

	int square_size = 100;

	for (int loopy = 0; loopy < square_size; loopy++)
	{
		for (int loopx = 0; loopx < square_size; loopx++)
		{
			framebuffer[(x + loopx) + ((y + loopy) * 320)] = 31;
		}
	}
}

int main(void)
{
	/* clear screen */
	Mars_Init();

	Mars_CommSlaveClearCache();

	/* use letter-boxed 240p mode */
	if (Mars_IsPAL())
	{
		Mars_InitVideo(-240);
		Mars_SetMDColor(1, 0);
	}

	setup_gray_palette();

	Mars_MDPutString("Hello world from 32x Side");

	while (1)
	{
		clear_framebuffer();

		int x = 40;
		int y = 50;

		draw_square(x, y);

		Mars_FlipFrameBuffers(1);
	}

	return 0;
}
