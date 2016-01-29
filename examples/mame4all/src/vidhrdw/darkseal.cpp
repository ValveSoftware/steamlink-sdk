/***************************************************************************

   Dark Seal Video emulation - Bryan McPhail, mish@tendril.co.uk

****************************************************************************

Data East custom chip 55:  Generates two playfields, playfield 1 is underneath
playfield 2.  Dark Seal uses two of these chips.  1 playfield is _always_ off
in this game.

	16 bytes of control registers per chip.

	Word 0:
		Mask 0x0080: Flip screen
		Mask 0x007f: ?
	Word 2:
		Mask 0xffff: Playfield 2 X scroll (top playfield)
	Word 4:
		Mask 0xffff: Playfield 2 Y scroll (top playfield)
	Word 6:
		Mask 0xffff: Playfield 1 X scroll (bottom playfield)
	Word 8:
		Mask 0xffff: Playfield 1 Y scroll (bottom playfield)
	Word 0xa:
		Mask 0xc000: Playfield 1 shape??
		Mask 0x3000: Playfield 1 rowscroll style (maybe mask 0x3800??)
		Mask 0x0300: Playfield 1 colscroll style (maybe mask 0x0700??)?

		Mask 0x00c0: Playfield 2 shape??
		Mask 0x0030: Playfield 2 rowscroll style (maybe mask 0x0038??)
		Mask 0x0003: Playfield 2 colscroll style (maybe mask 0x0007??)?
	Word 0xc:
		Mask 0x8000: Playfield 1 is 8*8 tiles else 16*16
		Mask 0x4000: Playfield 1 rowscroll enabled
		Mask 0x2000: Playfield 1 colscroll enabled
		Mask 0x1f00: ?

		Mask 0x0080: Playfield 2 is 8*8 tiles else 16*16
		Mask 0x0040: Playfield 2 rowscroll enabled
		Mask 0x0020: Playfield 2 colscroll enabled
		Mask 0x001f: ?
	Word 0xe:
		??

Locations 0 & 0xe are mostly unknown:

							 0		14
Caveman Ninja (bottom):		0053	1100 (changes to 1111 later)
Caveman Ninja (top):		0010	0081
Two Crude (bottom):			0053	0000
Two Crude (top):			0010	0041
Dark Seal (bottom):			0010	0000
Dark Seal (top):			0053	4101
Tumblepop:					0010	0000
Super Burger Time:			0010	0000

Location 0xe looks like it could be a mirror of another byte..

**************************************************************************

Sprites - Data East custom chip 52

	8 bytes per sprite, unknowns bits seem unused.

	Word 0:
		Mask 0x8000 - ?
		Mask 0x4000 - Y flip
		Mask 0x2000 - X flip
		Mask 0x1000 - Sprite flash
		Mask 0x0800 - ?
		Mask 0x0600 - Sprite height (1x, 2x, 4x, 8x)
		Mask 0x01ff - Y coordinate

	Word 2:
		Mask 0xffff - Sprite number

	Word 4:
		Mask 0x8000 - ?
		Mask 0x4000 - Sprite is drawn beneath top 8 pens of playfield 4
		Mask 0x3e00 - Colour (32 palettes, most games only use 16)
		Mask 0x01ff - X coordinate

	Word 6:
		Always unused.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *darkseal_pf12_row,*darkseal_pf34_row;
unsigned char *darkseal_pf1_data,*darkseal_pf2_data,*darkseal_pf3_data;

static unsigned char darkseal_control_0[16];
static unsigned char darkseal_control_1[16];

static struct tilemap *pf1_tilemap,*pf2_tilemap,*pf3_tilemap;
static unsigned char *gfx_base;
static int gfx_bank,flipscreen;

/***************************************************************************/

/* Function for all 16x16 1024x1024 layers */
static UINT32 darkseal_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return (col & 0x1f) + ((row & 0x1f) << 5) + ((col & 0x20) << 5) + ((row & 0x20) << 6);
}

