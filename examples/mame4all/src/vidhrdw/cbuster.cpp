/***************************************************************************

   Crude Buster Video emulation - Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *twocrude_pf1_data,*twocrude_pf2_data,*twocrude_pf3_data,*twocrude_pf4_data;

static struct tilemap *pf1_tilemap,*pf2_tilemap,*pf3_tilemap,*pf4_tilemap;
static unsigned char *gfx_base;
static int gfx_bank,twocrude_pri,flipscreen,last_flip;

static unsigned char twocrude_control_0[16];
static unsigned char twocrude_control_1[16];

unsigned char *twocrude_pf1_rowscroll,*twocrude_pf2_rowscroll;
unsigned char *twocrude_pf3_rowscroll,*twocrude_pf4_rowscroll;

static unsigned char *twocrude_spriteram;



/* Function for all 16x16 1024 by 512 layers */
static UINT32 back_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return (col & 0x1f) + ((row & 0x1f) << 5) + ((col & 0x20) << 5);
}

static void get_back_tile_info(int tile_index)
{
	int tile,color;

	tile=READ_WORD(&gfx_base[2*tile_index]);
	color=tile >> 12;
	tile=tile&0xfff;

	SET_TILE_INFO(gfx_bank,tile,color)
}

/* 8x8 top layer */
static void get_fore_tile_info(int tile_index)
{
	int tile=READ_WORD(&twocrude_pf1_data[2*tile_index]);
	int color=tile >> 12;

	tile=tile&0xfff;

	SET_TILE_INFO(0,tile,color)
}

/******************************************************************************/

void twocrude_vh_stop (void)
{
	free(twocrude_spriteram);
}

int twocrude_vh_start(void)
{
	pf2_tilemap = tilemap_create(get_back_tile_info,back_scan,        TILEMAP_OPAQUE,16,16,64,32);
	pf3_tilemap = tilemap_create(get_back_tile_info,back_scan,        TILEMAP_TRANSPARENT,16,16,64,32);
	pf4_tilemap = tilemap_create(get_back_tile_info,back_scan,        TILEMAP_TRANSPARENT,16,16,64,32);
	pf1_tilemap = tilemap_create(get_fore_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);

	if (!pf1_tilemap || !pf2_tilemap || !pf3_tilemap || !pf4_tilemap)
		return 1;

	pf1_tilemap->transparent_pen = 0;
	pf3_tilemap->transparent_pen = 0;
	pf4_tilemap->transparent_pen = 0;

	twocrude_spriteram = (unsigned char*)malloc(0x800);

	return 0;
}

/******************************************************************************/

WRITE_HANDLER( twocrude_update_sprites_w )
{
	memcpy(twocrude_spriteram,spriteram,0x800);
}

static void update_24bitcol(int offset)
{
	int r,g,b;

	r = (READ_WORD(&paletteram[offset]) >> 0) & 0xff;
	g = (READ_WORD(&paletteram[offset]) >> 8) & 0xff;
	b = (READ_WORD(&paletteram_2[offset]) >> 0) & 0xff;

	palette_change_color(offset / 2,r,g,b);
}

WRITE_HANDLER( twocrude_palette_24bit_rg_w )
{
	COMBINE_WORD_MEM(&paletteram[offset],data);
	update_24bitcol(offset);
}

WRITE_HANDLER( twocrude_palette_24bit_b_w )
{
	COMBINE_WORD_MEM(&paletteram_2[offset],data);
	update_24bitcol(offset);
}

READ_HANDLER( twocrude_palette_24bit_rg_r )
{
	return READ_WORD(&paletteram[offset]);
}

READ_HANDLER( twocrude_palette_24bit_b_r )
{
	return READ_WORD(&paletteram_2[offset]);
}

/******************************************************************************/

void twocrude_pri_w(int pri)
{
	twocrude_pri=pri;
}

WRITE_HANDLER( twocrude_pf1_data_w )
{
	int oldword = READ_WORD(&twocrude_pf1_data[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&twocrude_pf1_data[offset],newword);
		tilemap_mark_tile_dirty(pf1_tilemap,offset/2);
	}
}

