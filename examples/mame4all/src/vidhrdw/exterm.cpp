/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/tms34010/tms34010.h"


#define FORCOL_TO_PEN(COL)	\
	if ((COL) & 0x8000)		\
	{						\
		(COL) &= 0x0fff;	\
	}						\
	else					\
	{						\
		(COL) += 0x1000;	\
	}


static struct osd_bitmap *tmpbitmap1, *tmpbitmap2;
unsigned char *exterm_master_videoram, *exterm_slave_videoram;

void exterm_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;

	palette += 3*4096;	/* first 4096 colors are dynamic */

	/* initialize 555 RGB lookup */
	for (i = 0;i < 32768;i++)
	{
		int r,g,b;

		r = (i >> 10) & 0x1f;
		g = (i >>  5) & 0x1f;
		b = (i >>  0) & 0x1f;

		(*palette++) = (r << 3) | (r >> 2);
		(*palette++) = (g << 3) | (g >> 2);
		(*palette++) = (b << 3) | (b >> 2);
	}
}


READ_HANDLER( exterm_master_videoram_r )
{
    return READ_WORD(&exterm_master_videoram[offset]);
}

static WRITE_HANDLER( exterm_master_videoram_16_w )
{
	COMBINE_WORD_MEM(&exterm_master_videoram[offset], data);

	FORCOL_TO_PEN(data);

	((unsigned short *)tmpbitmap->line[offset >> 9])[(offset >> 1) & 0xff] = Machine->pens[data];
}

static WRITE_HANDLER( exterm_master_videoram_8_w )
{
	COMBINE_WORD_MEM(&exterm_master_videoram[offset], data);

	FORCOL_TO_PEN(data);

	tmpbitmap->line[offset >> 9][(offset >> 1) & 0xff] = Machine->pens[data];
}

READ_HANDLER( exterm_slave_videoram_r )
{
    return READ_WORD(&exterm_slave_videoram[offset]);
}

static WRITE_HANDLER( exterm_slave_videoram_16_w )
{
	int x,y;
	unsigned short *pens = Machine->pens;
	struct osd_bitmap *foreground;

	COMBINE_WORD_MEM(&exterm_slave_videoram[offset], data);

	x = offset & 0xff;
	y = (offset >> 8) & 0xff;

	foreground = (offset & 0x10000) ? tmpbitmap2 : tmpbitmap1;

	((unsigned short *)foreground->line[y])[x  ] = pens[ (data       & 0xff)];
	((unsigned short *)foreground->line[y])[x+1] = pens[((data >> 8) & 0xff)];
}

static WRITE_HANDLER( exterm_slave_videoram_8_w )
{
	int x,y;
	unsigned short *pens = Machine->pens;
	struct osd_bitmap *foreground;

	COMBINE_WORD_MEM(&exterm_slave_videoram[offset], data);

	x = offset & 0xff;
	y = (offset >> 8) & 0xff;

	foreground = (offset & 0x10000) ? tmpbitmap2 : tmpbitmap1;

	foreground->line[y][x  ] = pens[ (data       & 0xff)];
	foreground->line[y][x+1] = pens[((data >> 8) & 0xff)];
}

WRITE_HANDLER( exterm_paletteram_w )
{
	if ((offset == 0xff*2) && (data == 0))
	{
		/* Turn shadow color into dark red */
		data = 0x400;
	}

	paletteram_xRRRRRGGGGGBBBBB_word_w(offset, data);
}


#define FROM_SHIFTREG_MASTER(TYPE)										\
	int i;																\
	unsigned TYPE *line = &((unsigned TYPE *)tmpbitmap->line[address>>12])[0];	\
	unsigned short *pens = Machine->pens;								\
																		\
    for (i = 0; i < 256; i++)											\
	{																	\
		int data = shiftreg[i];			                                \
																		\
		FORCOL_TO_PEN(data);											\
																		\
		*(line++) = pens[data];	 										\
	}																	\
																		\
	memcpy(&exterm_master_videoram[address>>3], shiftreg, 256*sizeof(unsigned short));


void exterm_to_shiftreg_master(unsigned int address, unsigned short* shiftreg)
{
	memcpy(shiftreg, &exterm_master_videoram[address>>3], 256*sizeof(unsigned short));
}

void exterm_from_shiftreg_master(unsigned int address, unsigned short* shiftreg)
{
	if (Machine->scrbitmap->depth == 16)
	{
		FROM_SHIFTREG_MASTER(short);
	}
	else
	{
		FROM_SHIFTREG_MASTER(char);
	}
}

