#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"


static int bg_colorbase,sprite_colorbase,zoom_colorbase;



/***************************************************************************

  Callbacks for the K053245

***************************************************************************/

static void sprite_callback(int *code,int *color,int *priority_mask)
{
#if 0
if (keyboard_pressed(KEYCODE_Q) && (*color & 0x80)) *color = rand();
if (keyboard_pressed(KEYCODE_W) && (*color & 0x40)) *color = rand();
if (keyboard_pressed(KEYCODE_E) && (*color & 0x20)) *color = rand();
if (keyboard_pressed(KEYCODE_R) && (*color & 0x10)) *color = rand();
#endif
	*priority_mask = (*color & 0x10) ? 0 : 0x02;
	*color = sprite_colorbase + (*color & 0x0f);
}


/***************************************************************************

  Callbacks for the K051316

***************************************************************************/

static void zoom_callback(int *code,int *color)
{
	tile_info.flags = TILE_FLIPYX((*color & 0xc0) >> 6);
	*code |= ((*color & 0x0f) << 8);
	*color = zoom_colorbase + ((*color & 0x30) >> 4);
}



/***************************************************************************

	Start the video hardware emulation.

***************************************************************************/

int rollerg_vh_start(void)
{
	bg_colorbase = 16;
	sprite_colorbase = 16;
	zoom_colorbase = 0;

	if (K053245_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,sprite_callback))
		return 1;
	if (K051316_vh_start_0(REGION_GFX2,4,zoom_callback))
	{
		K053245_vh_stop();
		return 1;
	}

	K051316_set_offset(0, 22, 1);
	return 0;
}

void rollerg_vh_stop(void)
{
	K053245_vh_stop();
	K051316_vh_stop_0();
}



/***************************************************************************

  Display refresh

***************************************************************************/

void rollerg_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;


	K051316_tilemap_update_0();

	palette_init_used_colors();
	K053245_mark_sprites_colors();
	/* set back pen for the zoom layer */
	for (i = 0;i < 16;i++)
		palette_used_colors[(zoom_colorbase + i) * 16] = PALETTE_COLOR_TRANSPARENT;
	palette_used_colors[16 * bg_colorbase] |= PALETTE_COLOR_VISIBLE;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[16 * bg_colorbase],&Machine->visible_area);
	K051316_zoom_draw_0(bitmap,1);

	K053245_sprites_draw(bitmap);
}
