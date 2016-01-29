/***************************************************************************

  Video Hardware for Double Dragon 3

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "tilemap.h"

unsigned char *ddragon3_bg_videoram;
static UINT16 ddragon3_bg_scrollx;
static UINT16 ddragon3_bg_scrolly;

static UINT16 ddragon3_bg_tilebase;
static UINT16 old_ddragon3_bg_tilebase;

unsigned char *ddragon3_fg_videoram;
static UINT16 ddragon3_fg_scrollx;
static UINT16 ddragon3_fg_scrolly;
UINT16 ddragon3_vreg;

static struct tilemap *background, *foreground;

/* scroll write function */
WRITE_HANDLER( ddragon3_scroll_w ){
	switch (offset) {
		case 0x0: /* Scroll X, BG1 */
		ddragon3_fg_scrollx = data;
		return;

		case 0x2: /* Scroll Y, BG1 */
		ddragon3_fg_scrolly = data;
		return;

		case 0x4: /* Scroll X, BG0 */
		ddragon3_bg_scrollx = data;
		return;

		case 0x6: /* Scroll Y, BG0 */
		ddragon3_bg_scrolly = data;
		return;

		case 0xc: /* BG Tile Base */
		ddragon3_bg_tilebase = COMBINE_WORD(ddragon3_bg_tilebase, data)&0x1ff;
		return;

		default:  /* Unknown */
		//logerror("OUTPUT c00[%02x] %02x \n", offset,data);
		break;
	}
}

/* background */
static void get_bg_tile_info(int tile_index)
{
	UINT16 data = ((UINT16 *)ddragon3_bg_videoram)[tile_index];
	SET_TILE_INFO( 0, (data&0xfff) | ((ddragon3_bg_tilebase&1)<<12), ((data&0xf000)>>12)+16 );  // GFX,NUMBER,COLOR
}

WRITE_HANDLER( ddragon3_bg_videoram_w )
{
	int oldword = READ_WORD(&ddragon3_bg_videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	if( oldword != newword )
	{
		WRITE_WORD(&ddragon3_bg_videoram[offset],newword);
		offset = offset/2;
		tilemap_mark_tile_dirty(background,offset);
	}
}

READ_HANDLER( ddragon3_bg_videoram_r )
{
	return READ_WORD( &ddragon3_bg_videoram[offset] );
}

/* foreground */
static void get_fg_tile_info(int tile_index)
{
	UINT16 data0 = ((UINT16 *)ddragon3_fg_videoram)[2*tile_index];
	UINT16 data1 = ((UINT16 *)ddragon3_fg_videoram)[2*tile_index+1];
	SET_TILE_INFO( 0, data1&0x1fff , data0&0xf );  // GFX,NUMBER,COLOR
        tile_info.flags = ((data0&0x40) >> 6);  // FLIPX
}

WRITE_HANDLER( ddragon3_fg_videoram_w )
{
	int oldword = READ_WORD(&ddragon3_fg_videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	if( oldword != newword )
	{
		WRITE_WORD(&ddragon3_fg_videoram[offset],newword);
		offset = offset/4;
		tilemap_mark_tile_dirty(foreground,offset);
	}
}

READ_HANDLER( ddragon3_fg_videoram_r )
{
	return READ_WORD( &ddragon3_fg_videoram[offset] );
}

/* start & stop */
int ddragon3_vh_start(void){
	ddragon3_bg_tilebase = 0;
	old_ddragon3_bg_tilebase = -1;

	background = tilemap_create(get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,     16,16,32,32);
	foreground = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);

	if (!background || !foreground)
		return 1;

	foreground->transparent_pen = 0;
	return 0;
}

/*
 * Sprite Format
 * ----------------------------------
 *
 * Word | Bit(s)           | Use
 * -----+-fedcba9876543210-+----------------
 *   0	| --------xxxxxxxx | ypos (signed)
 * -----+------------------+
 *   1	| --------xxx----- | height
 *   1  | -----------xx--- | yflip, xflip
 *   1  | -------------x-- | msb x
 *   1  | --------------x- | msb y?
 *   1  | ---------------x | enable
 * -----+------------------+
 *   2  | --------xxxxxxxx | tile number
 * -----+------------------+
 *   3  | --------xxxxxxxx | bank
 * -----+------------------+
 *   4  | ------------xxxx |color
 * -----+------------------+
 *   5  | --------xxxxxxxx | xpos
 * -----+------------------+
 *   6,7| unused
 */

static void draw_sprites( struct osd_bitmap *bitmap ){
	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[1];
	UINT16 *source = (UINT16 *)spriteram;
	UINT16 *finish = source+0x800;

	while( source<finish ){
		UINT16 attributes = source[1];
		if( attributes&0x01 ){ /* enable */
			int flipx = attributes&0x10;
			int flipy = attributes&0x08;
			int height = (attributes>>5)&0x7;

			int sy = source[0]&0xff;
			int sx = source[5]&0xff;
			UINT16 tile_number = source[2]&0xff;
			UINT16 color = source[4]&0xf;
			int bank = source[3]&0xff;
			int i;

			if (attributes&0x04) sx|=0x100;
			if (attributes&0x02) sy=239+(0x100-sy); else sy=240-sy;
			if (sx>0x17f) sx=0-(0x200-sx);

			tile_number += (bank*256);

			for( i=0; i<=height; i++ ){
				int tile_index = tile_number + i;

				drawgfx(bitmap,gfx,
					tile_index,
					color,
					flipx,flipy,
					sx,sy-i*16,
					clip,TRANSPARENCY_PEN,0);
			}
		}
		source+=8;
	}
}

static void mark_sprite_colors( void )
{
	int offs,color,i,pal_base,sprite,multi,attr;
	int colmask[16];
    unsigned int *pen_usage; /* Save some struct derefs */

	/* Sprites */
	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;
	pen_usage=Machine->gfx[1]->pen_usage;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0;offs < 0x1000;offs += 16)
	{
		attr = READ_WORD (&spriteram[offs+2]);
		if (!(attr&1)) continue;

		multi = (attr>>5)&0x7;
		sprite = READ_WORD (&spriteram[offs+4]) & 0xff;
		sprite += ((READ_WORD (&spriteram[offs+6]) & 0xff)<<8);
		color = READ_WORD (&spriteram[offs+8]) & 0xf;

		while (multi >= 0)
		{
			colmask[color] |= pen_usage[sprite + multi];
			multi--;
		}
	}

	for (color = 0;color < 16;color++)
	{
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}
}