WRITE_HANDLER( twocrude_pf2_data_w )
{
	int oldword = READ_WORD(&twocrude_pf2_data[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&twocrude_pf2_data[offset],newword);
		tilemap_mark_tile_dirty(pf2_tilemap,offset/2);
	}
}

WRITE_HANDLER( twocrude_pf3_data_w )
{
	int oldword = READ_WORD(&twocrude_pf3_data[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&twocrude_pf3_data[offset],newword);
		tilemap_mark_tile_dirty(pf3_tilemap,offset/2);
	}
}

WRITE_HANDLER( twocrude_pf4_data_w )
{
	int oldword = READ_WORD(&twocrude_pf4_data[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&twocrude_pf4_data[offset],newword);
		tilemap_mark_tile_dirty(pf4_tilemap,offset/2);
	}
}

WRITE_HANDLER( twocrude_control_0_w )
{
	COMBINE_WORD_MEM(&twocrude_control_0[offset],data);
}

WRITE_HANDLER( twocrude_control_1_w )
{
	COMBINE_WORD_MEM(&twocrude_control_1[offset],data);
}

WRITE_HANDLER( twocrude_pf1_rowscroll_w )
{
	COMBINE_WORD_MEM(&twocrude_pf1_rowscroll[offset],data);
}

WRITE_HANDLER( twocrude_pf2_rowscroll_w )
{
	COMBINE_WORD_MEM(&twocrude_pf2_rowscroll[offset],data);
}

WRITE_HANDLER( twocrude_pf3_rowscroll_w )
{
	COMBINE_WORD_MEM(&twocrude_pf3_rowscroll[offset],data);
}

WRITE_HANDLER( twocrude_pf4_rowscroll_w )
{
	COMBINE_WORD_MEM(&twocrude_pf4_rowscroll[offset],data);
}

/******************************************************************************/

static void twocrude_update_palette(void)
{
	int offs,color,i,pal_base;
	int colmask[32];

	palette_init_used_colors();

	/* Sprites */
	pal_base = Machine->drv->gfxdecodeinfo[4].color_codes_start;
	for (color = 0;color < 32;color++) colmask[color] = 0;
	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,multi;

		sprite = READ_WORD (&twocrude_spriteram[offs+2]) & 0x7fff;
		if (!sprite) continue;

		y = READ_WORD(&twocrude_spriteram[offs]);
		x = READ_WORD(&twocrude_spriteram[offs+4]);
		color = (x >> 9) &0x1f;

		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */
		sprite &= ~multi;

		while (multi >= 0)
		{
			colmask[color] |= Machine->gfx[4]->pen_usage[sprite + multi];
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

	for (color = 16;color < 32;color++)
	{
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[256 + 1024 + 16 * (color-16) + i] = PALETTE_COLOR_USED;
		}
	}

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
}

