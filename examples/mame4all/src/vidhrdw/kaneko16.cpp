/***************************************************************************

						-= Kaneko 16 Bit Games =-

				driver by	Luca Elia (eliavit@unina.it)


Note:	if MAME_DEBUG is defined, pressing Z with:

		Q			shows the background
		W/E/R/T		shows the foreground (priority 0/1/2/3)
		A/S/D/F		shows the sprites    (priority 0/1/2/3)

		Keys can be used togheter!

							[ 1 High Color Layer ]

	[Background 0]	(Optional)

							[ 4 Scrolling Layers ]

	[Background 1]
	[Foreground 1]
	[Background 2?]	Unused
	[Foreground 2?]	Unused

		Layer Size:				512 x 512
		Tiles:					16x16x4
		Tile Format:

				0000.w			fedc ba-- ---- ----		unused?
								---- --98 ---- ----		Priority
								---- ---- 7654 32--		Color
								---- ---- ---- --1-		Flip X
								---- ---- ---- ---0		Flip Y

				0002.w									Code



							[ 1024 Sprites ]

	Sprites are 16x16x4 in the older games 16x16x8 in gtmr&gtmr2

Offset:			Format:						Value:

0000.w			Attribute (type 0, older games: shogwarr, berlwall)

					f--- ---- ---- ----		Multisprite: Use Latched Code + 1
					-e-- ---- ---- ----		Multisprite: Use Latched Color (And Flip?)
					--d- ---- ---- ----		Multisprite: Use Latched X,Y As Offsets
					---c ba-- ---- ----
					---- --98 ---- ----		Priority?
					---- ---- 7654 32--		Color
					---- ---- ---- --1-		X Flip
					---- ---- ---- ---0		Y Flip


				Attribute (type 1: gtmr, gtmr2)

					f--- ---- ---- ----		Multisprite: Use Latched Code + 1
					-e-- ---- ---- ----		Multisprite: Use Latched Color (And Flip?)
					--d- ---- ---- ----		Multisprite: Use Latched X,Y As Offsets
					---c ba-- ---- ----		unused?
					---- --9- ---- ----		X Flip
					---- ---8 ---- ----		Y Flip
					---- ---- 76-- ----		Priority
					---- ---- --54 3210		Color

0002.w										Code
0004.w										X Position << 6
0006.w										Y Position << 6

Note:
	type 2 sprites (berlwall) are like type 0 but the data is held
	in the last 8 bytes of every 16.


**************************************************************************/

#include "vidhrdw/generic.h"

/* Variables only used here: */

static struct tilemap *bg_tilemap, *fg_tilemap;
static struct osd_bitmap *kaneko16_bg15_bitmap;
static int flipsprites;

/* Variables that driver has access to: */

unsigned char *kaneko16_bgram, *kaneko16_fgram;
unsigned char *kaneko16_layers1_regs, *kaneko16_layers2_regs, *kaneko16_screen_regs;
unsigned char *kaneko16_bg15_select, *kaneko16_bg15_reg;
int kaneko16_spritetype;

/* Variables defined in drivers: */


/***************************************************************************

								Palette RAM

***************************************************************************/

WRITE_HANDLER( kaneko16_paletteram_w )
{
	/*	byte 0    	byte 1		*/
	/*	xGGG GGRR   RRRB BBBB	*/
	/*	x432 1043 	2104 3210	*/

	int newword, r,g,b;

	COMBINE_WORD_MEM(&paletteram[offset], data);

	newword = READ_WORD (&paletteram[offset]);
	r = (newword >>  5) & 0x1f;
	g = (newword >> 10) & 0x1f;
	b = (newword >>  0) & 0x1f;

	palette_change_color( offset/2,	 (r * 0xFF) / 0x1F,
									 (g * 0xFF) / 0x1F,
									 (b * 0xFF) / 0x1F	 );
}