void ddragon3_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	if( ddragon3_bg_tilebase != old_ddragon3_bg_tilebase )
	{
		old_ddragon3_bg_tilebase = ddragon3_bg_tilebase;
		tilemap_mark_all_tiles_dirty( background );
	}

	tilemap_set_scrolly( background, 0, ddragon3_bg_scrolly );
	tilemap_set_scrollx( background, 0, ddragon3_bg_scrollx );

	tilemap_set_scrolly( foreground, 0, ddragon3_fg_scrolly );
	tilemap_set_scrollx( foreground, 0, ddragon3_fg_scrollx );

	tilemap_update( background );
	tilemap_update( foreground );

	palette_init_used_colors();
	mark_sprite_colors();
	if( palette_recalc() ) tilemap_mark_all_pixels_dirty( ALL_TILEMAPS );

	tilemap_render( background );
	tilemap_render( foreground );

	if (ddragon3_vreg&0x40) {
		tilemap_draw( bitmap, background, 0 );
		tilemap_draw( bitmap, foreground, 0 );
		draw_sprites( bitmap );
	}
	else {
		tilemap_draw( bitmap, background, 0 );
		draw_sprites( bitmap );
		tilemap_draw( bitmap, foreground, 0 );
	}
}

void ctribe_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	if( ddragon3_bg_tilebase != old_ddragon3_bg_tilebase )
	{
		old_ddragon3_bg_tilebase = ddragon3_bg_tilebase;
		tilemap_mark_all_tiles_dirty( background );
	}

	tilemap_set_scrolly( background, 0, ddragon3_bg_scrolly );
	tilemap_set_scrollx( background, 0, ddragon3_bg_scrollx );
	tilemap_set_scrolly( foreground, 0, ddragon3_fg_scrolly );
	tilemap_set_scrollx( foreground, 0, ddragon3_fg_scrollx );

	tilemap_update( background );
	tilemap_update( foreground );

	palette_init_used_colors();
	mark_sprite_colors();
	if( palette_recalc() ) tilemap_mark_all_pixels_dirty( ALL_TILEMAPS );

	tilemap_render( background );
	tilemap_render( foreground );

	tilemap_draw( bitmap, background, 0 );
	tilemap_draw( bitmap, foreground, 0 );
	draw_sprites( bitmap );
}
