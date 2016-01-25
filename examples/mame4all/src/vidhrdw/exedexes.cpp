/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *exedexes_bg_scroll;

unsigned char *exedexes_nbg_yscroll;
unsigned char *exedexes_nbg_xscroll;

#define TileMap(offs) (memory_region(REGION_GFX5)[offs])
#define BackTileMap(offs) (memory_region(REGION_GFX5)[offs+0x4000])


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Exed Exes has three 256x4 palette PROMs (one per gun), three 256x4 lookup
  table PROMs (one for characters, one for sprites, one for background tiles)
  and one 256x4 sprite palette bank selector PROM.

  The palette PROMs are connected to the RGB output this way:

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void exedexes_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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

	/* characters use colors 192-207 */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = (*color_prom++) + 192;

	/* 32x32 tiles use colors 0-15 */
	for (i = 0;i < TOTAL_COLORS(1);i++)
		COLOR(1,i) = (*color_prom++);

	/* 16x16 tiles use colors 64-79 */
	for (i = 0;i < TOTAL_COLORS(2);i++)
		COLOR(2,i) = (*color_prom++) + 64;

	/* sprites use colors 128-191 in four banks */
	for (i = 0;i < TOTAL_COLORS(3);i++)
	{
		COLOR(3,i) = color_prom[0] + 128 + 16 * color_prom[256];
		color_prom++;
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void exedexes_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,sx,sy;


/* TODO: this is very slow, have to optimize it using a temporary bitmap */
	/* draw the background graphics */
	/* back layer */
	for(sy = 0;sy <= 8;sy++)
	{
		for(sx = 0;sx < 8;sx++)
		{
	   		int xo,yo,tile;


			xo = sx*32;
			yo = ((exedexes_bg_scroll[1])<<8)+exedexes_bg_scroll[0] + sy*32;

			tile = ((yo & 0xe0) >> 5) + ((xo & 0xe0) >> 2) + ((yo & 0x3f00) >> 1);

			drawgfx(bitmap,Machine->gfx[1],
					BackTileMap(tile) & 0x3f,
					BackTileMap(tile+8*8),
					BackTileMap(tile) & 0x40,BackTileMap(tile) & 0x80,
					sy*32-(yo&0x1F),sx*32,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	/* front layer */
	for(sy = 0;sy <= 16;sy++)
	{
		for(sx = 0;sx < 16;sx++)
		{
	   		int xo,yo,tile;


			xo = ((exedexes_nbg_xscroll[1])<<8)+exedexes_nbg_xscroll[0] + sx*16;
			yo = ((exedexes_nbg_yscroll[1])<<8)+exedexes_nbg_yscroll[0] + sy*16;

			tile = ((yo & 0xf0) >> 4) + (xo & 0xF0) + (yo & 0x700) + ((xo & 0x700) << 3);

			drawgfx(bitmap,Machine->gfx[2],
				TileMap(tile),
				0,
				0,0,
				sy*16-(yo&0xF),sx*16-(xo&0xF),
				&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}


	/* Draw the sprites. */
	for (offs = spriteram_size - 32;offs >= 0;offs -= 32)
	{
		drawgfx(bitmap,Machine->gfx[3],
				spriteram[offs],
				spriteram[offs + 1] & 0x0f,
				spriteram[offs + 1] & 0x10, spriteram[offs + 1] & 0x20,
				spriteram[offs + 3] - 0x10 * (spriteram[offs + 1] & 0x80),spriteram[offs + 2],
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		sx = offs % 32;
		sy = offs / 32;

		drawgfx(bitmap,Machine->gfx[0],
				videoram[offs] + 2 * (colorram[offs] & 0x80),
				colorram[offs] & 0x3f,
				0,0,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_COLOR,207);
	}
}
