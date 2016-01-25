/***************************************************************************

						  -= Power Instinct =-
							(C) 1993 Atlus

				driver by	Luca Elia (eliavit@unina.it)


Note:	if MAME_DEBUG is defined, pressing Z with:

		Q			shows layer 1
		W			shows layer 2
		A			shows the sprites

		Keys can be used togheter!

		[ 2 Scrolling Layers ]

		Each Layer is made of various pages of 256x256 pixels.

			[Layer 0]
				Pages:				16x2
				Tiles:				16x16x4
				Scroll:				X,Y

			[Layer 1]
				Pages:				2x1
				Tiles:				8x8x4
				Scroll:				No

		[ 256 Sprites ]

		Each sprite is made of a variable amount of 16x16 tiles.
		Size can therefore vary from 16x16 (1 tile) to 256x256
		(16x16 tiles)


**************************************************************************/
#include "vidhrdw/generic.h"

/* Variables that driver has access to: */
unsigned char *powerins_vram_0, *powerins_vctrl_0;
unsigned char *powerins_vram_1, *powerins_vctrl_1;
unsigned char *powerins_vregs;

/* Variables only used here: */
static struct tilemap *tilemap_0, *tilemap_1;
static int flipscreen, tile_bank;
static int oki_bank;



/***************************************************************************

						Hardware registers access

***************************************************************************/

READ_HANDLER( powerins_vregs_r )
{
	return READ_WORD(&powerins_vregs[offset]);
}

WRITE_HANDLER( powerins_vregs_w )
{
	COMBINE_WORD_MEM(&powerins_vregs[offset],data);

	switch (offset)
	{
		case 0x14:	// Flipscreen
			flipscreen = data & 1;
			tilemap_set_flip( ALL_TILEMAPS, flipscreen?(TILEMAP_FLIPX | TILEMAP_FLIPY): 0 );
			break;

//		case 0x16:	// ? always 1

		case 0x18:	// Tiles Banking (VRAM 0)
			if (data != tile_bank)
			{
				tile_bank = data;
				tilemap_mark_all_tiles_dirty(tilemap_0);
			}
			break;

//		case 0x1e:	// ? there is an hidden test mode screen (set 18ff08 to 4
					//   during test mode) that calls this: sound code (!?)

		case 0x30:	// OKI banking
		{
			unsigned char *RAM = memory_region(REGION_SOUND1);
			int new_bank = data & 0x7;

			if (new_bank != oki_bank)
			{
				oki_bank = new_bank;
				memcpy(&RAM[0x30000],&RAM[0x40000 + 0x10000*new_bank],0x10000);
			}
		}
		break;

		case 0x3e:	// OKI data
			OKIM6295_data_0_w(0,data);
			break;

		default:
			//logerror("PC %06X - Register %02X <- %02X !\n", cpu_get_pc(), offset, data);
		    break;
	}
}




/***************************************************************************

									Palette

***************************************************************************/


WRITE_HANDLER( powerins_paletteram_w )
{
	/*	byte 0    byte 1	*/
	/*	RRRR GGGG BBBB RGBx	*/
	/*	4321 4321 4321 000x	*/

	int oldword = READ_WORD (&paletteram[offset]);
	int newword = COMBINE_WORD (oldword, data);

	int r = ((newword >> 8) & 0xF0 ) | ((newword << 0) & 0x08);
	int g = ((newword >> 4) & 0xF0 ) | ((newword << 1) & 0x08);
	int b = ((newword >> 0) & 0xF0 ) | ((newword << 2) & 0x08);

	palette_change_color( offset/2, r,g,b );

	WRITE_WORD (&paletteram[offset], newword);
}


/***************************************************************************

						Callbacks for the TileMap code

***************************************************************************/


/***************************************************************************
						  [ Tiles Format VRAM 0]

Offset:

0.w		fedc ---- ---- ----		Color Low  Bits
		---- b--- ---- ----		Color High Bit
		---- -a98 7654 3210		Code (Banked)


***************************************************************************/

/* Layers are made of 256x256 pixel pages */
#define TILES_PER_PAGE_X	(0x10)
#define TILES_PER_PAGE_Y	(0x10)
#define TILES_PER_PAGE		(TILES_PER_PAGE_X * TILES_PER_PAGE_Y)

#define DIM_NX_0			(0x100)
#define DIM_NY_0			(0x20)


