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

  Centipede doesn't have a color PROM. Eight RAM locations control
  the color of characters and sprites. The meanings of the four bits are
  (all bits are inverted):

  bit 3 alternate
        blue
        green
  bit 0 red

  The alternate bit affects blue and green, not red. The way I weighted its
  effect might not be perfectly accurate, but is reasonably close.

***************************************************************************/
static void setcolor(int pen,int data)
{
	int r,g,b;


	r = 0xff * ((~data >> 0) & 1);
	g = 0xff * ((~data >> 1) & 1);
	b = 0xff * ((~data >> 2) & 1);

	if (~data & 0x08) /* alternate = 1 */
	{
		/* when blue component is not 0, decrease it. When blue component is 0, */
		/* decrease green component. */
		if (b) b = 0xc0;
		else if (g) g = 0xc0;
	}

	palette_change_color(pen,r,g,b);
}

WRITE_HANDLER( centiped_paletteram_w )
{
	paletteram[offset] = data;

	/* the char palette will be effectively updated by the next interrupt handler */

	if (offset >= 12 && offset < 16)	/* sprites palette */
	{
		int start = Machine->drv->gfxdecodeinfo[1].color_codes_start;

		setcolor(start + (offset - 12),data);
	}
}

static int powerup_counter;

void centiped_init_machine(void)
{
	powerup_counter = 10;
}

int centiped_interrupt(void)
{
	int offset;
	int slice = 3 - cpu_getiloops();
	int start = Machine->drv->gfxdecodeinfo[0].color_codes_start;


	/* set the palette for the previous screen slice to properly support */
	/* midframe palette changes in test mode */
	for (offset = 4;offset < 8;offset++)
		setcolor(4 * slice + start + (offset - 4),paletteram[offset]);

	/* Centipede doesn't like to receive interrupts just after a reset. */
	/* The only workaround I've found is to wait a little before starting */
	/* to generate them. */
	if (powerup_counter == 0)
		return interrupt();
	else
	{
		powerup_counter--;
		return ignore_interrupt();
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void centiped_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;

	if (palette_recalc() || full_refresh)
		memset (dirtybuffer, 1, videoram_size);

	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy;


			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;

			drawgfx(bitmap,Machine->gfx[0],
					(videoram[offs] & 0x3f) + 0x40,
					(sy + 1) / 8,	/* support midframe palette changes in test mode */
					flip_screen,flip_screen,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	/* Draw the sprites */
	for (offs = 0;offs < 0x10;offs++)
	{
		int spritenum,color;
		int flipx;
		int x, y;
		int sx, sy;


		spritenum = spriteram[offs] & 0x3f;
		if (spritenum & 1) spritenum = spritenum / 2 + 64;
		else spritenum = spritenum / 2;

		flipx = (spriteram[offs] & 0x80);
		x = spriteram[offs + 0x20];
		y = 240 - spriteram[offs + 0x10];

		if (flip_screen)
		{
			flipx = !flipx;
		}

		/* Centipede is unusual because the sprite color code specifies the */
		/* colors to use one by one, instead of a combination code. */
		/* bit 5-4 = color to use for pen 11 */
		/* bit 3-2 = color to use for pen 10 */
		/* bit 1-0 = color to use for pen 01 */
		/* pen 00 is transparent */
		color = spriteram[offs+0x30];
		Machine->gfx[1]->colortable[3] =
				Machine->pens[Machine->drv->gfxdecodeinfo[1].color_codes_start + ((color >> 4) & 3)];
		Machine->gfx[1]->colortable[2] =
				Machine->pens[Machine->drv->gfxdecodeinfo[1].color_codes_start + ((color >> 2) & 3)];
		Machine->gfx[1]->colortable[1] =
				Machine->pens[Machine->drv->gfxdecodeinfo[1].color_codes_start + ((color >> 0) & 3)];

		drawgfx(bitmap,Machine->gfx[1],
				spritenum,0,
				flip_screen,flipx,
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
					if ((x2 < 32) && (y2 < 30) && (x2 >= 0) && (y2 >= 0))
						dirtybuffer[x2 + 32*y2] = 1;
				}
			}
		}

	}
}
