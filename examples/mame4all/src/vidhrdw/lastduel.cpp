/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *lastduel_vram,*lastduel_scroll2,*lastduel_scroll1;
static int scroll[16];

static struct tilemap *bg_tilemap,*fg_tilemap,*tx_tilemap;
static unsigned char *gfx_base;
static int gfx_bank,flipscreen;

WRITE_HANDLER( lastduel_flip_w )
{
	flipscreen=data&1;
	coin_lockout_w(0,~data & 0x10);
	coin_lockout_w(1,~data & 0x20);
}

WRITE_HANDLER( lastduel_scroll_w )
{
	scroll[offset]=data&0xffff;  /* Scroll data */
}

READ_HANDLER( lastduel_scroll1_r )
{
	return READ_WORD(&lastduel_scroll1[offset]);
}

READ_HANDLER( lastduel_scroll2_r )
{
	return READ_WORD(&lastduel_scroll2[offset]);
}

WRITE_HANDLER( lastduel_scroll1_w )
{
	COMBINE_WORD_MEM(&lastduel_scroll1[offset],data);
	tilemap_mark_tile_dirty(fg_tilemap,offset/4);
}

WRITE_HANDLER( lastduel_scroll2_w )
{
	COMBINE_WORD_MEM(&lastduel_scroll2[offset],data);
	tilemap_mark_tile_dirty(bg_tilemap,offset/4);
}

READ_HANDLER( lastduel_vram_r )
{
	return READ_WORD(&lastduel_vram[offset]);
}

WRITE_HANDLER( lastduel_vram_w )
{
 	COMBINE_WORD_MEM(&lastduel_vram[offset],data);
	tilemap_mark_tile_dirty(tx_tilemap,offset/2);
}

WRITE_HANDLER( madgear_scroll1_w )
{
	COMBINE_WORD_MEM(&lastduel_scroll1[offset],data);

	tilemap_mark_tile_dirty(fg_tilemap,(offset & 0xfff)/2);
}

WRITE_HANDLER( madgear_scroll2_w )
{
	COMBINE_WORD_MEM(&lastduel_scroll2[offset],data);

	tilemap_mark_tile_dirty(bg_tilemap,(offset & 0xfff)/2);
}

static void get_ld_tile_info(int tile_index)
{
	int tile=READ_WORD(&gfx_base[4*tile_index])&0x1fff;
	int color=READ_WORD(&gfx_base[4*tile_index+2]);

	SET_TILE_INFO(gfx_bank,tile,color&0xf)
	tile_info.flags = TILE_FLIPYX((color & 0x60)>>5);
	tile_info.priority = (color&0x80)>>7;
}

static void get_tile_info(int tile_index)
{
	int tile=READ_WORD(&gfx_base[2*tile_index])&0x1fff;
	int color=READ_WORD(&gfx_base[2*tile_index+0x1000]);

	SET_TILE_INFO(gfx_bank,tile,color&0xf)
	tile_info.flags = TILE_FLIPYX((color & 0x60)>>5);
	tile_info.priority = (color&0x10)>>4;
}

static void get_fix_info(int tile_index)
{
	int tile=READ_WORD(&lastduel_vram[2*tile_index]);

	SET_TILE_INFO(1,tile&0xfff,tile>>12)
}

