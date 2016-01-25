/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



static struct rectangle spritevisiblearea =
{
	1*8, 31*8-1,
	0*8, 30*8-1
};



/***************************************************************************

  Millipede doesn't have a color PROM, it uses RAM.
  The RAM seems to be conncted to the video output this way:

  bit 7 red
        red
        red
        green
        green
        blue
        blue
  bit 0 blue

***************************************************************************/
WRITE_HANDLER( milliped_paletteram_w )
{
	int bit0,bit1,bit2;
	int r,g,b;


	paletteram[offset] = data;

	/* red component */
	bit0 = (~data >> 5) & 0x01;
	bit1 = (~data >> 6) & 0x01;
	bit2 = (~data >> 7) & 0x01;
	r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	/* green component */
	bit0 = 0;
	bit1 = (~data >> 3) & 0x01;
	bit2 = (~data >> 4) & 0x01;
	g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	/* blue component */
	bit0 = (~data >> 0) & 0x01;
	bit1 = (~data >> 1) & 0x01;
	bit2 = (~data >> 2) & 0x01;
	b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	palette_change_color(offset,r,g,b);
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void milliped_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	if (palette_recalc() || full_refresh)
		memset (dirtybuffer, 1, videoram_size);

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy;
			int bank;
			int color;


			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;

			if (videoram[offs] & 0x40)
				bank = 1;
			else bank = 0;

			if (videoram[offs] & 0x80)
				color = 2;
			else color = 0;

			drawgfx(bitmap,Machine->gfx[0],
					0x40 + (videoram[offs] & 0x3f) + 0x80 * bank,
					bank + color,
					0,0,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}


	/* Draw the sprites */
	for (offs = 0;offs < 0x10;offs++)
	{
		int spritenum,color;
		int x, y;
		int sx, sy;


		x = spriteram[offs + 0x20];
		y = 240 - spriteram[offs + 0x10];

		spritenum = spriteram[offs] & 0x3f;
		if (spritenum & 1) spritenum = spritenum / 2 + 64;
		else spritenum = spritenum / 2;

		/* Millipede is unusual because the sprite color code specifies the */
		/* colors to use one by one, instead of a combination code. */
		/* bit 7-6 = palette bank (there are 4 groups of 4 colors) */
		/* bit 5-4 = color to use for pen 11 */
		/* bit 3-2 = color to use for pen 10 */
		/* bit 1-0 = color to use for pen 01 */
		/* pen 00 is transparent */
		color = spriteram[offs + 0x30];
		Machine->gfx[1]->colortable[3] =
				Machine->pens[16+((color >> 4) & 3)+4*((color >> 6) & 3)];
		Machine->gfx[1]->colortable[2] =
				Machine->pens[16+((color >> 2) & 3)+4*((color >> 6) & 3)];
		Machine->gfx[1]->colortable[1] =
				Machine->pens[16+((color >> 0) & 3)+4*((color >> 6) & 3)];

		drawgfx(bitmap,Machine->gfx[1],
				spritenum,
				0,
				0,spriteram[offs] & 0x80,
				x,y,
				&spritevisiblearea,TRANSPARENCY_PEN,0);

		/* mark tiles underneath as dirty */
		sx = x >> 3;
		sy = y >> 3;

		{
			int max_x = 1;
			int max_y = 2;
			int x2, y2;

			if (x & 0x07) max_x ++;
			if (y & 0x0f) max_y ++;

			for (y2 = sy; y2 < sy + max_y; y2 ++)
			{
				for (x2 = sx; x2 < sx + max_x; x2 ++)
				{
					if ((x2 < 32) && (y2 < 32) && (x2 >= 0) && (y2 >= 0))
						dirtybuffer[x2 + 32*y2] = 1;
				}
			}
		}

	}
}
