#include "driver.h"
#include "vidhrdw/konamiic.h"



int bottom9_video_enable;

static int layer_colorbase[3],sprite_colorbase,zoom_colorbase;


/***************************************************************************

  Callbacks for the K052109

***************************************************************************/

static void tile_callback(int layer,int bank,int *code,int *color)
{
	*code |= (*color & 0x3f) << 8;
	*color = layer_colorbase[layer] + ((*color & 0xc0) >> 6);
}


/***************************************************************************

  Callbacks for the K051960

***************************************************************************/

static void sprite_callback(int *code,int *color,int *priority,int *shadow)
{
	/* bit 4 = priority over zoom (0 = have priority) */
	/* bit 5 = priority over B (1 = have priority) */
	*priority = (*color & 0x30) >> 4;
	*color = sprite_colorbase + (*color & 0x0f);
}


/***************************************************************************

  Callbacks for the K051316

***************************************************************************/

static void zoom_callback(int *code,int *color)
{
	tile_info.flags = (*color & 0x40) ? TILE_FLIPX : 0;
	*code |= ((*color & 0x03) << 8);
	*color = zoom_colorbase + ((*color & 0x3c) >> 2);
}


/***************************************************************************

	Start the video hardware emulation.

***************************************************************************/

int bottom9_vh_start(void)
{
	layer_colorbase[0] = 0;	/* not used */
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

void bottom9_vh_stop(void)
{
	K052109_vh_stop();
	K051960_vh_stop();
	K051316_vh_stop_0();
}



/***************************************************************************

  Display refresh

***************************************************************************/

void bottom9_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
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

	/* note: FIX layer is not used */
	fillbitmap(bitmap,Machine->pens[layer_colorbase[1]],&Machine->visible_area);
//	if (bottom9_video_enable)
	{
		K051960_sprites_draw(bitmap,1,1);
		K051316_zoom_draw_0(bitmap,0);
		K051960_sprites_draw(bitmap,0,0);
		K052109_tilemap_draw(bitmap,2,0);
		/* note that priority 3 is opposite to the basic layer priority! */
		/* (it IS used, but hopefully has no effect) */
		K051960_sprites_draw(bitmap,2,3);
		K052109_tilemap_draw(bitmap,1,0);
	}
}
