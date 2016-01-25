/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *shaolins_scroll;
static int palettebank;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Shao-lin's Road has three 256x4 palette PROMs (one per gun) and two 256x4
  lookup table PROMs (one for characters, one for sprites).
  I don't know for sure how the palette PROMs are connected to the RGB
  output, but it's probably the usual:

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void shaolins_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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


	/* there are eight 32 colors palette banks; sprites use colors 0-15 and */
	/* characters 16-31 of each bank. */
	for (i = 0;i < TOTAL_COLORS(0)/8;i++)
	{
		int j;


		for (j = 0;j < 8;j++)
			COLOR(0,i + j * TOTAL_COLORS(0)/8) = (*color_prom & 0x0f) + 32 * j + 16;

		color_prom++;
	}

	for (i = 0;i < TOTAL_COLORS(1)/8;i++)
	{
		int j;


		for (j = 0;j < 8;j++)
		{
			/* preserve transparency */
			if ((*color_prom & 0x0f) == 0) COLOR(1,i + j * TOTAL_COLORS(1)/8) = 0;
			else COLOR(1,i + j * TOTAL_COLORS(1)/8) = (*color_prom & 0x0f) + 32 * j;
		}

		color_prom++;
	}
}



WRITE_HANDLER( shaolins_palettebank_w )
{
	if (palettebank != (data & 7))
	{
		palettebank = data & 7;
		memset(dirtybuffer,1,videoram_size);
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void shaolins_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;
	int sx,sy;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = 0;offs <= videoram_size;offs++)
	{
		if (dirtybuffer[offs])
		{
			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + ((colorram[offs] & 0x40) << 2),
					(colorram[offs] & 0x0f) + 16 * palettebank,
					0,colorram[offs] & 0x20,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scroll[32], i;

		for (i = 0;i < 4;i++)
			scroll[i] = 0;
		for (i = 4;i < 32;i++)
			scroll[i] = -*shaolins_scroll-1;
		copyscrollbitmap(bitmap,tmpbitmap,0,0,32,scroll,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	for (offs = spriteram_size-32; offs >= 0; offs-=32 ) /* max 24 sprites */
	{
		if (spriteram[offs] && spriteram[offs+6]) /* stop rogue sprites on high score screen */
		{
			drawgfx(bitmap,Machine->gfx[1],
					spriteram[offs+8],
					(spriteram[offs+9] & 0x0f) + 16 * palettebank,
					!(spriteram[offs+9] & 0x40),(spriteram[offs+9] & 0x80),
					240-spriteram[offs+6],248-spriteram[offs+4],
					&Machine->visible_area,TRANSPARENCY_COLOR,0);
					/* transparency_color, otherwise sprites in test mode are not visible */
		}
	}
}
