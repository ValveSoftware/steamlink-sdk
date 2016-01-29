#include "driver.h"
#include "vidhrdw/konamiic.h"


static int layer_colorbase[3],sprite_colorbase;

/***************************************************************************

  Callbacks for the K052109

***************************************************************************/

static void tile_callback(int layer,int bank,int *code,int *color)
{
	*code |= ((*color & 0x3f) << 8) | (bank << 14);
	*color = layer_colorbase[layer] + ((*color & 0xc0) >> 6);
}


/***************************************************************************

  Callbacks for the K051960

***************************************************************************/

static void sprite_callback(int *code,int *color,int *priority_mask,int *shadow)
{
	/* The PROM allows for mixed priorities, where sprites would have */
	/* priority over text but not on one or both of the other two planes. */
	switch (*color & 0x70)
	{
		case 0x10: *priority_mask = 0x00; break;			/* over ABF */
		case 0x00: *priority_mask = 0xf0          ; break;	/* over AB, not F */
		case 0x40: *priority_mask = 0xf0|0xcc     ; break;	/* over A, not BF */
		case 0x20:
		case 0x60: *priority_mask = 0xf0|0xcc|0xaa; break;	/* over -, not ABF */
		case 0x50: *priority_mask =      0xcc     ; break;	/* over AF, not B */
		case 0x30:
		case 0x70: *priority_mask =      0xcc|0xaa; break;	/* over F, not AB */
	}
	*code |= (*color & 0x80) << 6;
	*color = sprite_colorbase + (*color & 0x0f);
	*shadow = 0;	/* shadows are not used by this game */
}



/***************************************************************************

	Start the video hardware emulation.

***************************************************************************/

int aliens_vh_start( void )
{
	paletteram = (unsigned char*)malloc(0x400);
	if (!paletteram) return 1;

	layer_colorbase[0] = 0;
	layer_colorbase[1] = 4;
	layer_colorbase[2] = 8;
	sprite_colorbase = 16;
	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,tile_callback))
	{
		free(paletteram);
		return 1;
	}
	if (K051960_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,sprite_callback))
	{
		free(paletteram);
		K052109_vh_stop();
		return 1;
	}

	return 0;
}

void aliens_vh_stop( void )
{
	free(paletteram);
	K052109_vh_stop();
	K051960_vh_stop();
}



/***************************************************************************

  Display refresh

***************************************************************************/

void aliens_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	K052109_tilemap_update();

	palette_init_used_colors();
	K051960_mark_sprites_colors();
	palette_used_colors[layer_colorbase[1] * 16] |= PALETTE_COLOR_VISIBLE;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[layer_colorbase[1] * 16],&Machine->visible_area);
	K052109_tilemap_draw(bitmap,1,1<<16);
	K052109_tilemap_draw(bitmap,2,2<<16);
	K052109_tilemap_draw(bitmap,0,4<<16);

	K051960_sprites_draw(bitmap,-1,-1);
}