static void get_tile_info_0( int tile_index )
{
	int code = READ_WORD(&powerins_vram_0[tile_index * 2]);
	SET_TILE_INFO( 0 , (code & 0x07ff) + (tile_bank*0x800), ((code & 0xf000) >> (16-4)) + ((code & 0x0800) >> (11-4)) );
}

WRITE_HANDLER( powerins_vram_0_w )
{
	COMBINE_WORD_MEM(&powerins_vram_0[offset],data);
	tilemap_mark_tile_dirty(tilemap_0, offset/2 );
}

UINT32 powerins_get_memory_offset_0(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	return 	(col * TILES_PER_PAGE_Y) +

			(row % TILES_PER_PAGE_Y) +
			(row / TILES_PER_PAGE_Y) * (TILES_PER_PAGE * 16);
}



/***************************************************************************
						  [ Tiles Format VRAM 1]

Offset:

0.w		fedc ---- ---- ----		Color
		---- ba98 7654 3210		Code


***************************************************************************/

#define DIM_NX_1	(0x40)
#define DIM_NY_1	(0x20)

static void get_tile_info_1( int tile_index )
{
	int code = READ_WORD(&powerins_vram_1[tile_index * 2]);
	SET_TILE_INFO( 1 , code & 0x0fff , (code & 0xf000) >> (16-4) );
}

WRITE_HANDLER( powerins_vram_1_w )
{
	COMBINE_WORD_MEM(&powerins_vram_1[offset],data);
	tilemap_mark_tile_dirty(tilemap_1, offset/2 );
}





/***************************************************************************


								Vh_Start


***************************************************************************/

int powerins_vh_start(void)
{
	tilemap_0 = tilemap_create(	get_tile_info_0,
								powerins_get_memory_offset_0,
								TILEMAP_OPAQUE,
								16,16,
								DIM_NX_0, DIM_NY_0 );

	tilemap_1 = tilemap_create(	get_tile_info_1,
								tilemap_scan_cols,
								TILEMAP_TRANSPARENT,
								8,8,
								DIM_NX_1, DIM_NY_1 );

	if (tilemap_0 && tilemap_1)
	{
		tilemap_set_scroll_rows(tilemap_0,1);
		tilemap_set_scroll_cols(tilemap_0,1);
		tilemap_0->transparent_pen = 15;

		tilemap_set_scroll_rows(tilemap_1,1);
		tilemap_set_scroll_cols(tilemap_1,1);
		tilemap_1->transparent_pen = 15;

		oki_bank = -1;	// samples bank "unitialised"
		return 0;
	}
	else return 1;
}






/***************************************************************************


								Sprites Drawing


***************************************************************************/



/* --------------------------[ Sprites Format ]----------------------------

Offset:		Format:					Value:

	00 		fedc ba98 7654 321-		-
	 		---- ---- ---- ---0		Display this sprite

	02 		fed- ---- ---- ----		-
	 		---c ---- ---- ----		Flip X
	 		---- ba9- ---- ----		-
	 		---- ---8 ---- ----		Code High Bit
			---- ---- 7654 ----		Number of tiles along Y, minus 1 (1-16)
			---- ---- ---- 3210		Number of tiles along X, minus 1 (1-16)

	04 								Unused?

	06		f--- ---- ---- ----		-
			-edc ba98 7654 3210		Code Low Bits

	08 								X

	0A 								Unused?

	0C								Y

	0E		fedc ba98 76-- ----		-
			---- ---- --54 3210		Color


------------------------------------------------------------------------ */

#define SIGN_EXTEND_POS(_var_)	{_var_ &= 0x3ff; if (_var_ > 0x1ff) _var_ -= 0x400;}

