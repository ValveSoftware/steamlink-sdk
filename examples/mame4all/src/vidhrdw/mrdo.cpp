/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *mrdo_bgvideoram,*mrdo_fgvideoram;
static struct tilemap *bg_tilemap,*fg_tilemap;
static int flipscreen;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Mr. Do! has two 32 bytes palette PROM and a 32 bytes sprite color lookup
  table PROM.
  The palette PROMs are connected to the RGB output this way:

  U2:
  bit 7 -- unused
        -- unused
        -- 100 ohm resistor  -- BLUE
        --  75 ohm resistor  -- BLUE
        -- 100 ohm resistor  -- GREEN
        --  75 ohm resistor  -- GREEN
        -- 100 ohm resistor  -- RED
  bit 0 --  75 ohm resistor  -- RED

  T2:
  bit 7 -- unused
        -- unused
        -- 150 ohm resistor  -- BLUE
        -- 120 ohm resistor  -- BLUE
        -- 150 ohm resistor  -- GREEN
        -- 120 ohm resistor  -- GREEN
        -- 150 ohm resistor  -- RED
  bit 0 -- 120 ohm resistor  -- RED

***************************************************************************/
void mrdo_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < 256;i++)
	{
		int a1,a2;
		int bit0,bit1,bit2,bit3;


		a1 = ((i >> 3) & 0x1c) + (i & 0x03) + 32;
		a2 = ((i >> 0) & 0x1c) + (i & 0x03);

		bit0 = (color_prom[a1] >> 1) & 0x01;
		bit1 = (color_prom[a1] >> 0) & 0x01;
		bit2 = (color_prom[a2] >> 1) & 0x01;
		bit3 = (color_prom[a2] >> 0) & 0x01;
		*(palette++) = 0x2c * bit0 + 0x37 * bit1 + 0x43 * bit2 + 0x59 * bit3;
		bit0 = (color_prom[a1] >> 3) & 0x01;
		bit1 = (color_prom[a1] >> 2) & 0x01;
		bit2 = (color_prom[a2] >> 3) & 0x01;
		bit3 = (color_prom[a2] >> 2) & 0x01;
		*(palette++) = 0x2c * bit0 + 0x37 * bit1 + 0x43 * bit2 + 0x59 * bit3;
		bit0 = (color_prom[a1] >> 5) & 0x01;
		bit1 = (color_prom[a1] >> 4) & 0x01;
		bit2 = (color_prom[a2] >> 5) & 0x01;
		bit3 = (color_prom[a2] >> 4) & 0x01;
		*(palette++) = 0x2c * bit0 + 0x37 * bit1 + 0x43 * bit2 + 0x59 * bit3;
	}

	color_prom += 64;

	/* sprites */
	for (i = 0;i < TOTAL_COLORS(2);i++)
	{
		int bits;

		if (i < 32)
			bits = color_prom[i] & 0x0f;		/* low 4 bits are for sprite color n */
		else
			bits = color_prom[i & 0x1f] >> 4;	/* high 4 bits are for sprite color n + 8 */

		COLOR(2,i) = bits + ((bits & 0x0c) << 3);
	}
}



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_bg_tile_info(int tile_index)
{
	unsigned char attr = mrdo_bgvideoram[tile_index];
	SET_TILE_INFO(1,mrdo_bgvideoram[tile_index+0x400] + ((attr & 0x80) << 1),attr & 0x3f)
	tile_info.flags = TILE_SPLIT((attr & 0x40) >> 6);
}

static void get_fg_tile_info(int tile_index)
{
	unsigned char attr = mrdo_fgvideoram[tile_index];
	SET_TILE_INFO(0,mrdo_fgvideoram[tile_index+0x400] + ((attr & 0x80) << 1),attr & 0x3f)
	tile_info.flags = TILE_SPLIT((attr & 0x40) >> 6);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int mrdo_vh_start(void)
{
	bg_tilemap = tilemap_create(get_bg_tile_info,tilemap_scan_rows,TILEMAP_SPLIT,8,8,32,32);
	fg_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_SPLIT,8,8,32,32);

	if (!bg_tilemap || !fg_tilemap)
		return 1;

	bg_tilemap->transmask[0] = 0x01; /* split type 0 has pen 1 transparent in front half */
	bg_tilemap->transmask[1] = 0x00; /* split type 1 is totally opaque in front half */
	fg_tilemap->transmask[0] = 0x01; /* split type 0 has pen 1 transparent in front half */
	fg_tilemap->transmask[1] = 0x00; /* split type 1 is totally opaque in front half */

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( mrdo_bgvideoram_w )
{
	if (mrdo_bgvideoram[offset] != data)
	{
		mrdo_bgvideoram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset & 0x3ff);
	}
}

WRITE_HANDLER( mrdo_fgvideoram_w )
{
	if (mrdo_fgvideoram[offset] != data)
	{
		mrdo_fgvideoram[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap,offset & 0x3ff);
	}
}


WRITE_HANDLER( mrdo_scrollx_w )
{
	tilemap_set_scrollx(bg_tilemap,0,data);
}

WRITE_HANDLER( mrdo_scrolly_w )
{
	tilemap_set_scrolly(bg_tilemap,0,data);
}


WRITE_HANDLER( mrdo_flipscreen_w )
{
	/* bits 1-3 control the playfield priority, but they are not used by */
	/* Mr. Do! so we don't emulate them */

	flipscreen = data & 0x01;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
}



/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;


	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		if (spriteram[offs + 1] != 0)
		{
			drawgfx(bitmap,Machine->gfx[2],
					spriteram[offs],spriteram[offs + 2] & 0x0f,
					spriteram[offs + 2] & 0x10,spriteram[offs + 2] & 0x20,
					spriteram[offs + 3],256 - spriteram[offs + 1],
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}
}

void mrdo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_update(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);
	tilemap_draw(bitmap,bg_tilemap,TILEMAP_FRONT);
	tilemap_draw(bitmap,fg_tilemap,TILEMAP_FRONT);
	draw_sprites(bitmap);
}