WRITE_HANDLER( gtmr_paletteram_w )
{
	if (offset < 0x10000)	kaneko16_paletteram_w(offset, data);
	else					COMBINE_WORD_MEM(&paletteram[offset], data);
}




/***************************************************************************

							Video Registers

***************************************************************************/

/*	[gtmr]

Initial self test:
600000:4BC0 94C0 4C40 94C0-0404 0002 0000 0000
680000:4BC0 94C0 4C40 94C0-1C1C 0002 0000 0000

700000:0040 0000 0001 0180-0000 0000 0000 0000
700010:0040 0000 0040 0000-0040 0000 2840 1E00

Race start:
600000:DC00 7D00 DC80 7D00-0404 0002 0000 0000
680000:DC00 7D00 DC80 7D00-1C1C 0002 0000 0000

700000:0040 0000 0001 0180-0000 0000 0000 0000
700010:0040 0000 0040 0000-0040 0000 2840 1E00

*/

READ_HANDLER( kaneko16_screen_regs_r )
{
	return READ_WORD(&kaneko16_screen_regs[offset]);
}

WRITE_HANDLER( kaneko16_screen_regs_w )
{
	int new_data;

	COMBINE_WORD_MEM(&kaneko16_screen_regs[offset],data);
	new_data  = READ_WORD(&kaneko16_screen_regs[offset]);

	switch (offset)
	{
		case 0x00:	flipsprites = new_data & 3;	break;
	}

	//logerror("CPU #0 PC %06X : Warning, screen reg %04X <- %04X\n",cpu_get_pc(),offset,data);
}



/*	[gtmr]

	car select screen scroll values:
	Flipscreen off:
		$6x0000: $72c0 ; $fbc0 ; 7340 ; 0
		$72c0/$40 = $1cb = $200-$35	/	$7340/$40 = $1cd = $1cb+2

		$fbc0/$40 = -$11

	Flipscreen on:
		$6x0000: $5d00 ; $3780 ; $5c80 ; $3bc0
		$5d00/$40 = $174 = $200-$8c	/	$5c80/$40 = $172 = $174-2

		$3780/$40 = $de	/	$3bc0/$40 = $ef

*/

WRITE_HANDLER( kaneko16_layers1_regs_w )
{
	COMBINE_WORD_MEM(&kaneko16_layers1_regs[offset],data);
}

WRITE_HANDLER( kaneko16_layers2_regs_w )
{
	COMBINE_WORD_MEM(&kaneko16_layers2_regs[offset],data);
}


/* Select the high color background image (out of 32 in the ROMs) */
READ_HANDLER( kaneko16_bg15_select_r )
{
	return READ_WORD(&kaneko16_bg15_select[0]);
}
WRITE_HANDLER( kaneko16_bg15_select_w )
{
	COMBINE_WORD_MEM(&kaneko16_bg15_select[0],data);
}

/* ? */
READ_HANDLER( kaneko16_bg15_reg_r )
{
	return READ_WORD(&kaneko16_bg15_reg[0]);
}
WRITE_HANDLER( kaneko16_bg15_reg_w )
{
	COMBINE_WORD_MEM(&kaneko16_bg15_reg[0],data);
}



/***************************************************************************

						Callbacks for the TileMap code

							  [ Tiles Format ]

Offset:

0000.w			fedc ba-- ---- ----		unused?
				---- --98 ---- ----		Priority
				---- ---- 7654 32--		Color
				---- ---- ---- --1-		Flip X
				---- ---- ---- ---0		Flip Y

0002.w									Code

***************************************************************************/


/* Background */

#define BG_GFX (0)
#define BG_NX  (0x20)
#define BG_NY  (0x20)

static void get_bg_tile_info(int tile_index)
{
	int code_hi = READ_WORD(&kaneko16_bgram[4*tile_index + 0]);
	int code_lo = READ_WORD(&kaneko16_bgram[4*tile_index + 2]);
	SET_TILE_INFO(BG_GFX, code_lo,(code_hi >> 2) & 0x3f);
	tile_info.flags 	=	TILE_FLIPXY( code_hi & 3 );
}

