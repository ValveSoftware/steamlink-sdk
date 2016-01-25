#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

static int flipscreen, sprite_flip_adjust;
static struct tilemap *fg_tilemap, *bg_tilemap, *tx_tilemap;
static unsigned char bg_color, fg_color, old_bg_color, old_fg_color;

/***************************************************************************
**
**  Palette Handling:
**
**  Each color entry is encoded by 12 bits in color proms
**
**  There are sixteen 8-color sprite palettes
**  sprite palette is selected by a nibble of spriteram
**
**  There are eight 16-color text layer character palettes
**  character palette is determined by character_number
**
**  Background and Foreground tilemap layers each have eight 16-color
**  palettes.  A palette bank select register is associated with the whole
**  layer.
**
***************************************************************************/

WRITE_HANDLER( marvins_palette_bank_w )
{
	bg_color = data>>4;
	fg_color = data&0xf;
}

static void stuff_palette( int source_index, int dest_index, int num_colors )
{
	unsigned char *color_prom = memory_region(REGION_PROMS) + source_index;
	int i;
	for( i=0; i<num_colors; i++ )
	{
		int bit0=0,bit1,bit2,bit3;
		int red, green, blue;

		bit0 = (color_prom[0x800] >> 2) & 0x01; // ?
		bit1 = (color_prom[0x000] >> 1) & 0x01;
		bit2 = (color_prom[0x000] >> 2) & 0x01;
		bit3 = (color_prom[0x000] >> 3) & 0x01;
		red = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (color_prom[0x800] >> 1) & 0x01; // ?
		bit1 = (color_prom[0x400] >> 2) & 0x01;
		bit2 = (color_prom[0x400] >> 3) & 0x01;
		bit3 = (color_prom[0x000] >> 0) & 0x01;
		green = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (color_prom[0x800] >> 0) & 0x01; // ?
		bit1 = (color_prom[0x800] >> 3) & 0x01; // ?
		bit2 = (color_prom[0x400] >> 0) & 0x01;
		bit3 = (color_prom[0x400] >> 1) & 0x01;
		blue = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		palette_change_color( dest_index++, red, green, blue );
		color_prom++;
	}
}

static void update_palette( int type )
{
	if( bg_color!=old_bg_color )
	{
		stuff_palette( 256+16*(bg_color&0x7), (0x11-type)*16, 16 );
		old_bg_color = bg_color;
	}

	if( fg_color!=old_fg_color )
	{
		stuff_palette( 128+16*(fg_color&0x7), (0x10+type)*16, 16 );
		old_fg_color = fg_color;
	}
}

/***************************************************************************
**
**  Memory Handlers
**
***************************************************************************/

WRITE_HANDLER( marvins_spriteram_w )
{
	spriteram[offset] = data;
}
READ_HANDLER( marvins_spriteram_r )
{
	return spriteram[offset];
}

READ_HANDLER( marvins_foreground_ram_r )
{
	return videoram[offset+0x1000];
}
WRITE_HANDLER( marvins_foreground_ram_w )
{
	if( offset<0x800 )
	{
		if( videoram[offset+0x1000]==data ) return;
		tilemap_mark_tile_dirty(fg_tilemap,offset);
	}
	videoram[offset+0x1000] = data;
}

READ_HANDLER( marvins_background_ram_r )
{
	return videoram[offset];
}
WRITE_HANDLER( marvins_background_ram_w )
{
	if( offset<0x800 )
	{
		if( videoram[offset]==data ) return;
		tilemap_mark_tile_dirty(bg_tilemap,offset);
	}
	videoram[offset] = data;
}

READ_HANDLER( marvins_text_ram_r )
{
	return videoram[offset+0x2000];
}
WRITE_HANDLER( marvins_text_ram_w )
{
	if( offset<0x400 )
	{
		if( videoram[offset+0x2000]==data ) return;
		tilemap_mark_tile_dirty(tx_tilemap,offset);
	}
	videoram[offset+0x2000] = data;
}

/***************************************************************************
**
**  Callbacks for Tilemap Manager
**
***************************************************************************/

static void get_bg_tilemap_info(int tile_index)
{
	SET_TILE_INFO(2,videoram[tile_index],0);
}

static void get_fg_tilemap_info(int tile_index)
{
	SET_TILE_INFO(1,videoram[tile_index+0x1000],0);
}

static void get_tx_tilemap_info(int tile_index)
{
	int tile_number = videoram[tile_index+0x2000];
	SET_TILE_INFO(0,tile_number,(tile_number>>5));
}

