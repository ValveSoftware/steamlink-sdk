/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"

// Real stuff
unsigned char *stfight_text_char_ram;
unsigned char *stfight_text_attr_ram;
unsigned char *stfight_vh_latch_ram;
unsigned char *stfight_sprite_ram;

static struct tilemap *fg_tilemap,*bg_tilemap,*tx_tilemap;
static int stfight_sprite_base = 0;

/*
		Graphics ROM Format
		===================

		Each tile is 8x8 pixels
		Each composite tile is 2x2 tiles, 16x16 pixels
		Each screen is 32x32 composite tiles, 64x64 tiles, 256x256 pixels
		Each layer is a 4-plane bitmap 8x16 screens, 2048x4096 pixels

		There are 4x256=1024 composite tiles defined for each layer

		Each layer is mapped using 2 bytes/composite tile
		- one byte for the tile
		- one byte for the tile bank, attribute
			- b7,b5     tile bank (0-3)

		Each pixel is 4 bits = 16 colours.

 */

void stfight_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	/* unique color for transparency */
	palette[256*3+0] = 0x04;
	palette[256*3+1] = 0x04;
	palette[256*3+2] = 0x04;

	/* text uses colors 192-207 */
	for (i = 0;i < TOTAL_COLORS(0);i++)
	{
		if ((*color_prom & 0x0f) == 0x0f) COLOR(0,i) = 256;	/* transparent */
		else COLOR(0,i) = (*color_prom & 0x0f) + 0xc0;
		color_prom++;
	}
	color_prom += 256 - TOTAL_COLORS(0);	/* rest of the PROM is unused */

	/* fg uses colors 64-127 */
	for (i = 0;i < TOTAL_COLORS(1);i++)
	{
		COLOR(1,i) = (color_prom[256] & 0x0f) + 16 * (color_prom[0] & 0x03) + 0x40;
		color_prom++;
	}
	color_prom += 256;

	/* bg uses colors 0-63 */
	for (i = 0;i < TOTAL_COLORS(2);i++)
	{
		COLOR(2,i) = (color_prom[256] & 0x0f) + 16 * (color_prom[0] & 0x03) + 0x00;
		color_prom++;
	}
	color_prom += 256;

	/* sprites use colors 128-191 */
	for (i = 0;i < TOTAL_COLORS(4);i++)
	{
		COLOR(4,i) = (color_prom[256] & 0x0f) + 16 * (color_prom[0] & 0x03) + 0x80;
		color_prom++;
	}
	color_prom += 256;
}

/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static UINT32 fg_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return (col & 0x0f) + ((row & 0x0f) << 4) + ((col & 0x70) << 4) + ((row & 0xf0) << 7);
}

static void get_fg_tile_info(int tile_index)
{
	unsigned char   *fgMap = memory_region(REGION_GFX5);
	int attr,tile_base;

	attr = fgMap[0x8000+tile_index];
	tile_base = ((attr & 0x80) << 2) | ((attr & 0x20) << 3);

	SET_TILE_INFO(1,tile_base + fgMap[tile_index],attr & 0x07);
}

static UINT32 bg_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return ((col & 0x0e) >> 1) + ((row & 0x0f) << 3) + ((col & 0x70) << 3) +
			((row & 0x80) << 3) + ((row & 0x10) << 7) + ((col & 0x01) << 12) +
			((row & 0x60) << 8);
}

static void get_bg_tile_info(int tile_index)
{
	unsigned char   *bgMap = memory_region(REGION_GFX6);
	int attr,tile_bank,tile_base;

	attr = bgMap[0x8000+tile_index];
	tile_bank = (attr & 0x20) >> 5;
	tile_base = (attr & 0x80) << 1;

	SET_TILE_INFO(2+tile_bank,tile_base + bgMap[tile_index],attr & 0x07);
}

