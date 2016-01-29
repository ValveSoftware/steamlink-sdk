#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"

static int layer_colorbase[2];
static int rockrage_vreg;

void rockrage_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])

	/* build the lookup table for sprites. Palette is dynamic. */
	for (i = 0;i < TOTAL_COLORS(0)/2; i++){
		COLOR(0,i) = 0x00 + (color_prom[i] & 0x0f);
		COLOR(0,(TOTAL_COLORS(0)/2)+i) = 0x10 + (color_prom[0x100+i] & 0x0f);
	}
}

/***************************************************************************

  Callback for the K007342

***************************************************************************/

static void tile_callback(int layer, int bank, int *code, int *color)
{
	if (layer == 1)
		*code |= ((*color & 0x40) << 2) | ((bank & 0x01) << 9);
	else
		*code |= ((*color & 0x40) << 2) | ((bank & 0x03) << 10) | ((rockrage_vreg & 0x04) << 7) | ((rockrage_vreg & 0x08) << 9);
	*color = layer_colorbase[layer] + (*color & 0x0f);
}

/***************************************************************************

  Callback for the K007420

***************************************************************************/

static void sprite_callback(int *code,int *color)
{
	*code |= ((*color & 0x40) << 2) | ((*color & 0x80) << 1)*(rockrage_vreg << 1);
	*code = (*code << 2) | ((*color & 0x30) >> 4);
	*color = 0;
}


WRITE_HANDLER( rockrage_vreg_w ){
	/* bits 4-7: unused */
	/* bit 3: bit 4 of bank # (layer 0) */
	/* bit 2: bit 1 of bank # (layer 0) */
	/* bits 0-1: sprite bank select */

	if ((data & 0x0c) != (rockrage_vreg & 0x0c))
		tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);

	rockrage_vreg = data;
}

/***************************************************************************

	Start the video hardware emulation.

***************************************************************************/

int rockrage_vh_start(void)
{
	layer_colorbase[0] = 0x00;
	layer_colorbase[1] = 0x10;

	if (K007342_vh_start(0,tile_callback))
	{
		return 1;
	}

	if (K007420_vh_start(1,sprite_callback))
	{
		K007420_vh_stop();
		return 1;
	}

	return 0;
}

void rockrage_vh_stop(void)
{
	K007342_vh_stop();
	K007420_vh_stop();
}

/***************************************************************************

  Screen Refresh

***************************************************************************/

void rockrage_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	K007342_tilemap_update();

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render( ALL_TILEMAPS );

	K007342_tilemap_draw( bitmap, 0, TILEMAP_IGNORE_TRANSPARENCY );
	K007420_sprites_draw( bitmap );
	K007342_tilemap_draw( bitmap, 0, 1 | TILEMAP_IGNORE_TRANSPARENCY );
	K007342_tilemap_draw( bitmap, 1, 0 );
	K007342_tilemap_draw( bitmap, 1, 1 );
}
