/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"

extern unsigned char *spriteram;
extern size_t spriteram_size;

unsigned char *gng_fgvideoram,*gng_fgcolorram;
unsigned char *gng_bgvideoram,*gng_bgcolorram;
static struct tilemap *bg_tilemap,*fg_tilemap;
static int flipscreen;



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_fg_tile_info(int tile_index)
{
	unsigned char attr = gng_fgcolorram[tile_index];
	SET_TILE_INFO(0,gng_fgvideoram[tile_index] + ((attr & 0xc0) << 2),attr & 0x0f)
	tile_info.flags = TILE_FLIPYX((attr & 0x30) >> 4);
}

static void get_bg_tile_info(int tile_index)
{
	unsigned char attr = gng_bgcolorram[tile_index];
	SET_TILE_INFO(1,gng_bgvideoram[tile_index] + ((attr & 0xc0) << 2),attr & 0x07)
	tile_info.flags = TILE_FLIPYX((attr & 0x30) >> 4) | TILE_SPLIT((attr & 0x08) >> 3);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int gng_vh_start(void)
{
	fg_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,32,32);
	bg_tilemap = tilemap_create(get_bg_tile_info,tilemap_scan_cols,TILEMAP_SPLIT,    16,16,32,32);

	if (!fg_tilemap || !bg_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 3;

	bg_tilemap->transmask[0] = 0xff; /* split type 0 is totally transparent in front half */
	bg_tilemap->transmask[1] = 0x01; /* split type 1 has pen 1 transparent in front half */

	return 0;
}


/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( gng_fgvideoram_w )
{
	if (gng_fgvideoram[offset] != data)
	{
		gng_fgvideoram[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap,offset);
	}
}

WRITE_HANDLER( gng_fgcolorram_w )
{
	if (gng_fgcolorram[offset] != data)
	{
		gng_fgcolorram[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap,offset);
	}
}

WRITE_HANDLER( gng_bgvideoram_w )
{
	if (gng_bgvideoram[offset] != data)
	{
		gng_bgvideoram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset);
	}
}

WRITE_HANDLER( gng_bgcolorram_w )
{
	if (gng_bgcolorram[offset] != data)
	{
		gng_bgcolorram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset);
	}
}


WRITE_HANDLER( gng_bgscrollx_w )
{
	static unsigned char scrollx[2];
	scrollx[offset] = data;
	tilemap_set_scrollx( bg_tilemap, 0, scrollx[0] + 256 * scrollx[1] );
}

WRITE_HANDLER( gng_bgscrolly_w )
{
	static unsigned char scrolly[2];
	scrolly[offset] = data;
	tilemap_set_scrolly( bg_tilemap, 0, scrolly[0] + 256 * scrolly[1] );
}


WRITE_HANDLER( gng_flipscreen_w )
{
	flipscreen = ~data & 1;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
}



/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_sprites(struct osd_bitmap *bitmap)
{
	const struct GfxElement *gfx = Machine->gfx[2];
	const struct rectangle *clip = &Machine->visible_area;
	int offs;
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4){
		unsigned char attributes = spriteram[offs+1];
		int sx = spriteram[offs + 3] - 0x100 * (attributes & 0x01);
		int sy = spriteram[offs + 2];
		int flipx = attributes & 0x04;
		int flipy = attributes & 0x08;

		if (flipscreen){
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		drawgfx(bitmap,gfx,
				spriteram[offs] + ((attributes<<2) & 0x300),
				(attributes >> 4) & 3,
				flipx,flipy,
				sx,sy,
				clip,TRANSPARENCY_PEN,15);
	}
}

void gng_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_update(ALL_TILEMAPS);

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap,bg_tilemap,TILEMAP_BACK);
	draw_sprites(bitmap);
	tilemap_draw(bitmap,bg_tilemap,TILEMAP_FRONT);
	tilemap_draw(bitmap,fg_tilemap,0);
}
