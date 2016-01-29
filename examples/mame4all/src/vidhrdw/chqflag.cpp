/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"
#include "cpu/z80/z80.h"

#define SPRITEROM_MEM_REGION REGION_GFX1
#define ZOOMROM0_MEM_REGION REGION_GFX2
#define ZOOMROM1_MEM_REGION REGION_GFX3

int sprite_colorbase,zoom_colorbase[2];

/***************************************************************************

  Callbacks for the K051960

***************************************************************************/

static void sprite_callback(int *code,int *color,int *priority,int *shadow)
{
//	*code |= (*color & 0x80) << 6;
	*color = sprite_colorbase + (*color & 0x0f);
}


/***************************************************************************

  Callbacks for the K051316

***************************************************************************/

static void zoom_callback_0(int *code,int *color)
{
	*code |= ((*color & 0x03) << 8);
	*color = zoom_colorbase[0] + ((*color & 0x3c) >> 2);
}

static void zoom_callback_1(int *code,int *color)
{
	tile_info.flags = TILE_FLIPYX((*color & 0xc0) >> 6);
	*code |= ((*color & 0x0f) << 8);
	*color = zoom_colorbase[1] + ((*color & 0x30) >> 4);
}

/***************************************************************************

	Start the video hardware emulation.

***************************************************************************/

int chqflag_vh_start( void )
{
	sprite_colorbase = 0;
	zoom_colorbase[0] = 0x10;
	zoom_colorbase[1] = 0x04;	/* not sure yet, because of bad dumps */

	if (K051960_vh_start(SPRITEROM_MEM_REGION,NORMAL_PLANE_ORDER,sprite_callback))
	{
		return 1;
	}

	if (K051316_vh_start_0(ZOOMROM0_MEM_REGION,4,zoom_callback_0))
	{
		K051960_vh_stop();
		return 1;
	}

	if (K051316_vh_start_1(ZOOMROM1_MEM_REGION,7,zoom_callback_1))
	{
		K051960_vh_stop();
		K051316_vh_stop_0();
		return 1;
	}

	K051316_set_offset(0, 8, 0);
	K051316_wraparound_enable(1, 1);

	return 0;
}

void chqflag_vh_stop( void )
{
	K051960_vh_stop();
	K051316_vh_stop_0();
	K051316_vh_stop_1();
}

/***************************************************************************

	Display Refresh

***************************************************************************/

void chqflag_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh )
{
	int i;

	K051316_tilemap_update_0();
	K051316_tilemap_update_1();

	palette_init_used_colors();
	K051960_mark_sprites_colors();

	/* set back pen for the zoom layer */
	for (i = 0; i < 16; i++){
		palette_used_colors[(zoom_colorbase[0] + i) * 16] = PALETTE_COLOR_TRANSPARENT;
	}

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);

	K051316_zoom_draw_1(bitmap,0);
	K051960_sprites_draw(bitmap,0,128);
	K051316_zoom_draw_0(bitmap,0);
}
