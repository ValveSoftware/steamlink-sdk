#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *solomon_bgvideoram;
unsigned char *solomon_bgcolorram;

static struct osd_bitmap *tmpbitmap2;
static unsigned char *dirtybuffer2;
static int flipscreen;



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int solomon_vh_start(void)
{
	int i;


	if (generic_vh_start() != 0)
		return 1;

	if ((tmpbitmap2 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		generic_vh_stop();
		return 1;
	}

	if ((dirtybuffer2 = (unsigned char*)malloc(videoram_size)) == 0)
	{
		bitmap_free(tmpbitmap2);
		generic_vh_stop();
		return 1;
	}
	memset(dirtybuffer2,1,videoram_size);

	/* leave everything at the default, but map all foreground 0 pens as transparent */
	for (i = 0;i < 8;i++) palette_used_colors[16 * i] = PALETTE_COLOR_TRANSPARENT;

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void solomon_vh_stop(void)
{
	bitmap_free(tmpbitmap2);
	free(dirtybuffer2);
	generic_vh_stop();
}


WRITE_HANDLER( solomon_bgvideoram_w )
{
	if (solomon_bgvideoram[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		solomon_bgvideoram[offset] = data;
	}
}

WRITE_HANDLER( solomon_bgcolorram_w )
{
	if (solomon_bgcolorram[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		solomon_bgcolorram[offset] = data;
	}
}



WRITE_HANDLER( solomon_flipscreen_w )
{
	if (flipscreen != (data & 1))
	{
		flipscreen = data & 1;
		memset(dirtybuffer,1,videoram_size);
		memset(dirtybuffer2,1,videoram_size);
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void solomon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* recalc the palette if necessary */
	if (palette_recalc())
	{
		memset(dirtybuffer,1,videoram_size);
		memset(dirtybuffer2,1,videoram_size);
	}


	for (offs = 0;offs < videoram_size;offs++)
	{
		if (dirtybuffer2[offs])
		{
			int sx,sy,flipx,flipy;


			dirtybuffer2[offs] = 0;
			sx = offs % 32;
			sy = offs / 32;
			flipx = solomon_bgcolorram[offs] & 0x80;
			flipy = solomon_bgcolorram[offs] & 0x08;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(tmpbitmap2,Machine->gfx[1],
					solomon_bgvideoram[offs] + 256 * (solomon_bgcolorram[offs] & 0x07),
					((solomon_bgcolorram[offs] & 0x70) >> 4),
					flipx,flipy,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap2,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* draw the frontmost playfield */
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
					videoram[offs] + 256 * (colorram[offs] & 0x07),
					(colorram[offs] & 0x70) >> 4,
					flipscreen,flipscreen,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);


	/* draw sprites */
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int sx,sy,flipx,flipy;


		sx = spriteram[offs+3];
		sy = 241-spriteram[offs+2];
		flipx = spriteram[offs+1] & 0x40;
		flipy =	spriteram[offs+1] & 0x80;
		if (flipscreen & 1)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[2],
				spriteram[offs] + 16*(spriteram[offs+1] & 0x10),
				(spriteram[offs + 1] & 0x0e) >> 1,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