WRITE_HANDLER( kaneko16_bgram_w )
{
int old_data, new_data;

	old_data  = READ_WORD(&kaneko16_bgram[offset]);
	COMBINE_WORD_MEM(&kaneko16_bgram[offset],data);
	new_data  = READ_WORD(&kaneko16_bgram[offset]);

	if (old_data != new_data)
		tilemap_mark_tile_dirty(bg_tilemap,offset/4);
}





/* Foreground */

#define FG_GFX (0)
#define FG_NX  (0x20)
#define FG_NY  (0x20)

static void get_fg_tile_info(int tile_index)
{
	int code_hi = READ_WORD(&kaneko16_fgram[4*tile_index + 0]);
	int code_lo = READ_WORD(&kaneko16_fgram[4*tile_index + 2]);
	SET_TILE_INFO(FG_GFX, code_lo,(code_hi >> 2) & 0x3f);
	tile_info.flags 	=	TILE_FLIPXY( code_hi & 3 );
	tile_info.priority	=	(code_hi >> 8) & 3;
}

WRITE_HANDLER( kaneko16_fgram_w )
{
int old_data, new_data;

	old_data  = READ_WORD(&kaneko16_fgram[offset]);
	COMBINE_WORD_MEM(&kaneko16_fgram[offset],data);
	new_data  = READ_WORD(&kaneko16_fgram[offset]);

	if (old_data != new_data)
		tilemap_mark_tile_dirty(fg_tilemap,offset/4);
}



WRITE_HANDLER( kaneko16_layers1_w )
{
	if (offset < 0x1000)	kaneko16_fgram_w(offset,data);
	else
	{
		if (offset < 0x2000)	kaneko16_bgram_w((offset-0x1000),data);
		else
		{
			COMBINE_WORD_MEM(&kaneko16_fgram[offset],data);
		}
	}
}








int kaneko16_vh_start(void)
{
	bg_tilemap = tilemap_create(get_bg_tile_info,tilemap_scan_rows,
								TILEMAP_TRANSPARENT, /* to handle the optional hi-color bg */
								16,16,BG_NX,BG_NY);

	fg_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,
								TILEMAP_TRANSPARENT,
								16,16,FG_NX,FG_NY);

	if (!bg_tilemap || !fg_tilemap)
		return 1;

	{
/*
gtmr background:
		flipscreen off: write (x)-$33
		[x=fetch point (e.g. scroll *left* with incresing x)]

		flipscreen on:  write (x+320)+$33
		[x=fetch point (e.g. scroll *right* with incresing x)]

		W = 320+$33+$33 = $1a6 = 422

berlwall background:
6940 off	1a5 << 6
5680 on		15a << 6
*/
		int xdim = Machine->drv->screen_width;
		int ydim = Machine->drv->screen_height;
		int dx, dy;

//		dx   = (422 - xdim) / 2;
		switch (xdim)
		{
			case 320:	dx = 0x33;	dy = 0;		break;
			case 256:	dx = 0x5b;	dy = -8;	break;

			default:	dx = dy = 0;
		}

		tilemap_set_scrolldx( bg_tilemap, -dx,		xdim + dx -1        );
		tilemap_set_scrolldx( fg_tilemap, -(dx+2),	xdim + (dx + 2) - 1 );

		tilemap_set_scrolldy( bg_tilemap, -dy,		ydim + dy -1 );
		tilemap_set_scrolldy( fg_tilemap, -dy,		ydim + dy -1);

		bg_tilemap->transparent_pen = 0;
		fg_tilemap->transparent_pen = 0;
		return 0;
	}
}




/* Berlwall has an additional hi-color background */

