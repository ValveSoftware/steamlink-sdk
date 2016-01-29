/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *bankp_videoram2;
unsigned char *bankp_colorram2;
static unsigned char *dirtybuffer2;
static struct osd_bitmap *tmpbitmap2;
static int scroll_x;
static int flipscreen;
static int priority;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Bank Panic has a 32x8 palette PROM (I'm not sure whether the second 16
  bytes are used - they contain the same colors as the first 16 with only
  one different) and two 256x4 lookup table PROMs (one for charset #1, one
  for charset #2 - only the first 128 nibbles seem to be used).

  I don't know for sure how the palette PROM is connected to the RGB output,
  but it's probably the usual:

  bit 7 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

***************************************************************************/
void bankp_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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

	/* charset #1 lookup table */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = *(color_prom++) & 0x0f;

	color_prom += 128;	/* skip the bottom half of the PROM - seems to be not used */

	/* charset #2 lookup table */
	for (i = 0;i < TOTAL_COLORS(1);i++)
		COLOR(1,i) = *(color_prom++) & 0x0f;

	/* the bottom half of the PROM seems to be not used */
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int bankp_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((dirtybuffer2 = (unsigned char*)malloc(videoram_size)) == 0)
	{
		generic_vh_stop();
		return 1;
	}
	memset(dirtybuffer2,1,videoram_size);

	if ((tmpbitmap2 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer2);
		generic_vh_stop();
		return 1;
	}

	return 0;
}



WRITE_HANDLER( bankp_scroll_w )
{
	scroll_x = data;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void bankp_vh_stop(void)
{
	free(dirtybuffer2);
	bitmap_free(tmpbitmap2);
	generic_vh_stop();
}



WRITE_HANDLER( bankp_videoram2_w )
{
	if (bankp_videoram2[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		bankp_videoram2[offset] = data;
	}
}



WRITE_HANDLER( bankp_colorram2_w )
{
	if (bankp_colorram2[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		bankp_colorram2[offset] = data;
	}
}



WRITE_HANDLER( bankp_out_w )
{
	/* bits 0-1 are playfield priority */
	/* TODO: understand how this works, currently the only thing I do is */
	/* invert the layer order when priority == 2 */
	priority = data & 0x03;

	/* bits 2-3 unknown (2 is used) */

	/* bit 4 controls NMI */
	if (data & 0x10) interrupt_enable_w(0,1);
	else interrupt_enable_w(0,0);

	/* bit 5 controls screen flip */
	if ((data & 0x20) != flipscreen)
	{
		flipscreen = data & 0x20;
		memset(dirtybuffer,1,videoram_size);
		memset(dirtybuffer2,1,videoram_size);
	}

	/* bits 6-7 unknown */
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void bankp_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,flipx;


			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			flipx = colorram[offs] & 0x04;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + 256 * ((colorram[offs] & 3) >> 0),
					colorram[offs] >> 3,
					flipx,flipscreen,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}

		if (dirtybuffer2[offs])
		{
			int sx,sy,flipx;


			dirtybuffer2[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			flipx = bankp_colorram2[offs] & 0x08;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
			}

			drawgfx(tmpbitmap2,Machine->gfx[1],
					bankp_videoram2[offs] + 256 * (bankp_colorram2[offs] & 0x07),
					bankp_colorram2[offs] >> 4,
					flipx,flipscreen,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}




	/* copy the temporary bitmaps to the screen */
	{
		int scroll;


		scroll = -scroll_x;

		if (priority == 2)
		{
			copyscrollbitmap(bitmap,tmpbitmap,1,&scroll,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
			copybitmap(bitmap,tmpbitmap2,0,0,0,0,&Machine->visible_area,TRANSPARENCY_COLOR,0);
		}
		else
		{
			copybitmap(bitmap,tmpbitmap2,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
			copyscrollbitmap(bitmap,tmpbitmap,1,&scroll,0,0,&Machine->visible_area,TRANSPARENCY_COLOR,0);
		}
	}
}
