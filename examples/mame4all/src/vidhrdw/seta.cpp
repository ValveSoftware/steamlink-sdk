/***************************************************************************

								-= Seta Games =-

					driver by	Luca Elia (eliavit@unina.it)


Note:	if MAME_DEBUG is defined, pressing Z with:

		Q			shows layer 0
		W			shows layer 1
		A			shows the sprites

		Keys can be used togheter!


						[ 0, 1 Or 2 Scrolling Layers ]

	Each layer consists of 2 tilemaps: only one can be displayed at a
	given time (the games usually flip continuously between the two).
	The two tilemaps share the same scrolling registers.

		Layer Size:				1024 x 512
		Tiles:					16x16x4 (16x16x6 in some games)
		Tile Format:

			Offset + 0x0000:
							f--- ---- ---- ----		Flip X
							-e-- ---- ---- ----		Flip Y
							--dc ba98 7654 3210		Code

			Offset + 0x1000:

							fedc ba98 765- ----		-
							---- ---- ---4 3210		Color

			The other tilemap for this layer (always?) starts at
			Offset + 0x2000.


							[ 1024 Sprites ]

	Sprites are 16x16x4. They are just like those in "The Newzealand Story",
	"Revenge of DOH" etc (tnzs.c). Obviously they're hooked to a 16 bit
	CPU here, so they're mapped a bit differently in memory. Additionally,
	there are two banks of sprites. The game can flip between the two to
	do double buffering, writing to a bit of a control register(see below)


		Spriteram_2 + 0x000.w

						f--- ---- ---- ----		Flip X
						-e-- ---- ---- ----		Flip Y
						--dc b--- ---- ----		-
						---- --98 7654 3210		Code (Lower bits)

		Spriteram_2 + 0x400.w

						fedc b--- ---- ----		Color
						---- -a-- ---- ----		?Code (Upper Bits)?
						---- --9- ---- ----		Code (Upper Bits)
						---- ---8 7654 3210		X

		Spriteram   + 0x000.w

						fedc ba98 ---- ----		-
						---- ---- 7654 3210		Y



							[ Floating Tilemap ]

	There's a floating tilemap made of vertical colums composed of 2x16
	"sprites". Each 32 consecutive "sprites" define a column.

	For column I:

		Spriteram_2 + 0x800 + 0x40 * I:

						f--- ---- ---- ----		Flip X
						-e-- ---- ---- ----		Flip Y
						--dc b--- ---- ----		-
						---- --98 7654 3210		Code (Lower bits)

		Spriteram_2 + 0xc00 + 0x40 * I:

						fedc b--- ---- ----		Color
						---- -a-- ---- ----		? Code (Upper Bits) ?
						---- --9- ---- ----		Code (Upper Bits)
						---- ---8 7654 3210		-

	Each column	has a variable horizontal position and a vertical scrolling
	value (see also the Sprite Control Registers). For column I:


		Spriteram   + 0x400 + 0x20 * I:

						fedc ba98 ---- ----		-
						---- ---- 7654 3210		Y

		Spriteram   + 0x408 + 0x20 * I:

						fedc ba98 ---- ----		-
						---- ---- 7654 3210		Low Bits Of X



						[ Sprites Control Registers ]


		Spriteram   + 0x601.b

						7--- ----		0
						-6-- ----		Flip Screen
						--5- ----		0
						---4 ----		1 (Sprite Enable?)
						---- 3---		?
						---- -210		0

		Spriteram   + 0x603.b

						7--- ----		0
						-6-- ----		Sprite Bank
						--5- ----		1 (?)
						---4 ----		0
						---- 3210		Columns To Draw (1 is the special value for 16?)

		Spriteram   + 0x605.b

						7654 3210		High Bit Of X For Columns 7-0

		Spriteram   + 0x607.b

						7654 3210		High Bit Of X For Columns f-8




***************************************************************************/
#include "vidhrdw/generic.h"

/* Variables only used here */
static struct tilemap *tilemap_0,*tilemap_1, *tilemap_2,*tilemap_3;

/* Variables that driver has access to */
unsigned char *seta_vram_0, *seta_vram_1, *seta_vctrl_0;
unsigned char *seta_vram_2, *seta_vram_3, *seta_vctrl_2;
unsigned char *seta_vregs;

