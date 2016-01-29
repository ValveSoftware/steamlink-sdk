#include "driver.h"
#include "tilemap.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"

#define SPRITEROM_MEM_REGION REGION_GFX1
#define ZOOMROM0_MEM_REGION REGION_GFX2
#define ZOOMROM1_MEM_REGION REGION_GFX3
#define ZOOMROM2_MEM_REGION REGION_GFX4

static int sprite_colorbase, zoom_colorbase[3];

extern unsigned char* ultraman_regs;

/***************************************************************************

  Callbacks for the K051960

***************************************************************************/

static void sprite_callback(int *code,int *color,int *priority,int *shadow)
{
	*priority = (*color & 0x80) >> 7;
	*color = sprite_colorbase + ((*color & 0x7e) >> 1);
}


/***************************************************************************

  Callbacks for the K051316

***************************************************************************/

static void zoom_callback_0(int *code,int *color)
{
	unsigned short data = READ_WORD(&ultraman_regs[0x18]);
	*code |= ((*color & 0x07) << 8) | (data & 0x02) << 10;
	*color = zoom_colorbase[0] + ((*color & 0xf8) >> 3);
}

static void zoom_callback_1(int *code,int *color)
{
	unsigned short data = READ_WORD(&ultraman_regs[0x18]);
	*code |= ((*color & 0x07) << 8) | (data & 0x08) << 8;
	*color = zoom_colorbase[1] + ((*color & 0xf8) >> 3);
}

static void zoom_callback_2(int *code,int *color)
{
	unsigned short data = READ_WORD(&ultraman_regs[0x18]);
	*code |= ((*color & 0x07) << 8) | (data & 0x20) << 6;
	*color = zoom_colorbase[2] + ((*color & 0xf8) >> 3);
}



/***************************************************************************

	Start the video hardware emulation.

***************************************************************************/

int ultraman_vh_start(void)
{
	sprite_colorbase = 192;
	zoom_colorbase[0] = 0;
	zoom_colorbase[1] = 64;
	zoom_colorbase[2] = 128;

	if (K051960_vh_start(SPRITEROM_MEM_REGION,NORMAL_PLANE_ORDER,sprite_callback))
	{
		return 1;
	}

	if (K051316_vh_start_0(ZOOMROM0_MEM_REGION,4,zoom_callback_0))
	{
		K051960_vh_stop();
		return 1;
	}

	if (K051316_vh_start_1(ZOOMROM1_MEM_REGION,4,zoom_callback_1))
	{
		K051960_vh_stop();
		K051316_vh_stop_0();
		return 1;
	}

	if (K051316_vh_start_2(ZOOMROM2_MEM_REGION,4,zoom_callback_2))
	{
		K051960_vh_stop();
		K051316_vh_stop_0();
		K051316_vh_stop_1();
		return 1;
	}

	K051316_set_offset(0, 8, 0);
	K051316_set_offset(1, 8, 0);
	K051316_set_offset(2, 8, 0);

	return 0;
}

void ultraman_vh_stop(void)
{
	K051960_vh_stop();
	K051316_vh_stop_0();
	K051316_vh_stop_1();
	K051316_vh_stop_2();
}



/***************************************************************************

	Display Refresh

***************************************************************************/

void ultraman_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;

	K051316_tilemap_update_0();
	K051316_tilemap_update_1();
	K051316_tilemap_update_2();

	palette_init_used_colors();
	K051960_mark_sprites_colors();

	/* set transparent pens for the K051316 */
	for (i = 0;i < 64;i++)
	{
		palette_used_colors[(zoom_colorbase[0] + i) * 16] = PALETTE_COLOR_TRANSPARENT;
		palette_used_colors[(zoom_colorbase[1] + i) * 16] = PALETTE_COLOR_TRANSPARENT;
		palette_used_colors[(zoom_colorbase[2] + i) * 16] = PALETTE_COLOR_TRANSPARENT;
	}

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(bitmap,Machine->pens[zoom_colorbase[2] * 16],&Machine->visible_area);

	K051316_zoom_draw_2(bitmap,0);
	K051316_zoom_draw_1(bitmap,0);
	K051960_sprites_draw(bitmap,0,0);
	K051316_zoom_draw_0(bitmap,0);
	K051960_sprites_draw(bitmap,1,1);
}
