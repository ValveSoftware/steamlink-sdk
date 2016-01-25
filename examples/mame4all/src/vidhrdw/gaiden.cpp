/***************************************************************************

	Ninja Gaiden / Tecmo Knights Video Hardware

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *gaiden_videoram;
unsigned char *gaiden_videoram2;
unsigned char *gaiden_videoram3;

static struct tilemap *text_layer,*foreground,*background;


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_bg_tile_info(int tile_index)
{
	UINT16 *videoram1 = (UINT16 *)&gaiden_videoram3[0x1000];
	UINT16 *videoram2 = (UINT16 *)gaiden_videoram3;
	SET_TILE_INFO(1,videoram1[tile_index] & 0xfff,(videoram2[tile_index] & 0xf0) >> 4)
}

static void get_fg_tile_info(int tile_index)
{
	UINT16 *videoram1 = (UINT16 *)&gaiden_videoram2[0x1000];
	UINT16 *videoram2 = (UINT16 *)gaiden_videoram2;
	SET_TILE_INFO(2,videoram1[tile_index] & 0xfff,(videoram2[tile_index] & 0xf0) >> 4)
}

static void get_tx_tile_info(int tile_index)
{
	UINT16 *videoram1 = (UINT16 *)&gaiden_videoram[0x0800];
	UINT16 *videoram2 = (UINT16 *)gaiden_videoram;
	SET_TILE_INFO(0,videoram1[tile_index] & 0x7ff,(videoram2[tile_index] & 0xf0) >> 4)
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int gaiden_vh_start(void)
{
	background = tilemap_create(get_bg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,64,32);
	foreground = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,64,32);
	text_layer = tilemap_create(get_tx_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,32,32);

	if (!text_layer || !foreground || !background)
		return 1;

	background->transparent_pen = 0;
	foreground->transparent_pen = 0;
	text_layer->transparent_pen = 0;
	palette_transparent_color = 0x200; /* background color */
	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( gaiden_txscrollx_w )
{
	static UINT16 oldword;
	oldword = COMBINE_WORD(oldword,data);
	tilemap_set_scrollx(text_layer,0,oldword);
}

WRITE_HANDLER( gaiden_txscrolly_w )
{
	static UINT16 oldword;
	oldword = COMBINE_WORD(oldword,data);
	tilemap_set_scrolly(text_layer,0,oldword);
}

WRITE_HANDLER( gaiden_fgscrollx_w )
{
	static UINT16 oldword;
	oldword = COMBINE_WORD(oldword,data);
	tilemap_set_scrollx(foreground,0,oldword);
}

WRITE_HANDLER( gaiden_fgscrolly_w )
{
	static UINT16 oldword;
	oldword = COMBINE_WORD(oldword,data);
	tilemap_set_scrolly(foreground,0,oldword);
}

WRITE_HANDLER( gaiden_bgscrollx_w )
{
	static UINT16 oldword;
	oldword = COMBINE_WORD(oldword,data);
	tilemap_set_scrollx(background,0,oldword);
}

WRITE_HANDLER( gaiden_bgscrolly_w )
{
	static UINT16 oldword;
	oldword = COMBINE_WORD(oldword,data);
	tilemap_set_scrolly(background,0,oldword);
}

