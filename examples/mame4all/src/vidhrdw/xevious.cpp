/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"


unsigned char *xevious_fg_videoram,*xevious_fg_colorram;
unsigned char *xevious_bg_videoram,*xevious_bg_colorram;

extern unsigned char *spriteram,*spriteram_2,*spriteram_3;
extern size_t spriteram_size;

static struct tilemap *fg_tilemap,*bg_tilemap;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Xevious has three 256x4 palette PROMs (one per gun) and four 512x4 lookup
  table PROMs (two for sprites, two for background tiles; foreground
  characters map directly to a palette color without using a PROM).
  The palette PROMs are connected to the RGB output this way:

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
		-- 470 ohm resistor  -- RED/GREEN/BLUE
		-- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void xevious_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < 128;i++)
	{
		int bit0,bit1,bit2,bit3;


		/* red component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* green component */
		bit0 = (color_prom[256] >> 0) & 0x01;
		bit1 = (color_prom[256] >> 1) & 0x01;
		bit2 = (color_prom[256] >> 2) & 0x01;
		bit3 = (color_prom[256] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* blue component */
		bit0 = (color_prom[2*256] >> 0) & 0x01;
		bit1 = (color_prom[2*256] >> 1) & 0x01;
		bit2 = (color_prom[2*256] >> 2) & 0x01;
		bit3 = (color_prom[2*256] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}

	/* color 0x80 is used by sprites to mark transparency */
	*(palette++) = 0;
	*(palette++) = 0;
	*(palette++) = 0;

	color_prom += 128;  /* the bottom part of the PROM is unused */
	color_prom += 2*256;
	/* color_prom now points to the beginning of the lookup table */

	/* background tiles */
	for (i = 0;i < TOTAL_COLORS(1);i++)
	{
		COLOR(1,i) = (color_prom[0] & 0x0f) | ((color_prom[TOTAL_COLORS(1)] & 0x0f) << 4);

		color_prom++;
	}
	color_prom += TOTAL_COLORS(1);

	/* sprites */
	for (i = 0;i < TOTAL_COLORS(2);i++)
	{
		int c = (color_prom[0] & 0x0f) | ((color_prom[TOTAL_COLORS(2)] & 0x0f) << 4);

		if (c & 0x80) COLOR(2,i) = c & 0x7f;
		else COLOR(2,i) = 0x80; /* transparent */

		color_prom++;
	}
	color_prom += TOTAL_COLORS(2);

	/* foreground characters */
	for (i = 0;i < TOTAL_COLORS(0);i++)
	{
		if (i % 2 == 0) COLOR(0,i) = 0x80;  /* transparent */
		else COLOR(0,i) = i / 2;
	}
}



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_fg_tile_info(int tile_index)
{
	unsigned char attr = xevious_fg_colorram[tile_index];
	SET_TILE_INFO(0,xevious_fg_videoram[tile_index],
			((attr & 0x03) << 4) | ((attr & 0x3c) >> 2))
	tile_info.flags = TILE_FLIPYX((attr & 0xc0) >> 6);
}

static void get_bg_tile_info(int tile_index)
{
	unsigned char code = xevious_bg_videoram[tile_index];
	unsigned char attr = xevious_bg_colorram[tile_index];
	SET_TILE_INFO(1,code + ((attr & 0x01) << 8),
			((attr & 0x3c) >> 2) | ((code & 0x80) >> 3) | ((attr & 0x03) << 5))
	tile_info.flags = TILE_FLIPYX((attr & 0xc0) >> 6);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int xevious_vh_start(void)
{
	bg_tilemap = tilemap_create(get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,     8,8,64,32);
	fg_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);

	if (!bg_tilemap || !fg_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 0;

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( xevious_fg_videoram_w )
{
	if (xevious_fg_videoram[offset] != data)
	{
		xevious_fg_videoram[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap,offset);
	}
}

WRITE_HANDLER( xevious_fg_colorram_w )
{
	if (xevious_fg_colorram[offset] != data)
	{
		xevious_fg_colorram[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap,offset);
	}
}

WRITE_HANDLER( xevious_bg_videoram_w )
{
	if (xevious_bg_videoram[offset] != data)
	{
		xevious_bg_videoram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset);
	}
}

WRITE_HANDLER( xevious_bg_colorram_w )
{
	if (xevious_bg_colorram[offset] != data)
	{
		xevious_bg_colorram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset);
	}
}

WRITE_HANDLER( xevious_vh_latch_w )
{
	int reg;

	data = data + ((offset&0x01)<<8);   /* A0 -> D8 */
	reg = (offset&0xf0)>>4;

	switch (reg)
	{
	case 0:
		if (flip_screen)
			tilemap_set_scrollx(bg_tilemap,0,data-312);
		else
			tilemap_set_scrollx(bg_tilemap,0,data+20);
		break;
	case 1:
		tilemap_set_scrollx(fg_tilemap,0,data+32);
		break;
	case 2:
		tilemap_set_scrolly(bg_tilemap,0,data+16);
		break;
	case 3:
		tilemap_set_scrolly(fg_tilemap,0,data+18);
		break;
	case 7:
		flip_screen_w(0,data & 1);
		break;
   default:
		   //logerror("CRTC WRITE REG: %x  Data: %03x\n",reg, data);
		   break;
	}
}




/*
background pattern data

colorram mapping
b000-bfff background attribute
		  bit 0-1 COL:palette set select
		  bit 2-5 AN :color select
		  bit 6   AFF:Y flip
		  bit 7   PFF:X flip
c000-cfff background pattern name
		  bit 0-7 PP0-7

seet 8A
										2	  +-------+
COL0,1 --------------------------------------->|backg. |
										1	  |color  |
PP7------------------------------------------->|replace|
										4	  | ROM   |  6
AN0-3 ---------------------------------------->|  4H   |-----> color code 6 bit
		1  +-----------+	  +--------+	   |  4F   |
COL0  ---->|B8   ROM 3C| 16   |custom  |  2	|	   |
		8  |		   |----->|shifter |------>|	   |
PP0-7 ---->|B0-7 ROM 3D|	  |16->2*8 |	   |	   |
		   +-----------+	  +--------+	   +-------+

font rom controller
	   1  +--------+	 +--------+
ANF   --->| ROM	|  8  |shift   |  1
	   8  | 3B	 |---->|reg	 |-----> font data
PP0-7 --->|		|	 |8->1*8  |
		  +--------+	 +--------+

font color ( not use color map )
		2  |
COL0-1 --->|  color code 6 bit
		4  |
AN0-3  --->|

sprite

ROM 3M,3L color reprace table for sprite



*/




/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs,sx,sy;


	for (offs = 0;offs < spriteram_size;offs += 2)
	{
		if ((spriteram[offs + 1] & 0x40) == 0)  /* I'm not sure about this one */
		{
			int bank,code,color,flipx,flipy;


			if (spriteram_3[offs] & 0x80)
			{
				bank = 4;
				code = spriteram[offs] & 0x3f;
			}
			else
			{
				bank = 2 + ((spriteram[offs] & 0x80) >> 7);
				code = spriteram[offs] & 0x7f;
			}

			color = spriteram[offs + 1] & 0x7f;
			flipx = spriteram_3[offs] & 4;
			flipy = spriteram_3[offs] & 8;
			if (flip_screen)
			{
				flipx = !flipx;
				flipy = !flipy;
			}
			sx = spriteram_2[offs + 1] - 40 + 0x100*(spriteram_3[offs + 1] & 1);
			sy = 28*8-spriteram_2[offs]-1;
			if (spriteram_3[offs] & 2)  /* double height (?) */
			{
				if (spriteram_3[offs] & 1)  /* double width, double height */
				{
					code &= 0x7c;
					drawgfx(bitmap,Machine->gfx[bank],
							code+3,color,flipx,flipy,
							flipx ? sx : sx+16,flipy ? sy-16 : sy,
							&Machine->visible_area,TRANSPARENCY_COLOR,0x80);
					drawgfx(bitmap,Machine->gfx[bank],
							code+1,color,flipx,flipy,
							flipx ? sx : sx+16,flipy ? sy : sy-16,
							&Machine->visible_area,TRANSPARENCY_COLOR,0x80);
				}
				code &= 0x7d;
				drawgfx(bitmap,Machine->gfx[bank],
						code+2,color,flipx,flipy,
						flipx ? sx+16 : sx,flipy ? sy-16 : sy,
						&Machine->visible_area,TRANSPARENCY_COLOR,0x80);
				drawgfx(bitmap,Machine->gfx[bank],
						code,color,flipx,flipy,
						flipx ? sx+16 : sx,flipy ? sy : sy-16,
						&Machine->visible_area,TRANSPARENCY_COLOR,0x80);
			}
			else if (spriteram_3[offs] & 1) /* double width */
			{
				code &= 0x7e;
				drawgfx(bitmap,Machine->gfx[bank],
						code,color,flipx,flipy,
						flipx ? sx+16 : sx,flipy ? sy-16 : sy,
						&Machine->visible_area,TRANSPARENCY_COLOR,0x80);
				drawgfx(bitmap,Machine->gfx[bank],
						code+1,color,flipx,flipy,
						flipx ? sx : sx+16,flipy ? sy-16 : sy,
						&Machine->visible_area,TRANSPARENCY_COLOR,0x80);
			}
			else	/* normal */
			{
				drawgfx(bitmap,Machine->gfx[bank],
						code,color,flipx,flipy,sx,sy,
						&Machine->visible_area,TRANSPARENCY_COLOR,0x80);
			}
		}
	}
}


void xevious_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_update(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap,bg_tilemap,0);
	draw_sprites(bitmap);
	tilemap_draw(bitmap,fg_tilemap,0);
}
