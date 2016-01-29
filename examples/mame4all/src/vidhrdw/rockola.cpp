/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *rockola_videoram2;
unsigned char *rockola_characterram;
unsigned char *rockola_scrollx,*rockola_scrolly;
static unsigned char dirtycharacter[256];
static data_t charbank;
static int backcolor;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Zarzon has a different PROM layout from the others.

***************************************************************************/
void rockola_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2;


		/* red component */
        bit0 = (*color_prom >> 0) & 0x01;
        bit1 = (*color_prom >> 1) & 0x01;
        bit2 = (*color_prom >> 2) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
        bit0 = (*color_prom >> 3) & 0x01;
        bit1 = (*color_prom >> 4) & 0x01;
        bit2 = (*color_prom >> 5) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
        bit1 = (*color_prom >> 6) & 0x01;
        bit2 = (*color_prom >> 7) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		color_prom++;
	}


	backcolor = 0;	/* background color can be changed by the game */

	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = i;

	for (i = 0;i < TOTAL_COLORS(1);i++)
	{
		if (i % 4 == 0) COLOR(1,i) = 4 * backcolor + 0x20;
		else COLOR(1,i) = i + 0x20;
	}
}

void satansat_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2;


		/* red component */
        bit0 = (*color_prom >> 0) & 0x01;
        bit1 = (*color_prom >> 1) & 0x01;
        bit2 = (*color_prom >> 2) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
        bit0 = (*color_prom >> 3) & 0x01;
        bit1 = (*color_prom >> 4) & 0x01;
        bit2 = (*color_prom >> 5) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
        bit1 = (*color_prom >> 6) & 0x01;
        bit2 = (*color_prom >> 7) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		color_prom++;
	}


	backcolor = 0;	/* background color can be changed by the game */

	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = 4 * (i % 4) + (i / 4);

	for (i = 0;i < TOTAL_COLORS(1);i++)
	{
		if (i % 4 == 0) COLOR(1,i) = backcolor + 0x10;
		else COLOR(1,i) = 4 * (i % 4) + (i / 4) + 0x10;
	}
}



WRITE_HANDLER( rockola_characterram_w )
{
	if (rockola_characterram[offset] != data)
	{
		dirtycharacter[(offset / 8) & 0xff] = 1;
		rockola_characterram[offset] = data;
	}
}



WRITE_HANDLER( rockola_flipscreen_w )
{
	/* bits 0-2 select background color */
	if (backcolor != (data & 7))
	{
		int i;


		backcolor = data & 7;

		for (i = 0;i < 32;i += 4)
			Machine->gfx[1]->colortable[i] = Machine->pens[4 * backcolor + 0x20];

		schedule_full_refresh();
	}

	/* bit 3 selects char bank */
	set_vh_global_attribute(&charbank,(~data & 0x08) >> 3);

	/* bit 7 flips screen */
	flip_screen_w(0,data & 0x80);
}


WRITE_HANDLER( satansat_b002_w )
{
	/* bit 0 flips screen */
	flip_screen_w(0,data & 0x01);

	/* bit 1 enables interrups */
	/* it controls only IRQs, not NMIs. Here I am affecting both, which */
	/* is wrong. */
	interrupt_enable_w(0,data & 0x02);

	/* other bits unused */
}



WRITE_HANDLER( satansat_backcolor_w )
{
	/* bits 0-1 select background color. Other bits unused. */
	if (backcolor != (data & 3))
	{
		int i;


		backcolor = data & 3;

		for (i = 0;i < 16;i += 4)
			Machine->gfx[1]->colortable[i] = Machine->pens[backcolor + 0x10];

		schedule_full_refresh();
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void rockola_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	if (full_refresh)
		memset(dirtybuffer,1,videoram_size);

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy;


			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			if (flip_screen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
			}

			drawgfx(tmpbitmap,Machine->gfx[1],
					videoram[offs] + 256 * charbank,
					(colorram[offs] & 0x38) >> 3,
					flip_screen,flip_screen,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the background graphics */
	{
		int scrollx,scrolly;


		scrollx = -*rockola_scrolly;
		scrolly = -*rockola_scrollx;

		if (flip_screen)
		{
			scrollx = -scrollx;
			scrolly = -scrolly;
		}

		copyscrollbitmap(bitmap,tmpbitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int charcode;
		int sx,sy;


		charcode = rockola_videoram2[offs];

		/* decode modified characters */
		if (dirtycharacter[charcode] != 0)
		{
			decodechar(Machine->gfx[0],charcode,rockola_characterram,
					   Machine->drv->gfxdecodeinfo[0].gfxlayout);
			dirtycharacter[charcode] = 0;
		}

		sx = offs % 32;
		sy = offs / 32;
		if (flip_screen)
		{
			sx = 31 - sx;
			sy = 31 - sy;
		}

		drawgfx(bitmap,Machine->gfx[0],
				charcode,
				colorram[offs] & 0x07,
				flip_screen,flip_screen,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}


/* Zarzon's background doesn't scroll, and the color code selection is different. */
void satansat_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	if (full_refresh)
		memset(dirtybuffer,1,videoram_size);

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
    for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
	        int sx,sy;


			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			if (flip_screen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
			}

			drawgfx(tmpbitmap,Machine->gfx[1],
					videoram[offs],
					(colorram[offs] & 0x0c) >> 2,
					flip_screen,flip_screen,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
    }

	/* copy the temporary bitmap to the screen */
    copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int charcode;
		int sx,sy;


		charcode = rockola_videoram2[offs];

		/* decode modified characters */
		if (dirtycharacter[charcode] != 0)
		{
			decodechar(Machine->gfx[0],charcode,rockola_characterram,
					   Machine->drv->gfxdecodeinfo[0].gfxlayout);
			dirtycharacter[charcode] = 0;
		}

		sx = offs % 32;
		sy = offs / 32;
		if (flip_screen)
		{
			sx = 31 - sx;
			sy = 31 - sy;
		}

		drawgfx(bitmap,Machine->gfx[0],
				charcode,
				colorram[offs] & 0x03,
				flip_screen,flip_screen,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
