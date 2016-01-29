#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *lkage_scroll, *lkage_vreg;
static unsigned char bg_tile_bank, fg_tile_bank;

/*
	lkage_scroll[0x00]: text layer horizontal scroll
	lkage_scroll[0x01]: text layer vertical scroll
	lkage_scroll[0x02]: foreground layer horizontal scroll
	lkage_scroll[0x03]: foreground layer vertical scroll
	lkage_scroll[0x04]: background layer horizontal scroll
	lkage_scroll[0x05]: background layer vertical scroll

	lkage_vreg[0]:
		0x04: fg tile bank select

	lkage_vreg[1]:
		0xf0: background, foreground palette select
		0x08: bg tile bank select
		0x07: text layer palette select (is it?)

	lkage_vreg[2]:
		0x03: flip screen x/y
		0xf0: normally 1111, but 1001 and 0001 inbetween stages (while the
		backgrounds are are being redrawn). These bits are probably used to enable
		individual layers, but we have no way of knowing the mapping.
*/

struct tilemap *bg_tilemap, *fg_tilemap, *tx_tilemap;

WRITE_HANDLER( lkage_videoram_w )
{
	if( videoram[offset]!=data )
	{
		videoram[offset] = data;

		switch( offset/0x400 )
		{
			case 0:
			tilemap_mark_tile_dirty(tx_tilemap,offset & 0x3ff);
			break;

			case 1:
			tilemap_mark_tile_dirty(fg_tilemap,offset & 0x3ff);
			break;

			case 2:
			tilemap_mark_tile_dirty(bg_tilemap,offset & 0x3ff);
			break;
		}
	}
}

static void get_bg_tile_info(int tile_index)
{
	SET_TILE_INFO(0,videoram[tile_index + 0x800] + 256 * (bg_tile_bank?5:1),0);
}

static void get_fg_tile_info(int tile_index)
{
	SET_TILE_INFO(0,videoram[tile_index + 0x400] + 256 * (fg_tile_bank?1:0),1);
}

static void get_tx_tile_info(int tile_index)
{
	SET_TILE_INFO(0,videoram[tile_index],2);
}

int lkage_vh_start(void)
{
	bg_tile_bank = fg_tile_bank = 0;

	bg_tilemap = tilemap_create(get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,     8,8,32,32);
	fg_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,32,32);
	tx_tilemap = tilemap_create(get_tx_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,32,32);

	if (!bg_tilemap || !fg_tilemap || !tx_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 0;
	tx_tilemap->transparent_pen = 0;

	tilemap_set_scrolldx(tx_tilemap,-9,15);
	tilemap_set_scrolldx(fg_tilemap,-15,13);
	tilemap_set_scrolldx(bg_tilemap,-13,19);

	return 0;
}

static void draw_sprites( struct osd_bitmap *bitmap, int priority )
{
	const struct rectangle *clip = &Machine->visible_area;
	const unsigned char *finish = spriteram;
	const unsigned char *source = spriteram+0x60-4;
	const struct GfxElement *gfx = Machine->gfx[1];

	while( source>=finish )
	{
		int attributes = source[2];
		/*
			bit 0: horizontal flip
			bit 1: vertical flip
			bit 2: bank select
			bit 3: sprite size
			bit 4..6: color
			bit 7: priority
		*/

		if( (attributes>>7) == priority )
		{
			int y;
			int color = (attributes>>4)&7;
			int flipx = attributes&0x01;
			int flipy = attributes&0x02;
			int height = (attributes&0x08) ? 2 : 1;
			int sx = source[0];
			int sy = 256 -16*height -source[1];
			int sprite_number = source[3] + ((attributes & 0x04) << 6);

			if (flip_screen_x)
			{
				sx = 240 - sx - 6;
				flipx = !flipx;
			}
			else
				sx -= 23;
			sx = ((sx + 8) & 0xff) - 8;
			if (flip_screen_y)
			{
				sy = 256 - 16*height - sy;
				flipy = !flipy;
			}
			sy -= 1;

			if (height == 2 && !flipy) sprite_number ^= 1;

			for (y = 0;y < height;y++)
			{
				drawgfx( bitmap,gfx,
						sprite_number ^ y,
						color,
						flipx,flipy,
						sx,sy + 16*y,
						clip,
						TRANSPARENCY_PEN,0 );
			}
		}
		source-=4;
	}
}

void lkage_set_palette_row( int virtual_row, int logical_row, int len )
{
	unsigned char *source = &paletteram[logical_row*32];
	int indx = virtual_row*16;
	while( len-- )
	{
		unsigned char greenblue = *source++;
		unsigned char red = *source++;
		palette_change_color( indx++,
			(red&0xf)*0x11,
			(greenblue>>4)*0x11,
			(greenblue&0xf)*0x11
		);
	}
}

void lkage_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	flip_screen_x_w(0,~lkage_vreg[2] & 0x01);
	flip_screen_y_w(0,~lkage_vreg[2] & 0x02);

	if( bg_tile_bank != (lkage_vreg[1]&0x08) )
	{
		bg_tile_bank = lkage_vreg[1]&0x08;
		tilemap_mark_all_tiles_dirty( bg_tilemap );
	}

	if( fg_tile_bank != (lkage_vreg[0]&0x04) )
	{
		fg_tile_bank = lkage_vreg[0]&0x04;
		tilemap_mark_all_tiles_dirty( fg_tilemap );
	}

	{
		lkage_set_palette_row( 0x0, 0x00, 16*8 ); /* sprite colors */
		lkage_set_palette_row( 0x8, 0x30 + (lkage_vreg[1]>>4),16 ); /* bg colors */
		lkage_set_palette_row( 0x9, 0x20 + (lkage_vreg[1]>>4),16 ); /* fg colors */
		lkage_set_palette_row( 0xa, 0x11, 16 ); /* text colors */
	}

	tilemap_set_scrollx(tx_tilemap,0,lkage_scroll[0]);
	tilemap_set_scrolly(tx_tilemap,0,lkage_scroll[1]);
	tilemap_set_scrollx(fg_tilemap,0,lkage_scroll[2]);
	tilemap_set_scrolly(fg_tilemap,0,lkage_scroll[3]);
	tilemap_set_scrollx(bg_tilemap,0,lkage_scroll[4]);
	tilemap_set_scrolly(bg_tilemap,0,lkage_scroll[5]);

	tilemap_update( ALL_TILEMAPS );
	if (palette_recalc())  tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
	tilemap_render( ALL_TILEMAPS );

	if ((lkage_vreg[2] & 0xf0) == 0xf0)
	{
		tilemap_draw( bitmap,bg_tilemap,0 );
		draw_sprites( bitmap, 1 );
		tilemap_draw( bitmap,fg_tilemap,0 );
		draw_sprites( bitmap, 0 );
		tilemap_draw( bitmap,tx_tilemap,0 );
	}
	else
	{
		tilemap_draw( bitmap,tx_tilemap,TILEMAP_IGNORE_TRANSPARENCY );
	}
}
