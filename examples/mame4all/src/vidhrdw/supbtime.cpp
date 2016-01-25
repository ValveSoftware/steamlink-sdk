/***************************************************************************

   Super Burger Time Video emulation - Bryan McPhail, mish@tendril.co.uk

*********************************************************************

Uses Data East custom chip 55 for backgrounds, custom chip 52 for sprites.

See Dark Seal & Caveman Ninja drivers for info on these chips.

End sequence uses rowscroll '98 c0' on pf1 (jmp to 1d61a)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *supbtime_pf2_data,*supbtime_pf1_data,*supbtime_pf1_row;
static unsigned char supbtime_control_0[16];
static struct tilemap *pf1_tilemap,*pf2_tilemap;
static int flipscreen;

/******************************************************************************/

static void supbtime_mark_sprite_colours(void)
{
	int offs,color,i,pal_base;
	int colmask[16];
    unsigned int *pen_usage;

	palette_init_used_colors();

	pen_usage=Machine->gfx[2]->pen_usage;
	pal_base = Machine->drv->gfxdecodeinfo[2].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;

	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,multi;

		sprite = READ_WORD (&spriteram[offs+2]) & 0x3fff;
		if (!sprite) continue;

		y = READ_WORD(&spriteram[offs]);
		x = READ_WORD(&spriteram[offs+4]);
		color = (x >>9) &0xf;

		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

		sprite &= ~multi;

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

static void supbtime_drawsprites(struct osd_bitmap *bitmap)
{
	int offs;

	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash,mult;

		sprite = READ_WORD (&spriteram[offs+2]) & 0x3fff;
		if (!sprite) continue;

		y = READ_WORD(&spriteram[offs]);
		flash=y&0x1000;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		x = READ_WORD(&spriteram[offs+4]);
		colour = (x >>9) & 0x1f;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 320) x -= 512;
		if (y >= 256) y -= 512;
		y = 240 - y;
        x = 304 - x;

		if (x>320) continue;

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
			x=304-x;
			if (fx) fx=0; else fx=1;
			if (fy) fy=0; else fy=1;
			mult=16;
		}
		else mult=-16;

		while (multi >= 0)
		{
			drawgfx(bitmap,Machine->gfx[2],
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

WRITE_HANDLER( supbtime_pf2_data_w )
{
	COMBINE_WORD_MEM(&supbtime_pf2_data[offset],data);
	tilemap_mark_tile_dirty(pf2_tilemap,offset/2);
}

WRITE_HANDLER( supbtime_pf1_data_w )
{
	COMBINE_WORD_MEM(&supbtime_pf1_data[offset],data);
	tilemap_mark_tile_dirty(pf1_tilemap,offset/2);
}

READ_HANDLER( supbtime_pf1_data_r )
{
	return READ_WORD(&supbtime_pf1_data[offset]);
}

READ_HANDLER( supbtime_pf2_data_r )
{
	return READ_WORD(&supbtime_pf2_data[offset]);
}

WRITE_HANDLER( supbtime_control_0_w )
{
	COMBINE_WORD_MEM(&supbtime_control_0[offset],data);
}

/******************************************************************************/

static UINT32 supbtime_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return (col & 0x1f) + ((row & 0x1f) << 5) + ((col & 0x20) << 5);
}

static void get_bg_tile_info(int tile_index)
{
	int tile,color;

	tile=READ_WORD(&supbtime_pf2_data[2*tile_index]);
	color=tile >> 12;
	tile=tile&0xfff;

	SET_TILE_INFO(1,tile,color)
}

static void get_fg_tile_info(int tile_index)
{
	int tile=READ_WORD(&supbtime_pf1_data[2*tile_index]);
	int color=tile >> 12;

	tile=tile&0xfff;
	SET_TILE_INFO(0,tile,color)
}

int supbtime_vh_start(void)
{
	pf1_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,64,64);
	pf2_tilemap = tilemap_create(get_bg_tile_info,supbtime_scan,    TILEMAP_TRANSPARENT,16,16,64,32);

	if (!pf1_tilemap || !pf2_tilemap)
		return 1;

	pf1_tilemap->transparent_pen = 0;
	pf2_tilemap->transparent_pen = 0;

	return 0;
}

/******************************************************************************/

void supbtime_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	flipscreen=(READ_WORD(&supbtime_control_0[0])&0x80);
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);

	tilemap_set_scrollx( pf1_tilemap,0, READ_WORD(&supbtime_control_0[2]) );
	tilemap_set_scrolly( pf1_tilemap,0, READ_WORD(&supbtime_control_0[4]) );
	tilemap_set_scrollx( pf2_tilemap,0, READ_WORD(&supbtime_control_0[6]) );
	tilemap_set_scrolly( pf2_tilemap,0, READ_WORD(&supbtime_control_0[8]) );

	/* 'Fake' rowscroll, used only in the end game message */
	if (READ_WORD (&supbtime_control_0[0xc])==0xc0)
		tilemap_set_scrollx( pf1_tilemap,0, READ_WORD(&supbtime_control_0[2]) + READ_WORD (&supbtime_pf1_row[8]) );

	tilemap_update(pf2_tilemap);
	tilemap_update(pf1_tilemap);

	supbtime_mark_sprite_colours();
	palette_used_colors[768] = PALETTE_COLOR_USED;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	/* The filled bitmap is unusual for Data East, but without this the title screen
	background colour is incorrect.  This also explains why the game initialises
	the previously unused palette ram to zero */
	fillbitmap(bitmap,Machine->pens[768],&Machine->visible_area);
	tilemap_draw(bitmap,pf2_tilemap,0);
	supbtime_drawsprites(bitmap);
	tilemap_draw(bitmap,pf1_tilemap,0);
}
