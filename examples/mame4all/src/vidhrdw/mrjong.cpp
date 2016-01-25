/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int flipscreen;


/***************************************************************************

  Convert the color PROMs. (from vidhrdw/penco.c)

***************************************************************************/
void mrjong_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn, offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + (offs)])

	for (i = 0; i < Machine->drv->total_colors; i++)
	{
		int bit0, bit1, bit2;

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

	color_prom += 0x10;
	/* color_prom now points to the beginning of the lookup table */

	/* character lookup table */
	/* sprites use the same color lookup table as characters */
	for (i = 0; i < TOTAL_COLORS(0); i++)
		COLOR(0, i) = *(color_prom++) & 0x0f;
}


/***************************************************************************

  Display control parameter.

***************************************************************************/
WRITE_HANDLER( mrjong_flipscreen_w )
{
	if (flipscreen != (data & 1))
	{
		flipscreen = (data & 1);
		memset(dirtybuffer, 1, videoram_size);
	}
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.

***************************************************************************/
void mrjong_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int offs;

	/* Draw the tiles. */
	for (offs = (videoram_size - 1); offs > 0; offs--)
	{
		if (dirtybuffer[offs])
		{
			int tile;
			int color;
			int sx, sy;
			int flipx, flipy;

			dirtybuffer[offs] = 0;

			tile = videoram[offs] | ((colorram[offs] & 0x20) << 3);
			flipx = (colorram[offs] & 0x40) >> 6;
			flipy = (colorram[offs] & 0x80) >> 7;
			color = colorram[offs] & 0x1f;

			sx = 31 - (offs % 32);
			sy = 31 - (offs / 32);

			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(tmpbitmap, Machine->gfx[0],
					tile,
					color,
					flipx, flipy,
					8*sx, 8*sy,
					&Machine->visible_area, TRANSPARENCY_NONE, 0);
		}
	}
	copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);

	/* Draw the sprites. */
	for (offs = (spriteram_size - 4); offs >= 0; offs -= 4)
	{
		int sprt;
		int color;
		int sx, sy;
		int flipx, flipy;

		sprt = (((spriteram[offs + 1] >> 2) & 0x3f) | ((spriteram[offs + 3] & 0x20) << 1));
		flipx = (spriteram[offs + 1] & 0x01) >> 0;
		flipy = (spriteram[offs + 1] & 0x02) >> 1;
		color = (spriteram[offs + 3] & 0x1f);

		sx = 224 - spriteram[offs + 2];
		sy = spriteram[offs + 0];
		if (flipscreen)
		{
			sx = 208 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		drawgfx(bitmap, Machine->gfx[1],
				sprt,
				color,
				flipx, flipy,
				sx, sy,
				&Machine->visible_area, TRANSPARENCY_PEN, 0);
	}
}
