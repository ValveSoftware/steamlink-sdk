/**************************************************************************

							Ginga NinkyouDen
						    (C) 1987 Jaleco

				    driver by Luca Elia (eliavit@unina.it)


Note:	if MAME_DEBUG is defined, pressing Z with:

		Q		shows background
		W		shows foreground
		E		shows frontmost (text) layer
		A		shows sprites

		Keys can be used togheter!


[Screen]
 	Visible Size:		256H x 240V
	Dynamic Colors:		256 x 4
	Color Space:		16R x 16G x 16B

[Scrolling layers]
	Format (all layers):	Offset:		0x400    0x000
							Bit:		fedc---- --------	Color
										----ba98 76543210	Code

	[Background]
		Size:				8192 x 512	(static: stored in ROM)
		Scrolling:			X,Y			(registers: $60006.w, $60004.w)
		Tiles Size:			16 x 16
		Tiles Number:		$400
		Colors:				$300-$3ff

	[Foreground]
		Size:				4096 x 512
		Scrolling:			X,Y			(registers: $60002.w, $60000.w)
		Tiles Size:			16 x 16
		Tiles Number:		$400
		Colors:				$200-$2ff

	[Frontmost]
		Size:				256 x 256
		Scrolling:			-
		Tiles Size:			8 x 8
		Tiles Number:		$200
		Colors:				$000-$0ff


[Sprites]
	On Screen:			256
	In ROM:				$a00
	Colors:				$100-$1ff
	Format:				See Below


**************************************************************************/
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"

/* Variables only used here */
static struct tilemap *bg_tilemap, *fg_tilemap, *tx_tilemap;
static int layers_ctrl, flipscreen;

/* Variables that driver has access to */
unsigned char *ginganin_fgram, *ginganin_txtram, *ginganin_vregs;

/* Variables defined in drivers */


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/


/* Background - Resides in ROM */

#define BG_GFX (0)
#define BG_NX  (16*32)
#define BG_NY  (16*2)

static void get_bg_tile_info(int tile_index)
{
	int code = memory_region(REGION_GFX5)[2*tile_index + 0] * 256 + memory_region(REGION_GFX5)[2*tile_index + 1];
	SET_TILE_INFO(BG_GFX, code, code >> 12);
}





/* Foreground - Resides in RAM */

#define FG_GFX (1)
#define FG_NX  (16*16)
#define FG_NY  (16*2)

static void get_fg_tile_info(int tile_index)
{
	int code = READ_WORD(&ginganin_fgram[2*tile_index]);
	SET_TILE_INFO(FG_GFX, code, code >> 12);
}

WRITE_HANDLER( ginganin_fgram_w )
{
	int old_data, new_data;

	old_data  = READ_WORD(&ginganin_fgram[offset]);
	COMBINE_WORD_MEM(&ginganin_fgram[offset],data);
	new_data  = READ_WORD(&ginganin_fgram[offset]);

	if (old_data != new_data)
		tilemap_mark_tile_dirty(fg_tilemap,offset/2);
}




/* Frontmost (text) Layer - Resides in RAM */

#define TXT_GFX (2)
#define TXT_NX  (32)
#define TXT_NY  (32)

static void get_txt_tile_info(int tile_index)
{
	int code = READ_WORD(&ginganin_txtram[2*tile_index]);
	SET_TILE_INFO(TXT_GFX, code, code >> 12);
}

WRITE_HANDLER( ginganin_txtram_w )
{
int old_data, new_data;

	old_data  = READ_WORD(&ginganin_txtram[offset]);
	COMBINE_WORD_MEM(&ginganin_txtram[offset],data);
	new_data  = READ_WORD(&ginganin_txtram[offset]);

	if (old_data != new_data)
		tilemap_mark_tile_dirty(tx_tilemap,offset/2);
}





int ginganin_vh_start(void)
{
	bg_tilemap = tilemap_create(get_bg_tile_info,tilemap_scan_cols,TILEMAP_OPAQUE,16,16,BG_NX,BG_NY);
	fg_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_cols,TILEMAP_TRANSPARENT,16,16,FG_NX,FG_NY);
	tx_tilemap = tilemap_create(get_txt_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,TXT_NX,TXT_NY);

	if (!fg_tilemap || !bg_tilemap || !tx_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 15;
	tx_tilemap->transparent_pen = 15;

	return 0;
}





