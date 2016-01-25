/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *slapfight_videoram;
unsigned char *slapfight_colorram;
size_t slapfight_videoram_size;
unsigned char *slapfight_scrollx_lo,*slapfight_scrollx_hi,*slapfight_scrolly;


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Slapfight has three 256x4 palette PROMs (one per gun) all colours for all
  outputs are mapped to the palette directly.

  The palette PROMs are connected to the RGB output this way:

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void slapfight_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


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
}




/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void slapfight_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
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

			sx = offs % 64;
			sy = offs / 64;

			drawgfx(tmpbitmap,Machine->gfx[1],
					videoram[offs] + ((colorram[offs] & 0x0f) << 8),
					(colorram[offs] & 0xf0) >> 4,
					0,0,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scrollx,scrolly;


		scrollx = -(*slapfight_scrollx_lo + 256 * *slapfight_scrollx_hi);
		scrolly = -*slapfight_scrolly+1;
		copyscrollbitmap(bitmap,tmpbitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* Draw the sprites */
	for (offs = 0;offs < spriteram_size;offs += 4)
	{
		drawgfx(bitmap,Machine->gfx[2],
			spriteram[offs] + ((spriteram[offs+2] & 0xc0) << 2),
			(spriteram[offs+2] & 0x1e) >>1,
			0,0,
		/* Mysterious fudge factor sprite offset */
			(spriteram[offs+1] + ((spriteram[offs+2] & 0x01) << 8)) - 13,spriteram[offs+3],
			&Machine->visible_area,TRANSPARENCY_PEN,0);
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = slapfight_videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy;


		sx = offs % 64;
		sy = offs / 64;

		drawgfx(bitmap,Machine->gfx[0],
			slapfight_videoram[offs] + ((slapfight_colorram[offs] & 0x03) << 8),
			(slapfight_colorram[offs] & 0xfc) >> 2,
			0,0,
			8*sx,8*sy,
			&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
