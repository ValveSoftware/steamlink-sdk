/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *cop01_videoram;
size_t cop01_videoram_size;

static unsigned char cop01_scrollx[1];
static unsigned char spritebank = 0;
static int flipscreen;

static struct osd_bitmap *tmpbitmap2;



void cop01_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])

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

	color_prom += 2*Machine->drv->total_colors;
	/* color_prom now points to the beginning of the lookup tables */

	/* characters use colors 0-15 */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = (*(color_prom++) & 0x0f);	/* ?? */

	/* background tiles use colors 192-255 */
	for (i = 0;i < TOTAL_COLORS(1);i++)
		COLOR(1,i) = i + 192;

	/* sprites use colors 128-143 */
	for (i = 0;i < TOTAL_COLORS(2);i++)
		COLOR(2,i) = (*(color_prom++) & 0x0f) + 128;
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int cop01_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((tmpbitmap2 = bitmap_alloc(2*Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		generic_vh_stop();
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/

void cop01_vh_stop(void)
{
	bitmap_free(tmpbitmap2);
	generic_vh_stop();
}



WRITE_HANDLER( cop01_scrollx_w )
{
	cop01_scrollx[offset] = data;
}


WRITE_HANDLER( cop01_gfxbank_w )
{
	/* bits 0 and 1 coin counters */
	coin_counter_w(0,data & 1);
	coin_counter_w(1,data & 2);

	/* bit 2 flip screen */
	if (flipscreen != (data & 0x04))
	{
		flipscreen = data & 0x04;
        memset(dirtybuffer,1,videoram_size);
	}

	/* bits 4 and 5 select sprite bank */
	spritebank = (data & 0x30) >> 4;

//logerror("gfxbank = %02x\n",data);
}




/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void cop01_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* draw the background */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy;

			dirtybuffer[offs] = 0;

			sx = offs % 64;
			sy = offs / 64;
			if (flipscreen)
			{
				sx = 63 - sx;
				sy = 31 - sy;
			}

			drawgfx(tmpbitmap2,Machine->gfx[1],
					videoram[offs] + ((colorram[offs] & 0x03) << 8),
					(colorram[offs] & 0x0c) >> 2,
					flipscreen,flipscreen,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the background graphics */
	{
		int scrollx;


		if (flipscreen)
			scrollx = (cop01_scrollx[0] + 256 * cop01_scrollx[1]) - 256;
		else
			scrollx = -(cop01_scrollx[0] + 256 * cop01_scrollx[1]);

		copyscrollbitmap(bitmap,tmpbitmap2,1,&scrollx,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* draw the sprites */
	for (offs = 0;offs < spriteram_size;offs += 4)
	{
		int attr = spriteram[offs+2];
		int numtile = spriteram[offs+1];
		int flipx = attr & 4;
		int sx,sy;

		if (numtile & 0x80)	/* high tiles are bankswitched */
		{
			if (spritebank & 1) numtile += 128;
			else if (spritebank & 2) numtile += 256;
		}

		sy = 240 - spriteram[offs];
		sx = (spriteram[offs+3] - 0x80) + 256 * (attr & 1);
		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
		}

		drawgfx(bitmap,Machine->gfx[2],
				numtile,
				(attr & 0xf0) >> 4,
				flipx,flipscreen,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}


	/* draw the foreground characters */
	for (offs = cop01_videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy;


		sx = offs % 32;
		sy = offs / 32;
		if (flipscreen)
		{
			sx = 31 - sx;
			sy = 31 - sy;
		}

		drawgfx(bitmap,Machine->gfx[0],
				cop01_videoram[offs],
				0,	/* is there a color selector missing? */
				flipscreen,flipscreen,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_PEN,15);
	}
}
