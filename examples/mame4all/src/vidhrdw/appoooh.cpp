/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

#define CHR1_OFST 0x00  /* palette page of char set #1 */
#define CHR2_OFST 0x10  /* palette page of char set #2 */

unsigned char *appoooh_videoram2;
unsigned char *appoooh_colorram2;
unsigned char *appoooh_spriteram2;
static unsigned char *dirtybuffer2;
static struct osd_bitmap *tmpbitmap2;

static int scroll_x;
static int flipscreen;
static int priority;

/***************************************************************************

  Convert the color PROMs into a more useable format.

  Palette information of appoooh is not known.

  The palette decoder of Bank Panic was used for this driver.
  Because these hardware is similar.

***************************************************************************/
void appoooh_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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
		COLOR(0,i) = (*(color_prom++) & 0x0f)|CHR1_OFST;

	/* charset #2 lookup table */
	for (i = 0;i < TOTAL_COLORS(1);i++)
		COLOR(1,i) = (*(color_prom++) & 0x0f)|CHR2_OFST;

	/* TODO: the driver currently uses only 16 of the 32 color codes. */
	/* 16-31 might be unused, but there might be a palette bank selector */
	/* to use them somewhere in the game. */
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int appoooh_vh_start(void)
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

WRITE_HANDLER( appoooh_scroll_w )
{
	scroll_x = data;
}

/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void appoooh_vh_stop(void)
{
	free(dirtybuffer2);
	bitmap_free(tmpbitmap2);
	generic_vh_stop();

}

WRITE_HANDLER( appoooh_videoram2_w )
{
	if (appoooh_videoram2[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		appoooh_videoram2[offset] = data;
	}
}



WRITE_HANDLER( appoooh_colorram2_w )
{
	if (appoooh_colorram2[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		appoooh_colorram2[offset] = data;
	}
}

WRITE_HANDLER( appoooh_out_w )
{
	/* bit 0 controls NMI */
	if (data & 0x01) interrupt_enable_w(0,1);
	else interrupt_enable_w(0,0);

	/* bit 1 flip screen */
	if ((data & 0x02) != flipscreen)
	{
		flipscreen = data & 0x02;
		memset(dirtybuffer,1,videoram_size);
		memset(dirtybuffer2,1,videoram_size);
	}

	/* bits 2-3 unknown */

	/* bits 4-5 are playfield/sprite priority */
	/* TODO: understand how this works, currently the only thing I do is draw */
	/* the front layer behind sprites when priority == 0, and invert the sprite */
	/* order when priority == 1 */
	priority = (data & 0x30) >> 4;

	/* bit 6 ROM bank select */
	{
		unsigned char *RAM = memory_region(REGION_CPU1);

		cpu_setbank(1,&RAM[data&0x40 ? 0x10000 : 0x0a000]);
	}

	/* bit 7 unknown (used) */
}

static void appoooh_draw_sprites(struct osd_bitmap *dest_bmp,
        const struct GfxElement *gfx,
        unsigned char *sprite)
{
	int offs;

	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int sy    = 256-16-sprite[offs+0];
		int code  = (sprite[offs+1]>>2) + ((sprite[offs+2]>>5) & 0x07)*0x40;
		int color = sprite[offs+2]&0x0f;	/* TODO: bit 4 toggles continuously, what is it? */
		int sx    = sprite[offs+3];
		int flipx = sprite[offs+1]&0x01;

		if(sx>=248) sx -= 256;

		if (flipscreen)
		{
			sx = 239- sx;
			sy = 255- sy;
			flipx = !flipx;
		}
		drawgfx( dest_bmp, gfx,
				code,
				color,
				flipx,flipscreen,
				sx, sy,
				&Machine->visible_area,
				TRANSPARENCY_PEN , 0);
	 }
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void appoooh_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;
	int scroll;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		/* char set #1 */
		if (dirtybuffer[offs])
		{
			int sx,sy,code,flipx;

			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			code = videoram[offs] + 256 * ((colorram[offs]>>5) & 7);

			flipx = colorram[offs] & 0x10;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
			}
			drawgfx(tmpbitmap,Machine->gfx[0],
					code,
					colorram[offs]&0x0f,
					flipx,flipscreen,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
		/* char set #2 */
		if (dirtybuffer2[offs])
		{
			int sx,sy,code,flipx;

			dirtybuffer2[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			code = appoooh_videoram2[offs] + 256 * ((appoooh_colorram2[offs]>>5) & 0x07);

			flipx = appoooh_colorram2[offs] & 0x10;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
			}
			drawgfx(tmpbitmap2,Machine->gfx[1],
					code,
					appoooh_colorram2[offs]&0x0f,
					flipx,flipscreen,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	scroll = -scroll_x;
	scroll = 0;

	/* copy the temporary bitmaps to the screen */
	copybitmap(bitmap,tmpbitmap2,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	if (priority == 0)	/* fg behind sprites */
		copyscrollbitmap(bitmap,tmpbitmap,1,&scroll,0,0,&Machine->visible_area,TRANSPARENCY_COLOR,CHR1_OFST);

	/* draw sprites */
	if (priority == 1)
	{
		/* sprite set #1 */
		appoooh_draw_sprites( bitmap, Machine->gfx[2],spriteram);
		/* sprite set #2 */
		appoooh_draw_sprites( bitmap, Machine->gfx[3],appoooh_spriteram2);
	}
	else
	{
		/* sprite set #2 */
		appoooh_draw_sprites( bitmap, Machine->gfx[3],appoooh_spriteram2);
		/* sprite set #1 */
		appoooh_draw_sprites( bitmap, Machine->gfx[2],spriteram);
	}

	if (priority != 0)	/* fg in front of sprites */
		copyscrollbitmap(bitmap,tmpbitmap,1,&scroll,0,0,&Machine->visible_area,TRANSPARENCY_COLOR,CHR1_OFST);
}
