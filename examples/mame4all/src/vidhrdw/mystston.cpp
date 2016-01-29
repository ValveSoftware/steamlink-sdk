/***************************************************************************

	vidhrdw.c

	Functions to emulate the video hardware of the machine.

	There are only a few differences between the video hardware of Mysterious
	Stones and Mat Mania. The tile bank select bit is different and the sprite
	selection seems to be different as well. Additionally, the palette is stored
	differently. I'm also not sure that the 2nd tile page is really used in
	Mysterious Stones.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *mystston_videoram2,*mystston_colorram2;
size_t mystston_videoram2_size;
unsigned char *mystston_scroll;
static int textcolor;
static int flipscreen;

/***************************************************************************

  Convert the color PROMs into a more useable format.

  Mysterious Stones has both palette RAM and a PROM. The PROM is used for
  text.

***************************************************************************/
void mystston_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	palette += 3*24;	/* first 24 colors are RAM */

	for (i = 0;i < 32;i++)
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
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int mystston_vh_start(void)
{
	if ((dirtybuffer = (unsigned char*)malloc(videoram_size)) == 0)
		return 1;
	memset(dirtybuffer,1,videoram_size);

	/* Mysterious Stones has a virtual screen twice as large as the visible screen */
	if ((tmpbitmap = bitmap_alloc(Machine->drv->screen_width,2*Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void mystston_vh_stop(void)
{
	free(dirtybuffer);
	bitmap_free(tmpbitmap);
}



WRITE_HANDLER( mystston_2000_w )
{
	/* bits 0 and 1 are text color */
	textcolor = ((data & 0x01) << 1) | ((data & 0x02) >> 1);

	/* bits 4 and 5 are coin counters */
	coin_counter_w(0,data & 0x10);
	coin_counter_w(1,data & 0x20);

	/* bit 7 is screen flip */
	if (flipscreen != (data & 0x80))
	{
		flipscreen = data & 0x80;
		memset(dirtybuffer,1,videoram_size);
	}

	/* other bits unused? */
//logerror("PC %04x: 2000 = %02x\n",cpu_get_pc(),data);
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void mystston_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	if (palette_recalc())
		memset(dirtybuffer,1,videoram_size);

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,flipy;


			dirtybuffer[offs] = 0;

			sx = 15 - offs / 32;
			sy = offs % 32;
			flipy = (sy >= 16) ? 1 : 0;	/* flip horizontally tiles on the right half of the bitmap */
			if (flipscreen)
			{
				sx = 15 - sx;
				sy = 31 - sy;
				flipy = !flipy;
			}
			drawgfx(tmpbitmap,Machine->gfx[1],
					videoram[offs] + 256 * (colorram[offs] & 0x01),
					0,
					flipscreen,flipy,
					16*sx,16*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scrolly;


		scrolly = -*mystston_scroll;
		if (flipscreen) scrolly = 256 - scrolly;

		copyscrollbitmap(bitmap,tmpbitmap,0,0,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* Draw the sprites */
	for (offs = 0;offs < spriteram_size;offs += 4)
	{
		if (spriteram[offs] & 0x01)
		{
			int sx,sy,flipx,flipy;


			sx = 240 - spriteram[offs+3];
			sy = (240 - spriteram[offs+2]) & 0xff;
			flipx = spriteram[offs] & 0x04;
			flipy = spriteram[offs] & 0x02;
			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(bitmap,Machine->gfx[2],
					spriteram[offs+1] + ((spriteram[offs] & 0x10) << 4),
					(spriteram[offs] & 0x08) >> 3,
					flipx,flipy,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = mystston_videoram2_size - 1;offs >= 0;offs--)
	{
		int sx,sy;


		sx = 31 - offs / 32;
		sy = offs % 32;
		if (flipscreen)
		{
			sx = 31 - sx;
			sy = 31 - sy;
		}

		drawgfx(bitmap,Machine->gfx[0],
				mystston_videoram2[offs] + 256 * (mystston_colorram2[offs] & 0x07),
				textcolor,
				flipscreen,flipscreen,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