void berlwall_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;

	palette += 2048 * 3;	/* first 2048 colors are dynamic */

	/* initialize 555 RGB lookup */
	for (i = 0; i < 32768; i++)
	{
		int r,g,b;

		r = (i >>  5) & 0x1f;
		g = (i >> 10) & 0x1f;
		b = (i >>  0) & 0x1f;

		(*palette++) = (r << 3) | (r >> 2);
		(*palette++) = (g << 3) | (g >> 2);
		(*palette++) = (b << 3) | (b >> 2);
	}
}

int berlwall_vh_start(void)
{
	int sx, x,y;
	unsigned char *RAM	=	memory_region(REGION_GFX3);

	/* Render the hi-color static backgrounds held in the ROMs */

	if ((kaneko16_bg15_bitmap = bitmap_alloc_depth(256 * 32, 256 * 1, 16)) == 0)
		return 1;

/*
	8aba is used as background color
	8aba/2 = 455d = 10001 01010 11101 = $11 $0a $1d
*/

	for (sx = 0 ; sx < 32 ; sx++)	// horizontal screens
	 for (x = 0 ; x < 256 ; x++)	// horizontal pixels
	  for (y = 0 ; y < 256 ; y++)	// vertical pixels
	  {
			int addr  = sx * (256 * 256) + x + y * 256;

			int color = ( RAM[addr * 2 + 0] * 256 + RAM[addr * 2 + 1] ) >> 1;
//				color ^= (0x8aba/2);

			plot_pixel( kaneko16_bg15_bitmap,
						sx * 256 + x, y,
						Machine->pens[2048 + color] );
	  }

	return kaneko16_vh_start();
}

void berlwall_vh_stop(void)
{
	if (kaneko16_bg15_bitmap)
		bitmap_free(kaneko16_bg15_bitmap);

	kaneko16_bg15_bitmap = 0;	// multisession safety
}


/***************************************************************************

								Sprites Drawing

Offset:			Format:						Value:

0000.w			Attribute (type 0, older games: shogwarr, berlwall)

					f--- ---- ---- ----		Multisprite: Use Latched Code + 1
					-e-- ---- ---- ----		Multisprite: Use Latched Color (And Flip?)
					--d- ---- ---- ----		Multisprite: Use Latched X,Y As Offsets
					---c ba-- ---- ----
					---- --98 ---- ----		Priority?
					---- ---- 7654 32--		Color
					---- ---- ---- --1-		X Flip
					---- ---- ---- ---0		Y Flip

				Attribute (type 1: gtmr, gtmr2)

					f--- ---- ---- ----		Multisprite: Use Latched Code + 1
					-e-- ---- ---- ----		Multisprite: Use Latched Color (And Flip?)
					--d- ---- ---- ----		Multisprite: Use Latched X,Y As Offsets
					---c ba-- ---- ----		unused?
					---- --9- ---- ----		X Flip
					---- ---8 ---- ----		Y Flip
					---- ---- 76-- ----		Priority
					---- ---- --54 3210		Color

0002.w										Code
0004.w										X Position << 6
0006.w										Y Position << 6

Note:
	type 2 sprites (berlwall) are like type 0 but the data is held
	in the last 8 bytes of every 16.


***************************************************************************/


/* Map the attribute word to that of the type 1 sprite hardware */

#define MAP_TO_TYPE1(attr) \
	if (kaneko16_spritetype != 1)	/* shogwarr, berlwall */ \
	{ \
		attr =	((attr & 0xfc00)     ) | \
				((attr & 0x03fc) >> 2) | \
				((attr & 0x0003) << 8) ; \
	}


/* Mark the pens of visible sprites */

