#include "driver.h"
#include "vidhrdw/generic.h"



static int flipscreen;


/***************************************************************************

  Convert the color PROMs into a more useable format.

***************************************************************************/
void higemaru_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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

	/* color_prom now points to the beginning of the lookup table */

	/* characters use colors 0-15 */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = *(color_prom++) & 0x0f;

	color_prom += 128;	/* the bottom half of the PROM doesn't seem to be used */

	/* sprites use colors 16-31 */
	for (i = 0;i < TOTAL_COLORS(1);i++)
		COLOR(1,i) = (*(color_prom++) & 0x0f) + 0x10;
}



WRITE_HANDLER( higemaru_c800_w )
{
	//if (data & 0x7c) logerror("c800 = %02x\n",data);

	/* bits 0 and 1 are coin counters */
	coin_counter_w(0,data & 2);
	coin_counter_w(1,data & 1);

	/* bit 7 flips screen */
	if (flipscreen != (data & 0x80))
	{
		flipscreen = data & 0x80;
		memset(dirtybuffer,1,videoram_size);
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void higemaru_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;

	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy;

			dirtybuffer[offs] = 0;
			sx = offs % 32;
			sy = offs / 32;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + ((colorram[offs] & 0x80) << 1),
					colorram[offs] & 0x1f,
					flipscreen,flipscreen,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the background graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);


	/* Draw the sprites. */
	for (offs = spriteram_size - 16;offs >= 0;offs -= 16)
	{
		int code,col,sx,sy,flipx,flipy;


		code = spriteram[offs] & 0x7f;
		col = spriteram[offs + 4] & 0x0f;
		sx = spriteram[offs + 12];
		sy = spriteram[offs + 8];
		flipx = spriteram[offs + 4] & 0x10;
		flipy = spriteram[offs + 4] & 0x20;
		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[1],
				code,
				col,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,15);

		/* draw again with wraparound */
		drawgfx(bitmap,Machine->gfx[1],
				code,
				col,
				flipx,flipy,
				sx - 256,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,15);
	}
}
