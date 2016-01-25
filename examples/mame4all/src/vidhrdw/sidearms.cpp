/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

#include "osdepend.h"

unsigned char *sidearms_bg_scrollx,*sidearms_bg_scrolly;
unsigned char *sidearms_bg2_scrollx,*sidearms_bg2_scrolly;
static struct osd_bitmap *tmpbitmap2;
static int flipscreen;
static int bgon,objon;


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int sidearms_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	/* create a temporary bitmap slightly larger than the screen for the background */
	if ((tmpbitmap2 = bitmap_alloc(48*8 + 32,Machine->drv->screen_height + 32)) == 0)
	{
		generic_vh_stop();
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void sidearms_vh_stop(void)
{
	bitmap_free(tmpbitmap2);
	generic_vh_stop();
}



WRITE_HANDLER( sidearms_c804_w )
{
	/* bits 0 and 1 are coin counters */
	coin_counter_w(0,data & 0x01);
	coin_counter_w(1,data & 0x02);

	/* bit 4 probably resets the sound CPU */

	/* TODO: I don't know about the other bits (all used) */

	/* bit 7 flips screen */
	if (flipscreen != (data & 0x80))
	{
		flipscreen = data & 0x80;
/* TODO: support screen flip */
//		memset(dirtybuffer,1,c1942_backgroundram_size);
	}
}

WRITE_HANDLER( sidearms_gfxctrl_w )
{
	objon = data & 0x01;
	bgon = data & 0x02;
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void sidearms_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs, sx, sy;
	int scrollx,scrolly;
	static int lastoffs;
	int dirtypalette = 0;


palette_init_used_colors();


{
	int color,code,i;
	int colmask[64];
	int pal_base;
	unsigned char *p=memory_region(REGION_GFX4);


	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;

	for (color = 0;color < 32;color++) colmask[color] = 0;

	scrollx = sidearms_bg_scrollx[0] + 256 * sidearms_bg_scrollx[1] + 64;
	scrolly = sidearms_bg_scrolly[0] + 256 * sidearms_bg_scrolly[1];
	offs = 2 * (scrollx >> 5) + 0x100 * (scrolly >> 5);
	scrollx = -(scrollx & 0x1f);
	scrolly = -(scrolly & 0x1f);

	for (sy = 0;sy < 9;sy++)
	{
		for (sx = 0; sx < 13; sx++)
		{
			int offset;


			offset = offs + 2 * sx;

			/* swap bits 1-7 and 8-10 of the address to compensate for the */
			/* funny layout of the ROM data */
			offset = (offset & 0xf801) | ((offset & 0x0700) >> 7) | ((offset & 0x00fe) << 3);

			code = p[offset] + 256 * (p[offset+1] & 0x01);
			color = (p[offset+1] & 0xf8) >> 3;
			colmask[color] |= Machine->gfx[1]->pen_usage[code];
		}
		offs += 0x100;
	}

	for (color = 0;color < 32;color++)
	{
		if (colmask[color] & (1 << 15))
			palette_used_colors[pal_base + 16 * color + 15] = PALETTE_COLOR_TRANSPARENT;
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}


	pal_base = Machine->drv->gfxdecodeinfo[2].color_codes_start;

	for (color = 0;color < 16;color++) colmask[color] = 0;

	for (offs = spriteram_size - 32;offs >= 0;offs -= 32)
	{
		code = spriteram[offs] + 8 * (spriteram[offs + 1] & 0xe0);
		color =	spriteram[offs + 1] & 0x0f;
		colmask[color] |= Machine->gfx[2]->pen_usage[code];
	}

	for (color = 0;color < 16;color++)
	{
		if (colmask[color] & (1 << 15))
			palette_used_colors[pal_base + 16 * color + 15] = PALETTE_COLOR_TRANSPARENT;
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}


	pal_base = Machine->drv->gfxdecodeinfo[0].color_codes_start;

	for (color = 0;color < 64;color++) colmask[color] = 0;

	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		code = videoram[offs] + 4 * (colorram[offs] & 0xc0);
		color = colorram[offs] & 0x3f;
		colmask[color] |= Machine->gfx[0]->pen_usage[code];
	}

	for (color = 0;color < 64;color++)
	{
		if (colmask[color] & (1 << 3))
			palette_used_colors[pal_base + 4 * color + 3] = PALETTE_COLOR_TRANSPARENT;
		for (i = 0;i < 3;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 4 * color + i] = PALETTE_COLOR_USED;
		}
	}
}

