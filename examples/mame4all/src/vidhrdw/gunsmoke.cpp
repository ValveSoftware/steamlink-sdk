/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *gunsmoke_bg_scrolly;
unsigned char *gunsmoke_bg_scrollx;
static int chon,objon,bgon;
static int sprite3bank;
static int flipscreen;

static struct osd_bitmap * bgbitmap;
static unsigned char bgmap[9][9][2];


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Gunsmoke has three 256x4 palette PROMs (one per gun) and a lot ;-) of
  256x4 lookup table PROMs.
  The palette PROMs are connected to the RGB output this way:

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void gunsmoke_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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
	/* color_prom now points to the beginning of the lookup table */

	/* characters use colors 64-79 */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = *(color_prom++) + 64;
	color_prom += 128;	/* skip the bottom half of the PROM - not used */

	/* background tiles use colors 0-63 */
	for (i = 0;i < TOTAL_COLORS(1);i++)
	{
		COLOR(1,i) = color_prom[0] + 16 * (color_prom[256] & 0x03);
		color_prom++;
	}
	color_prom += TOTAL_COLORS(1);

	/* sprites use colors 128-255 */
	for (i = 0;i < TOTAL_COLORS(2);i++)
	{
		COLOR(2,i) = color_prom[0] + 16 * (color_prom[256] & 0x07) + 128;
		color_prom++;
	}
	color_prom += TOTAL_COLORS(2);
}



int gunsmoke_vh_start(void)
{
	if ((bgbitmap = bitmap_alloc(9*32,9*32)) == 0)
		return 1;

	if (generic_vh_start() == 1)
	{
		bitmap_free(bgbitmap);
		return 1;
	}

	memset (bgmap, 0xff, sizeof (bgmap));

	return 0;
}


void gunsmoke_vh_stop(void)
{
	bitmap_free(bgbitmap);
}



WRITE_HANDLER( gunsmoke_c804_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	/* bits 0 and 1 are for coin counters? - we ignore them */

	/* bits 2 and 3 select the ROM bank */
	bankaddress = 0x10000 + (data & 0x0c) * 0x1000;
	cpu_setbank(1,&RAM[bankaddress]);

	/* bit 5 resets the sound CPU? - we ignore it */

	/* bit 6 flips screen */
	if (flipscreen != (data & 0x40))
	{
		flipscreen = data & 0x40;
//		memset(dirtybuffer,1,c1942_backgroundram_size);
	}

	/* bit 7 enables characters? */
	chon = data & 0x80;
}



WRITE_HANDLER( gunsmoke_d806_w )
{
	/* bits 0-2 select the sprite 3 bank */
	sprite3bank = data & 0x07;

	/* bit 4 enables bg 1? */
	bgon = data & 0x10;

	/* bit 5 enables sprites? */
	objon = data & 0x20;
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void gunsmoke_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,sx,sy;
	int bg_scrolly, bg_scrollx;
	unsigned char *p=memory_region(REGION_GFX4);
	int top,left,xscroll,yscroll;


/* TODO: support flipscreen */
	if (bgon)
	{
		bg_scrolly = gunsmoke_bg_scrolly[0] + 256 * gunsmoke_bg_scrolly[1];
		bg_scrollx = gunsmoke_bg_scrollx[0];
		offs = 16 * ((bg_scrolly>>5)+8)+2*(bg_scrollx>>5) ;
		if (bg_scrollx & 0x80) offs -= 0x10;

		top = 8 - (bg_scrolly>>5) % 9;
		left = (bg_scrollx>>5) % 9;

		bg_scrolly&=0x1f;
		bg_scrollx&=0x1f;

		for (sy = 0;sy <9;sy++)
		{
			int ty = (sy + top) % 9;
			offs &= 0x7fff; /* Enforce limits (for top of scroll) */

			for (sx = 0;sx < 9;sx++)
			{
				int tile, attr, offset;
				int tx = (sx + left) % 9;
				unsigned char *map = &bgmap[ty][tx][0];
				offset=offs+(sx*2);

				tile=p[offset];
				attr=p[offset+1];

				if (tile != map[0] || attr != map[1])
				{
					map[0] = tile;
					map[1] = attr;
					tile+=256*(attr&0x01);
					drawgfx(bgbitmap,Machine->gfx[1],
							tile,
							(attr & 0x3c) >> 2,
							attr & 0x40,attr & 0x80,
							(8-ty)*32, tx*32,
							0,
							TRANSPARENCY_NONE,0);
				}
				map += 2;
			}
			offs-=0x10;
		}

		xscroll = (top*32-bg_scrolly);
		yscroll = -(left*32+bg_scrollx);
		copyscrollbitmap(bitmap,bgbitmap,
			1,&xscroll,
			1,&yscroll,
			&Machine->visible_area,
			TRANSPARENCY_NONE,0);
	}
	else fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);



	if (objon)
	{
		/* Draw the sprites. */
		for (offs = spriteram_size - 32;offs >= 0;offs -= 32)
		{
			int bank,flipx,flipy;


			bank = (spriteram[offs + 1] & 0xc0) >> 6;
			if (bank == 3) bank += sprite3bank;

			sx = spriteram[offs + 3] - ((spriteram[offs + 1] & 0x20) << 3);
 			sy = spriteram[offs + 2];
			flipx = 0;
			flipy = spriteram[offs + 1] & 0x10;
			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(bitmap,Machine->gfx[2],
					spriteram[offs] + 256 * bank,
					spriteram[offs + 1] & 0x0f,
					flipx,flipy,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}


	if (chon)
	{
		/* draw the frontmost playfield. They are characters, but draw them as sprites */
		for (offs = videoram_size - 1;offs >= 0;offs--)
		{
			sx = offs % 32;
			sy = offs / 32;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
			}

			drawgfx(bitmap,Machine->gfx[0],
					videoram[offs] + ((colorram[offs] & 0xc0) << 2),
					colorram[offs] & 0x1f,
					!flipscreen,!flipscreen,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_COLOR,79);
		}
	}
}