static void get_bg_tile_info(int tile_index)
{
	int tile,color;

	tile=READ_WORD(&gfx_base[2*tile_index]);
	color=tile >> 12;
	tile=tile&0xfff;

	SET_TILE_INFO(gfx_bank,tile,color)
}

static void get_fg_tile_info(int tile_index)
{
	int tile=READ_WORD(&darkseal_pf1_data[2*tile_index]);
	int color=tile >> 12;

	tile=tile&0xfff;
	SET_TILE_INFO(0,tile,color)
}

/******************************************************************************/

static void update_24bitcol(int offset)
{
	int r,g,b;

	r = (READ_WORD(&paletteram[offset]) >> 0) & 0xff;
	g = (READ_WORD(&paletteram[offset]) >> 8) & 0xff;
	b = (READ_WORD(&paletteram_2[offset]) >> 0) & 0xff;

	palette_change_color(offset / 2,r,g,b);
}

WRITE_HANDLER( darkseal_palette_24bit_rg_w )
{
	/* only 1280 out of 2048 colors are actually used */
	if (offset >= 2*Machine->drv->total_colors) return;

	COMBINE_WORD_MEM(&paletteram[offset],data);
	update_24bitcol(offset);
}

WRITE_HANDLER( darkseal_palette_24bit_b_w )
{
	/* only 1280 out of 2048 colors are actually used */
	if (offset >= 2*Machine->drv->total_colors) return;

	COMBINE_WORD_MEM(&paletteram_2[offset],data);
	update_24bitcol(offset);
}

READ_HANDLER( darkseal_palette_24bit_rg_r )
{
	return READ_WORD(&paletteram[offset]);
}

READ_HANDLER( darkseal_palette_24bit_b_r )
{
	return READ_WORD(&paletteram_2[offset]);
}

/******************************************************************************/

static void darkseal_mark_sprite_colours(void)
{
	int offs,color,i,pal_base,colmask[32];

	palette_init_used_colors();

	pal_base = Machine->drv->gfxdecodeinfo[3].color_codes_start;
	for (color = 0;color < 32;color++) colmask[color] = 0;
	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,multi;

		sprite = READ_WORD (&buffered_spriteram[offs+2]) & 0x1fff;
		if (!sprite) continue;

		y = READ_WORD(&buffered_spriteram[offs]);
		x = READ_WORD(&buffered_spriteram[offs+4]);
		color = (x >> 9) &0x1f;

		x = x & 0x01ff;
		if (x >= 256) x -= 512;
		x = 240 - x;
		if (x>256) continue; /* Speedup */

		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

		sprite &= ~multi;

		while (multi >= 0)
		{
			colmask[color] |= Machine->gfx[3]->pen_usage[sprite + multi];
			multi--;
		}
	}

	for (color = 0;color < 32;color++)
	{
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
}

static void darkseal_drawsprites(struct osd_bitmap *bitmap)
{
	int offs;

	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash,mult;

		sprite = READ_WORD (&buffered_spriteram[offs+2]) & 0x1fff;
		if (!sprite) continue;

		y = READ_WORD(&buffered_spriteram[offs]);
		x = READ_WORD(&buffered_spriteram[offs+4]);

		flash=y&0x1000;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		colour = (x >> 9) &0x1f;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 256) x -= 512;
		if (y >= 256) y -= 512;
		x = 240 - x;
		y = 240 - y;

		if (x>256) continue; /* Speedup */

		sprite &= ~multi;
		if (fy)
			inc = -1;
		else
		{
			sprite += multi;
			inc = 1;
		}

		if (flipscreen)
		{
			y=240-y;
			x=240-x;
			if (fx) fx=0; else fx=1;
			if (fy) fy=0; else fy=1;
			mult=16;
		}
		else mult=-16;

		while (multi >= 0)
		{
			drawgfx(bitmap,Machine->gfx[3],
					sprite - multi * inc,
					colour,
					fx,fy,
					x,y + mult * multi,
					&Machine->visible_area,TRANSPARENCY_PEN,0);

			multi--;
		}
	}
}

