/*******************************************************************************

	actfancr - Bryan McPhail, mish@tendril.co.uk

*******************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int actfancr_control_1[0x20],actfancr_control_2[0x20];
unsigned char *actfancr_pf1_data,*actfancr_pf2_data,*actfancr_pf1_rowscroll_data;
static struct tilemap *pf1_tilemap;
static int flipscreen;



static UINT32 actfancr_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return (col & 0x0f) + ((row & 0x0f) << 4) + ((col & 0xf0) << 4);
}

static void get_tile_info(int tile_index)
{
	int tile,color;

	tile=actfancr_pf1_data[2*tile_index]+(actfancr_pf1_data[2*tile_index+1]<<8);
	color=tile >> 12;
	tile=tile&0xfff;

	SET_TILE_INFO(2,tile,color)
}

static UINT32 triothep_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return (col & 0x0f) + ((row & 0x0f) << 4) + ((row & 0x10) << 4) + ((col & 0x10) << 5);
}

static void get_trio_tile_info(int tile_index)
{
	int tile,color;

	tile=actfancr_pf1_data[2*tile_index]+(actfancr_pf1_data[2*tile_index+1]<<8);
	color=tile >> 12;
	tile=tile&0xfff;

	SET_TILE_INFO(2,tile,color)
}

/******************************************************************************/

int actfancr_vh_start (void)
{
	pf1_tilemap = tilemap_create(get_tile_info,actfancr_scan,TILEMAP_OPAQUE,16,16,256,16);

	if (!pf1_tilemap)
		return 1;

	return 0;
}

int triothep_vh_start (void)
{
	pf1_tilemap = tilemap_create(get_trio_tile_info,triothep_scan,TILEMAP_OPAQUE,16,16,32,32);

	if (!pf1_tilemap)
		return 1;

	return 0;
}

/******************************************************************************/

WRITE_HANDLER( actfancr_pf1_control_w )
{
	actfancr_control_1[offset]=data;
}

WRITE_HANDLER( actfancr_pf2_control_w )
{
	actfancr_control_2[offset]=data;
}

WRITE_HANDLER( actfancr_pf1_data_w )
{
	actfancr_pf1_data[offset]=data;
	tilemap_mark_tile_dirty(pf1_tilemap,offset/2);
}

READ_HANDLER( actfancr_pf1_data_r )
{
	return actfancr_pf1_data[offset];
}

WRITE_HANDLER( actfancr_pf2_data_w )
{
	actfancr_pf2_data[offset]=data;
}

READ_HANDLER( actfancr_pf2_data_r )
{
	return actfancr_pf2_data[offset];
}

/******************************************************************************/