/***************************************************************************
**
**  Video Initialization
**
***************************************************************************/

int marvins_vh_start( void )
{
	flipscreen = -1; old_bg_color = old_fg_color = -1;

	stuff_palette( 0, 0, 16*8 ); /* load sprite colors */
	stuff_palette( 16*8*3, 16*8, 16*8 ); /* load text colors */

	fg_tilemap = tilemap_create(get_fg_tilemap_info,tilemap_scan_cols,TILEMAP_TRANSPARENT,8,8,64,32);
	bg_tilemap = tilemap_create(get_bg_tilemap_info,tilemap_scan_cols,TILEMAP_TRANSPARENT,8,8,64,32);
	tx_tilemap = tilemap_create(get_tx_tilemap_info,tilemap_scan_cols,TILEMAP_TRANSPARENT,8,8,32,32);

	if (!fg_tilemap || !bg_tilemap || !tx_tilemap)
		return 1;

	{
		struct rectangle clip = Machine->visible_area;
		clip.max_x-=16;
		clip.min_x+=16;
		tilemap_set_clip( fg_tilemap, &clip );
		tilemap_set_clip( bg_tilemap, &clip );
		tilemap_set_clip( tx_tilemap, &clip );

		fg_tilemap->transparent_pen = 0xf;
		bg_tilemap->transparent_pen = 0xf;
		tx_tilemap->transparent_pen = 0xf;

		if( strcmp(Machine->gamedrv->name,"marvins")==0 )
		{
			tilemap_set_scrolldx( bg_tilemap,   271, 287 );
			tilemap_set_scrolldx( fg_tilemap,   15, 13+18 );
			sprite_flip_adjust = 256+182+1;
		}
		else
		{
			tilemap_set_scrolldx( bg_tilemap,   -16, -10 );
			tilemap_set_scrolldx( fg_tilemap,   16, 22 );
			sprite_flip_adjust = 256+182;
		}

		tilemap_set_scrolldx( tx_tilemap, 16, 16 );
		tilemap_set_scrolldy( bg_tilemap, 0, -40 );
		tilemap_set_scrolldy( fg_tilemap, 0, -40 );
		tilemap_set_scrolldy( tx_tilemap, 0, 0 );

		return 0;
	}
}

/***************************************************************************
**
**  Screen Refresh
**
***************************************************************************/

static void draw_status( struct osd_bitmap *bitmap )
{
	const unsigned char *base = videoram+0x2400;
	struct rectangle clip = Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[0];
	int row;
	for( row=0; row<4; row++ )
	{
		int sy,sx = (row&1)*8;
		const unsigned char *source = base + (row&1)*32;
		if( row>1 )
		{
			sx+=256+16;
		}
		else
		{
			source+=30*32;
		}

		for( sy=0; sy<256; sy+=8 )
		{
			int tile_number = *source++;
			drawgfx( bitmap, gfx,
			    tile_number, tile_number>>5,
			    0,0, /* no flip */
			    sx,sy,
			    &clip,
			    TRANSPARENCY_NONE, 0xf );
		}
	}
}

static void draw_sprites( struct osd_bitmap *bitmap, int scrollx, int scrolly,
		int priority, unsigned char sprite_partition )
{
	const struct GfxElement *gfx = Machine->gfx[3];
	struct rectangle clip = Machine->visible_area;
	const unsigned char *source, *finish;

	if( sprite_partition>0x64 ) sprite_partition = 0x64;

	if( priority )
	{
		source = spriteram + sprite_partition;
		finish = spriteram + 0x64;
	}
	else
	{
		source = spriteram;
		finish = spriteram + sprite_partition;
	}

	while( source<finish )
	{
		int attributes = source[3]; /* Y?F? CCCC */
		int tile_number = source[1];
		int sy = (-16+source[0] - scrolly)&0xff;
		int sx = source[2] - scrollx + ((attributes&0x80)?256:0);
		int color = attributes&0xf;
		int flipy = (attributes&0x20);
		int flipx = 0;

		if( flipscreen )
		{
			if( flipy )
			{
			    flipx = 1; flipy = 0;
			}
			else
			{
			    flipx = flipy = 1;
			}
			sx = sprite_flip_adjust-sx;
			sy = 246-sy;
		}

		if( sy>240 ) sy -= 256;

		drawgfx( bitmap,gfx,
			tile_number,
			color,
			flipx, flipy,
			(256-sx)&0x1ff,sy,
			&clip,TRANSPARENCY_PEN,7);

		source+=4;
	}
}

