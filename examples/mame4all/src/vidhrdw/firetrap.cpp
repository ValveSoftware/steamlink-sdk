/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *firetrap_bg1videoram,*firetrap_bg2videoram;
unsigned char *firetrap_videoram,*firetrap_colorram;
unsigned char *firetrap_scroll1x,*firetrap_scroll1y;
unsigned char *firetrap_scroll2x,*firetrap_scroll2y;
size_t firetrap_bgvideoram_size;
size_t firetrap_videoram_size;
static unsigned char *dirtybuffer2;
static struct osd_bitmap *tmpbitmap2;
static int flipscreen;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Fire Trap has one 256x8 and one 256x4 palette PROMs.
  I don't know for sure how the palette PROMs are connected to the RGB
  output, but it's probably the usual:

  bit 7 -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 2.2kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
        -- 1  kohm resistor  -- RED
  bit 0 -- 2.2kohm resistor  -- RED

  bit 3 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 1  kohm resistor  -- BLUE
  bit 0 -- 2.2kohm resistor  -- BLUE

***************************************************************************/
void firetrap_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < 256;i++)
	{
		int bit0,bit1,bit2,bit3;


		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[0] >> 4) & 0x01;
		bit1 = (color_prom[0] >> 5) & 0x01;
		bit2 = (color_prom[0] >> 6) & 0x01;
		bit3 = (color_prom[0] >> 7) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[256] >> 0) & 0x01;
		bit1 = (color_prom[256] >> 1) & 0x01;
		bit2 = (color_prom[256] >> 2) & 0x01;
		bit3 = (color_prom[256] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}

	/* reserve the last color for the transparent pen (none of the game colors can have */
	/* these RGB components) */
	*(palette++) = 1;
	*(palette++) = 1;
	*(palette++) = 1;


	/* characters use colors 0-63 */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = i;

	/* background #1 tiles use colors 128-191 */
	for (i = 0;i < TOTAL_COLORS(1);i++)
	{
		if (i % Machine->gfx[1]->color_granularity == 0)
			COLOR(1,i) = 256;	/* transparent */
		else COLOR(1,i) = i + 128;
	}

	/* background #2 tiles use colors 192-255 */
	for (i = 0;i < TOTAL_COLORS(5);i++)
		COLOR(5,i) = i + 192;

	/* sprites use colors 64-127 */
	for (i = 0;i < TOTAL_COLORS(9);i++)
		COLOR(9,i) = i + 64;
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int firetrap_vh_start(void)
{
	if ((dirtybuffer = (unsigned char*)malloc(firetrap_bgvideoram_size)) == 0)
	{
		return 1;
	}
	memset(dirtybuffer,1,firetrap_bgvideoram_size);

	/* the background area is twice as wide and twice as tall as the screen */
	if ((tmpbitmap = bitmap_alloc(2*Machine->drv->screen_width,2*Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}

	if ((dirtybuffer2 = (unsigned char*)malloc(firetrap_bgvideoram_size)) == 0)
	{
		bitmap_free(tmpbitmap);
		free(dirtybuffer);
		return 1;
	}
	memset(dirtybuffer2,1,firetrap_bgvideoram_size);

	/* the background area is twice as wide and twice as tall as the screen */
	if ((tmpbitmap2 = bitmap_alloc(2*Machine->drv->screen_width,2*Machine->drv->screen_height)) == 0)
	{
		bitmap_free(tmpbitmap);
		free(dirtybuffer2);
		free(dirtybuffer);
		generic_vh_stop();
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void firetrap_vh_stop(void)
{
	bitmap_free(tmpbitmap);
	bitmap_free(tmpbitmap2);
	free(dirtybuffer);
	free(dirtybuffer2);
}



WRITE_HANDLER( firetrap_bg1videoram_w )
{
	if (firetrap_bg1videoram[offset] != data)
	{
		dirtybuffer[offset] = 1;

		firetrap_bg1videoram[offset] = data;
	}
}

WRITE_HANDLER( firetrap_bg2videoram_w )
{
	if (firetrap_bg2videoram[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		firetrap_bg2videoram[offset] = data;
	}
}



WRITE_HANDLER( firetrap_flipscreen_w )
{
	if (flipscreen != (data & 1))
	{
		flipscreen = data & 1;
		memset(dirtybuffer,1,firetrap_bgvideoram_size);
		memset(dirtybuffer2,1,firetrap_bgvideoram_size);
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void firetrap_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = firetrap_bgvideoram_size - 1;offs >= 0;offs--)
	{
		if ((offs & 0x100) == 0)
		{
			if (dirtybuffer[offs] || dirtybuffer[offs+0x100])
			{
				int sx,sy,flipx,flipy;


				dirtybuffer[offs] = dirtybuffer[offs+0x100] = 0;

				sx = (offs / 16) & 0x0f;
				sy = 31 - offs % 16;
				if (offs & 0x200) sy -= 16;
				if (offs & 0x400) sx += 16;
				flipx = firetrap_bg1videoram[offs+0x100] & 0x08;
				flipy = firetrap_bg1videoram[offs+0x100] & 0x04;
				if (flipscreen)
				{
					sx = 31 - sx;
					sy = 31 - sy;
					flipx = !flipx;
					flipy = !flipy;
				}

				drawgfx(tmpbitmap,Machine->gfx[1 + (firetrap_bg1videoram[offs+0x100] & 0x03)],
						firetrap_bg1videoram[offs],
						(firetrap_bg1videoram[offs+0x100] & 0x30) >> 4,
						flipx,flipy,
						16*sx,16*sy,
						0,TRANSPARENCY_NONE,0);
			}

			if (dirtybuffer2[offs] || dirtybuffer2[offs+0x100])
			{
				int sx,sy,flipx,flipy;


				dirtybuffer2[offs] = dirtybuffer2[offs+0x100] = 0;

				sx = (offs / 16) & 0x0f;
				sy = 31 - offs % 16;
				if (offs & 0x200) sy -= 16;
				if (offs & 0x400) sx += 16;
				flipx = firetrap_bg2videoram[offs+0x100] & 0x08;
				flipy = firetrap_bg2videoram[offs+0x100] & 0x04;
				if (flipscreen)
				{
					sx = 31 - sx;
					sy = 31 - sy;
					flipx = !flipx;
					flipy = !flipy;
				}

				drawgfx(tmpbitmap2,Machine->gfx[5 + (firetrap_bg2videoram[offs+0x100] & 0x03)],
						firetrap_bg2videoram[offs],
						(firetrap_bg2videoram[offs+0x100] & 0x30) >> 4,
						flipx,flipy,
						16*sx,16*sy,
						0,TRANSPARENCY_NONE,0);
			}
		}
	}


	/* copy the background graphics */
	{
		int scrollx,scrolly;


		if (flipscreen)
		{
			scrolly = -(firetrap_scroll2x[0] + 256 * firetrap_scroll2x[1]);
			scrollx = 256 + (firetrap_scroll2y[0] + 256 * firetrap_scroll2y[1]);
		}
		else
		{
			scrolly = 256 + (firetrap_scroll2x[0] + 256 * firetrap_scroll2x[1]);
			scrollx = -(firetrap_scroll2y[0] + 256 * firetrap_scroll2y[1]);
		}
		copyscrollbitmap(bitmap,tmpbitmap2,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);

		if (flipscreen)
		{
			scrolly = -(firetrap_scroll1x[0] + 256 * firetrap_scroll1x[1]);
			scrollx = 256 + (firetrap_scroll1y[0] + 256 * firetrap_scroll1y[1]);
		}
		else
		{
			scrolly = 256 + (firetrap_scroll1x[0] + 256 * firetrap_scroll1x[1]);
			scrollx = -(firetrap_scroll1y[0] + 256 * firetrap_scroll1y[1]);
		}
		copyscrollbitmap(bitmap,tmpbitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_COLOR,256);
	}


	/* Draw the sprites. */
	for (offs = 0;offs < spriteram_size; offs += 4)
	{
//		if (spriteram[offs] & 0x20)
		{
			int sx,sy,flipx,flipy,code,color;


			/* the meaning of bit 3 of [offs] is unknown */

			sy = spriteram[offs];
			sx = spriteram[offs + 2];
			code = spriteram[offs + 3] + 4 * (spriteram[offs + 1] & 0xc0);
			color = ((spriteram[offs + 1] & 0x08) >> 2) | (spriteram[offs + 1] & 0x01);
			flipx = spriteram[offs + 1] & 0x04;
			flipy = spriteram[offs + 1] & 0x02;
			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			if (spriteram[offs + 1] & 0x10)	/* double width */
			{
				if (flipscreen) sy -= 16;

				drawgfx(bitmap,Machine->gfx[9],
						code & ~1,
						color,
						flipx,flipy,
						sx,flipy ? sy : sy + 16,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
				drawgfx(bitmap,Machine->gfx[9],
						code | 1,
						color,
						flipx,flipy,
						sx,flipy ? sy + 16 : sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);

				/* redraw with wraparound */
				drawgfx(bitmap,Machine->gfx[9],
						code & ~1,
						color,
						flipx,flipy,
						sx - 256,flipy ? sy : sy + 16,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
				drawgfx(bitmap,Machine->gfx[9],
						code | 1,
						color,
						flipx,flipy,
						sx - 256,flipy ? sy + 16 : sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
			else
			{
				drawgfx(bitmap,Machine->gfx[9],
						code,
						color,
						flipx,flipy,
						sx,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);

				/* redraw with wraparound */
				drawgfx(bitmap,Machine->gfx[9],
						code,
						color,
						flipx,flipy,
						sx - 256,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
		}
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = firetrap_videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy;


		sx = offs / 32;
		sy = 31 - offs % 32;
		if (flipscreen)
		{
			sx = 31 - sx;
			sy = 31 - sy;
		}

		drawgfx(bitmap,Machine->gfx[0],
				firetrap_videoram[offs] + 256 * (firetrap_colorram[offs] & 0x01),
				(firetrap_colorram[offs] & 0xf0) >> 4,
				flipscreen,flipscreen,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
