#include "driver.h"
#include "vidhrdw/konamiic.h"



int gbusters_priority;
static int layer_colorbase[3],sprite_colorbase;

/***************************************************************************

  Callbacks for the K052109

***************************************************************************/

static void tile_callback(int layer,int bank,int *code,int *color)
{
	/* (color & 0x02) is flip y handled internally by the 052109 */
	*code |= ((*color & 0x0d) << 8) | ((*color & 0x10) << 5) | (bank << 12);
	*color = layer_colorbase[layer] + ((*color & 0xe0) >> 5);
}

/***************************************************************************

  Callbacks for the K051960

***************************************************************************/

static void sprite_callback(int *code,int *color,int *priority,int *shadow)
{
	*priority = (*color & 0x30) >> 4;
	*color = sprite_colorbase + (*color & 0x0f);
}


/***************************************************************************

	Start the video hardware emulation.

***************************************************************************/

int gbusters_vh_start(void)
{
	layer_colorbase[0] = 48;
	layer_colorbase[1] = 0;
	layer_colorbase[2] = 16;
	sprite_colorbase = 32;

	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,tile_callback))
		return 1;
	if (K051960_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}

	return 0;
}

void gbusters_vh_stop(void)
{
	K052109_vh_stop();
	K051960_vh_stop();
}


void gbusters_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	K052109_tilemap_update();

	palette_init_used_colors();
	K051960_mark_sprites_colors();
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	/* sprite priority 3 = disable */
	if (gbusters_priority)
	{
//		K051960_sprites_draw(bitmap,1,1);	/* are these used? */
		K052109_tilemap_draw(bitmap,2,TILEMAP_IGNORE_TRANSPARENCY);
		K051960_sprites_draw(bitmap,2,2);
		K052109_tilemap_draw(bitmap,1,0);
		K051960_sprites_draw(bitmap,0,0);
		K052109_tilemap_draw(bitmap,0,0);
	}
	else
	{
//		K051960_sprites_draw(bitmap,1,1);	/* are these used? */
		K052109_tilemap_draw(bitmap,1,TILEMAP_IGNORE_TRANSPARENCY);
		K051960_sprites_draw(bitmap,2,2);
		K052109_tilemap_draw(bitmap,2,0);
		K051960_sprites_draw(bitmap,0,0);
		K052109_tilemap_draw(bitmap,0,0);
	}
}