WRITE_HANDLER( ginganin_vregs_w )
{
	int new_data;

	COMBINE_WORD_MEM(&ginganin_vregs[offset],data);
	new_data  = READ_WORD(&ginganin_vregs[offset]);

	switch (offset)
	{
		case 0x0 : { tilemap_set_scrolly(fg_tilemap, 0, new_data); } break;
		case 0x2 : { tilemap_set_scrollx(fg_tilemap, 0, new_data); } break;
		case 0x4 : { tilemap_set_scrolly(bg_tilemap, 0, new_data); } break;
		case 0x6 : { tilemap_set_scrollx(bg_tilemap, 0, new_data); } break;
		case 0x8 : { layers_ctrl = new_data; } break;
//		case 0xa : break;
		case 0xc : { flipscreen = !(new_data & 1);	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0); } break;
		case 0xe : { soundlatch_w(0,new_data);
					 cpu_cause_interrupt(1,M6809_INT_NMI);
				   } break;

		default  : //{logerror("CPU #0 PC %06X : Warning, videoreg %04X <- %04X\n",cpu_get_pc(),offset,data);}
		    break;
	}
}






/* --------------------------[ Sprites Format ]----------------------------

Offset:			Values:			Format:

0000.w			y position		fedc ba9- ---- ----		unused
								---- ---8 ---- ----		subtract 256
								---- ---- 7654 3210		position

0002.w			x position		See above

0004.w			code			f--- ---- ---- ----		y flip
								-e-- ---- ---- ----		x flip
								--dc ---- ---- ----		unused?
								---- ba98 7654 3210		code

0006.w			colour			fedc ---- ---- ----		colour code
								---- ba98 7654 3210		unused?

------------------------------------------------------------------------ */

static void draw_sprites(struct osd_bitmap *bitmap)
{
int offs;

	for ( offs = 0 ; offs < spriteram_size ; offs += 8 )
	{
		int	y		=	READ_WORD(&spriteram[offs + 0]);
		int	x		=	READ_WORD(&spriteram[offs + 2]);
		int	code	=	READ_WORD(&spriteram[offs + 4]);
		int	attr	=	READ_WORD(&spriteram[offs + 6]);
		int	flipx	=	code & 0x4000;
		int	flipy	=	code & 0x8000;

		x = (x & 0xFF) - (x & 0x100);
		y = (y & 0xFF) - (y & 0x100);

		if (flipscreen)
		{
			x = 240 - x;		y = 240 - y;
			flipx = !flipx;		flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[3],
				code & 0x3fff,
				attr >> 12,
				flipx, flipy,
				x,y,
				&Machine->visible_area,TRANSPARENCY_PEN,15);

	}
}







void ginganin_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
int i, offs;
int layers_ctrl1;

layers_ctrl1 = layers_ctrl;

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();


/* Palette stuff: visible sprites */
{
int color, colmask[16];

	int xmin = Machine->visible_area.min_x - 16 - 1;
	int xmax = Machine->visible_area.max_x;
	int ymin = Machine->visible_area.min_y - 16 - 1;
	int ymax = Machine->visible_area.max_y;

	int nmax				=	Machine->gfx[3]->total_elements;
	unsigned int *pen_usage	=	Machine->gfx[3]->pen_usage;
	int color_codes_start	=	Machine->drv->gfxdecodeinfo[3].color_codes_start;

	for (color = 0 ; color < 16 ; color++) colmask[color] = 0;

	for (offs = 0 ; offs < spriteram_size ; offs += 8)
	{
	int x,y,code;

		y	=	READ_WORD(&spriteram[offs + 0]);
		y	=	(y & 0xff) - (y & 0x100);
		if ((y < ymin) || (y > ymax))	continue;

		x	=	READ_WORD(&spriteram[offs + 2]);
		x	=	(x & 0xff) - (x & 0x100);
		if ((x < xmin) || (x > xmax))	continue;

		code	=	(READ_WORD(&spriteram[offs + 4]) & 0x3fff)% nmax;
		color	=	READ_WORD(&spriteram[offs + 6]) >> 12;

		colmask[color] |= pen_usage[code];
	}

	for (color = 0; color < 16; color++)
	{
		if (colmask[color])
		{
			for (i = 0; i < 16; i++)
				if (colmask[color] & (1 << i))
					palette_used_colors[16 * color + i + color_codes_start] = PALETTE_COLOR_USED;
		}
	}

}

	if (palette_recalc())	tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	if (layers_ctrl1 & 1)	tilemap_draw(bitmap, bg_tilemap,  0);
	else					osd_clearbitmap(Machine->scrbitmap);

	if (layers_ctrl1 & 2)	tilemap_draw(bitmap, fg_tilemap,  0);
	if (layers_ctrl1 & 8)	draw_sprites(bitmap);
	if (layers_ctrl1 & 4)	tilemap_draw(bitmap, tx_tilemap, 0);

}
