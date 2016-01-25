#include "driver.h"
#include "vidhrdw/generic.h"


static struct tilemap *bg_tilemap;
static int flipscreen;


void sidepckt_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;

	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2,bit3;

		/* red component */
		bit0 = (color_prom[0] >> 4) & 0x01;
		bit1 = (color_prom[0] >> 5) & 0x01;
		bit2 = (color_prom[0] >> 6) & 0x01;
		bit3 = (color_prom[0] >> 7) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* green component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* blue component */
		bit0 = (color_prom[Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}
}



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_tile_info(int tile_index)
{
	unsigned char attr = colorram[tile_index];
	SET_TILE_INFO(0,videoram[tile_index] + ((attr & 0x07) << 8),
			((attr & 0x10) >> 3) | ((attr & 0x20) >> 5))
	tile_info.flags = TILE_FLIPX | TILE_SPLIT((attr & 0x80) >> 7);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int sidepckt_vh_start(void)
{
	bg_tilemap = tilemap_create(get_tile_info,tilemap_scan_rows,TILEMAP_SPLIT,8,8,32,32);

	if (!bg_tilemap)
		return 1;

	bg_tilemap->transmask[0] = 0xff; /* split type 0 is totally transparent in front half */
	bg_tilemap->transmask[1] = 0x01; /* split type 1 has pen 1 transparent in front half */

	tilemap_set_flip(ALL_TILEMAPS,TILEMAP_FLIPX);

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( sidepckt_videoram_w )
{
	if (videoram[offset] != data)
	{
		videoram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset);
	}
}

WRITE_HANDLER( sidepckt_colorram_w )
{
	if (colorram[offset] != data)
	{
		colorram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset);
	}
}

WRITE_HANDLER( sidepckt_flipscreen_w )
{
	flipscreen = data;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? TILEMAP_FLIPY : TILEMAP_FLIPX);
}


/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;

	for (offs = 0;offs < spriteram_size; offs += 4)
	{
		int sx,sy,code,color,flipx,flipy;

		code = spriteram[offs+3] + ((spriteram[offs+1] & 0x03) << 8);
		color = (spriteram[offs+1] & 0xf0) >> 4;

		sx = spriteram[offs+2]-2;
		sy = spriteram[offs];

		flipx = spriteram[offs+1] & 0x08;
		flipy = spriteram[offs+1] & 0x04;

		drawgfx(bitmap,Machine->gfx[1],
				code,
				color,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
		/* wraparound */
		drawgfx(bitmap,Machine->gfx[1],
				code,
				color,
				flipx,flipy,
				sx-256,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}


void sidepckt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_update(ALL_TILEMAPS);
	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap,bg_tilemap,TILEMAP_BACK);
	draw_sprites(bitmap);
	tilemap_draw(bitmap,bg_tilemap,TILEMAP_FRONT);
}