/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int lastduel_vh_start(void)
{
	bg_tilemap = tilemap_create(get_ld_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,16,16,64,64);
	fg_tilemap = tilemap_create(get_ld_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT | TILEMAP_SPLIT,16,16,64,64);
	tx_tilemap = tilemap_create(get_fix_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);

	if (!bg_tilemap || !fg_tilemap || !tx_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 0;
	fg_tilemap->transmask[0] = 0x007f;
	fg_tilemap->transmask[1] = 0xff10;
	tx_tilemap->transparent_pen = 3;

	return 0;
}

int madgear_vh_start(void)
{
	bg_tilemap = tilemap_create(get_tile_info,tilemap_scan_cols,TILEMAP_OPAQUE,16,16,64,32);
	fg_tilemap = tilemap_create(get_tile_info,tilemap_scan_cols,TILEMAP_TRANSPARENT | TILEMAP_SPLIT,16,16,64,32);
	tx_tilemap = tilemap_create(get_fix_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);

	if (!bg_tilemap || !fg_tilemap || !tx_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 15;
	fg_tilemap->transmask[0] = 0x80ff;
	fg_tilemap->transmask[1] = 0x7f00;
	tx_tilemap->transparent_pen = 3;

	return 0;
}

/***************************************************************************/

void lastduel_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i,offs,color,code;
	int colmask[16];
	unsigned int *pen_usage; /* Save some struct derefs */

	/* Update tilemaps */
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	tilemap_set_scrollx( bg_tilemap,0, scroll[6] );
	tilemap_set_scrolly( bg_tilemap,0, scroll[4] );
	tilemap_set_scrollx( fg_tilemap,0, scroll[2] );
	tilemap_set_scrolly( fg_tilemap,0, scroll[0] );

	gfx_bank=2;
	gfx_base=lastduel_scroll2;
	tilemap_update(bg_tilemap);

	gfx_bank=3;
	gfx_base=lastduel_scroll1;
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	/* Build the dynamic palette */
	palette_init_used_colors();

	pen_usage= Machine->gfx[0]->pen_usage;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for(offs=0x800-8;offs>-1;offs-=8)
	{
		int attributes = READ_WORD(&buffered_spriteram[offs+2]);
		code=READ_WORD(&buffered_spriteram[offs]) & 0xfff;
		color = attributes&0xf;

		colmask[color] |= pen_usage[code];
	}
	for (color = 0;color < 16;color++)
	{
		if (colmask[color] & (1 << 0))
			palette_used_colors[32*16 + 16 * color +15] = PALETTE_COLOR_TRANSPARENT;
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[32*16 + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	/* Check for complete remap and redirty if needed */
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);
	tilemap_draw(bitmap,fg_tilemap,TILEMAP_BACK | 0);
	tilemap_draw(bitmap,fg_tilemap,TILEMAP_BACK | 1);
	tilemap_draw(bitmap,fg_tilemap,TILEMAP_FRONT | 0);

	/* Sprites */
	for(offs=0x800-8;offs>=0;offs-=8)
	{
		int attributes,sy,sx,flipx,flipy;
		code=READ_WORD(&buffered_spriteram[offs]);
		if (!code) continue;

		attributes = READ_WORD(&buffered_spriteram[offs+2]);
		sy = READ_WORD(&buffered_spriteram[offs+4]) & 0x1ff;
		sx = READ_WORD(&buffered_spriteram[offs+6]) & 0x1ff;

		flipx = attributes&0x20;
		flipy = attributes&0x40;
		color = attributes&0xf;

		if( sy>0x100 )
			sy -= 0x200;

		if (flipscreen) {
			sx=384+128-16-sx;
			sy=240-sy;
			if (flipx) flipx=0; else flipx=1;
			if (flipy) flipy=0; else flipy=1;
		}

		drawgfx(bitmap,Machine->gfx[0],
			code,
			color,
			flipx,flipy,
			sx,sy,
			&Machine->visible_area,
			TRANSPARENCY_PEN,15);
	}

	tilemap_draw(bitmap,fg_tilemap,TILEMAP_FRONT | 1);
	tilemap_draw(bitmap,tx_tilemap,0);
}

static void ledstorm_sprites(struct osd_bitmap *bitmap, int pri)
{
	int offs;

	for(offs=0x800-8;offs>=0;offs-=8)
	{
		int attributes,sy,sx,flipx,flipy,color,code;
		sy = READ_WORD(&buffered_spriteram[offs+4]) & 0x1ff;
		if (sy==0x180) continue;

		code=READ_WORD(&buffered_spriteram[offs]);
		attributes = READ_WORD(&buffered_spriteram[offs+2]);
		sx = READ_WORD(&buffered_spriteram[offs+6]) & 0x1ff;

		flipx = attributes&0x20;
		flipy = attributes&0x80; /* Different from Last Duel */
		color = attributes&0xf;
		if (pri==1 && (attributes&0x10)) continue;
		if (pri==0 && !(attributes&0x10)) continue;

		if( sy>0x100 )
			sy -= 0x200;

		if (flipscreen) {
			sx=384+128-16-sx;
			sy=240-sy;
			if (flipx) flipx=0; else flipx=1;
			if (flipy) flipy=0; else flipy=1;
		}

		drawgfx(bitmap,Machine->gfx[0],
			code,
			color,
			flipx,flipy,
			sx,sy,
			&Machine->visible_area,
			TRANSPARENCY_PEN,15);
	}
}

void ledstorm_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i,offs,color,code;
	int colmask[16];
	unsigned int *pen_usage; /* Save some struct derefs */

	/* Update tilemaps */
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	tilemap_set_scrollx( bg_tilemap,0, scroll[6] );
	tilemap_set_scrolly( bg_tilemap,0, scroll[4] );
	tilemap_set_scrollx( fg_tilemap,0, scroll[2] );
	tilemap_set_scrolly( fg_tilemap,0, scroll[0] );

	gfx_bank=2;
	gfx_base=lastduel_scroll2;
	tilemap_update(bg_tilemap);

	gfx_bank=3;
	gfx_base=lastduel_scroll1;
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	/* Build the dynamic palette */
	palette_init_used_colors();

	pen_usage= Machine->gfx[0]->pen_usage;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for(offs=0x800-8;offs>-1;offs-=8)
	{
		int attributes = READ_WORD(&buffered_spriteram[offs+2]);
		code=READ_WORD(&buffered_spriteram[offs]) & 0xfff;
		color = attributes&0xf;

		colmask[color] |= pen_usage[code];
	}
	for (color = 0;color < 16;color++)
	{
		if (colmask[color] & (1 << 0))
			palette_used_colors[32*16 + 16 * color +15] = PALETTE_COLOR_TRANSPARENT;
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[32*16 + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	/* Check for complete remap and redirty if needed */
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);
	tilemap_draw(bitmap,fg_tilemap,TILEMAP_BACK | 0);
	tilemap_draw(bitmap,fg_tilemap,TILEMAP_BACK | 1);
	tilemap_draw(bitmap,fg_tilemap,TILEMAP_FRONT | 0);
	ledstorm_sprites(bitmap,0);
	tilemap_draw(bitmap,fg_tilemap,TILEMAP_FRONT | 1);
	ledstorm_sprites(bitmap,1);
	tilemap_draw(bitmap,tx_tilemap,0);
}


void lastduel_eof_callback(void)
{
	/* Spriteram is always 1 frame ahead, suggesting buffering.  I can't find
		a register to control this so I assume it happens automatically
		every frame at the end of vblank */
	buffer_spriteram_w(0,0);
}