int seta_tiles_offset;	// tiles banking, can be 0 or $4000


/* Variables defined in driver */

extern int blandia_samples_bank;






WRITE_HANDLER( seta_vregs_w )
{
	COMBINE_WORD_MEM(&seta_vregs[offset], data);
	switch (offset)
	{
		case 0:
			if (!(data & 0x00ff0000))
			{	// typical value: 002c
				coin_lockout_w(0, (data >> 0) & 1 );
				coin_lockout_w(1, (data >> 1) & 1 );
			}
			break;

		case 2:
		{
			unsigned char *RAM = memory_region(REGION_SOUND1);
			int new_bank;

			// Handled in vh_screenrefresh:
			// bit 0: swap layers order
			// bit 1: ?
			// bit 2: ?

			// bit 3-5: samples bank (blandia)

			new_bank = (data >> 3) & 0x7;

			if (new_bank != blandia_samples_bank)
			{
				int samples_len, addr;

				blandia_samples_bank = new_bank;

				samples_len = memory_region_length(REGION_SOUND1);

				addr = 0x40000 * new_bank;
				if (new_bank >= 3)	addr += 0x40000;

				if ( (samples_len > 0x100000) && ((addr+0x40000) <= samples_len) )
					memcpy(&RAM[0xc0000],&RAM[addr],0x40000);
				/*else
					logerror("PC %06X - Invalid samples bank %02X !\n", cpu_get_pc(), new_bank);*/
			}

		}
		break;


		case 4:	// ?
			break;
	}
}




/***************************************************************************

						Callbacks for the TileMap code

							  [ Tiles Format ]

Offset + 0x0000:
					f--- ---- ---- ----		Flip X
					-e-- ---- ---- ----		Flip Y
					--dc ba98 7654 3210		Code

Offset + 0x1000:

					fedc ba98 765- ----		-
					---- ---- ---4 3210		Color


***************************************************************************/
#define DIM_NX		(64)
#define DIM_NY		(32)

