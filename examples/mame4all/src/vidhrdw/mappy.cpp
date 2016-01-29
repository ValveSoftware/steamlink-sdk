/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char mappy_scroll;

static int special_display;
static int flipscreen;

/***************************************************************************

  Convert the color PROMs into a more useable format.

  mappy has one 32x8 palette PROM and two 256x4 color lookup table PROMs
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
void mappy_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;

	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2;

		bit0 = (*color_prom >> 0) & 0x01;
		bit1 = (*color_prom >> 1) & 0x01;
		bit2 = (*color_prom >> 2) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = (*color_prom >> 3) & 0x01;
		bit1 = (*color_prom >> 4) & 0x01;
		bit2 = (*color_prom >> 5) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = 0;
		bit1 = (*color_prom >> 6) & 0x01;
		bit2 = (*color_prom >> 7) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		color_prom++;
	}

	/* characters */
	for (i = 0*4;i < 64*4;i++)
		colortable[i] = (color_prom[(i^3)] & 0x0f) + 0x10;

	/* sprites */
	for (i = 64*4;i < Machine->drv->color_table_len;i++)
		colortable[i] = color_prom[i] & 0x0f;
}


static int common_vh_start(void)
{
	if ((dirtybuffer = (unsigned char*)malloc(videoram_size)) == 0)
		return 1;
	memset (dirtybuffer, 1, videoram_size);

	if ((tmpbitmap = bitmap_alloc (36*8,60*8)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}

	return 0;
}

int mappy_vh_start(void)
{
	special_display = 0;
	return common_vh_start();
}

int motos_vh_start(void)
{
	special_display = 1;
	return common_vh_start();
}

int todruaga_vh_start(void)
{
	special_display = 2;
	return common_vh_start();
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void mappy_vh_stop(void)
{
	free(dirtybuffer);
	bitmap_free(tmpbitmap);
}



WRITE_HANDLER( mappy_videoram_w )
{
	if (videoram[offset] != data)
	{
		dirtybuffer[offset] = 1;
		videoram[offset] = data;
	}
}


WRITE_HANDLER( mappy_colorram_w )
{
	if (colorram[offset] != data)
	{
		dirtybuffer[offset] = 1;
		colorram[offset] = data;
	}
}


WRITE_HANDLER( mappy_scroll_w )
{
	mappy_scroll = offset >> 3;
}



void mappy_draw_sprite(struct osd_bitmap *dest,unsigned int code,unsigned int color,
	int flipx,int flipy,int sx,int sy)
{
	if (special_display == 1) sy++;	/* Motos */

	drawgfx(dest,Machine->gfx[1],code,color,flipx,flipy,sx,sy,&Machine->visible_area,
		TRANSPARENCY_COLOR,15);
}

WRITE_HANDLER( mappy_flipscreen_w )
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
void mappy_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,mx,my;

			dirtybuffer[offs] = 0;

			if (offs >= videoram_size - 64)
			{
				int off = offs;

				if (special_display == 1)
				{
					/* Motos */
					if (off == 0x07d1 || off == 0x07d0 || off == 0x07f1 || off == 0x07f0)
						off -= 0x10;
					if (off == 0x07c1 || off == 0x07c0 || off == 0x07e1 || off == 0x07e0)
						off += 0x10;
				}

				/* Draw the top 2 lines. */
				mx = (off - (videoram_size - 64)) / 32;
				my = off % 32;

				sx = mx;
				sy = my - 2;
			}
			else if (offs >= videoram_size - 128)
			{
				int off = offs;

				if (special_display == 2)
				{
					/* Tower of Druaga */
					if (off == 0x0791 || off == 0x0790 || off == 0x07b1 || off == 0x07b0)
						off -= 0x10;
					if (off == 0x0781 || off == 0x0780 || off == 0x07a1 || off == 0x07a0)
						off += 0x10;
				}

				/* Draw the bottom 2 lines. */
				mx = (off - (videoram_size - 128)) / 32;
				my = off % 32;

				sx = mx + 34;
				sy = my - 2;
			}
			else
			{
				/* draw the rest of the screen */
				mx = offs % 32;
				my = offs / 32;

				sx = mx + 2;
				sy = my;
			}

			if (flipscreen)
			{
				sx = 35 - sx;
				sy = 59 - sy;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs],
					colorram[offs] & 0x3f,
					flipscreen,flipscreen,8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the temporary bitmap to the screen */
	{
		int scroll[36];

		for (offs = 0;offs < 2;offs++)
			scroll[offs] = 0;
		for (offs = 2;offs < 34;offs++)
			scroll[offs] = -mappy_scroll;
		for (offs = 34;offs < 36;offs++)
			scroll[offs] = 0;

		if (flipscreen)
		{
			for (offs = 0;offs < 36;offs++)
				scroll[offs] = 224 - scroll[offs];
		}

		copyscrollbitmap(bitmap,tmpbitmap,0,0,36,scroll,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}

	/* Draw the sprites. */
	for (offs = 0;offs < spriteram_size;offs += 2)
	{
		/* is it on? */
		if ((spriteram_3[offs+1] & 2) == 0)
		{
			int sprite = spriteram[offs];
			int color = spriteram[offs+1];
			int x = (spriteram_2[offs+1]-40) + 0x100*(spriteram_3[offs+1] & 1);
			int y = 28*8-spriteram_2[offs];
			int flipx = spriteram_3[offs] & 1;
			int flipy = spriteram_3[offs] & 2;

			if (flipscreen)
			{
				flipx = !flipx;
				flipy = !flipy;
			}

			switch (spriteram_3[offs] & 0x0c)
			{
				case 0:		/* normal size */
					mappy_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y);
					break;

				case 4:		/* 2x horizontal */
					sprite &= ~1;
					if (!flipx)
					{
						mappy_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y);
						mappy_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x+16,y);
					}
					else
					{
						mappy_draw_sprite(bitmap,sprite,color,flipx,flipy,x+16,y);
						mappy_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x,y);
					}
					break;

				case 8:		/* 2x vertical */
					sprite &= ~2;
					if (!flipy)
					{
						mappy_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x,y);
						mappy_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y-16);
					}
					else
					{
						mappy_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y);
						mappy_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x,y-16);
					}
					break;

				case 12:		/* 2x both ways */
					sprite &= ~3;
					if (!flipx && !flipy)
					{
						mappy_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x,y);
						mappy_draw_sprite(bitmap,3+sprite,color,flipx,flipy,x+16,y);
						mappy_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y-16);
						mappy_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x+16,y-16);
					}
					else if (flipx && flipy)
					{
						mappy_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x,y);
						mappy_draw_sprite(bitmap,sprite,color,flipx,flipy,x+16,y);
						mappy_draw_sprite(bitmap,3+sprite,color,flipx,flipy,x,y-16);
						mappy_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x+16,y-16);
					}
					else if (flipy)
					{
						mappy_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y);
						mappy_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x+16,y);
						mappy_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x,y-16);
						mappy_draw_sprite(bitmap,3+sprite,color,flipx,flipy,x+16,y-16);
					}
					else /* flipx */
					{
						mappy_draw_sprite(bitmap,3+sprite,color,flipx,flipy,x,y);
						mappy_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x+16,y);
						mappy_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x,y-16);
						mappy_draw_sprite(bitmap,sprite,color,flipx,flipy,x+16,y-16);
					}
					break;
			}
		}
	}

	/* Draw the high priority characters */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (colorram[offs] & 0x40)
		{
			int sx,sy,mx,my;

				if (offs >= videoram_size - 64)
				{
					/* Draw the top 2 lines. */
					mx = (offs - (videoram_size - 64)) / 32;
					my = offs % 32;

					sx = mx;
					sy = my - 2;

					sy *= 8;
				}
				else if (offs >= videoram_size - 128)
				{
					/* Draw the bottom 2 lines. */
					mx = (offs - (videoram_size - 128)) / 32;
					my = offs % 32;

					sx = mx + 34;
					sy = my - 2;

					sy *= 8;
				}
				else
				{
					/* draw the rest of the screen */
					mx = offs % 32;
					my = offs / 32;

					sx = mx + 2;
					sy = my;

					sy = (8*sy-mappy_scroll);
				}

				if (flipscreen)
				{
					sx = 35 - sx;
					sy = 216 - sy;
				}

				drawgfx(bitmap,Machine->gfx[0],
						videoram[offs],
						colorram[offs] & 0x3f,
						flipscreen,flipscreen,8*sx,sy,
						0,TRANSPARENCY_COLOR,31);
		}
	}
}