if (palette_recalc())
	dirtypalette = 1;


	/* There is a scrolling blinking star background behind the tile */
	/* background, but I have absolutely NO IDEA how to render it. */
	/* The scroll registers have a 64 pixels resolution. */
#if IHAVETHEBACKGROUND
	{
		int x,y;
		for (x = 0;x < 48;x+=8)
		{
			for (y = 0;y < 32;y+=8)
			{
				drawgfx(tmpbitmap,Machine->gfx[0],
						(y%8)*48+(x%8),
						0,
						0,0,
						8*x,8*y,
						0,TRANSPARENCY_NONE,0);
			}
		}
	}

	/* copy the temporary bitmap to the screen */
	scrollx = -(*sidearms_bg2_scrollx & 0x3f);
	scrolly = -(*sidearms_bg2_scrolly & 0x3f);

	copyscrollbitmap(bitmap,tmpbitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
#endif


	if (bgon)
	{
		scrollx = sidearms_bg_scrollx[0] + 256 * sidearms_bg_scrollx[1] + 64;
		scrolly = sidearms_bg_scrolly[0] + 256 * sidearms_bg_scrolly[1];
		offs = 2 * (scrollx >> 5) + 0x100 * (scrolly >> 5);
		scrollx = -(scrollx & 0x1f);
		scrolly = -(scrolly & 0x1f);

		if (offs != lastoffs || dirtypalette)
		{
			unsigned char *p=memory_region(REGION_GFX4);


			lastoffs = offs;

			/* Draw the entire background scroll */
			for (sy = 0;sy < 9;sy++)
			{
				for (sx = 0; sx < 13; sx++)
				{
					int offset;


					offset = offs + 2 * sx;

					/* swap bits 1-7 and 8-10 of the address to compensate for the */
					/* funny layout of the ROM data */
					offset = (offset & 0xf801) | ((offset & 0x0700) >> 7) | ((offset & 0x00fe) << 3);

					drawgfx(tmpbitmap2,Machine->gfx[1],
							p[offset] + 256 * (p[offset+1] & 0x01),
							(p[offset+1] & 0xf8) >> 3,
							p[offset+1] & 0x02,p[offset+1] & 0x04,
							32*sx,32*sy,
							0,TRANSPARENCY_NONE,0);
				}
				offs += 0x100;
			}
		}

	scrollx += 64;
#if IHAVETHEBACKGROUND
	copyscrollbitmap(bitmap,tmpbitmap2,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_COLOR,1);
#else
	copyscrollbitmap(bitmap,tmpbitmap2,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
#endif
	}
	else fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);

	/* Draw the sprites. */
	if (objon)
	{
		for (offs = spriteram_size - 32;offs >= 0;offs -= 32)
		{
			sx = spriteram[offs + 3] + ((spriteram[offs + 1] & 0x10) << 4);
			sy = spriteram[offs + 2];
			if (flipscreen)
			{
				sx = 496 - sx;
				sy = 240 - sy;
			}

			drawgfx(bitmap,Machine->gfx[2],
					spriteram[offs] + 8 * (spriteram[offs + 1] & 0xe0),
					spriteram[offs + 1] & 0x0f,
					flipscreen,flipscreen,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,15);
		}
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		sx = offs % 64;
		sy = offs / 64;

		if (flipscreen)
		{
			sx = 63 - sx;
			sy = 31 - sy;
		}

		drawgfx(bitmap,Machine->gfx[0],
				videoram[offs] + 4 * (colorram[offs] & 0xc0),
				colorram[offs] & 0x3f,
				flipscreen,flipscreen,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_PEN,3);
	}
}
