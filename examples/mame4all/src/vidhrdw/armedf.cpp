/***************************************************************************

  Video Hardware for Armed Formation and Terra Force and Kodure Ookami

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int scroll_type;

UINT16 armedf_vreg;

unsigned char *armedf_bg_videoram;
UINT16 armedf_bg_scrollx;
UINT16 armedf_bg_scrolly;

unsigned char *armedf_fg_videoram;
UINT16 armedf_fg_scrollx;
UINT16 armedf_fg_scrolly;

UINT16 terraf_scroll_msb;

static struct tilemap *background, *foreground, *text_layer;

/******************************************************************/

static UINT32 armedf_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return 32*col+row + 0x80;
}

static void get_tx_tile_info(int tile_index)
{
	UINT16 *source = (UINT16 *)videoram;
	unsigned char attributes = source[tile_index+0x800]&0xff;
	int tile_number = (source[tile_index]&0xff) + 256*(attributes&3);
	int color = attributes>>4;
	SET_TILE_INFO( 0, tile_number, color );
}

WRITE_HANDLER( armedf_text_videoram_w )
{
	int oldword = READ_WORD(&videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	if( oldword != newword )
	{
		WRITE_WORD(&videoram[offset],newword);
		tilemap_mark_tile_dirty(text_layer,(offset/2) & 0x7ff);
	}
}

READ_HANDLER( armedf_text_videoram_r )
{
	return READ_WORD (&videoram[offset]);
}

READ_HANDLER( terraf_text_videoram_r )
{
	return READ_WORD( &videoram[offset] );
}

WRITE_HANDLER( terraf_text_videoram_w )
{
	int oldword = READ_WORD(&videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	if( oldword != newword )
	{
		WRITE_WORD(&videoram[offset],newword);
		offset = offset/2;
		tilemap_mark_tile_dirty(text_layer,offset & 0xbff);
	}
}

static UINT32 terraf_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	int tile_index = 32*(31-row);
	if( col<3 )
	{
		tile_index += 0x800+col+29;
	}
	else if( col<35 )
	{
		tile_index += (col-3);
	}
	else
	{
		tile_index += 0x800+col-35;
	}
	return tile_index;
}

static void terraf_get_tx_tile_info(int tile_index)
{
	UINT16 *source = (UINT16 *)videoram;
	unsigned char attributes = source[tile_index+0x400]&0xff;
	int tile_number = source[tile_index]&0xff;

	SET_TILE_INFO(0,tile_number + 256 * (attributes & 0x3),attributes >> 4);
}

/******************************************************************/

static void get_fg_tile_info( int tile_index )
{
	UINT16 data = ((UINT16 *)armedf_fg_videoram)[tile_index];
	SET_TILE_INFO( 1, data&0x7ff, data>>11 );
}

WRITE_HANDLER( armedf_fg_videoram_w )
{
	int oldword = READ_WORD(&armedf_fg_videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	if( oldword != newword )
	{
		WRITE_WORD(&armedf_fg_videoram[offset],newword);
		tilemap_mark_tile_dirty( foreground, offset/2 );
	}
}

READ_HANDLER( armedf_fg_videoram_r )
{
	return READ_WORD (&armedf_fg_videoram[offset]);
}

/******************************************************************/

static void get_bg_tile_info( int tile_index )
{
	UINT16 data = ((UINT16 *)armedf_bg_videoram)[tile_index];
	SET_TILE_INFO( 2, data&0x3ff, data>>11 );
}

WRITE_HANDLER( armedf_bg_videoram_w )
{
	int oldword = READ_WORD(&armedf_bg_videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	if( oldword != newword )
	{
		WRITE_WORD(&armedf_bg_videoram[offset],newword);
		tilemap_mark_tile_dirty( background, offset/2 );
	}
}

READ_HANDLER( armedf_bg_videoram_r )
{
	return READ_WORD( &armedf_bg_videoram[offset] );
}

/******************************************************************/

int terraf_vh_start(void)
{
	scroll_type = 0;

	text_layer = tilemap_create(terraf_get_tx_tile_info,terraf_scan,TILEMAP_TRANSPARENT,8,8,38,32);
	background = tilemap_create(get_bg_tile_info,tilemap_scan_cols,TILEMAP_OPAQUE,16,16,64,32);
	foreground = tilemap_create(get_fg_tile_info,tilemap_scan_cols,TILEMAP_TRANSPARENT,16,16,64,32);

	if (!background || !foreground || !text_layer)
		return 1;

	foreground->transparent_pen = 0xf;
	text_layer->transparent_pen = 0xf;

	return 0;
}

int armedf_vh_start(void)
{
	scroll_type = 1;

	text_layer = tilemap_create(get_tx_tile_info,armedf_scan,TILEMAP_TRANSPARENT,8,8,38,32);
	background = tilemap_create(get_bg_tile_info,tilemap_scan_cols,TILEMAP_OPAQUE,16,16,64,32);
	foreground = tilemap_create(get_fg_tile_info,tilemap_scan_cols,TILEMAP_TRANSPARENT,16,16,64,32);

	if (!background || !foreground || !text_layer)
		return 1;

	foreground->transparent_pen = 0xf;
	text_layer->transparent_pen = 0xf;

	return 0;
}

int kodure_vh_start(void)
{
	scroll_type = 2;

	text_layer = tilemap_create(terraf_get_tx_tile_info,terraf_scan,TILEMAP_TRANSPARENT,8,8,38,32);
	background = tilemap_create(get_bg_tile_info,tilemap_scan_cols,TILEMAP_OPAQUE,16,16,64,32);
	foreground = tilemap_create(get_fg_tile_info,tilemap_scan_cols,TILEMAP_TRANSPARENT,16,16,64,32);

	if (!background || !foreground || !text_layer)
		return 1;

	foreground->transparent_pen = 0xf;
	text_layer->transparent_pen = 0xf;

	return 0;
}

void armedf_vh_stop(void)
{
}

static void draw_sprites( struct osd_bitmap *bitmap, int priority )
{
	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[3];

	UINT16 *source = (UINT16 *)spriteram;
	UINT16 *finish = source+512;

	while( source<finish )
	{
		int sy = 128+240-(source[0]&0x1ff);
		int tile_number = source[1]; /* ??YX?TTTTTTTTTTT */

		int color = (source[2]>>8)&0x1f;
		int sx = source[3] - 0x60;

		if( ((source[0]&0x2000)?0:1) == priority )
		{
			drawgfx(bitmap,gfx,
				tile_number,
				color,
 				tile_number&0x2000,tile_number&0x1000, /* flip */
				sx,sy,
				clip,TRANSPARENCY_PEN,0xf);
		}

		source+=4;
	}
}

static void mark_sprite_colors( void )
{
	UINT16 *source = (UINT16 *)spriteram;
	UINT16 *finish = source+512;
	int i;
	char flag[32];

	for( i=0; i<32; i++ ) flag[i] = 0;

	while( source<finish )
	{
		int color = (source[2]>>8)&0x1f;
		flag[color] = 1;
		source+=4;
	}

	{
		unsigned char *pen_ptr = &palette_used_colors[Machine->drv->gfxdecodeinfo[3].color_codes_start];
		int pen;
		for( i = 0; i<32; i++ )
		{
			if( flag[i] )
			{
				for( pen = 0; pen<0xf; pen++ ) pen_ptr[pen] = PALETTE_COLOR_USED;
			}
			pen_ptr += 16;
		}
	}
}

void armedf_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int sprite_enable = armedf_vreg & 0x200;

	tilemap_set_enable( background, armedf_vreg&0x800 );
	tilemap_set_enable( foreground, armedf_vreg&0x400 );
	tilemap_set_enable( text_layer, armedf_vreg&0x100 );

	tilemap_set_scrollx( background, 0, armedf_bg_scrollx+96 );
	tilemap_set_scrolly( background, 0, armedf_bg_scrolly );

	switch (scroll_type)
	{
		case	0:		/* terra force */
			tilemap_set_scrollx( foreground, 0, (armedf_fg_scrolly>>8) + ((terraf_scroll_msb>>12)&3)*256 - 160-256*3);
			tilemap_set_scrolly( foreground, 0, (armedf_fg_scrollx>>8) + ((terraf_scroll_msb>>8)&3)*256 );
			break;
		case	1:		/* armed formation */
		case	2:		/* kodure ookami */
			tilemap_set_scrollx( foreground, 0, armedf_fg_scrollx+96 );
			tilemap_set_scrolly( foreground, 0, armedf_fg_scrolly );
	}

	if (scroll_type == 2)		/* kodure ookami */
	{
		tilemap_set_scrollx( text_layer, 0, -8 );
		tilemap_set_scrolly( text_layer, 0, 0 );
	}

	tilemap_update(  ALL_TILEMAPS  );

	palette_init_used_colors();
	mark_sprite_colors();
	palette_used_colors[0] = PALETTE_COLOR_USED;	/* background */

	if( palette_recalc() ) tilemap_mark_all_pixels_dirty( ALL_TILEMAPS );

	tilemap_render(  ALL_TILEMAPS  );

	if( armedf_vreg & 0x0800 )
		tilemap_draw( bitmap, background, 0 );
	else
		fillbitmap( bitmap, Machine->pens[0], 0 ); /* disabled background - all black? */

	if( sprite_enable ) draw_sprites( bitmap, 0 );
	tilemap_draw( bitmap, foreground, 0 );
	if( sprite_enable ) draw_sprites( bitmap, 1 );
	tilemap_draw( bitmap, text_layer, 0 );
}


static void cclimbr2_draw_sprites( struct osd_bitmap *bitmap, int priority )
{
	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[3];

	UINT16 *source = (UINT16 *)spriteram;
	UINT16 *finish = source+1024;

	while( source<finish )
	{
		int sy = 240-(source[0]&0x1ff);				// ???
		int tile_number = source[1]; /* ??YX?TTTTTTTTTTT */

		int color = (source[2]>>8)&0x1f;
		int sx = source[3] - 0x68;

		if (((source[0] & 0x3000) >> 12) == priority)
		{
			drawgfx(bitmap,gfx,
				tile_number,
				color,
 				tile_number&0x2000,tile_number&0x1000, /* flip */
				sx,sy,
				clip,TRANSPARENCY_PEN,0xf);
		}

		source+=4;
	}
}

void cclimbr2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	unsigned char *RAM;
	int sprite_enable = armedf_vreg & 0x200;

	tilemap_set_enable( background, armedf_vreg&0x800 );
	tilemap_set_enable( foreground, armedf_vreg&0x400 );
	tilemap_set_enable( text_layer, armedf_vreg&0x100 );

	tilemap_set_scrollx( text_layer, 0, 0 );
	tilemap_set_scrolly( text_layer, 0, 0 );

	tilemap_set_scrollx( background, 0, armedf_bg_scrollx+104);
	tilemap_set_scrolly( background, 0, armedf_bg_scrolly );

	RAM = memory_region(REGION_CPU1);
	tilemap_set_scrollx( foreground, 0, READ_WORD(&RAM[0x6123c]) - (160 + 256 * 3)+8);	// ???
	tilemap_set_scrolly( foreground, 0, READ_WORD(&RAM[0x6123e]) - 1);			// ???

	tilemap_update(  ALL_TILEMAPS  );

	palette_init_used_colors();
	mark_sprite_colors();
	palette_used_colors[0] = PALETTE_COLOR_USED;	/* background */

	if( palette_recalc() ) tilemap_mark_all_pixels_dirty( ALL_TILEMAPS );

	tilemap_render(  ALL_TILEMAPS  );

	if( armedf_vreg & 0x0800 )
		tilemap_draw( bitmap, background, 0 );
	else
		fillbitmap( bitmap, Machine->pens[0], 0 ); /* disabled background - all black? */

	if( sprite_enable ) cclimbr2_draw_sprites( bitmap, 2 );
	tilemap_draw( bitmap, foreground, 0 );
	if( sprite_enable ) cclimbr2_draw_sprites( bitmap, 1 );
	tilemap_draw( bitmap, text_layer, 0 );
	if( sprite_enable ) cclimbr2_draw_sprites( bitmap, 0 );
}
