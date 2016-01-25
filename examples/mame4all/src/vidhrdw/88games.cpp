#include "driver.h"
#include "vidhrdw/konamiic.h"


int k88games_priority;

static int layer_colorbase[3],sprite_colorbase,zoom_colorbase;



/***************************************************************************

  Callbacks for the K052109

***************************************************************************/

static void tile_callback(int layer,int bank,int *code,int *color)
{
	*code |= ((*color & 0x0f) << 8) | (bank << 12);
	*color = layer_colorbase[layer] + ((*color & 0xf0) >> 4);
}


/***************************************************************************

  Callbacks for the K051960

***************************************************************************/

static void sprite_callback(int *code,int *color,int *priority,int *shadow)
{
	*priority = (*color & 0x20) >> 5;	/* ??? */
	*color = sprite_colorbase + (*color & 0x0f);
}


/***************************************************************************

  Callbacks for the K051316

***************************************************************************/

static void zoom_callback(int *code,int *color)
{
	tile_info.flags = (*color & 0x40) ? TILE_FLIPX : 0;
	*code |= ((*color & 0x07) << 8);
	*color = zoom_colorbase + ((*color & 0x38) >> 3) + ((*color & 0x80) >> 4);
}


/***************************************************************************

	Start the video hardware emulation.

***************************************************************************/

int k88games_vh_start(void)
{
	layer_colorbase[0] = 64;
	layer_colorbase[1] = 0;
	layer_colorbase[2] = 16;
	sprite_colorbase = 32;
	zoom_colorbase = 48;
	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,tile_callback))
	{
		return 1;
	}
	if (K051960_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}
	if (K051316_vh_start_0(REGION_GFX3,4,zoom_callback))
	{
		K052109_vh_stop();
		K051960_vh_stop();
		return 1;
	}

	return 0;
}

void k88games_vh_stop(void)
{
	K052109_vh_stop();
	K051960_vh_stop();
	K051316_vh_stop_0();
}



/***************************************************************************

  Display refresh

***************************************************************************/

void k88games_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;


	K052109_tilemap_update();
	K051316_tilemap_update_0();

	palette_init_used_colors();
	K051960_mark_sprites_colors();
	/* set back pen for the zoom layer */
	for (i = 0;i < 16;i++)
		palette_used_colors[(zoom_colorbase + i) * 16] = PALETTE_COLOR_TRANSPARENT;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	if (k88games_priority)
	{
		K052109_tilemap_draw(bitmap,0,TILEMAP_IGNORE_TRANSPARENCY);
		K051960_sprites_draw(bitmap,1,1);
		K052109_tilemap_draw(bitmap,2,0);
		K052109_tilemap_draw(bitmap,1,0);
		K051960_sprites_draw(bitmap,0,0);
		K051316_zoom_draw_0(bitmap,0);
	}
	else
	{
		K052109_tilemap_draw(bitmap,2,TILEMAP_IGNORE_TRANSPARENCY);
		K051316_zoom_draw_0(bitmap,0);
		K051960_sprites_draw(bitmap,0,0);
		K052109_tilemap_draw(bitmap,1,0);
		K051960_sprites_draw(bitmap,1,1);
		K052109_tilemap_draw(bitmap,0,0);
	}
}