static void powerins_mark_sprite_colors(void)
{
	int i,col,colmask[0x100];

	unsigned int *pen_usage	=	Machine->gfx[2]->pen_usage;
	int total_elements		=	Machine->gfx[2]->total_elements;
	int color_codes_start	=	Machine->drv->gfxdecodeinfo[2].color_codes_start;
	int total_color_codes	=	Machine->drv->gfxdecodeinfo[2].total_color_codes;

	unsigned char *source = spriteram + 0x8000;
	unsigned char *finish = spriteram + 0x9000;

	int xmin = Machine->visible_area.min_x;
	int xmax = Machine->visible_area.max_x;
	int ymin = Machine->visible_area.min_y;
	int ymax = Machine->visible_area.max_y;

	memset(colmask, 0, sizeof(colmask));

	for ( ; source < finish; source += 16 )
	{
		int x, y;

		int	attr	=	READ_WORD(&source[ 0x0 ]);
		int	size	=	READ_WORD(&source[ 0x2 ]);
		int	code	=	READ_WORD(&source[ 0x6 ]);
		int	sx		=	READ_WORD(&source[ 0x8 ]);
		int	sy		=	READ_WORD(&source[ 0xc ]);
		int	color	=	READ_WORD(&source[ 0xe ]) % total_color_codes;

		int	dimx	=	((size >> 0) & 0xf ) + 1;
		int	dimy	=	((size >> 4) & 0xf ) + 1;

		if (!(attr&1)) continue;

		SIGN_EXTEND_POS(sx)
		SIGN_EXTEND_POS(sy)

		sx += 32;

		code = (code & 0x7fff) + ( (size & 0x0100) << 7 );

		for (x = 0 ; x < dimx*16 ; x+=16)
		{
			for (y = 0 ; y < dimy*16 ; y+=16)
			{
				if (((sx+x+15) < xmin) || ((sx+x) > xmax) ||
					((sy+y+15) < ymin) || ((sy+y) > ymax))	continue;

				colmask[color] |= pen_usage[(code++) % total_elements];
			}
		}
	}

	for (col = 0; col < total_color_codes; col++)
	 for (i = 0; i < 15; i++)	// pen 15 is transparent
	  if (colmask[col] & (1 << i)) palette_used_colors[16 * col + i + color_codes_start] = PALETTE_COLOR_USED;
}




static void powerins_draw_sprites(struct osd_bitmap *bitmap)
{
	unsigned char *source = spriteram + 0x8000;
	unsigned char *finish = spriteram + 0x9000;

	int screen_w	=	Machine->drv->screen_width;
	int screen_h	=	Machine->drv->screen_height;

	for ( ; source < finish; source += 16 )
	{
		int x,y,inc;

		int	attr	=	READ_WORD(&source[ 0x0 ]);
		int	size	=	READ_WORD(&source[ 0x2 ]);
		int	code	=	READ_WORD(&source[ 0x6 ]);
		int	sx		=	READ_WORD(&source[ 0x8 ]);
		int	sy		=	READ_WORD(&source[ 0xc ]);
		int	color	=	READ_WORD(&source[ 0xe ]);

		int	flipx	=	size & 0x1000;
		int	flipy	=	0;	// ??

		int	dimx	=	((size >> 0) & 0xf ) + 1;
		int	dimy	=	((size >> 4) & 0xf ) + 1;

		if (!(attr&1)) continue;

		SIGN_EXTEND_POS(sx)
		SIGN_EXTEND_POS(sy)

		/* Handle Flipscreen. Apply a global offset of 32 pixels along x too */
		if (flipscreen)	{	sx = screen_w - sx - dimx*16 - 32;	flipx = !flipx;
							sy = screen_h - sy - dimy*16;		flipy = !flipy;
							code += dimx*dimy-1;			inc = -1;	}
		else			{	sx += 32;						inc = +1;	}

		code = (code & 0x7fff) + ( (size & 0x0100) << 7 );

		for (x = 0 ; x < dimx ; x++)
		{
			for (y = 0 ; y < dimy ; y++)
			{
				drawgfx(bitmap,Machine->gfx[2],
						code,
						color,
						flipx, flipy,
						sx + x*16,
						sy + y*16,
						&Machine->visible_area,TRANSPARENCY_PEN,15);

				code += inc;
			}
		}


	}
}





/***************************************************************************


								Screen Drawing


***************************************************************************/


void powerins_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layers_ctrl = -1;

	int scrollx = (READ_WORD(&powerins_vctrl_0[2])&0xff) + (READ_WORD(&powerins_vctrl_0[0])&0xff)*256;
	int scrolly = (READ_WORD(&powerins_vctrl_0[6])&0xff) + (READ_WORD(&powerins_vctrl_0[4])&0xff)*256;
	tilemap_set_scrollx( tilemap_0, 0, scrollx - 0x20);
	tilemap_set_scrolly( tilemap_0, 0, scrolly );

	tilemap_set_scrollx( tilemap_1, 0, -0x20);	// fixed offset
	tilemap_set_scrolly( tilemap_1, 0,  0x00);

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();

	powerins_mark_sprite_colors();

	if (palette_recalc())	tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	if (layers_ctrl&1)		tilemap_draw(bitmap, tilemap_0, 0);
	else					osd_clearbitmap(bitmap);
	if (layers_ctrl&8)		powerins_draw_sprites(bitmap);
	if (layers_ctrl&2)		tilemap_draw(bitmap, tilemap_1, 0);
}