void kaneko16_mark_sprites_colors(void)
{
	int offs,inc;

	int xmin = Machine->visible_area.min_x - (16 - 1);
	int xmax = Machine->visible_area.max_x;
	int ymin = Machine->visible_area.min_y - (16 - 1);
	int ymax = Machine->visible_area.max_y;

	int nmax				=	Machine->gfx[0]->total_elements;
	int color_granularity	=	Machine->gfx[0]->color_granularity;
	int color_codes_start	=	Machine->drv->gfxdecodeinfo[0].color_codes_start;
	int total_color_codes	=	Machine->drv->gfxdecodeinfo[0].total_color_codes;

	int sx = 0;
	int sy = 0;
	int scode = 0;
	int scolor = 0;

	switch (kaneko16_spritetype)
	{
		case 2:		offs = 8; inc = 16;	break;
		default:	offs = 0; inc = 8;	break;
	}

	for ( ;  offs < spriteram_size ; offs += inc)
	{
		int	attr	=	READ_WORD(&spriteram[offs + 0]);
		int	code	=	READ_WORD(&spriteram[offs + 2]) % nmax;
		int	x		=	READ_WORD(&spriteram[offs + 4]);
		int	y		=	READ_WORD(&spriteram[offs + 6]);

		/* Map the attribute word to that of the type 1 sprite hardware */
		MAP_TO_TYPE1(attr)

		if (x & 0x8000)	x -= 0x10000;
		if (y & 0x8000)	y -= 0x10000;

		x /= 0x40;		y /= 0x40;

		if (attr & 0x8000)		scode++;
		else					scode = code;

		if (!(attr & 0x4000))	scolor = attr % total_color_codes;

		if (attr & 0x2000)		{ sx += x;	sy += y; }
		else					{ sx  = x;	sy  = y; }

		/* Visibility check. No need to account for sprites flipping */
		if ((sx < xmin) || (sx > xmax))	continue;
		if ((sy < ymin) || (sy > ymax))	continue;

		memset(&palette_used_colors[color_granularity * scolor + color_codes_start + 1],PALETTE_COLOR_USED,color_granularity - 1);
	}

}



/* Draw the sprites */

void kaneko16_draw_sprites(struct osd_bitmap *bitmap, int priority)
{
	int offs,inc;

	int max_x	=	Machine->drv->screen_width  - 16;
	int max_y	=	Machine->drv->screen_height - 16;

	int sx = 0;
	int sy = 0;
	int scode = 0;
	int sattr = 0;
	int sflipx = 0;
	int sflipy = 0;

	priority = ( priority & 3 ) << 6;

	switch (kaneko16_spritetype)
	{
		case 2:		offs = 8; inc = 16;	break;
		default:	offs = 0; inc = 8;	break;
	}

	for ( ;  offs < spriteram_size ; offs += inc)
	{
		int	attr	=	READ_WORD(&spriteram[offs + 0]);
		int	code	=	READ_WORD(&spriteram[offs + 2]);
		int	x		=	READ_WORD(&spriteram[offs + 4]);
		int	y		=	READ_WORD(&spriteram[offs + 6]);

		/* Map the attribute word to that of the type 1 sprite hardware */
		MAP_TO_TYPE1(attr)

		if (x & 0x8000)	x -= 0x10000;
		if (y & 0x8000)	y -= 0x10000;

		x /= 0x40;		y /= 0x40;

		if (attr & 0x8000)		scode++;
		else					scode = code;

		if (!(attr & 0x4000))
		{
			sattr  = attr;
			sflipx = attr & 0x200;	sflipy = attr & 0x100;
		}

		if (attr & 0x2000)		{ sx += x;	sy += y; }
		else					{ sx  = x;	sy  = y; }

		if ((sattr & 0xc0) != priority)	continue;

		if (flipsprites & 2) { sx = max_x - sx;		sflipx = !sflipx; }
		if (flipsprites & 1) { sy = max_y - sy;		sflipy = !sflipy; }

		drawgfx(bitmap,Machine->gfx[1],
				scode,
				sattr,
				sflipx, sflipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);

		/* let's get back to normal to support multi sprites */
		if (flipsprites & 2) { sx = max_x - sx;		sflipx = !sflipx; }
		if (flipsprites & 1) { sy = max_y - sy;		sflipy = !sflipy; }
	}

}






