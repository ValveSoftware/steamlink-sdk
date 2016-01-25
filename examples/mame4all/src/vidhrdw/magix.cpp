/***************************************************************************

								-=  Magix  =-
							 (c) 1995  Yun Sung

				driver by	Luca Elia (eliavit@unina.it)


Note:	if MAME_DEBUG is defined, pressing Z with:

		Q 		shows the background layer
		W 		shows the foreground layer

		[ 2 Fixed Layers ]

			[ Background ]

			Layer Size:				512 x 256
			Tiles:					6 x 8 x 8 (!)

			[ Foreground ]

			Layer Size:				512 x 256
			Tiles:					8 x 8 x 4


		There are no sprites.

***************************************************************************/
#include "vidhrdw/generic.h"


/* Variables that driver has access to: */

unsigned char *magix_videoram_0,*magix_videoram_1;


/* Variables only used here: */

static struct tilemap *tilemap_0, *tilemap_1;
static int magix_videobank;


/***************************************************************************

							Memory Handlers

***************************************************************************/

WRITE_HANDLER( magix_videobank_w )
{
	magix_videobank = data;
}


READ_HANDLER( magix_videoram_r )
{
	int bank;

	/*	Bit 1 of the bankswitching register contols the c000-c7ff
		area (Palette). Bit 0 controls the c800-dfff area (Tiles) */

	if (offset < 0x0800)	bank = magix_videobank & 2;
	else					bank = magix_videobank & 1;

	if (bank)	return magix_videoram_0[offset];
	else		return magix_videoram_1[offset];
}


WRITE_HANDLER( magix_videoram_w )
{
	if (offset < 0x0800)		// c000-c7ff	Banked Palette RAM
	{
		int bank = magix_videobank & 2;
		unsigned char *RAM;
		int r,g,b;

		if (bank)	RAM = magix_videoram_0;
		else		RAM = magix_videoram_1;

		RAM[offset] = data;
		data = RAM[offset & ~1] | (RAM[offset | 1] << 8);

		/* BBBBBGGGGGRRRRRx */
		r = (data >>  0) & 0x1f;
		g = (data >>  5) & 0x1f;
		b = (data >> 10) & 0x1f;

		palette_change_color(offset/2 + (bank ? 0x400:0), (r << 3)|(r >> 2), (g << 3)|(g >> 2), (b << 3)|(b >> 2));
	}
	else
	{
		int tile;
		int bank = magix_videobank & 1;

		if (offset < 0x1000)	tile = (offset-0x0800);		// c800-cfff: Banked Color RAM
		else				 	tile = (offset-0x1000)/2;	// d000-dfff: Banked Tiles RAM

		if (bank)	{	magix_videoram_0[offset] = data;
						tilemap_mark_tile_dirty(tilemap_0, tile);	}
		else		{	magix_videoram_1[offset] = data;
						tilemap_mark_tile_dirty(tilemap_1, tile);	}
	}
}


WRITE_HANDLER( magix_flipscreen_w )
{
	tilemap_set_flip(ALL_TILEMAPS, (data & 1) ? (TILEMAP_FLIPX|TILEMAP_FLIPY) : 0);
}




/***************************************************************************

						Callbacks for the TileMap code

***************************************************************************/


/***************************************************************************

							  [ Tiles Format ]

Offset:

	Videoram + 0000.w		Code
	Colorram + 0000.b		Color


***************************************************************************/

/* Background */

#define DIM_NX_0			(0x40)
#define DIM_NY_0			(0x20)

static void get_tile_info_0( int tile_index )
{
	int code  =  magix_videoram_0[0x1000+tile_index * 2 + 0] + magix_videoram_0[0x1000+tile_index * 2 + 1] * 256;
	int color =  magix_videoram_0[0x0800+ tile_index] & 0x07;
	SET_TILE_INFO( 0, code, color );
}

/* Text Plane */

#define DIM_NX_1			(0x40)
#define DIM_NY_1			(0x20)

static void get_tile_info_1( int tile_index )
{
	int code  =  magix_videoram_1[0x1000+ tile_index * 2 + 0] + magix_videoram_1[0x1000+tile_index * 2 + 1] * 256;
	int color =  magix_videoram_1[0x0800+ tile_index] & 0x3f;
	SET_TILE_INFO( 1, code, color );
}




/***************************************************************************


								Vh_Start


***************************************************************************/

int magix_vh_start(void)
{
	tilemap_0 = tilemap_create(	get_tile_info_0,
								tilemap_scan_rows,
								TILEMAP_OPAQUE,
								8,8,
								DIM_NX_0, DIM_NY_0 );

	tilemap_1 = tilemap_create(	get_tile_info_1,
								tilemap_scan_rows,
								TILEMAP_TRANSPARENT,
								8,8,
								DIM_NX_1, DIM_NY_1 );

	if (tilemap_0 && tilemap_1)
	{
		tilemap_1->transparent_pen = 0;
		return 0;
	}
	else return 1;
}



/***************************************************************************


								Screen Drawing


***************************************************************************/

void magix_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layers_ctrl = -1;

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();

	/* No Sprites ... */

	if (palette_recalc())	tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	if (layers_ctrl&1)	tilemap_draw(bitmap, tilemap_0, 0);
	else				osd_clearbitmap(bitmap);

	if (layers_ctrl&2)	tilemap_draw(bitmap, tilemap_1, 0);
}
