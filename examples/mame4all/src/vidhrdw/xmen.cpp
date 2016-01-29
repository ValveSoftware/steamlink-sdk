#include "driver.h"
#include "vidhrdw/konamiic.h"


static int layer_colorbase[3],sprite_colorbase,bg_colorbase;
static int layerpri[3];


/***************************************************************************

  Callbacks for the K052109

***************************************************************************/

static void xmen_tile_callback(int layer,int bank,int *code,int *color)
{
	/* (color & 0x02) is flip y handled internally by the 052109 */
	if (layer == 0)
		*color = layer_colorbase[layer] + ((*color & 0xf0) >> 4);
	else
		*color = layer_colorbase[layer] + ((*color & 0x7c) >> 2);
}

/***************************************************************************

  Callbacks for the K053247

***************************************************************************/

static void xmen_sprite_callback(int *code,int *color,int *priority_mask)
{
	int pri = (*color & 0x00e0) >> 4;	/* ??????? */
	if (pri <= layerpri[2])								*priority_mask = 0;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority_mask = 0xf0;
	else if (pri > layerpri[1] && pri <= layerpri[0])	*priority_mask = 0xf0|0xcc;
	else 												*priority_mask = 0xf0|0xcc|0xaa;

	*color = sprite_colorbase + (*color & 0x001f);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int xmen_vh_start(void)
{
	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,xmen_tile_callback))
		return 1;
	if (K053247_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,xmen_sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}
	return 0;
}

void xmen_vh_stop(void)
{
	K052109_vh_stop();
	K053247_vh_stop();
}



/***************************************************************************

  Display refresh

***************************************************************************/

/* useful function to sort the three tile layers by priority order */
static void sortlayers(int *layer,int *pri)
{
#define SWAP(a,b) \
	if (pri[a] < pri[b]) \
	{ \
		int t; \
		t = pri[a]; pri[a] = pri[b]; pri[b] = t; \
		t = layer[a]; layer[a] = layer[b]; layer[b] = t; \
	}

	SWAP(0,1)
	SWAP(0,2)
	SWAP(1,2)
}


void xmen_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layer[3];


	bg_colorbase       = K053251_get_palette_index(K053251_CI4);
	sprite_colorbase   = K053251_get_palette_index(K053251_CI1);
	layer_colorbase[0] = K053251_get_palette_index(K053251_CI3);
	layer_colorbase[1] = K053251_get_palette_index(K053251_CI0);
	layer_colorbase[2] = K053251_get_palette_index(K053251_CI2);

	K052109_tilemap_update();

	palette_init_used_colors();
	K053247_mark_sprites_colors();
	palette_used_colors[16 * bg_colorbase+1] |= PALETTE_COLOR_VISIBLE;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	layer[0] = 0;
	layerpri[0] = K053251_get_priority(K053251_CI3);
	layer[1] = 1;
	layerpri[1] = K053251_get_priority(K053251_CI0);
	layer[2] = 2;
	layerpri[2] = K053251_get_priority(K053251_CI2);

	sortlayers(layer,layerpri);

	fillbitmap(priority_bitmap,0,NULL);
	/* note the '+1' in the background color!!! */
	fillbitmap(bitmap,Machine->pens[16 * bg_colorbase+1],&Machine->visible_area);
	K052109_tilemap_draw(bitmap,layer[0],1<<16);
	K052109_tilemap_draw(bitmap,layer[1],2<<16);
	K052109_tilemap_draw(bitmap,layer[2],4<<16);

	K053247_sprites_draw(bitmap);
}
