/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"


static int layer_colorbase[3],sprite_colorbase;



/***************************************************************************

  Callbacks for the K052109

***************************************************************************/

static void mainevt_tile_callback(int layer,int bank,int *code,int *color)
{
	tile_info.flags = (*color & 0x02) ? TILE_FLIPX : 0;

	/* priority relative to HALF priority sprites */
	if (layer == 2) tile_info.priority = (*color & 0x20) >> 5;
	else tile_info.priority = 0;

	*code |= ((*color & 0x01) << 8) | ((*color & 0x1c) << 7);
	*color = layer_colorbase[layer] + ((*color & 0xc0) >> 6);
}

static void dv_tile_callback(int layer,int bank,int *code,int *color)
{
	/* (color & 0x02) is flip y handled internally by the 052109 */
	*code |= ((*color & 0x01) << 8) | ((*color & 0x3c) << 7);
	*color = layer_colorbase[layer] + ((*color & 0xc0) >> 6);
}


/***************************************************************************

  Callbacks for the K051960

***************************************************************************/

static void mainevt_sprite_callback(int *code,int *color,int *priority_mask,int *shadow)
{
	/* bit 5 = priority over layer B (has precedence) */
	/* bit 6 = HALF priority over layer B (used for crowd when you get out of the ring) */
	if (*color & 0x20)		*priority_mask = 0xff00;
	else if (*color & 0x40)	*priority_mask = 0xff00|0xf0f0;
	else					*priority_mask = 0xff00|0xf0f0|0xcccc;
	/* bit 7 is shadow, not used */

	*color = sprite_colorbase + (*color & 0x03);
}

static void dv_sprite_callback(int *code,int *color,int *priority,int *shadow)
{
	/* TODO: the priority/shadow handling (bits 5-7) seems to be quite complex (see PROM) */
	*color = sprite_colorbase + (*color & 0x07);
}


/*****************************************************************************/

int mainevt_vh_start(void)
{
	layer_colorbase[0] = 0;
	layer_colorbase[1] = 8;
	layer_colorbase[2] = 4;
	sprite_colorbase = 12;

	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,mainevt_tile_callback))
		return 1;
	if (K051960_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,mainevt_sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}

	return 0;
}

int dv_vh_start(void)
{
	layer_colorbase[0] = 0;
	layer_colorbase[1] = 0;
	layer_colorbase[2] = 4;
	sprite_colorbase = 8;

	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,dv_tile_callback))
		return 1;
	if (K051960_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,dv_sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}

	return 0;
}

void mainevt_vh_stop(void)
{
	K052109_vh_stop();
	K051960_vh_stop();
}

/*****************************************************************************/

void mainevt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	K052109_tilemap_update();

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(priority_bitmap,0,NULL);
	K052109_tilemap_draw(bitmap,1,TILEMAP_IGNORE_TRANSPARENCY|(1<<16));
	K052109_tilemap_draw(bitmap,2,1|(2<<16));	/* low priority part of layer */
	K052109_tilemap_draw(bitmap,2,0|(4<<16));	/* high priority part of layer */
	K052109_tilemap_draw(bitmap,0,8<<16);

	K051960_sprites_draw(bitmap,-1,-1);
}

void dv_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	K052109_tilemap_update();

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	K052109_tilemap_draw(bitmap,1,TILEMAP_IGNORE_TRANSPARENCY);
	K052109_tilemap_draw(bitmap,2,0);
	K051960_sprites_draw(bitmap,0,0);
	K052109_tilemap_draw(bitmap,0,0);
}
