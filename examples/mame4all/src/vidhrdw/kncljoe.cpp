/***************************************************************************

Knuckle Joe - (c) 1985 Taito Corporation

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static struct tilemap *bg_tilemap;
static int tile_bank,sprite_bank;
static int flipscreen;


/***************************************************************************

  Convert the color PROMs into a more useable format.

***************************************************************************/

void kncljoe_vh_convert_color_prom(unsigned char *palette,unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < 128;i++)
	{
		int bit0,bit1,bit2,bit3;

		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[0x100] >> 0) & 0x01;
		bit1 = (color_prom[0x100] >> 1) & 0x01;
		bit2 = (color_prom[0x100] >> 2) & 0x01;
		bit3 = (color_prom[0x100] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[0x200] >> 0) & 0x01;
		bit1 = (color_prom[0x200] >> 1) & 0x01;
		bit2 = (color_prom[0x200] >> 2) & 0x01;
		bit3 = (color_prom[0x200] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}

	color_prom += 2*256 + 128;	/* bottom half is not used */

	for (i = 0;i < 16;i++)
	{
		int bit0,bit1,bit2;

		/* red component */
		bit0 = 0;
		bit1 = (*color_prom >> 6) & 0x01;
		bit2 = (*color_prom >> 7) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (*color_prom >> 3) & 0x01;
		bit1 = (*color_prom >> 4) & 0x01;
		bit2 = (*color_prom >> 5) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = (*color_prom >> 0) & 0x01;
		bit1 = (*color_prom >> 1) & 0x01;
		bit2 = (*color_prom >> 2) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		color_prom ++;
	}

	color_prom += 16;	/* bottom half is not used */

	/* sprite lookup table */
	for (i = 0;i < 128;i++)
	{
		COLOR(1,i) = 128 + (*(color_prom++) & 0x0f);
	}
}



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_bg_tile_info(int tile_index)
{
	int attr = videoram[2*tile_index+1];
	int code = videoram[2*tile_index] + ((attr & 0xc0) << 2) + (tile_bank << 10);

	SET_TILE_INFO(0,code,attr & 0xf)

	tile_info.flags = TILE_FLIPXY((attr & 0x30) >> 4);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int kncljoe_vh_start( void )
{
	bg_tilemap = tilemap_create(get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,8,8,64,32);

	if (!bg_tilemap)
		return 1;

	tilemap_set_scroll_rows(bg_tilemap,4);
	//tilemap_set_flip(ALL_TILEMAPS, TILEMAP_FLIPY);
	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( kncljoe_videoram_w )
{
	if (videoram[offset] != data)
	{
		videoram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset/2);
	}
}

WRITE_HANDLER( kncljoe_control_w )
{
/*	0x01	screen flip
	0x02	coin counter#1
	0x04	sprite bank
	0x10	character bank
	0x20	coin counter#2
*/
	/* coin counters:
		reset when IN0 - Coin 1 goes low (active)
		set after IN0 - Coin 1 goes high AND the credit has been added
	*/

	flipscreen = data & 0x01;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? TILEMAP_FLIPX : TILEMAP_FLIPY);

	coin_counter_w(0,data & 0x02);
	coin_counter_w(1,data & 0x20);

	if (tile_bank != ((data & 0x10) >> 4))
	{
		tile_bank = (data & 0x10) >> 4;
		tilemap_mark_all_tiles_dirty(bg_tilemap);
	}

	sprite_bank = (data & 0x04) >> 2;
}

WRITE_HANDLER( kncljoe_scroll_w )
{
	tilemap_set_scrollx(bg_tilemap,0,data);
	tilemap_set_scrollx(bg_tilemap,1,data);
	tilemap_set_scrollx(bg_tilemap,2,data);
	tilemap_set_scrollx(bg_tilemap,3,0);
}



/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_sprites( struct osd_bitmap *bitmap )
{
	struct rectangle clip = Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[1 + sprite_bank];
	int offs;

	/* score covers sprites */
	if (flipscreen)
		clip.max_y -= 64;
	else
		clip.min_y += 64;

	for (offs = spriteram_size;offs >= 0;offs -= 4)
	{
		int sy = spriteram[offs];
		int sx = spriteram[offs+3];
		int code = spriteram[offs+2];
		int attr = spriteram[offs+1];
		int flipx = attr & 0x40;
		int flipy = !(attr & 0x80);
		int color = attr & 0x0f;
		if (attr & 0x10) code += 512;
		if (attr & 0x20) code += 256;

		if (flipscreen)
		{
			flipx = !flipx;
			flipy = !flipy;
			sx = 240 - sx;
			sy = 240 - sy;
		}

		drawgfx(bitmap,gfx,
				code,
				color,
				flipx,flipy,
				sx,sy,
				&clip,TRANSPARENCY_PEN,0);
	}
}

void kncljoe_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_update(ALL_TILEMAPS);
        tilemap_render(ALL_TILEMAPS);

	palette_recalc();

	tilemap_draw(bitmap,bg_tilemap,0);
	draw_sprites(bitmap);
}