static void get_tx_tile_info(int tile_index)
{
	unsigned char attr = stfight_text_attr_ram[tile_index];

	SET_TILE_INFO(0,stfight_text_char_ram[tile_index] + ((attr & 0x80) << 1),attr & 0x0f);
	tile_info.flags = TILE_FLIPYX((attr & 0x60) >> 5);
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int stfight_vh_start(void)
{
	bg_tilemap = tilemap_create(get_bg_tile_info,bg_scan,TILEMAP_OPAQUE,     16,16,128,256);
	fg_tilemap = tilemap_create(get_fg_tile_info,fg_scan,TILEMAP_TRANSPARENT,16,16,128,256);
	tx_tilemap = tilemap_create(get_tx_tile_info,tilemap_scan_rows,
			TILEMAP_TRANSPARENT_COLOR,8,8,32,32);

	if (!fg_tilemap || !bg_tilemap || !tx_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 0x0F;
	tx_tilemap->transparent_pen = 256;

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( stfight_text_char_w )
{
	if (stfight_text_char_ram[offset] != data)
	{
		stfight_text_char_ram[offset] = data;
		tilemap_mark_tile_dirty(tx_tilemap,offset);
	}
}

WRITE_HANDLER( stfight_text_attr_w )
{
	if (stfight_text_attr_ram[offset] != data)
	{
		stfight_text_attr_ram[offset] = data;
		tilemap_mark_tile_dirty(tx_tilemap,offset);
	}
}

WRITE_HANDLER( stfight_sprite_bank_w )
{
	stfight_sprite_base = ( ( data & 0x04 ) << 7 ) |
				          ( ( data & 0x01 ) << 8 );
}

WRITE_HANDLER( stfight_vh_latch_w )
{
	int scroll;


	stfight_vh_latch_ram[offset] = data;

	switch( offset )
	{
		case 0x00:
		case 0x01:
			scroll = (stfight_vh_latch_ram[1] << 8) | stfight_vh_latch_ram[0];
			tilemap_set_scrollx(fg_tilemap,0,scroll);
			break;

		case 0x02:
		case 0x03:
			scroll = (stfight_vh_latch_ram[3] << 8) | stfight_vh_latch_ram[2];
			tilemap_set_scrolly(fg_tilemap,0,scroll);
			break;

		case 0x04:
		case 0x05:
			scroll = (stfight_vh_latch_ram[5] << 8) | stfight_vh_latch_ram[4];
			tilemap_set_scrollx(bg_tilemap,0,scroll);
			break;

		case 0x06:
		case 0x08:
			scroll = (stfight_vh_latch_ram[8] << 8) | stfight_vh_latch_ram[6];
			tilemap_set_scrolly(bg_tilemap,0,scroll);
			break;

		case 0x07:
			tilemap_set_enable(tx_tilemap,data & 0x80);
			/* 0x40 = sprites */
			tilemap_set_enable(bg_tilemap,data & 0x20);
			tilemap_set_enable(fg_tilemap,data & 0x10);
			flip_screen_w(0,data & 0x01);
			break;
	}
}

/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs,sx,sy;

	for (offs = 0;offs < 4096;offs += 32)
	{
		int code;
		int attr = stfight_sprite_ram[offs+1];
		int flipx = attr & 0x10;
		int color = attr & 0x0f;
		int pri = (attr & 0x20) >> 5;

		sy = stfight_sprite_ram[offs+2];
		sx = stfight_sprite_ram[offs+3];

		// non-active sprites have zero y coordinate value
		if( sy > 0 )
		{
			// sprites which wrap onto/off the screen have
			// a sign extension bit in the sprite attribute
			if( sx >= 0xf0 )
			{
				if (attr & 0x80)
				    sx -= 0x100;
			}

			if (flip_screen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
			}

			code = stfight_sprite_base + stfight_sprite_ram[offs];

			pdrawgfx(bitmap,Machine->gfx[4],
				     code,
					 color,
					 flipx,flip_screen,
					 sx,sy,
				     &Machine->visible_area,TRANSPARENCY_PEN,0x0f,
					 pri ? 0x02 : 0);
		}
	}
}


void stfight_vh_screenrefresh( struct osd_bitmap *bitmap,int full_refresh )
{
	tilemap_update(ALL_TILEMAPS);

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);


	fillbitmap(priority_bitmap,0,NULL);

	if (bg_tilemap->enable)
	    tilemap_draw(bitmap,bg_tilemap,0);
	else
		fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);

	tilemap_draw(bitmap,fg_tilemap,1<<16);

	/* Draw sprites (may be obscured by foreground layer) */
	if (stfight_vh_latch_ram[0x07] & 0x40)
		draw_sprites(bitmap);

	tilemap_draw(bitmap,tx_tilemap,0);
}
