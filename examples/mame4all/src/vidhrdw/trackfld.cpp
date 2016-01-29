/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *trackfld_scroll;
unsigned char *trackfld_scroll2;
static int flipscreen;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Track 'n Field has one 32x8 palette PROM and two 256x4 lookup table PROMs
  (one for characters, one for sprites).
  The palette PROM is connected to the RGB output this way:

  bit 7 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

***************************************************************************/
void trackfld_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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


	/* sprites */
	for (i = 0;i < TOTAL_COLORS(1);i++)
		COLOR(1,i) = *(color_prom++) & 0x0f;

	/* characters */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = (*(color_prom++) & 0x0f) + 0x10;
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int trackfld_vh_start(void)
{
	if ((dirtybuffer = (unsigned char*)malloc(videoram_size)) == 0)
		return 1;
	memset(dirtybuffer,1,videoram_size);

	/* TracknField has a virtual screen twice as large as the visible screen */
	if ((tmpbitmap = bitmap_alloc(2 * Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void trackfld_vh_stop(void)
{
	free(dirtybuffer);
	bitmap_free(tmpbitmap);
}



WRITE_HANDLER( trackfld_flipscreen_w )
{
	if (flipscreen != (data & 1))
	{
		flipscreen = data & 1;
		memset(dirtybuffer,1,videoram_size);
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void trackfld_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,flipx,flipy;


			dirtybuffer[offs] = 0;

			sx = offs % 64;
			sy = offs / 64;
			flipx = colorram[offs] & 0x10;
			flipy = colorram[offs] & 0x20;
			if (flipscreen)
			{
				sx = 63 - sx;
				sy = 31 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + 4 * (colorram[offs] & 0xc0),
					colorram[offs] & 0x0f,
					flipx,flipy,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scroll[32];


		if (flipscreen)
		{
			for (offs = 0;offs < 32;offs++)
				scroll[31-offs] = 256 - (trackfld_scroll[offs] + 256 * (trackfld_scroll2[offs] & 1));
		}
		else
		{
			for (offs = 0;offs < 32;offs++)
				scroll[offs] = -(trackfld_scroll[offs] + 256 * (trackfld_scroll2[offs] & 1));
		}

		copyscrollbitmap(bitmap,tmpbitmap,32,scroll,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* Draw the sprites. */
	for (offs = spriteram_size - 2;offs >= 0;offs -= 2)
	{
		int sx,sy,flipx,flipy;


		sx = spriteram[offs] - 1;
		sy = 240 - spriteram_2[offs + 1];
		flipx = ~spriteram_2[offs] & 0x40;
		flipy = spriteram_2[offs] & 0x80;
		if (flipscreen)
		{
			sy = 240 - sy;
			flipy = !flipy;
		}

		/* Note that this adjustement must be done AFTER handling flipscreen, thus */
		/* proving that this is a hardware related "feature" */
		sy += 1;

		drawgfx(bitmap,Machine->gfx[1],
				spriteram[offs + 1],
				spriteram_2[offs] & 0x0f,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_COLOR,0);

		/* redraw with wraparound */
		drawgfx(bitmap,Machine->gfx[1],
				spriteram[offs + 1],
				spriteram_2[offs] & 0x0f,
				flipx,flipy,
				sx-256,sy,
				&Machine->visible_area,TRANSPARENCY_COLOR,0);
	}
}