/***************************************************************************

								Screen Drawing

***************************************************************************/

void kaneko16_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int flag;
	int layers_ctrl = -1;
	int layers_flip = READ_WORD(&kaneko16_layers1_regs[0x08]);

	tilemap_set_flip(fg_tilemap,((layers_flip & 0x0001) ? TILEMAP_FLIPY : 0) |
								((layers_flip & 0x0002) ? TILEMAP_FLIPX : 0) );

	tilemap_set_flip(bg_tilemap,((layers_flip & 0x0100) ? TILEMAP_FLIPY : 0) |
								((layers_flip & 0x0200) ? TILEMAP_FLIPX : 0) );

	tilemap_set_scrollx(fg_tilemap, 0, READ_WORD(&kaneko16_layers1_regs[0x00]) >> 6 );
	tilemap_set_scrolly(fg_tilemap, 0, READ_WORD(&kaneko16_layers1_regs[0x02]) >> 6 );

	tilemap_set_scrollx(bg_tilemap, 0, READ_WORD(&kaneko16_layers1_regs[0x04]) >> 6 );
	tilemap_set_scrolly(bg_tilemap, 0, READ_WORD(&kaneko16_layers1_regs[0x06]) >> 6 );

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();

	kaneko16_mark_sprites_colors();

	if (palette_recalc())	tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	flag = TILEMAP_IGNORE_TRANSPARENCY;
	if (kaneko16_bg15_bitmap)
	{
/*
	firstscreen	?				(hw: b8,00/06/7/8/9(c8). 202872 = 0880)
	press start	?				(hw: 80-d0,0a. 202872 = 0880)
	teaching	?				(hw: e0,1f. 202872 = 0880 )
	hiscores	!rom5,scr1($9)	(hw: b0,1f. 202872 = )
	lev1-1		rom6,scr2($12)	(hw: cc,0e. 202872 = 0880)
	lev2-1		?				(hw: a7,01. 202872 = 0880)
	lev2-2		rom6,scr1($11)	(hw: b0,0f. 202872 = 0880)
	lev2-4		rom6,scr0($10)	(hw: b2,10. 202872 = 0880)
	lev2-6?		rom5,scr7($f)	(hw: c0,11. 202872 = 0880)
	lev4-2		rom5,scr6($e)	(hw: d3,12. 202872 = 0880)
	redcross	?				(hw: d0,0a. 202872 = )
*/
		int select	=	READ_WORD(&kaneko16_bg15_select[0]);
//		int reg		=	READ_WORD(&kaneko16_bg15_reg[0]);
		int flip	=	select & 0x20;
		int sx, sy;

		if (flip)	select ^= 0x1f;

		sx		=	(select & 0x1f) * 256;
		sy		=	0;

		copybitmap(
			bitmap, kaneko16_bg15_bitmap,
			flip, flip,
			-sx, -sy,
			&Machine->visible_area, TRANSPARENCY_NONE,0 );

		flag = 0;
	}

	if (layers_ctrl & 0x01)	tilemap_draw(bitmap, bg_tilemap, flag);
	else					osd_clearbitmap(Machine->scrbitmap);

	if (layers_ctrl & 0x02)	tilemap_draw(bitmap, fg_tilemap, 0);
	if (layers_ctrl & 0x08)	kaneko16_draw_sprites(bitmap,0);

	if (layers_ctrl & 0x04)	tilemap_draw(bitmap, fg_tilemap, 1);
	if (layers_ctrl & 0x08)	kaneko16_draw_sprites(bitmap,1);

	if (layers_ctrl & 0x10)	tilemap_draw(bitmap, fg_tilemap, 2);
	if (layers_ctrl & 0x08)	kaneko16_draw_sprites(bitmap,2);

	if (layers_ctrl & 0x20)	tilemap_draw(bitmap, fg_tilemap, 3);
	if (layers_ctrl & 0x08)	kaneko16_draw_sprites(bitmap,3);
}
