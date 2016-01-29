/***************************************************************************

  Video Hardware for Double Dragon (bootleg) & Double Dragon II

***************************************************************************/

#include "driver.h"


unsigned char *ddragon_bgvideoram,*ddragon_fgvideoram;
int ddragon_scrollx_hi, ddragon_scrolly_hi;
unsigned char *ddragon_scrollx_lo;
unsigned char *ddragon_scrolly_lo;
unsigned char *ddragon_spriteram;
int dd2_video;

static struct tilemap *fg_tilemap,*bg_tilemap;



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static UINT32 background_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return (col & 0x0f) + ((row & 0x0f) << 4) + ((col & 0x10) << 4) + ((row & 0x10) << 5);
}

static void get_bg_tile_info(int tile_index)
{
	unsigned char attr = ddragon_bgvideoram[2*tile_index];
	SET_TILE_INFO(2,ddragon_bgvideoram[2*tile_index+1] + ((attr & 0x07) << 8),(attr >> 3) & 0x07)
	tile_info.flags = TILE_FLIPYX((attr & 0xc0) >> 6);
}

static void get_fg_tile_info(int tile_index)
{
	unsigned char attr = ddragon_fgvideoram[2*tile_index];
	SET_TILE_INFO(0,ddragon_fgvideoram[2*tile_index+1] + ((attr & 0x07) << 8),attr >> 5)
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int ddragon_vh_start(void)
{
	bg_tilemap = tilemap_create(get_bg_tile_info,background_scan,  TILEMAP_OPAQUE,     16,16,32,32);
	fg_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,32,32);

	if (!bg_tilemap || !fg_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 0;

	return 0;
}


/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( ddragon_bgvideoram_w )
{
	if (ddragon_bgvideoram[offset] != data)
	{
		ddragon_bgvideoram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset/2);
	}
}

WRITE_HANDLER( ddragon_fgvideoram_w )
{
	if (ddragon_fgvideoram[offset] != data)
	{
		ddragon_fgvideoram[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap,offset/2);
	}
}



/***************************************************************************

  Display refresh

***************************************************************************/

#define DRAW_SPRITE( order, sx, sy ) drawgfx( bitmap, gfx, \
					(which+order),color,flipx,flipy,sx,sy, \
					clip,TRANSPARENCY_PEN,0);

static void draw_sprites(struct osd_bitmap *bitmap)
{
	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[1];

	unsigned char *src = &( ddragon_spriteram[0x800] );
	int i;

	for( i = 0; i < ( 64 * 5 ); i += 5 ) {
		int attr = src[i+1];
		if ( attr & 0x80 ) { /* visible */
			int sx = 240 - src[i+4] + ( ( attr & 2 ) << 7 );
			int sy = 240 - src[i+0] + ( ( attr & 1 ) << 8 );
			int size = ( attr & 0x30 ) >> 4;
			int flipx = ( attr & 8 );
			int flipy = ( attr & 4 );
			int dx = -16,dy = -16;

			int which;
			int color;

			if ( dd2_video ) {
				color = ( src[i+2] >> 5 );
				which = src[i+3] + ( ( src[i+2] & 0x1f ) << 8 );
			} else {
				color = ( src[i+2] >> 4 ) & 0x07;
				which = src[i+3] + ( ( src[i+2] & 0x0f ) << 8 );
			}

			if (flip_screen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
				dx = -dx;
				dy = -dy;
			}

			switch ( size ) {
				case 0: /* normal */
				DRAW_SPRITE( 0, sx, sy );
				break;

				case 1: /* double y */
				DRAW_SPRITE( 0, sx, sy + dy );
				DRAW_SPRITE( 1, sx, sy );
				break;

				case 2: /* double x */
				DRAW_SPRITE( 0, sx + dx, sy );
				DRAW_SPRITE( 2, sx, sy );
				break;

				case 3:
				DRAW_SPRITE( 0, sx + dx, sy + dy );
				DRAW_SPRITE( 1, sx + dx, sy );
				DRAW_SPRITE( 2, sx, sy + dy );
				DRAW_SPRITE( 3, sx, sy );
				break;
			}
		}
	}
}

#undef DRAW_SPRITE


void ddragon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int scrollx = ddragon_scrollx_hi + *ddragon_scrollx_lo;
	int scrolly = ddragon_scrolly_hi + *ddragon_scrolly_lo;

	tilemap_set_scrollx(bg_tilemap,0,scrollx);
	tilemap_set_scrolly(bg_tilemap,0,scrolly);

	tilemap_update(ALL_TILEMAPS);

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap,bg_tilemap,0);
	draw_sprites(bitmap);
	tilemap_draw(bitmap,fg_tilemap,0);
}
