/***************************************************************************

   Vapour Trail Video emulation - Bryan McPhail, mish@tendril.co.uk

****************************************************************************

	2 Data East 55 chips for playfields (same as Dark Seal, etc)
	1 Data East MXC-06 chip for sprites (same as Bad Dudes, etc)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *vaportra_pf1_data,*vaportra_pf2_data,*vaportra_pf3_data,*vaportra_pf4_data;

static unsigned char vaportra_control_0[16];
static unsigned char vaportra_control_1[16];
static unsigned char vaportra_control_2[4];

static struct tilemap *pf1_tilemap,*pf2_tilemap,*pf3_tilemap,*pf4_tilemap;
static unsigned char *gfx_base;
static int gfx_bank,flipscreen;

static unsigned char *vaportra_spriteram;



/* Function for all 16x16 1024x1024 layers */
static UINT32 vaportra_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
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

/* 8x8 top layer */
static void get_fg_tile_info(int tile_index)
{
	int tile=READ_WORD(&vaportra_pf1_data[2*tile_index]);
	int color=tile >> 12;

	tile=tile&0xfff;

	SET_TILE_INFO(0,tile,color)
}

/******************************************************************************/

void vaportra_vh_stop (void)
{
	free(vaportra_spriteram);
}

int vaportra_vh_start(void)
{
	pf1_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,64,64);
	pf2_tilemap = tilemap_create(get_bg_tile_info,vaportra_scan,    TILEMAP_TRANSPARENT,16,16,64,32);
	pf3_tilemap = tilemap_create(get_bg_tile_info,vaportra_scan,    TILEMAP_TRANSPARENT,16,16,64,32);
	pf4_tilemap = tilemap_create(get_bg_tile_info,vaportra_scan,    TILEMAP_TRANSPARENT,16,16,64,32);

	if (!pf1_tilemap || !pf2_tilemap || !pf3_tilemap || !pf4_tilemap)
		return 1;

	vaportra_spriteram = (unsigned char*)malloc(0x800);
	if (!vaportra_spriteram)
		return 1;

	pf1_tilemap->transparent_pen = 0;
	pf2_tilemap->transparent_pen = 0;
	pf3_tilemap->transparent_pen = 0;
	pf4_tilemap->transparent_pen = 0;

	return 0;
}

/******************************************************************************/

READ_HANDLER( vaportra_pf1_data_r )
{
	return READ_WORD(&vaportra_pf1_data[offset]);
}

READ_HANDLER( vaportra_pf2_data_r )
{
	return READ_WORD(&vaportra_pf2_data[offset]);
}

READ_HANDLER( vaportra_pf3_data_r )
{
	return READ_WORD(&vaportra_pf3_data[offset]);
}

READ_HANDLER( vaportra_pf4_data_r )
{
	return READ_WORD(&vaportra_pf4_data[offset]);
}

WRITE_HANDLER( vaportra_pf1_data_w )
{
	COMBINE_WORD_MEM(&vaportra_pf1_data[offset],data);
	tilemap_mark_tile_dirty(pf1_tilemap,offset/2);
}

WRITE_HANDLER( vaportra_pf2_data_w )
{
	COMBINE_WORD_MEM(&vaportra_pf2_data[offset],data);
	tilemap_mark_tile_dirty(pf2_tilemap,offset/2);
}

WRITE_HANDLER( vaportra_pf3_data_w )
{
	COMBINE_WORD_MEM(&vaportra_pf3_data[offset],data);
	tilemap_mark_tile_dirty(pf3_tilemap,offset/2);
}

WRITE_HANDLER( vaportra_pf4_data_w )
{
	COMBINE_WORD_MEM(&vaportra_pf4_data[offset],data);
	tilemap_mark_tile_dirty(pf4_tilemap,offset/2);
}

WRITE_HANDLER( vaportra_control_0_w )
{
	COMBINE_WORD_MEM(&vaportra_control_0[offset],data);
}

WRITE_HANDLER( vaportra_control_1_w )
{
	COMBINE_WORD_MEM(&vaportra_control_1[offset],data);
}

WRITE_HANDLER( vaportra_control_2_w )
{
	COMBINE_WORD_MEM(&vaportra_control_2[offset],data);
}

/******************************************************************************/

WRITE_HANDLER( vaportra_update_sprites_w )
{
	memcpy(vaportra_spriteram,spriteram,0x800);
}

static void update_24bitcol(int offset)
{
	int r,g,b;

	r = (READ_WORD(&paletteram[offset]) >> 0) & 0xff;
	g = (READ_WORD(&paletteram[offset]) >> 8) & 0xff;
	b = (READ_WORD(&paletteram_2[offset]) >> 0) & 0xff;

	palette_change_color(offset / 2,r,g,b);
}

WRITE_HANDLER( vaportra_palette_24bit_rg_w )
{
	COMBINE_WORD_MEM(&paletteram[offset],data);
	update_24bitcol(offset);
}

WRITE_HANDLER( vaportra_palette_24bit_b_w )
{
	COMBINE_WORD_MEM(&paletteram_2[offset],data);
	update_24bitcol(offset);
}

/******************************************************************************/