/******************************************************************************/

WRITE_HANDLER( darkseal_pf1_data_w )
{
	COMBINE_WORD_MEM(&darkseal_pf1_data[offset],data);
	tilemap_mark_tile_dirty(pf1_tilemap,offset/2);
}

WRITE_HANDLER( darkseal_pf2_data_w )
{
	COMBINE_WORD_MEM(&darkseal_pf2_data[offset],data);
	tilemap_mark_tile_dirty(pf2_tilemap,offset/2);
}

WRITE_HANDLER( darkseal_pf3_data_w )
{
	COMBINE_WORD_MEM(&darkseal_pf3_data[offset],data);
	tilemap_mark_tile_dirty(pf3_tilemap,offset/2);
}

WRITE_HANDLER( darkseal_pf3b_data_w ) /* Mirror */
{
	darkseal_pf3_data_w(offset+0x1000,data);
}

WRITE_HANDLER( darkseal_control_0_w )
{
	COMBINE_WORD_MEM(&darkseal_control_0[offset],data);
}

WRITE_HANDLER( darkseal_control_1_w )
{
	COMBINE_WORD_MEM(&darkseal_control_1[offset],data);
}

/******************************************************************************/

int darkseal_vh_start(void)
{
	pf1_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,64,64);
	pf2_tilemap = tilemap_create(get_bg_tile_info,darkseal_scan,    TILEMAP_TRANSPARENT,16,16,64,64);
	pf3_tilemap = tilemap_create(get_bg_tile_info,darkseal_scan,    TILEMAP_OPAQUE,     16,16,64,64);

	if (!pf1_tilemap || !pf2_tilemap || !pf3_tilemap)
		return 1;

	pf1_tilemap->transparent_pen = 0;
	pf2_tilemap->transparent_pen = 0;

	return 0;
}

/******************************************************************************/

void darkseal_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	flipscreen=!(READ_WORD(&darkseal_control_0[0])&0x80);
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);

	/* Update scroll registers */
	tilemap_set_scrollx( pf1_tilemap,0, READ_WORD(&darkseal_control_1[6]) );
	tilemap_set_scrolly( pf1_tilemap,0, READ_WORD(&darkseal_control_1[8]) );
	tilemap_set_scrollx( pf2_tilemap,0, READ_WORD(&darkseal_control_1[2]) );
	tilemap_set_scrolly( pf2_tilemap,0, READ_WORD(&darkseal_control_1[4]) );

	if (READ_WORD(&darkseal_control_0[0xc])&0x4000) { /* Rowscroll enable */
		int offs,scrollx=READ_WORD(&darkseal_control_0[6]);

		tilemap_set_scroll_rows(pf3_tilemap,512);
		for (offs = 0;offs < 512;offs++)
			tilemap_set_scrollx( pf3_tilemap,offs, scrollx + READ_WORD(&darkseal_pf34_row[(offs<<1)+0x80]) );
	}
	else {
		tilemap_set_scroll_rows(pf3_tilemap,1);
		tilemap_set_scrollx( pf3_tilemap,0, READ_WORD(&darkseal_control_0[6]) );
	}
	tilemap_set_scrolly( pf3_tilemap,0, READ_WORD(&darkseal_control_0[8]) );

	gfx_bank=1;
	gfx_base=darkseal_pf2_data;
	tilemap_update(pf2_tilemap);
	gfx_bank=2;
	gfx_base=darkseal_pf3_data;
	tilemap_update(pf3_tilemap);
	tilemap_update(pf1_tilemap);
	darkseal_mark_sprite_colours();
	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap,pf3_tilemap,0);
	tilemap_draw(bitmap,pf2_tilemap,0);
	darkseal_drawsprites(bitmap);
	tilemap_draw(bitmap,pf1_tilemap,0);
}

/******************************************************************************/
