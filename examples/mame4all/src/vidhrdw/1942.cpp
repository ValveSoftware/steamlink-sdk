/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *c1942_foreground_videoram;
unsigned char *c1942_foreground_colorram;
size_t c1942_foreground_videoram_size;
unsigned char *c1942_scroll;
static data_t c1942_palette_bank;
static struct osd_bitmap *tmpbitmap2;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  1942 has three 256x4 palette PROMs (one per gun) and three 256x4 lookup
  table PROMs (one for characters, one for sprites, one for background tiles).
  The palette PROMs are connected to the RGB output this way:

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void c1942_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2,bit3;


		/* red component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* green component */
		bit0 = (color_prom[Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* blue component */
		bit0 = (color_prom[2*Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[2*Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[2*Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[2*Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}

	color_prom += 2*Machine->drv->total_colors;
	/* color_prom now points to the beginning of the lookup table */


	/* characters use colors 128-143 */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = *(color_prom++) + 128;

	/* background tiles use colors 0-63 in four banks */
	for (i = 0;i < TOTAL_COLORS(1)/4;i++)
	{
		COLOR(1,i) = *color_prom;
		COLOR(1,i+32*8) = *color_prom + 16;
		COLOR(1,i+2*32*8) = *color_prom + 32;
		COLOR(1,i+3*32*8) = *color_prom + 48;
		color_prom++;
	}

	/* sprites use colors 64-79 */
	for (i = 0;i < TOTAL_COLORS(2);i++)
		COLOR(2,i) = *(color_prom++) + 64;
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int c1942_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	/* the background area is twice as wide as the screen (actually twice as tall, */
	/* because this is a vertical game) */
	if ((tmpbitmap2 = bitmap_alloc(2*Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		generic_vh_stop();
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void c1942_vh_stop(void)
{
	bitmap_free(tmpbitmap2);
	generic_vh_stop();
}



WRITE_HANDLER( c1942_palette_bank_w )
{
	set_vh_global_attribute( &c1942_palette_bank, data );
}


WRITE_HANDLER( c1942_c804_w )
{
	/* bit 7: flip screen
       bit 4: cpu B reset
	   bit 0: coin counter */

	coin_counter_w(offset, data & 0x01);

	cpu_set_reset_line(1,(data & 0x10) ? ASSERT_LINE : CLEAR_LINE);

	flip_screen_w(offset, data & 0x80);
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void c1942_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	if (full_refresh)
	{
		memset(dirtybuffer, 1, videoram_size);
	}

	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if ((offs & 0x10) == 0 && (dirtybuffer[offs] != 0 || dirtybuffer[offs + 16] != 0))
		{
			int sx,sy,flipx,flipy;


			dirtybuffer[offs] = dirtybuffer[offs + 16] = 0;

			sx = offs / 32;
			sy = offs % 32;
			flipx = videoram[offs + 16] & 0x20;
			flipy = videoram[offs + 16] & 0x40;
			if (flip_screen)
			{
				sx = 31 - sx;
				sy = 15 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(tmpbitmap2,Machine->gfx[1],
					videoram[offs] + 2*(videoram[offs + 16] & 0x80),
					(videoram[offs + 16] & 0x1f) + 32 * c1942_palette_bank,
					flipx,flipy,
					16 * sx,16 * sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the background graphics */
	{
		int scroll;


		scroll = -(c1942_scroll[0] + 256 * c1942_scroll[1]);
		if (flip_screen) scroll = 256-scroll;

		copyscrollbitmap(bitmap,tmpbitmap2,1,&scroll,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* Draw the sprites. */
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int i,code,col,sx,sy,dir;


		code = (spriteram[offs] & 0x7f) + 4*(spriteram[offs + 1] & 0x20)
				+ 2*(spriteram[offs] & 0x80);
		col = spriteram[offs + 1] & 0x0f;
		sx = spriteram[offs + 3] - 0x10 * (spriteram[offs + 1] & 0x10);
		sy = spriteram[offs + 2];
		dir = 1;
		if (flip_screen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			dir = -1;
		}

		/* handle double / quadruple height (actually width because this is a rotated game) */
		i = (spriteram[offs + 1] & 0xc0) >> 6;
		if (i == 2) i = 3;

		do
		{
			drawgfx(bitmap,Machine->gfx[2],
					code + i,col,
					flip_screen,flip_screen,
					sx,sy + 16 * i * dir,
					&Machine->visible_area,TRANSPARENCY_PEN,15);

			i--;
		} while (i >= 0);
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = c1942_foreground_videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy;


		sx = offs % 32;
		sy = offs / 32;
		if (flip_screen)
		{
			sx = 31 - sx;
			sy = 31 - sy;
		}

		drawgfx(bitmap,Machine->gfx[0],
				c1942_foreground_videoram[offs] + 2 * (c1942_foreground_colorram[offs] & 0x80),
				c1942_foreground_colorram[offs] & 0x3f,
				flip_screen,flip_screen,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
