/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *jrpacman_scroll,*jrpacman_bgpriority;
unsigned char *jrpacman_charbank,*jrpacman_spritebank;
unsigned char *jrpacman_palettebank,*jrpacman_colortablebank;
static int flipscreen;


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Jr. Pac Man has two 256x4 palette PROMs (the three msb of the address are
  grounded, so the effective colors are only 32) and one 256x4 color lookup
  table PROM.
  The palette PROMs are connected to the RGB output this way:

  bit 3 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 220 ohm resistor  -- GREEN
  bit 0 -- 470 ohm resistor  -- GREEN

  bit 3 -- 1  kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

***************************************************************************/
void jrpacman_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


	for (i = 0;i < 32;i++)
	{
		int bit0,bit1,bit2;


		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		palette[3*i] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i+256] >> 0) & 0x01;
		bit2 = (color_prom[i+256] >> 1) & 0x01;
		palette[3*i + 1] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = 0;
		bit1 = (color_prom[i+256] >> 2) & 0x01;
		bit2 = (color_prom[i+256] >> 3) & 0x01;
		palette[3*i + 2] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
	}

	color_prom += 2*256;

	for (i = 0;i < 64*4;i++)
	{
		/* chars */
		colortable[i] = color_prom[i];

		/* sprites */
		if (color_prom[i]) colortable[i + 64*4] = color_prom[i] + 0x10;
		else colortable[i + 64*4] = 0;
	}
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int jrpacman_vh_start(void)
{
	if ((dirtybuffer = (unsigned char*)malloc(videoram_size)) == 0)
		return 1;
	memset(dirtybuffer,1,videoram_size);

	/* Jr. Pac Man has a virtual screen twice as large as the visible screen */
	if ((tmpbitmap = bitmap_alloc(Machine->drv->screen_width,2*Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void jrpacman_vh_stop(void)
{
	free(dirtybuffer);
	bitmap_free(tmpbitmap);
}



WRITE_HANDLER( jrpacman_videoram_w )
{
	if (videoram[offset] != data)
	{
		dirtybuffer[offset] = 1;

		videoram[offset] = data;

		if (offset < 32)	/* line color - mark whole line as dirty */
		{
			int i;


			for (i = 2*32;i < 56*32;i += 32)
				dirtybuffer[i + offset] = 1;
		}
		else if (offset > 1792)	/* colors for top and bottom two rows */
		{
			dirtybuffer[offset & ~0x80] = 1;
		}
	}
}



WRITE_HANDLER( jrpacman_palettebank_w )
{
	if (*jrpacman_palettebank != data)
	{
		*jrpacman_palettebank = data;
		memset(dirtybuffer,1,videoram_size);
	}
}



WRITE_HANDLER( jrpacman_colortablebank_w )
{
	if (*jrpacman_colortablebank != data)
	{
		*jrpacman_colortablebank = data;
		memset(dirtybuffer,1,videoram_size);
	}
}



WRITE_HANDLER( jrpacman_charbank_w )
{
	if (*jrpacman_charbank != data)
	{
		*jrpacman_charbank = data;
		memset(dirtybuffer,1,videoram_size);
	}
}


WRITE_HANDLER( jrpacman_flipscreen_w )
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
void jrpacman_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i,offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int mx,my;


			dirtybuffer[offs] = 0;

			/* Jr. Pac Man's screen layout is quite awkward */
			mx = offs % 32;
			my = offs / 32;

			if (my >= 2 && my < 60)
			{
				int sx, sy;

				if (my < 56)
				{
					sy = my;
					sx = mx+2;
					if (flipscreen)
					{
						sx = 35 - sx;
						sy = 55 - sy;
					}

					drawgfx(tmpbitmap,Machine->gfx[0],
							videoram[offs] + 256 * *jrpacman_charbank,
						/* color is set line by line */
							(videoram[mx] & 0x1f) + 0x20 * (*jrpacman_colortablebank & 1)
									+ 0x40 * (*jrpacman_palettebank & 1),
							flipscreen,flipscreen,
							8*sx,8*sy,
							0,TRANSPARENCY_NONE,0);
				}
				else
				{
					if (my >= 58)
					{
						sy = mx - 2;
						sx = my - 58;
						if (flipscreen)
						{
							sx = 35 - sx;
							sy = 55 - sy;
						}

						drawgfx(tmpbitmap,Machine->gfx[0],
								videoram[offs],
								(videoram[offs + 4*32] & 0x1f) + 0x20 * (*jrpacman_colortablebank & 1)
										+ 0x40 * (*jrpacman_palettebank & 1),
								flipscreen,flipscreen,
								8*sx,8*sy,
								0,TRANSPARENCY_NONE,0);
					}
					else
					{
						sy = mx - 2;
						sx = my - 22;
						if (flipscreen)
						{
							sx = 35 - sx;
							sy = 55 - sy;
						}

						drawgfx(tmpbitmap,Machine->gfx[0],
								videoram[offs] + 0x100 * (*jrpacman_charbank & 1),
								(videoram[offs + 4*32] & 0x1f) + 0x20 * (*jrpacman_colortablebank & 1)
										+ 0x40 * (*jrpacman_palettebank & 1),
								flipscreen,flipscreen,
								8*sx,8*sy,
								0,TRANSPARENCY_NONE,0);
					}
				}
			}
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scrolly[36];


		for (i = 0;i < 2;i++)
			scrolly[i] = 0;
		for (i = 2;i < 34;i++)
			scrolly[i] = -*jrpacman_scroll - 16;
		for (i = 34;i < 36;i++)
			scrolly[i] = 0;

		if (flipscreen)
		{
			for (i = 0;i < 36;i++)
				scrolly[i] = 224 - scrolly[i];
		}

		copyscrollbitmap(bitmap,tmpbitmap,0,0,36,scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = spriteram_size - 2;offs > 2*2;offs -= 2)
	{
		drawgfx(bitmap,Machine->gfx[1],
				(spriteram[offs] >> 2) + 0x40 * (*jrpacman_spritebank & 1),
				(spriteram[offs + 1] & 0x1f) + 0x20 * (*jrpacman_colortablebank & 1)
						+ 0x40 * (*jrpacman_palettebank & 1),
				spriteram[offs] & 1,spriteram[offs] & 2,
				272 - spriteram_2[offs + 1],spriteram_2[offs]-31,
				&Machine->visible_area,
				(*jrpacman_bgpriority & 1) ? TRANSPARENCY_THROUGH : TRANSPARENCY_COLOR,
				(*jrpacman_bgpriority & 1) ? Machine->pens[0]     : 0);
	}
	/* the first two sprites must be offset one pixel to the left */
	for (offs = 2*2;offs > 0;offs -= 2)
	{
		drawgfx(bitmap,Machine->gfx[1],
				(spriteram[offs] >> 2) + 0x40 * (*jrpacman_spritebank & 1),
				(spriteram[offs + 1] & 0x1f) + 0x20 * (*jrpacman_colortablebank & 1)
						+ 0x40 * (*jrpacman_palettebank & 1),
				spriteram[offs] & 1,spriteram[offs] & 2,
				272 - spriteram_2[offs + 1],spriteram_2[offs]-30,
				&Machine->visible_area,
				(*jrpacman_bgpriority & 1) ? TRANSPARENCY_THROUGH : TRANSPARENCY_COLOR,
				(*jrpacman_bgpriority & 1) ? Machine->pens[0]     : 0);
	}
}