#define FROM_SHIFTREG_SLAVE(TYPE)										\
	int i;																\
	int y = (address >> 11) & 0xff;										\
	unsigned TYPE *line1, *line2;										\
	unsigned short *pens = Machine->pens;								\
																		\
	if (address & 0x80000)												\
	{																	\
		line1 = &((unsigned TYPE *)tmpbitmap2->line[y  ])[0];			\
		line2 = &((unsigned TYPE *)tmpbitmap2->line[y+1])[0];			\
	}																	\
	else																\
	{																	\
		line1 = &((unsigned TYPE *)tmpbitmap1->line[y  ])[0];			\
		line2 = &((unsigned TYPE *)tmpbitmap1->line[y+1])[0];			\
	}																	\
																		\
    for (i = 0; i < 256; i++)											\
	{																	\
		*(line1++) = pens[((unsigned char*)shiftreg)[i    ]];			\
		*(line2++) = pens[((unsigned char*)shiftreg)[i+256]];			\
	}																	\
																		\
	memcpy(&exterm_slave_videoram[address>>3], shiftreg, 256*2*sizeof(unsigned char));


void exterm_to_shiftreg_slave(unsigned int address, unsigned short* shiftreg)
{
	memcpy(shiftreg, &exterm_slave_videoram[address>>3], 256*2*sizeof(unsigned char));
}

void exterm_from_shiftreg_slave(unsigned int address, unsigned short* shiftreg)
{
	if (Machine->scrbitmap->depth == 16)
	{
		FROM_SHIFTREG_SLAVE(short);
	}
	else
	{
		FROM_SHIFTREG_SLAVE(char);
	}
}


int exterm_vh_start(void)
{
	if ((tmpbitmap = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		return 1;
	}

	if ((tmpbitmap1 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		bitmap_free(tmpbitmap);
		return 1;
	}

	if ((tmpbitmap2 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		bitmap_free(tmpbitmap);
		bitmap_free(tmpbitmap1);
		return 1;
	}

	/* Install depth specific handler */
	if (Machine->scrbitmap->depth == 16)
	{
		install_mem_write_handler(0, TOBYTE(0x00000000), TOBYTE(0x000fffff), exterm_master_videoram_16_w);
		install_mem_write_handler(1, TOBYTE(0x00000000), TOBYTE(0x000fffff), exterm_slave_videoram_16_w);
	}
	else
	{
		install_mem_write_handler(0, TOBYTE(0x00000000), TOBYTE(0x000fffff), exterm_master_videoram_8_w);
		install_mem_write_handler(1, TOBYTE(0x00000000), TOBYTE(0x000fffff), exterm_slave_videoram_8_w);
	}

	palette_used_colors[0] = PALETTE_COLOR_TRANSPARENT;

	return 0;
}

void exterm_vh_stop (void)
{
	bitmap_free(tmpbitmap);
	bitmap_free(tmpbitmap1);
	bitmap_free(tmpbitmap2);
}


static struct rectangle foregroundvisiblearea =
{
	0, 255, 40, 238
};


#define REFRESH(TYPE)										\
	unsigned TYPE *bg1, *bg2, *fg1, *fg2;					\
	unsigned short *pens = Machine->pens;					\
															\
	for (y = 0; y < 256; y++)								\
	{														\
		bg1  = &((unsigned TYPE *)   bitmap ->line[y])[0];	\
		bg2  = &((unsigned TYPE *)tmpbitmap ->line[y])[0];	\
		fg1  = &((unsigned TYPE *)tmpbitmap1->line[y])[0];	\
		fg2  = &((unsigned TYPE *)tmpbitmap2->line[y])[0];	\
															\
		for (x = 256; x; x--, bgsrc++)						\
		{													\
			int data = READ_WORD(bgsrc);					\
			FORCOL_TO_PEN(data);							\
			*(bg1++) = *(bg2++) = pens[data];				\
		}													\
		for (x = 128; x; x--, fgsrc1++, fgsrc2++)			\
		{													\
			int data1 = READ_WORD(fgsrc1);					\
			int data2 = READ_WORD(fgsrc2);					\
			*(fg1++) = pens[data1 & 0xff];					\
			*(fg1++) = pens[data1 >> 8];					\
			*(fg2++) = pens[data2 & 0xff];					\
			*(fg2++) = pens[data2 >> 8];					\
		}													\
	}

void exterm_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	if (tms34010_io_display_blanked(0))
	{
		fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);
		return;
	}

	if (palette_recalc() != 0)
	{
		/* Redraw screen */
		int x,y;
		unsigned short *bgsrc  = (unsigned short *)&exterm_master_videoram[0];
		unsigned short *fgsrc1 = (unsigned short *)&exterm_slave_videoram[0];
		unsigned short *fgsrc2 = (unsigned short *)&exterm_slave_videoram[256*256];

		if (tmpbitmap1->depth == 16)
		{
			REFRESH(short);
		}
		else
		{
			REFRESH(char);
		}
	}
	else
	{
		copybitmap(bitmap,tmpbitmap, 0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}

    if (tms34010_get_DPYSTRT(1) & 0x800)
	{
		copybitmap(bitmap,tmpbitmap2,0,0,0,0,&foregroundvisiblearea,TRANSPARENCY_PEN, palette_transparent_pen);
	}
	else
	{
		copybitmap(bitmap,tmpbitmap1,0,0,0,0,&foregroundvisiblearea,TRANSPARENCY_PEN, palette_transparent_pen);
	}
}