void actfancr_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int my,mx,offs,color,tile,pal_base,colmask[16],i,mult;
	int scrollx=(actfancr_control_1[0x10]+(actfancr_control_1[0x11]<<8));
	int scrolly=(actfancr_control_1[0x12]+(actfancr_control_1[0x13]<<8));

	/* Draw playfield */
	flipscreen=actfancr_control_2[0]&0x80;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);

	tilemap_set_scrollx( pf1_tilemap,0, scrollx );
	tilemap_set_scrolly( pf1_tilemap,0, scrolly );

	tilemap_update(pf1_tilemap);

	palette_init_used_colors();
	pal_base = Machine->drv->gfxdecodeinfo[0].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0; offs < 0x800; offs += 2)
	{
		tile = actfancr_pf2_data[offs]+(actfancr_pf2_data[offs+1]<<8);
		colmask[tile>>12] |= Machine->gfx[0]->pen_usage[tile&0xfff];
	}
	for (color = 0;color < 16;color++)
	{
		if (colmask[color] & (1 << 0))
			palette_used_colors[pal_base + 16 * color] = PALETTE_COLOR_TRANSPARENT;
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0; offs < 0x800; offs += 2)
	{
		tile = buffered_spriteram[offs+2]+(buffered_spriteram[offs+3]<<8);
		color=buffered_spriteram[offs+5]>>4;
		colmask[color] |= Machine->gfx[1]->pen_usage[tile&0xfff];
	}
	for (color = 0;color < 16;color++)
	{
		if (colmask[color] & (1 << 0))
			palette_used_colors[pal_base + 16 * color] = PALETTE_COLOR_TRANSPARENT;
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,pf1_tilemap,0);

	/* Sprites */
	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash;

		y=buffered_spriteram[offs]+(buffered_spriteram[offs+1]<<8);
 		if ((y&0x8000) == 0) continue;
		x = buffered_spriteram[offs+4]+(buffered_spriteram[offs+5]<<8);
		colour = ((x & 0xf000) >> 12);
		flash=x&0x800;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x1800) >> 11)) - 1;	/* 1x, 2x, 4x, 8x height */

											/* multi = 0   1   3   7 */
		sprite = buffered_spriteram[offs+2]+(buffered_spriteram[offs+3]<<8);
		sprite &= 0x0fff;

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 256) x -= 512;
		if (y >= 256) y -= 512;
		x = 240 - x;
		y = 240 - y;

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
			drawgfx(bitmap,Machine->gfx[1],
					sprite - multi * inc,
					colour,
					fx,fy,
					x,y + mult * multi,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
			multi--;
		}
	}

	/* Draw character tiles */
	for (offs = 0x800 - 2;offs >= 0;offs -= 2) {
		tile=actfancr_pf2_data[offs]+(actfancr_pf2_data[offs+1]<<8);
		if (!tile) continue;
		color=tile>>12;
		tile=tile&0xfff;
		mx = (offs/2) % 32;
		my = (offs/2) / 32;
		if (flipscreen) {mx=31-mx; my=31-my;}
		drawgfx(bitmap,Machine->gfx[0],
			tile,color,flipscreen,flipscreen,8*mx,8*my,
			&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

void triothep_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int my,mx,offs,color,tile,pal_base,colmask[16],i,mult;
	int scrollx=(actfancr_control_1[0x10]+(actfancr_control_1[0x11]<<8));
	int scrolly=(actfancr_control_1[0x12]+(actfancr_control_1[0x13]<<8));

	/* Draw playfield */
	flipscreen=actfancr_control_2[0]&0x80;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);

	if (actfancr_control_2[0]&0x4) {
		tilemap_set_scroll_rows(pf1_tilemap,32);
		tilemap_set_scrolly( pf1_tilemap,0, scrolly );
		for (i=0; i<32; i++)
			tilemap_set_scrollx( pf1_tilemap,i, scrollx+(actfancr_pf1_rowscroll_data[i*2] | actfancr_pf1_rowscroll_data[i*2+1]<<8) );
	}
	else {
		tilemap_set_scroll_rows(pf1_tilemap,1);
		tilemap_set_scrollx( pf1_tilemap,0, scrollx );
		tilemap_set_scrolly( pf1_tilemap,0, scrolly );
	}

	tilemap_update(pf1_tilemap);

	palette_init_used_colors();
	pal_base = Machine->drv->gfxdecodeinfo[0].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0; offs < 0x800; offs += 2)
	{
		tile = actfancr_pf2_data[offs]+(actfancr_pf2_data[offs+1]<<8);
		colmask[tile>>12] |= Machine->gfx[0]->pen_usage[tile&0xfff];
	}
	for (color = 0;color < 16;color++)
	{
		if (colmask[color] & (1 << 0))
			palette_used_colors[pal_base + 16 * color] = PALETTE_COLOR_TRANSPARENT;
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0; offs < 0x800; offs += 2)
	{
		tile = buffered_spriteram[offs+2]+(buffered_spriteram[offs+3]<<8);
		color= buffered_spriteram[offs+5]>>4;
		colmask[color] |= Machine->gfx[1]->pen_usage[tile&0xfff];
	}
	for (color = 0;color < 16;color++)
	{
		if (colmask[color] & (1 << 0))
			palette_used_colors[pal_base + 16 * color] = PALETTE_COLOR_TRANSPARENT;
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,pf1_tilemap,0);

	/* Sprites */
	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash;

		y=buffered_spriteram[offs]+(buffered_spriteram[offs+1]<<8);
 		if ((y&0x8000) == 0) continue;
		x = buffered_spriteram[offs+4]+(buffered_spriteram[offs+5]<<8);
		colour = ((x & 0xf000) >> 12);
		flash=x&0x800;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x1800) >> 11)) - 1;	/* 1x, 2x, 4x, 8x height */

											/* multi = 0   1   3   7 */
		sprite = buffered_spriteram[offs+2]+(buffered_spriteram[offs+3]<<8);
		sprite &= 0x0fff;

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 256) x -= 512;
		if (y >= 256) y -= 512;
		x = 240 - x;
		y = 240 - y;

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
			drawgfx(bitmap,Machine->gfx[1],
					sprite - multi * inc,
					colour,
					fx,fy,
					x,y + mult * multi,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
			multi--;
		}
	}

	/* Draw character tiles */
	for (offs = 0x800 - 2;offs >= 0;offs -= 2) {
		tile=actfancr_pf2_data[offs]+(actfancr_pf2_data[offs+1]<<8);
		if (!tile) continue;
		color=tile>>12;
		tile=tile&0xfff;
		mx = (offs/2) % 32;
		my = (offs/2) / 32;
		if (flipscreen) {mx=31-mx; my=31-my;}
		drawgfx(bitmap,Machine->gfx[0],
			tile,color,flipscreen,flipscreen,8*mx,8*my,
			&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