WRITE_HANDLER( gaiden_videoram3_w )
{
	int oldword = READ_WORD(&gaiden_videoram3[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		int tile_index = (offset/2)&0x7ff;
		WRITE_WORD(&gaiden_videoram3[offset],newword);
		tilemap_mark_tile_dirty(background,tile_index);
	}
}

READ_HANDLER( gaiden_videoram3_r )
{
   return READ_WORD(&gaiden_videoram3[offset]);
}

WRITE_HANDLER( gaiden_videoram2_w )
{
	int oldword = READ_WORD(&gaiden_videoram2[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		int tile_index = (offset/2)&0x7ff;
		WRITE_WORD(&gaiden_videoram2[offset],newword);
		tilemap_mark_tile_dirty(foreground,tile_index);
	}
}

READ_HANDLER( gaiden_videoram2_r )
{
   return READ_WORD(&gaiden_videoram2[offset]);
}

WRITE_HANDLER( gaiden_videoram_w )
{
	int oldword = READ_WORD(&gaiden_videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		int tile_index = (offset/2)&0x3ff;
		WRITE_WORD(&gaiden_videoram[offset],newword);
		tilemap_mark_tile_dirty(text_layer,tile_index);
	}
}

READ_HANDLER( gaiden_videoram_r )
{
	return READ_WORD(&gaiden_videoram[offset]);
}



/***************************************************************************

  Display refresh

***************************************************************************/

/* sprite format:
 *
 *	word		bit					usage
 * --------+-fedcba9876543210-+----------------
 *    0    | ---------------x | flip x
 *         | --------------x- | flip y
 *         | -------------x-- | enable
 *         | ----------x----- | flicker
 *         | --------xx------ | sprite-tile priority
 *    1    | xxxxxxxxxxxxxxxx | number
 *    2    | --------xxxx---- | palette
 *         | --------------xx | size: 8x8, 16x16, 32x32, 64x64
 *    3    | xxxxxxxxxxxxxxxx | y position
 *    4    | xxxxxxxxxxxxxxxx | x position
 *    5,6,7|                  | unused
 */

#define NUM_SPRITES 128

static void mark_sprite_colors(void)
{
	const UINT16 *source = (UINT16 *)spriteram;
	const struct GfxElement *gfx = Machine->gfx[3];
	int i;
	for (i = 0;i < NUM_SPRITES;i++)
	{
		UINT32 attributes = source[0];
		if (attributes & 0x04)	/* visible */
		{
			UINT32 pen_usage = 0xfffe;
			UINT32 color = (source[2] >> 4) & 0xf;
			const UINT16 *pal_data = &gfx->colortable[gfx->color_granularity * color];
			int indx = pal_data - Machine->remapped_colortable;
			while (pen_usage)
			{
				if (pen_usage & 1) palette_used_colors[indx] = PALETTE_COLOR_USED;
				pen_usage >>= 1;
				indx++;
			}
		}
		source += 8;
	}
}

static void draw_sprites( struct osd_bitmap *bitmap )
{
	const UINT8 layout[8][8] =
	{
		{0,1,4,5,16,17,20,21},
		{2,3,6,7,18,19,22,23},
		{8,9,12,13,24,25,28,29},
		{10,11,14,15,26,27,30,31},
		{32,33,36,37,48,49,52,53},
		{34,35,38,39,50,51,54,55},
		{40,41,44,45,56,57,60,61},
		{42,43,46,47,58,59,62,63}
	};

	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[3];
	const UINT16 *source = (NUM_SPRITES-1)*8 + (UINT16 *)spriteram;
	int count = NUM_SPRITES;

	/* draw all sprites from front to back */
	while( count-- )
	{
		UINT32 attributes = source[0];
		if ( (attributes&0x04) && ((attributes&0x20)==0 || (cpu_getcurrentframe() & 1)) )
		{
			UINT32 priority = (attributes>>6)&3;
			UINT32 number = (source[1]&0x7fff);
			UINT32 color = source[2];
			UINT32 size = 1<<(color&0x3); // 1,2,4,8
			UINT32 flipx = (attributes&1);
			UINT32 flipy = (attributes&2);
			UINT32 priority_mask;
			int ypos = source[3] & 0x1ff;
			int xpos = source[4] & 0x1ff;
			int col,row;
			color = (color>>4)&0xf;

			/* wraparound */
			if( xpos >= 256) xpos -= 512;
			if( ypos >= 256) ypos -= 512;

			/* bg: 1; fg:2; text: 4 */
			switch( priority )
			{
				default:
				case 0x0: priority_mask = 0; break;
				case 0x1: priority_mask = 0xf0; break; /* obscured by text layer */
				case 0x2: priority_mask = 0xf0|0xcc; break;	/* obscured by foreground */
				case 0x3: priority_mask = 0xf0|0xcc|0xaa; break; /* obscured by bg and fg */
			}

			for( row=0; row<size; row++ )
			{
				for( col=0; col<size; col++ )
				{
					int sx = xpos + 8*(flipx?(size-1-col):col);
					int sy = ypos + 8*(flipy?(size-1-row):row);
					pdrawgfx(bitmap,gfx,
						number + layout[row][col],
						color,
						flipx,flipy,
						sx,sy,
						clip,TRANSPARENCY_PEN,0,
						priority_mask);
				}
			}
		}
		source -= 8;
	}
}

void gaiden_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();
	mark_sprite_colors();
	palette_used_colors[0x200] = PALETTE_COLOR_USED;

	if (palette_recalc()) tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[0x200],&Machine->visible_area);
	tilemap_draw(bitmap,background,1<<16);
	tilemap_draw(bitmap,foreground,2<<16);
	tilemap_draw(bitmap,text_layer,4<<16);

	draw_sprites( bitmap );
}