static void vaportra_update_palette(void)
{
	int offs,color,i,pal_base;
	int colmask[16];

	palette_init_used_colors();

	/* Sprites */
	pal_base = Machine->drv->gfxdecodeinfo[4].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,multi;

		y = READ_WORD(&vaportra_spriteram[offs]);
		if ((y&0x8000) == 0) continue;

		sprite = READ_WORD (&vaportra_spriteram[offs+2]) & 0x1fff;

		x = READ_WORD(&vaportra_spriteram[offs+4]);
		color = (x >> 12) &0xf;

		x = x & 0x01ff;
		if (x >= 256) x -= 512;
		x = 240 - x;
		if (x>256) continue; /* Speedup */

		multi = (1 << ((y & 0x1800) >> 11)) - 1;	/* 1x, 2x, 4x, 8x height */
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

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
}

static void vaportra_drawsprites(struct osd_bitmap *bitmap, int pri)
{
	int offs,priority_value;

	priority_value=READ_WORD(&vaportra_control_2[2]);

	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash,mult;

		y = READ_WORD(&vaportra_spriteram[offs]);
		if ((y&0x8000) == 0) continue;

		sprite = READ_WORD (&vaportra_spriteram[offs+2]) & 0x1fff;
		x = READ_WORD(&vaportra_spriteram[offs+4]);
		colour = (x >> 12) &0xf;
		if (pri && (colour>=priority_value)) continue;
		if (!pri && !(colour>=priority_value)) continue;

		flash=x&0x800;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x1800) >> 11)) - 1;	/* 1x, 2x, 4x, 8x height */

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


void vaportra_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int pri=READ_WORD(&vaportra_control_2[0]);
	static int last_pri=0;

	/* Update flipscreen */
	if (READ_WORD(&vaportra_control_1[0])&0x80)
		flipscreen=0;
	else
		flipscreen=1;

	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);

	/* Update scroll registers */
	tilemap_set_scrollx( pf1_tilemap,0, READ_WORD(&vaportra_control_1[2]) );
	tilemap_set_scrolly( pf1_tilemap,0, READ_WORD(&vaportra_control_1[4]) );
	tilemap_set_scrollx( pf2_tilemap,0, READ_WORD(&vaportra_control_0[2]) );
	tilemap_set_scrolly( pf2_tilemap,0, READ_WORD(&vaportra_control_0[4]) );
	tilemap_set_scrollx( pf3_tilemap,0, READ_WORD(&vaportra_control_1[6]) );
	tilemap_set_scrolly( pf3_tilemap,0, READ_WORD(&vaportra_control_1[8]) );
	tilemap_set_scrollx( pf4_tilemap,0, READ_WORD(&vaportra_control_0[6]) );
	tilemap_set_scrolly( pf4_tilemap,0, READ_WORD(&vaportra_control_0[8]) );

	pri&=0x3;
	if (pri!=last_pri)
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
	last_pri=pri;

	/* Update playfields */
	switch (pri) {
		case 0:
		case 2:
			pf4_tilemap->type=TILEMAP_OPAQUE;
			pf3_tilemap->type=TILEMAP_TRANSPARENT;
			pf2_tilemap->type=TILEMAP_TRANSPARENT;
			break;
		case 1:
		case 3:
			pf2_tilemap->type=TILEMAP_OPAQUE;
			pf3_tilemap->type=TILEMAP_TRANSPARENT;
			pf4_tilemap->type=TILEMAP_TRANSPARENT;
			break;
	}

	gfx_bank=1;
	gfx_base=vaportra_pf2_data;
	tilemap_update(pf2_tilemap);

	gfx_bank=2;
	gfx_base=vaportra_pf3_data;
	tilemap_update(pf3_tilemap);

	gfx_bank=3;
	gfx_base=vaportra_pf4_data;
	tilemap_update(pf4_tilemap);

	tilemap_update(pf1_tilemap);
	vaportra_update_palette();

	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);

	if (pri==0) {
		tilemap_draw(bitmap,pf4_tilemap,0);
		tilemap_draw(bitmap,pf2_tilemap,0);
		vaportra_drawsprites(bitmap,0);
		tilemap_draw(bitmap,pf3_tilemap,0);
	}
	else if (pri==1) {
		tilemap_draw(bitmap,pf2_tilemap,0);
		tilemap_draw(bitmap,pf4_tilemap,0);
		vaportra_drawsprites(bitmap,0);
		tilemap_draw(bitmap,pf3_tilemap,0);
	}
	else if (pri==2) {
		tilemap_draw(bitmap,pf4_tilemap,0);
		tilemap_draw(bitmap,pf3_tilemap,0);
		vaportra_drawsprites(bitmap,0);
		tilemap_draw(bitmap,pf2_tilemap,0);
	}
	else {
		tilemap_draw(bitmap,pf2_tilemap,0);
		tilemap_draw(bitmap,pf3_tilemap,0);
		vaportra_drawsprites(bitmap,0);
		tilemap_draw(bitmap,pf4_tilemap,0);
	}

	vaportra_drawsprites(bitmap,1);
	tilemap_draw(bitmap,pf1_tilemap,0);
}