void marvins_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh )
{
	unsigned char *mem = memory_region(REGION_CPU1);

	unsigned char sprite_partition = mem[0xfe00];

	int attributes = mem[0x8600]; /* 0x20: normal, 0xa0: video flipped */
	int scroll_attributes = mem[0xff00];
	int sprite_scrolly = mem[0xf800];
	int sprite_scrollx = mem[0xf900];

	int bg_scrolly = mem[0xfa00];
	int bg_scrollx = mem[0xfb00];
	int fg_scrolly = mem[0xfc00];
	int fg_scrollx = mem[0xfd00];

	if( (scroll_attributes & 4)==0 ) bg_scrollx += 256;
	if( scroll_attributes & 1 ) sprite_scrollx += 256;
	if( scroll_attributes & 2 ) fg_scrollx += 256;

	/* palette bank for background/foreground is set by a memory-write handler */
	update_palette(0);

	if( flipscreen != (attributes&0x80) )
	{
		flipscreen = attributes&0x80;
		tilemap_set_flip( ALL_TILEMAPS, flipscreen?TILEMAP_FLIPY|TILEMAP_FLIPX:0);
	}

	tilemap_set_scrollx( bg_tilemap,    0, bg_scrollx );
	tilemap_set_scrolly( bg_tilemap,    0, bg_scrolly );
	tilemap_set_scrollx( fg_tilemap,    0, fg_scrollx );
	tilemap_set_scrolly( fg_tilemap,    0, fg_scrolly );
	tilemap_set_scrollx( tx_tilemap,  0, 0 );
	tilemap_set_scrolly( tx_tilemap,  0, 0 );

	tilemap_update( ALL_TILEMAPS );
	if( palette_recalc() ) tilemap_mark_all_pixels_dirty( ALL_TILEMAPS );
	tilemap_render( ALL_TILEMAPS );

	tilemap_draw( bitmap,fg_tilemap,TILEMAP_IGNORE_TRANSPARENCY );
	draw_sprites( bitmap, sprite_scrollx+29+1, sprite_scrolly, 0, sprite_partition );
	tilemap_draw( bitmap,bg_tilemap,0 );
	draw_sprites( bitmap, sprite_scrollx+29+1, sprite_scrolly, 1, sprite_partition );
	tilemap_draw( bitmap,tx_tilemap,0 );
	draw_status( bitmap );
}

void madcrash_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh )
{
	extern int madcrash_vreg;
	unsigned char *mem = memory_region(REGION_CPU1)+madcrash_vreg;

	int attributes = mem[0x8600]; /* 0x20: normal, 0xa0: video flipped */
	int bg_scrolly = mem[0xf800];
	int bg_scrollx = mem[0xf900];
	int scroll_attributes = mem[0xfb00];
	int sprite_scrolly = mem[0xfc00];
	int sprite_scrollx = mem[0xfd00];
	int fg_scrolly = mem[0xfe00];
	int fg_scrollx = mem[0xff00];
	if( (scroll_attributes & 4)==0 ) bg_scrollx += 256;
	if( scroll_attributes & 1 ) sprite_scrollx += 256;
	if( scroll_attributes & 2 ) fg_scrollx += 256;

	marvins_palette_bank_w( 0, mem[0xc800] );
	update_palette(1);

	if( flipscreen != (attributes&0x80) )
	{
		flipscreen = attributes&0x80;
		tilemap_set_flip( ALL_TILEMAPS, flipscreen?TILEMAP_FLIPY|TILEMAP_FLIPX:0);
	}

	tilemap_set_scrollx( bg_tilemap, 0, bg_scrollx );
	tilemap_set_scrolly( bg_tilemap, 0, bg_scrolly );
	tilemap_set_scrollx( fg_tilemap, 0, fg_scrollx );
	tilemap_set_scrolly( fg_tilemap, 0, fg_scrolly );
	tilemap_set_scrollx( tx_tilemap,  0, 0 );
	tilemap_set_scrolly( tx_tilemap,  0, 0 );

	tilemap_update( ALL_TILEMAPS );
	if( palette_recalc() ) tilemap_mark_all_pixels_dirty( ALL_TILEMAPS );
	tilemap_render( ALL_TILEMAPS );

	tilemap_draw( bitmap,bg_tilemap,TILEMAP_IGNORE_TRANSPARENCY );
	tilemap_draw( bitmap,fg_tilemap,0 );
	draw_sprites( bitmap, sprite_scrollx+29, sprite_scrolly+1, 1, 0 );

	tilemap_draw( bitmap,tx_tilemap,0 );
	draw_status( bitmap );
}
