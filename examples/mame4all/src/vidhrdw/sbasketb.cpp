/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *sbasketb_scroll;
unsigned char *sbasketb_palettebank;
unsigned char *sbasketb_spriteram_select;

static int flipscreen = 0;
static struct rectangle scroll_area = { 0*8, 32*8-1, 0*8, 32*8-1 };



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Super Basketball has three 256x4 palette PROMs (one per gun) and two 256x4
  lookup table PROMs (one for characters, one for sprites).
  I don't know for sure how the palette PROMs are connected to the RGB
  output, but it's probably the usual:

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void sbasketb_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2,bit3;


		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[2*Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[2*Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[2*Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[2*Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}

	color_prom += 2*Machine->drv->total_colors;
	/* color_prom now points to the beginning of the character lookup table */


	/* characters use colors 240-255 */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = (*(color_prom++) & 0x0f) + 240;

	/* sprites use colors 0-256 (?) in 16 banks */
	for (i = 0;i < TOTAL_COLORS(1)/16;i++)
	{
		int j;


		for (j = 0;j < 16;j++)
			COLOR(1,i + j * TOTAL_COLORS(1)/16) = (*color_prom & 0x0f) + 16 * j;

		color_prom++;
	}
}


WRITE_HANDLER( sbasketb_flipscreen_w )
{
	if (flipscreen != data)
	{
		flipscreen = data;
		memset(dirtybuffer,1,videoram_size);
	}
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void sbasketb_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,i;
	int sx,sy,code,color,flipx,flipy;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;

			code  = videoram[offs] + ((colorram[offs] & 0x20) << 3);
			color = colorram[offs] & 0x0f;
			flipx = colorram[offs] & 0x40;
			flipy = colorram[offs] & 0x80;

			if (flipscreen)
			{
				flipx = !flipx;
				flipy = !flipy;

				sx = 31 - sx;
				sy = 31 - sy;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					code, color,
					flipx, flipy,
					8*sx,8*sy,
					&scroll_area,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scroll[32];

		if (!flipscreen)
		{
			for (i = 0;i < 6;i++)
				scroll[i] = 0;

			for (i = 6;i < 32;i++)
				scroll[i] = -*sbasketb_scroll - 1;
		}
		else
		{
			for (i = 26;i < 32;i++)
				scroll[i] = 0;

			for (i = 0;i < 26;i++)
				scroll[i] = *sbasketb_scroll + 1;
		}

		copyscrollbitmap(bitmap,tmpbitmap,0,0,32,scroll,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}

	/* Draw the sprites */
	offs = (*sbasketb_spriteram_select & 0x01) * 0x100;

	for (i = 0; i < 64; i++, offs += 4)
	{
		sx = spriteram[offs + 2];
		sy = spriteram[offs + 3];

		if (sx || sy)
		{
			code  =  spriteram[offs + 0] | ((spriteram[offs + 1] & 0x20) << 3);
			color = (spriteram[offs + 1] & 0x0f) + 16 * *sbasketb_palettebank;
			flipx =  spriteram[offs + 1] & 0x40;
			flipy =  spriteram[offs + 1] & 0x80;

			if (flipscreen)
			{
				flipx = !flipx;
				flipy = !flipy;

				sx = 240 - sx;
				sy = 240 - sy;
			}

			drawgfx(bitmap,Machine->gfx[1],
					code, color,
					flipx, flipy,
					sx, sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}
}