#define SETA_TILEMAP(_n_) \
static void get_tile_info_##_n_( int tile_index ) \
{ \
	int code =	READ_WORD(&seta_vram_##_n_[ tile_index * 2]); \
	int attr =	READ_WORD(&seta_vram_##_n_[ tile_index * 2 + DIM_NX*DIM_NY*2]); \
	SET_TILE_INFO( 1 + _n_/2, seta_tiles_offset + (code & 0x3fff), attr & 0x1f ); \
	tile_info.flags = TILE_FLIPXY( code >> (16-2) ); \
} \
\
WRITE_HANDLER( seta_vram_##_n_##_w ) \
{ \
	COMBINE_WORD_MEM(&seta_vram_##_n_[offset],data); \
	offset %= DIM_NX * DIM_NY * 2; \
	tilemap_mark_tile_dirty(tilemap_##_n_, offset/2 ); \
}

SETA_TILEMAP(0)
SETA_TILEMAP(1)
SETA_TILEMAP(2)
SETA_TILEMAP(3)


/* 2 layers */
int seta_vh_start_2_layers(void)
{
	/* Each layer consists of 2 tilemaps: only one can be displayed
	   at any given time */

	/* layer 0 */
	tilemap_0 = tilemap_create(	get_tile_info_0,
								tilemap_scan_rows,
								TILEMAP_TRANSPARENT,
								16,16,
								DIM_NX,DIM_NY );

	tilemap_1 = tilemap_create(	get_tile_info_1,
								tilemap_scan_rows,
								TILEMAP_TRANSPARENT,
								16,16,
								DIM_NX,DIM_NY );


	/* layer 1 */
	tilemap_2 = tilemap_create(	get_tile_info_2,
								tilemap_scan_rows,
								TILEMAP_TRANSPARENT,
								16,16,
								DIM_NX,DIM_NY );

	tilemap_3 = tilemap_create(	get_tile_info_3,
								tilemap_scan_rows,
								TILEMAP_TRANSPARENT,
								16,16,
								DIM_NX,DIM_NY );

	if (tilemap_0 && tilemap_1 && tilemap_2 && tilemap_3)
	{
		tilemap_set_scroll_rows(tilemap_0,1);
		tilemap_set_scroll_cols(tilemap_0,1);
		tilemap_0->transparent_pen = 0;

		tilemap_set_scroll_rows(tilemap_1,1);
		tilemap_set_scroll_cols(tilemap_1,1);
		tilemap_1->transparent_pen = 0;

		tilemap_set_scroll_rows(tilemap_2,1);
		tilemap_set_scroll_cols(tilemap_2,1);
		tilemap_2->transparent_pen = 0;

		tilemap_set_scroll_rows(tilemap_3,1);
		tilemap_set_scroll_cols(tilemap_3,1);
		tilemap_3->transparent_pen = 0;

		tilemap_set_scrolldx(tilemap_0, -0x01, 0x00);	// see zingzip test mode
		tilemap_set_scrolldx(tilemap_1, -0x01, 0x00);
		tilemap_set_scrolldx(tilemap_2, -0x01, 0x00);
		tilemap_set_scrolldx(tilemap_3, -0x01, 0x00);

		tilemap_set_scrolldy(tilemap_0, 0x00, 0x00);
		tilemap_set_scrolldy(tilemap_1, 0x00, 0x00);
		tilemap_set_scrolldy(tilemap_2, 0x00, 0x00);
		tilemap_set_scrolldy(tilemap_3, 0x00, 0x00);

		return 0;
	}
	else return 1;
}


/* 1 layer */
int seta_vh_start_1_layer(void)
{
	/* Each layer consists of 2 tilemaps: only one can be displayed
	   at any given time */

	/* layer 0 */
	tilemap_0 = tilemap_create(	get_tile_info_0,
								tilemap_scan_rows,
								TILEMAP_TRANSPARENT,
								16,16,
								DIM_NX,DIM_NY );

	tilemap_1 = tilemap_create(	get_tile_info_1,
								tilemap_scan_rows,
								TILEMAP_TRANSPARENT,
								16,16,
								DIM_NX,DIM_NY );


	/* NO layer 1 */
	tilemap_2 = 0;
	tilemap_3 = 0;

	if (tilemap_0 && tilemap_1)
	{
		tilemap_set_scroll_rows(tilemap_0,1);
		tilemap_set_scroll_cols(tilemap_0,1);
		tilemap_0->transparent_pen = 0;

		tilemap_set_scroll_rows(tilemap_1,1);
		tilemap_set_scroll_cols(tilemap_1,1);
		tilemap_1->transparent_pen = 0;

		tilemap_set_scrolldx(tilemap_0, 0x00, 0x00); // see metafox test mode
		tilemap_set_scrolldx(tilemap_1, 0x00, 0x00);

		tilemap_set_scrolldy(tilemap_0, 0x00, 0x00);
		tilemap_set_scrolldy(tilemap_1, 0x00, 0x00);

		return 0;
	}
	else return 1;
}


int seta_vh_start_1_layer_offset_0x02(void)
{
	if (seta_vh_start_1_layer())	return 1;

	tilemap_set_scrolldx(tilemap_0, -0x02, 0x00); // see calibr50's rescue
	tilemap_set_scrolldx(tilemap_1, -0x02, 0x00);

	tilemap_set_scrolldy(tilemap_0, 0x00, 0x00);
	tilemap_set_scrolldy(tilemap_1, 0x00, 0x00);

	return 0;
}


/***************************************************************************


							Palette Init Functions


***************************************************************************/


/* 2 layers, 6 bit deep. The color codes have a 16 color granularity.

   One layer only uses the first 16 colors of the palette (repeated
   4 times to fill the 64 colors) and regardless of the color code!

   The other uses the first 64 colors of the palette regardless of
   the color code too!

   I think that's because this game's a prototype..
*/
void blandia_vh_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int color, pen;
	for( color = 0; color < 32; color++ )
		for( pen = 0; pen < 64; pen++ )
		{
			colortable[color * 64 + pen + 16*32]       = (pen%16) + 16*32*1;
			colortable[color * 64 + pen + 16*32+64*32] = pen      + 16*32*2;
		}
}



/* layer 0 is 6 bit per pixel, but the color code has a 16 colors granularity */
void zingzip_vh_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int color, pen;
	for( color = 0; color < 32; color++ )
		for( pen = 0; pen < 64; pen++ )
			colortable[color * 64 + pen + 32*16*2] = ((color * 16 + pen)%(32*16)) + 32*16*2;
}




/* 6 bit layer. The colors are still WRONG */
void usclssic_vh_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int color, pen;
	for( color = 0; color < 32; color++ )
		for( pen = 0; pen < 64; pen++ )
			colortable[color * 64 + pen + 512] = (((color & 0xf) * 16 + pen)%(512));
}











/***************************************************************************


								Sprites Drawing


***************************************************************************/


void seta_mark_sprite_color(void)
{
	int offs, col;
	int xoffs, yoffs;

	int color_granularity	=	Machine->gfx[0]->color_granularity;
	int color_codes_start	=	Machine->drv->gfxdecodeinfo[0].color_codes_start;
	int total_color_codes	=	Machine->drv->gfxdecodeinfo[0].total_color_codes;

	int xmin = Machine->visible_area.min_x - (16 - 1);
	int xmax = Machine->visible_area.max_x;
	int ymin = Machine->visible_area.min_y - (16 - 1);
	int ymax = Machine->visible_area.max_y;

	/* Floating tilemap made of sprites */

	int ctrl	=	READ_WORD(&spriteram[0x600]);
	int ctrl2	=	READ_WORD(&spriteram[0x602]);
	int flip	=	ctrl & 0x40;
	int numcol	=	ctrl2 & 0x000f;

	unsigned char *spriteram1 = spriteram_2 + ((ctrl2 & 0x40) ? 0x2000 : 0);

	/* Number of columns to draw - the value 1 seems special, meaning:
	   draw every column */
	if (numcol == 1)	numcol = 16;


	for ( col = 0 ; col < numcol; col ++ )
	{
		for ( offs = 0 ; offs < 0x40; offs += 2 )
		{
			int	color	=	READ_WORD(&spriteram1[col * 0x40 + offs + 0xc00]);
			color		=	( color >> (16-5) ) % total_color_codes;
			memset(&palette_used_colors[color_granularity * color + color_codes_start + 1],PALETTE_COLOR_USED,color_granularity - 1);
		}
	}


	/* Normal sprites */

	xoffs	=	flip ? 0x10 : 0x11;
	yoffs	=	flip ? 0x06 : 0x06;

	for ( offs = 0x400-2 ; offs >= 0; offs -= 2 )
	{
//		int	code	=	READ_WORD(&spriteram1[offs + 0x000]);
		int	x		=	READ_WORD(&spriteram1[offs + 0x400]);
		int	y		=	READ_WORD(&spriteram [offs + 0x000]);
		int color	=	( x >> (16-5) ) % total_color_codes;

		x = (x + xoffs) & 0x1ff;
		y = (ymax+1) - ((y + yoffs) & 0x0ff);

		/* Visibility check. No need to account for sprites flipping */
		if ((x < xmin) || (x > xmax))	continue;
		if ((y < ymin) || (y > ymax))	continue;

		memset(&palette_used_colors[color_granularity * color + color_codes_start + 1],PALETTE_COLOR_USED,color_granularity - 1);
	}
}




static void seta_draw_sprites_map(struct osd_bitmap *bitmap)
{
	int offs, col;
	int xoffs, yoffs;

	int total_color_codes	=	Machine->drv->gfxdecodeinfo[0].total_color_codes;

	int ctrl	=	READ_WORD(&spriteram[0x600]);
	int ctrl2	=	READ_WORD(&spriteram[0x602]);

	int flip	=	ctrl & 0x40;
	int numcol	=	ctrl2 & 0x000f;

	unsigned char *spriteram1 = spriteram_2 + ((ctrl2 & 0x40) ? 0x2000 : 0);

	int upper	=	( READ_WORD(&spriteram[0x604]) & 0xFF ) +
					( READ_WORD(&spriteram[0x606]) & 0xFF ) * 256;

//	int max_x	=	Machine->drv->screen_width  - 16;
//	int max_y	=	Machine->visible_area.max_y+1;	// see pic of metafox
int max_y	=	240;

	xoffs	=	flip ? 0x10 : 0x10;	// see wrofaero test mode: made of sprites map
	yoffs	=	flip ? 0x09 : 0x07;

	/* Number of columns to draw - the value 1 seems special, meaning:
	   draw every column */
	if (numcol == 1)	numcol = 16;


	/* The first column is the frontmost, see twineagl test mode */
	for ( col = numcol - 1 ; col >= 0; col -- )
	{
		int	x	=	READ_WORD(&spriteram[col * 0x20 + 0x08 + 0x400]) & 0xff;
		int	y	=	READ_WORD(&spriteram[col * 0x20 + 0x00 + 0x400]) & 0xff;

		/* draw this column */
		for ( offs = 0 ; offs < 0x40; offs += 2 )
		{
			int	code	=	READ_WORD(&spriteram1[col * 0x40 + offs + 0x800]);
			int	color	=	READ_WORD(&spriteram1[col * 0x40 + offs + 0xc00]);

			int	flipx	=	code & 0x8000;
			int	flipy	=	code & 0x4000;

			int bank	=	color & 0x0200;

			int sx		=	  x + xoffs  + ((offs/2) & 1) * 16;
			int sy		=	-(y + yoffs) + ((offs/2) / 2) * 16;

			if (upper & (1 << col))	sx += 256;

			if (flip)
			{
				sy = max_y - 16 - sy - 0x100;
				flipx = !flipx;
				flipy = !flipy;
			}

			color	=	( color >> (16-5) ) % total_color_codes;
			code	=	(code & 0x3fff) + (bank ? 0x4000 : 0);

#define DRAWTILE(_x_,_y_)  \
			drawgfx(bitmap,Machine->gfx[0], \
					code, \
					color, \
					flipx, flipy, \
					_x_,_y_, \
					&Machine->visible_area,TRANSPARENCY_PEN,0);

			DRAWTILE(sx - 0x000, sy + 0x000)
			DRAWTILE(sx - 0x200, sy + 0x000)
			DRAWTILE(sx - 0x000, sy + 0x100)
			DRAWTILE(sx - 0x200, sy + 0x100)

		}

	/* next column */
	}

}



static void seta_draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;
	int xoffs, yoffs;

	int total_color_codes	=	Machine->drv->gfxdecodeinfo[0].total_color_codes;

	int ctrl	=	READ_WORD(&spriteram[0x600]);
	int ctrl2	=	READ_WORD(&spriteram[0x602]);

	int flip	=	ctrl & 0x40;
	unsigned char *spriteram1 = spriteram_2 + ((ctrl2 & 0x40) ? 0x2000 : 0);

//	int max_x	=	Machine->drv->screen_width  - 16;
//	int max_y	=	Machine->visible_area.max_y+1;	// see pic of metafox
int max_y	=	240;



	seta_draw_sprites_map(bitmap);



//	xoffs	=	flip ? 0x10 : 0x11;	// see downtown test mode: made of normal sprites
xoffs	=	flip ? 0x10 : 0x10;	// see blandia test mode: made of normal sprites
	yoffs	=	flip ? 0x06 : 0x06;

	for ( offs = 0x400-2 ; offs >= 0; offs -= 2 )
	{
		int	code	=	READ_WORD(&spriteram1[offs + 0x000]);
		int	x		=	READ_WORD(&spriteram1[offs + 0x400]);
		int	y		=	READ_WORD(&spriteram [offs + 0x000]);

		int	flipx	=	code & 0x8000;
		int	flipy	=	code & 0x4000;

		int bank	=	 x & 0x0200;
		int color	=	( x >> (16-5) ) % total_color_codes;

		if (flip)
		{
			y = max_y - y;
			flipx = !flipx;
			flipy = !flipy;
		}

		code = (code & 0x3fff) + (bank ? 0x4000 : 0);

		drawgfx(bitmap,Machine->gfx[0],
				code,
				color,
				flipx, flipy,
				(x + xoffs) & 0x1ff,
				max_y - ((y + yoffs) & 0x0ff),
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}

}





/***************************************************************************

								Screen Drawing

***************************************************************************/

/* For games without tilemaps */
void seta_vh_screenrefresh_no_layers(struct osd_bitmap *bitmap,int full_refresh)
{
	palette_init_used_colors();
	seta_mark_sprite_color();
	palette_recalc();
	osd_clearbitmap(Machine->scrbitmap);
	seta_draw_sprites(bitmap);
}



/* For games with 1 or 2 tilemaps */
void seta_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layers_ctrl = -1;
	int enab_0, enab_1, x_0, x_1, y_0, y_1;

	int order	= 	0;
	int flip	=	READ_WORD(&spriteram[0x600]) & 0x40;

	tilemap_set_flip(ALL_TILEMAPS, flip ? (TILEMAP_FLIPX|TILEMAP_FLIPY) : 0 );

	x_0		=	READ_WORD(&seta_vctrl_0[0]);
	y_0		=	READ_WORD(&seta_vctrl_0[2]);
	enab_0	=	READ_WORD(&seta_vctrl_0[4]);

	/* Only one tilemap per layer is enabled! */
	tilemap_set_enable(tilemap_0, (!(enab_0 & 0x0008)) /*&& (enab_0 & 0x0001)*/ );
	tilemap_set_enable(tilemap_1, ( (enab_0 & 0x0008)) /*&& (enab_0 & 0x0001)*/ );

	/* the hardware wants different scroll values when flipped */

	if (flip)	{	x_0 = -402 - x_0;	y_0 = y_0 - 0x100;	}
	tilemap_set_scrollx (tilemap_0, 0, x_0);
	tilemap_set_scrollx (tilemap_1, 0, x_0);
	tilemap_set_scrolly (tilemap_0, 0, y_0);
	tilemap_set_scrolly (tilemap_1, 0, y_0);

	if (tilemap_2)
	{
		x_1		=	READ_WORD(&seta_vctrl_2[0]);
		y_1		=	READ_WORD(&seta_vctrl_2[2]);
		enab_1	=	READ_WORD(&seta_vctrl_2[4]);

		tilemap_set_enable(tilemap_2, (!(enab_1 & 0x0008)) /*&& (enab_1 & 0x0001)*/ );
		tilemap_set_enable(tilemap_3, ( (enab_1 & 0x0008)) /*&& (enab_1 & 0x0001)*/ );

		if (flip)	{	x_1 = -402 - x_1;	y_1 = y_1 - 0x100;	}
		tilemap_set_scrollx (tilemap_2, 0, x_1);
		tilemap_set_scrollx (tilemap_3, 0, x_1);
		tilemap_set_scrolly (tilemap_2, 0, y_1);
		tilemap_set_scrolly (tilemap_3, 0, y_1);

		order	=	READ_WORD(&seta_vregs[2]);
	}

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();

	seta_mark_sprite_color();

	if (palette_recalc())	tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	osd_clearbitmap(Machine->scrbitmap);

	if (order & 1)	// swap the layers?
	{
		if (tilemap_2)
		{
			if (layers_ctrl & 2)	tilemap_draw(bitmap, tilemap_2, TILEMAP_IGNORE_TRANSPARENCY);
			if (layers_ctrl & 2)	tilemap_draw(bitmap, tilemap_3, TILEMAP_IGNORE_TRANSPARENCY);
		}

		if (order & 2)	// layer-sprite priority?
		{
			if (layers_ctrl & 8)	seta_draw_sprites(bitmap);
			if (layers_ctrl & 1)	tilemap_draw(bitmap, tilemap_0,  0);
			if (layers_ctrl & 1)	tilemap_draw(bitmap, tilemap_1,  0);
		}
		else
		{
			if (layers_ctrl & 1)	tilemap_draw(bitmap, tilemap_0,  0);
			if (layers_ctrl & 1)	tilemap_draw(bitmap, tilemap_1,  0);
			if (layers_ctrl & 8)	seta_draw_sprites(bitmap);
		}
	}
	else
	{
		if (layers_ctrl & 1)	tilemap_draw(bitmap, tilemap_0,  TILEMAP_IGNORE_TRANSPARENCY);
		if (layers_ctrl & 1)	tilemap_draw(bitmap, tilemap_1,  TILEMAP_IGNORE_TRANSPARENCY);

		if (order & 2)	// layer-sprite priority?
		{
			if (layers_ctrl & 8)	seta_draw_sprites(bitmap);

			if (tilemap_2)
			{
				if (layers_ctrl & 2)	tilemap_draw(bitmap, tilemap_2,  0);
				if (layers_ctrl & 2)	tilemap_draw(bitmap, tilemap_3,  0);
			}
		}
		else
		{
			if (tilemap_2)
			{
				if (layers_ctrl & 2)	tilemap_draw(bitmap, tilemap_2,  0);
				if (layers_ctrl & 2)	tilemap_draw(bitmap, tilemap_3,  0);
			}

			if (layers_ctrl & 8)	seta_draw_sprites(bitmap);
		}
	}

}
