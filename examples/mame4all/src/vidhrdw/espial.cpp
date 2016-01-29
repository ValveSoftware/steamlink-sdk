/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *espial_attributeram;
unsigned char *espial_column_scroll;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Espial has two 256x4 palette PROMs.

  I don't know for sure how the palette PROMs are connected to the RGB
  output, but it's probably the usual:

  bit 3 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 220 ohm resistor  -- GREEN
  bit 0 -- 470 ohm resistor  -- GREEN
  bit 3 -- 1  kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

***************************************************************************/
void espial_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2;


		/* red component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (color_prom[0] >> 3) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 0) & 0x01;
		bit2 = (color_prom[Machine->drv->total_colors] >> 1) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[Machine->drv->total_colors] >> 2) & 0x01;
		bit2 = (color_prom[Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		color_prom++;
	}
}



WRITE_HANDLER( espial_attributeram_w )
{
	if (espial_attributeram[offset] != data)
	{
		espial_attributeram[offset] = data;
		memset(dirtybuffer,1,videoram_size);
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void espial_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


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

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + 256*(espial_attributeram[offs] & 0x03),
					colorram[offs],
					espial_attributeram[offs] & 0x04,espial_attributeram[offs] & 0x08,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scroll[32];


		for (offs = 0;offs < 32;offs++)
			scroll[offs] = -espial_column_scroll[offs];

		copyscrollbitmap(bitmap,tmpbitmap,0,0,32,scroll,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = 0;offs < spriteram_size/2;offs++)
	{
		int sx,sy,code,color,flipx,flipy;


		sx = spriteram[offs + 16];
		sy = 240 - spriteram_2[offs];
		code = spriteram[offs] >> 1;
		color = spriteram_2[offs + 16];
		flipx = spriteram_3[offs] & 0x04;
		flipy = spriteram_3[offs] & 0x08;

		if (spriteram[offs] & 1)	/* double height */
		{
			drawgfx(bitmap,Machine->gfx[1],
					code,
					color,
					flipx,flipy,
					sx,sy - 16,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
			drawgfx(bitmap,Machine->gfx[1],
					code + 1,
					color,
					flipx,flipy,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
		else
			drawgfx(bitmap,Machine->gfx[1],
					code,
					color,
					flipx,flipy,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
