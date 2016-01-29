/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



static int background_image;
static int flipscreen;



WRITE_HANDLER( bombjack_background_w )
{
	if (background_image != data)
	{
		memset(dirtybuffer,1,videoram_size);
		background_image = data;
	}
}



WRITE_HANDLER( bombjack_flipscreen_w )
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
void bombjack_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,base;


	if (palette_recalc())
		memset(dirtybuffer,1,videoram_size);

	base = 0x200 * (background_image & 0x07);

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy;
		int tilecode,tileattribute;


		sx = offs % 32;
		sy = offs / 32;

		if (background_image & 0x10)
		{
			int bgoffs;


			bgoffs = base+16*(sy/2)+sx/2;

			tilecode = memory_region(REGION_GFX4)[bgoffs],
			tileattribute = memory_region(REGION_GFX4)[bgoffs + 0x100];
		}
		else
		{
			tilecode = 0xff;
			tileattribute = 0;	/* avoid compiler warning */
		}

		if (dirtybuffer[offs])
		{
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
			}

			/* draw the background (this can be handled better) */
			if (tilecode != 0xff)
			{
				struct rectangle clip;
				int flipy;


				clip.min_x = 8*sx;
				clip.max_x = 8*sx+7;
				clip.min_y = 8*sy;
				clip.max_y = 8*sy+7;

				flipy = tileattribute & 0x80;
				if (flipscreen) flipy = !flipy;

				drawgfx(tmpbitmap,Machine->gfx[1],
						tilecode,
						tileattribute & 0x0f,
						flipscreen,flipy,
						16*(sx/2),16*(sy/2),
						&clip,TRANSPARENCY_NONE,0);

				drawgfx(tmpbitmap,Machine->gfx[0],
						videoram[offs] + 16 * (colorram[offs] & 0x10),
						colorram[offs] & 0x0f,
						flipscreen,flipscreen,
						8*sx,8*sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
			else
				drawgfx(tmpbitmap,Machine->gfx[0],
						videoram[offs] + 16 * (colorram[offs] & 0x10),
						colorram[offs] & 0x0f,
						flipscreen,flipscreen,
						8*sx,8*sy,
						&Machine->visible_area,TRANSPARENCY_NONE,0);


			dirtybuffer[offs] = 0;
		}
	}


	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);


	/* Draw the sprites. */
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{

/*
 abbbbbbb cdefgggg hhhhhhhh iiiiiiii

 a        use big sprites (32x32 instead of 16x16)
 bbbbbbb  sprite code
 c        x flip
 d        y flip (used only in death sequence?)
 e        ? (set when big sprites are selected)
 f        ? (set only when the bonus (B) materializes?)
 gggg     color
 hhhhhhhh x position
 iiiiiiii y position
*/
		int sx,sy,flipx,flipy;


		sx = spriteram[offs+3];
		if (spriteram[offs] & 0x80)
			sy = 225-spriteram[offs+2];
		else
			sy = 241-spriteram[offs+2];
		flipx = spriteram[offs+1] & 0x40;
		flipy =	spriteram[offs+1] & 0x80;
		if (flipscreen)
		{
			if (spriteram[offs+1] & 0x20)
			{
				sx = 224 - sx;
				sy = 224 - sy;
			}
			else
			{
				sx = 240 - sx;
				sy = 240 - sy;
			}
			flipx = !flipx;
			flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[(spriteram[offs] & 0x80) ? 3 : 2],
				spriteram[offs] & 0x7f,
				spriteram[offs+1] & 0x0f,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