static void twocrude_drawsprites(struct osd_bitmap *bitmap, int pri)
{
	int offs;

	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash,mult;

		sprite = READ_WORD (&twocrude_spriteram[offs+2]) & 0x7fff;
		if (!sprite) continue;

		y = READ_WORD(&twocrude_spriteram[offs]);
		x = READ_WORD(&twocrude_spriteram[offs+4]);

		if ((y&0x8000) && pri==1) continue;
		if (!(y&0x8000) && pri==0) continue;

		colour = (x >> 9) &0xf;
		if (x&0x2000) colour+=64;

		flash=y&0x1000;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

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

		if (flipscreen) {
			y=240-y;
			x=240-x;
			if (fx) fx=0; else fx=1;
			if (fy) fy=0; else fy=1;
			mult=16;
		}
		else mult=-16;

		while (multi >= 0)
		{
			drawgfx(bitmap,Machine->gfx[4],
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

void twocrude_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;
	int pf23_control,pf14_control;

	/* Update flipscreen */
	if (READ_WORD(&twocrude_control_1[0])&0x80)
		flipscreen=0;
	else
		flipscreen=1;

	if (last_flip!=flipscreen)
		tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	last_flip=flipscreen;

	pf23_control=READ_WORD (&twocrude_control_0[0xc]);
	pf14_control=READ_WORD (&twocrude_control_1[0xc]);

	/* Background - Rowscroll enable */
	if (pf23_control&0x4000) {
		int scrollx=READ_WORD(&twocrude_control_0[6]),rows;
		tilemap_set_scroll_cols(pf2_tilemap,1);
		tilemap_set_scrolly( pf2_tilemap,0, READ_WORD(&twocrude_control_0[8]) );

		/* Several different rowscroll styles! */
		switch ((READ_WORD (&twocrude_control_0[0xa])>>11)&7) {
			case 0: rows=512; break;/* Every line of 512 height bitmap */
			case 1: rows=256; break;
			case 2: rows=128; break;
			case 3: rows=64; break;
			case 4: rows=32; break;
			case 5: rows=16; break;
			case 6: rows=8; break;
			case 7: rows=4; break;
			default: rows=1; break;
		}

		tilemap_set_scroll_rows(pf2_tilemap,rows);
		for (offs = 0;offs < rows;offs++)
			tilemap_set_scrollx( pf2_tilemap,offs, scrollx + READ_WORD(&twocrude_pf2_rowscroll[2*offs]) );
	}
	else {
		tilemap_set_scroll_rows(pf2_tilemap,1);
		tilemap_set_scroll_cols(pf2_tilemap,1);
		tilemap_set_scrollx( pf2_tilemap,0, READ_WORD(&twocrude_control_0[6]) );
		tilemap_set_scrolly( pf2_tilemap,0, READ_WORD(&twocrude_control_0[8]) );
	}

	/* Playfield 3 */
	if (pf23_control&0x40) { /* Rowscroll */
		int scrollx=READ_WORD(&twocrude_control_0[2]),rows;
		tilemap_set_scroll_cols(pf3_tilemap,1);
		tilemap_set_scrolly( pf3_tilemap,0, READ_WORD(&twocrude_control_0[4]) );

		/* Several different rowscroll styles! */
		switch ((READ_WORD (&twocrude_control_0[0xa])>>3)&7) {
			case 0: rows=512; break;/* Every line of 512 height bitmap */
			case 1: rows=256; break;
			case 2: rows=128; break;
			case 3: rows=64; break;
			case 4: rows=32; break;
			case 5: rows=16; break;
			case 6: rows=8; break;
			case 7: rows=4; break;
			default: rows=1; break;
		}

		tilemap_set_scroll_rows(pf3_tilemap,rows);
		for (offs = 0;offs < rows;offs++)
			tilemap_set_scrollx( pf3_tilemap,offs, scrollx + READ_WORD(&twocrude_pf3_rowscroll[2*offs]) );
	}
	else if (pf23_control&0x20) { /* Colscroll */
		int scrolly=READ_WORD(&twocrude_control_0[4]),cols;
		tilemap_set_scroll_rows(pf3_tilemap,1);
		tilemap_set_scrollx( pf3_tilemap,0, READ_WORD(&twocrude_control_0[2]) );

		/* Several different colscroll styles! */
		switch ((READ_WORD (&twocrude_control_0[0xa])>>0)&7) {
			case 0: cols=64; break;
			case 1: cols=32; break;
			case 2: cols=16; break;
			case 3: cols=8; break;
			case 4: cols=4; break;
			case 5: cols=2; break;
			case 6: cols=1; break;
			case 7: cols=1; break;
			default: cols=1; break;
		}

		tilemap_set_scroll_cols(pf3_tilemap,cols);
		for (offs = 0;offs < cols;offs++)
			tilemap_set_scrolly( pf3_tilemap,offs,scrolly + READ_WORD(&twocrude_pf3_rowscroll[2*offs+0x400]) );
	}
	else {
		tilemap_set_scroll_rows(pf3_tilemap,1);
		tilemap_set_scroll_cols(pf3_tilemap,1);
		tilemap_set_scrollx( pf3_tilemap,0, READ_WORD(&twocrude_control_0[2]) );
		tilemap_set_scrolly( pf3_tilemap,0, READ_WORD(&twocrude_control_0[4]) );
	}

	/* Playfield 4 - Rowscroll enable */
	if (pf14_control&0x4000) {
		int scrollx=READ_WORD(&twocrude_control_1[6]),rows;
		tilemap_set_scroll_cols(pf4_tilemap,1);
		tilemap_set_scrolly( pf4_tilemap,0, READ_WORD(&twocrude_control_1[8]) );

		/* Several different rowscroll styles! */
		switch ((READ_WORD (&twocrude_control_1[0xa])>>11)&7) {
			case 0: rows=512; break;/* Every line of 512 height bitmap */
			case 1: rows=256; break;
			case 2: rows=128; break;
			case 3: rows=64; break;
			case 4: rows=32; break;
			case 5: rows=16; break;
			case 6: rows=8; break;
			case 7: rows=4; break;
			default: rows=1; break;
		}

		tilemap_set_scroll_rows(pf4_tilemap,rows);
		for (offs = 0;offs < rows;offs++)
			tilemap_set_scrollx( pf4_tilemap,offs, scrollx + READ_WORD(&twocrude_pf4_rowscroll[2*offs]) );
	}
	else {
		tilemap_set_scroll_rows(pf4_tilemap,1);
		tilemap_set_scroll_cols(pf4_tilemap,1);
		tilemap_set_scrollx( pf4_tilemap,0, READ_WORD(&twocrude_control_1[6]) );
		tilemap_set_scrolly( pf4_tilemap,0, READ_WORD(&twocrude_control_1[8]) );
	}

	/* Playfield 1 */
	if (pf14_control&0x40) { /* Rowscroll */
		int scrollx=READ_WORD(&twocrude_control_1[2]),rows;
		tilemap_set_scroll_cols(pf1_tilemap,1);
		tilemap_set_scrolly( pf1_tilemap,0, READ_WORD(&twocrude_control_1[4]) );

		/* Several different rowscroll styles! */
		switch ((READ_WORD (&twocrude_control_1[0xa])>>3)&7) {
			case 0: rows=256; break;/* Every line of 256 height bitmap */
			case 1: rows=128; break;
			case 2: rows=64; break;
			case 3: rows=32; break;
			case 4: rows=16; break;
			case 5: rows=8; break;
			case 6: rows=4; break;
			case 7: rows=2; break;
			default: rows=1; break;
		}

		tilemap_set_scroll_rows(pf1_tilemap,rows);
		for (offs = 0;offs < rows;offs++)
			tilemap_set_scrollx( pf1_tilemap,offs, scrollx + READ_WORD(&twocrude_pf1_rowscroll[2*offs]) );
	}
	else {
		tilemap_set_scroll_rows(pf1_tilemap,1);
		tilemap_set_scroll_cols(pf1_tilemap,1);
		tilemap_set_scrollx( pf1_tilemap,0, READ_WORD(&twocrude_control_1[2]) );
		tilemap_set_scrolly( pf1_tilemap,0, READ_WORD(&twocrude_control_1[4]) );
	}

	/* Update playfields */
	gfx_bank=1;
	gfx_base=twocrude_pf2_data;
	tilemap_update(pf2_tilemap);

	gfx_bank=2;
	gfx_base=twocrude_pf3_data;
	tilemap_update(pf3_tilemap);

	gfx_bank=3;
	gfx_base=twocrude_pf4_data;
	tilemap_update(pf4_tilemap);
	tilemap_update(pf1_tilemap);
	twocrude_update_palette();

	/* Draw playfields & sprites */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,pf2_tilemap,0);
	twocrude_drawsprites(bitmap,0);

	if (twocrude_pri) {
		tilemap_draw(bitmap,pf4_tilemap,0);
		tilemap_draw(bitmap,pf3_tilemap,0);
	}
	else {
		tilemap_draw(bitmap,pf3_tilemap,0);
		tilemap_draw(bitmap,pf4_tilemap,0);
	}

	twocrude_drawsprites(bitmap,1);
	tilemap_draw(bitmap,pf1_tilemap,0);
}
