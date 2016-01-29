#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *ohmygod_videoram;

static int spritebank;
static struct tilemap *bg_tilemap;



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_tile_info(int tile_index)
{
	UINT16 code = READ_WORD(&ohmygod_videoram[4*tile_index+2]);
	UINT16 attr = READ_WORD(&ohmygod_videoram[4*tile_index]);
	SET_TILE_INFO(0,code,(attr & 0x0f00) >> 8)
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int ohmygod_vh_start(void)
{
	bg_tilemap = tilemap_create(get_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,8,8,64,64);

	if (!bg_tilemap)
		return 1;

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

READ_HANDLER( ohmygod_videoram_r )
{
   return READ_WORD(&ohmygod_videoram[offset]);
}

WRITE_HANDLER( ohmygod_videoram_w )
{
	int oldword = READ_WORD(&ohmygod_videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&ohmygod_videoram[offset],newword);
		tilemap_mark_tile_dirty(bg_tilemap,offset/4);
	}
}

WRITE_HANDLER( ohmygod_spritebank_w )
{
	spritebank = data & 0x8000;
}

WRITE_HANDLER( ohmygod_scroll_w )
{
	if (offset == 0) tilemap_set_scrollx(bg_tilemap,0,data - 0x81ec);
	else tilemap_set_scrolly(bg_tilemap,0,data - 0x81ef);
}


/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;

	for (offs = 0;offs < spriteram_size;offs += 8)
	{
		int sx,sy,code,color,flipx;
		unsigned char *sr;

		sr = spritebank ? spriteram_2 : spriteram;

		code = READ_WORD(&sr[offs+6]) & 0x0fff;
		color = READ_WORD(&sr[offs+4]) & 0x000f;
		sx = READ_WORD(&sr[offs+0]) - 29;
		sy = READ_WORD(&sr[offs+2]);
		if (sy >= 32768) sy -= 65536;
		flipx = READ_WORD(&sr[offs+6]) & 0x8000;

		drawgfx(bitmap,Machine->gfx[1],
				code,
				color,
				flipx,0,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

void ohmygod_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_update(ALL_TILEMAPS);

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap,bg_tilemap,0);
	draw_sprites(bitmap);
}
