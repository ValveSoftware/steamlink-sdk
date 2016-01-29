#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *taitob_fscroll;
unsigned char *taitob_bscroll;

unsigned char *b_backgroundram;
unsigned char *b_foregroundram;
unsigned char *b_textram;
unsigned char *taitob_pixelram;

size_t b_backgroundram_size;
size_t b_foregroundram_size;
size_t b_textram_size;
size_t b_pixelram_size;

size_t b_paletteram_size;


static struct osd_bitmap *pixel_layer;
static UINT32 pixel_layer_colors[256]; /*to keep track of colors used*/
static UINT8  pixel_layer_dirty[512];

static UINT8 video_control = 0;
static UINT8 text_video_control = 0;

/*TileMaps*/
static struct tilemap *bg_tilemap, *fg_tilemap, *tx_tilemap;
static int b_bg_color_base = 0;
static int b_fg_color_base = 0;
static int b_sp_color_base = 0;
static int b_tx_color_base = 0;
static int b_px_color_base = 0;
static int flipscreen = 0;



READ_HANDLER( taitob_videoram_r )
{
	return READ_WORD(&videoram[offset]);
}

WRITE_HANDLER( taitob_videoram_w )
{
	COMBINE_WORD_MEM(&videoram[offset],data);
}

READ_HANDLER( taitob_video_control_r )
{
	return (video_control<<8);
}

WRITE_HANDLER( taitob_video_control_w )
{
	int val = (data>>8)&0xff;

/*
* tetrist:
* 0x8f (10001111) - ???
* 0xcf (11001111) - ???
* 0xaf (10101111) - display lower 256 lines of pixel layer
* 0xef (11101111) - display upper 256 lines of pixel_layer
*
* crimec:
* 0xef - display upper part of pixel layer
*
* 0x28 - don't display  pixel layer (or maybe diplay the other part of it?)
*        (but continue displaying the background, foreground, text, sprites)
*
* 0x29 - set just before enabling pixel layer
*        (note that the game doesn't clear the pixel layer between enabling and
*         disabling it, it just writes 0x29 here instead)
*
* 0x20 - set at the third level (in the garage) in Crime City
*        also set at the start in Ashura Blaster and Puzzle Bobble.
*        My best guess so far is that bit 3 (0x08) changes display mode
*        (priority system). When set, draw order is (from back to front):
*        background, foreground, sprites 0 and 1, text,
*        when it is cleared: background, sprites 0, foreground, sprites 1, text
*        where sprites 1 and sprites 0 have _some_ priority. In Ashura Blaster
*        the priority bit is bit 0 of color code.
*
* bit 3 (0x08) see comment above
* bit 4 (0x10) - when set screen flip is on (this one is for sure)
* bit 5 (0x20) seems to be global video enable switch (Hit the Ice resets that bit, clears videoram portions and sets that bit)
*/

	if ( ((video_control & 1)==0) && ((val & 1)==1) ) /*kludge ?*/
	{
		//usrintf_showmessage("pixel layer clear");
		memset(taitob_pixelram,0,b_pixelram_size);
		memset(pixel_layer_dirty, 1, sizeof(pixel_layer_dirty));
		memset(pixel_layer_colors, 0, sizeof(pixel_layer_colors));
		pixel_layer_colors[0] = pixel_layer->width * pixel_layer->height;
	}

#if 0
	if (val != video_control)
		usrintf_showmessage("video control = %02x",val);
#endif

	video_control = val;

	//flip_screen_w(0, video_control & 0x10);
	//tilemap_set_flip(ALL_TILEMAPS, (video_control & 0x10) ? (TILEMAP_FLIPX | TILEMAP_FLIPY) : 0);
}

READ_HANDLER( taitob_text_video_control_r )
{
	return (text_video_control<<8);
}

WRITE_HANDLER( taitob_text_video_control_w )
{
	int val = (data>>8)&0xff;
/*
* hitice:
* 0x08 (00001000) - show game text: credits XX, player1, etc...
* 0x09 (00001001) - show FBI logo
*
* rambo3:
* 0x08 (00001000) - show game text
* 0x09 (00001001) - show taito logo
* 0x0a (00001010) - used in pair with 0x09 to smooth screen transitions (attract mode)
*
* This location controls which page of video text ram to view
* Don't know if bit 3 (0x08) is video text enable/disable ????
*/

#if 0
	if (val != text_video_control)
		usrintf_showmessage("text video control = %02x",val);
#endif
	text_video_control = val;
}



READ_HANDLER( taitob_pixelram_r )
{
	return READ_WORD(&taitob_pixelram[offset]);
}
WRITE_HANDLER( taitob_pixelram_w )
{
	int oldword = READ_WORD (&taitob_pixelram[offset]);
	int newword = COMBINE_WORD (oldword, data);

	if (oldword != newword) /*this results in bad colors, if you reset the game on warning screen, because MAME will invalidate the pens*/
	{
		pixel_layer_colors[(oldword>>8) & 0xff]--;
		pixel_layer_colors[   (oldword) & 0xff]--;

		pixel_layer_colors[(newword>>8) & 0xff]++;
		pixel_layer_colors[   (newword) & 0xff]++;

		WRITE_WORD (&taitob_pixelram[offset], newword);

		pixel_layer_dirty[ offset>>9 ] = 1;
	}
}

void taitob_redraw_pixel_layer_dirty(void)
{
	unsigned short *pens = Machine->pens + b_px_color_base;
	int sy;

	for (sy=0; sy < 512; sy++)
	{
		if (pixel_layer_dirty[sy])
		{
			int sx;

			pixel_layer_dirty[sy] = 0;
			for (sx = 0; sx < 512; sx += 2)
			{
				int color = READ_WORD(&taitob_pixelram[ sy*512 + sx ]);

				plot_pixel(pixel_layer, sx,   sy, pens[(color>>8) & 0xff]);
				plot_pixel(pixel_layer, sx+1, sy, pens[     color & 0xff]);
			}
		}
	}
}


WRITE_HANDLER( masterw_pixelram_w )
{
	unsigned short *pens = Machine->pens + b_px_color_base;
    int sx,sy,color1,color2;
	int i;

	COMBINE_WORD_MEM(&taitob_pixelram[offset],data);

	sx = (offset >> 5) & 0xff;
	sy = offset & 0x1e;
	sx*=1;
	sy*=8;

	color1 = READ_WORD(&taitob_pixelram[offset & ~0x2000]);
	color1 = (color1 >> 8) | (color1 << 8);
	color2 = READ_WORD(&taitob_pixelram[offset | 0x2000]);
	color2 = (color2 >> 8) | (color2 << 8);

	for (i=0; i<16; i++)
	{
		plot_pixel(pixel_layer, sx, sy + i, pens[((color1>>i)&1)|((color2>>i)&1)] );
	}
}


READ_HANDLER( hitice_pixelram_r )
{
	return READ_WORD(&taitob_pixelram[offset]);
}
WRITE_HANDLER( hitice_pixelram_w )
{
	unsigned short *pens = Machine->pens + b_px_color_base;
    int sx,sy,color;

	{
		COMBINE_WORD_MEM(&taitob_pixelram[offset],data);

		sx = (offset) & 0x1ff;
		sy = (offset >> 9);


		color = READ_WORD(&taitob_pixelram[offset]);

		//if (color != 0)
		//	usrintf_showmessage("hitice write to pixelram != 0 offs=%05x data=%08x",offset,color);

		plot_pixel(pixel_layer, sx	, sy, pens[(color>>8) & 0xff]);
		plot_pixel(pixel_layer, sx+1, sy, pens[ 	color & 0xff]);
	}
}

void taitob_vh_stop (void)
{
#if 0
FILE * fo;
int i;
	if (taitob_pixelram)
	{
		fo = fopen("mastpix.bin","wb");
		for (i=0; i<0x10000-0x9000; i+=2)
		{
			int j = READ_WORD(&taitob_pixelram[i]);
			fputc( (j>>0)&255 , fo);
			fputc( (j>>8)&255 , fo);
		}
		fclose(fo);
	}
#endif

	bitmap_free(pixel_layer);
	pixel_layer = 0;
}


static void get_bg_tile_info(int tile_index)
{
	int tile  = READ_WORD(&b_backgroundram[2*tile_index]);
	int color = READ_WORD(&b_backgroundram[2*tile_index + 0x2000]);

	/*there are 0x1fff tiles in all games but rambo3 where it is 0x3fff, qzshowby 0x7fff*/
	SET_TILE_INFO(1, tile & 0x7fff, b_bg_color_base + (color&0x3f) )
	tile_info.flags = TILE_FLIPYX((color & 0x00c0)>>6);
}

static void get_fg_tile_info(int tile_index)
{
	int tile  = READ_WORD(&b_foregroundram[2*tile_index]);
	int color = READ_WORD(&b_foregroundram[2*tile_index + 0x2000]);

	/*there are 0x1fff tiles in all games but rambo3 where it is 0x3fff, qzshowby 0x7fff*/
	SET_TILE_INFO(1, tile & 0x7fff, b_fg_color_base + (color&0x3f) )
	tile_info.flags = TILE_FLIPYX((color & 0x00c0)>>6);
}

static void get_tx_tile_info(int tile_index)
{
	int tile  = READ_WORD(&b_textram[2*tile_index]);

	SET_TILE_INFO(0, tile & 0x0fff, b_tx_color_base + ((tile>>12) & 0x0f) )
	/*no flip attribute*/
}


int taitob_vh_start_core (void)
{
	pixel_layer = 0;

	bg_tilemap = tilemap_create(get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,16,16,64,64);
	fg_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,64,64);
	tx_tilemap = tilemap_create(get_tx_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32*4); /*there is a simple paging system used in Rambo3 and Hit the Ice*/

	if (!bg_tilemap || !fg_tilemap || !tx_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 0; /* ?? */
	tx_tilemap->transparent_pen = 0; /* ?? */

	flipscreen = 0; /*maybe not needed*/

	/* size of pixel ram is 512x512 for tetris, ashura, crimec,
	*  and twice that for hitice, but I'm not sure if the latter
	*  actually uses *pixel ram*. It might be something else. */

	if ((pixel_layer = bitmap_alloc (512*2, 512*2)) == 0)
	{
		return 1;
	}

	memset(pixel_layer_colors, 0, sizeof(pixel_layer_colors));
	pixel_layer_colors[0] = pixel_layer->width * pixel_layer->height;

	memset(pixel_layer_dirty, 1, sizeof(pixel_layer_dirty));

	return 0;
}

int taitob_vh_start_color_order0 (void)
{
	/*graphics are shared, only that they use different palette*/
	/*this is the basic layout used in: Nastar, Ashura Blaster, Hit the Ice, Rambo3, Tetris*/

	/*Note that in both this and color order 1
	* pixel_color_base/color_granularity is equal to sprites color base.
	* Pure coincidence ????
	*/

	b_bg_color_base = 0xc0;	/*background*/
	b_fg_color_base = 0x80;	/*foreground*/
	b_sp_color_base = 0x40;	/*sprites   */
	b_tx_color_base = 0x00;	/*text      */
	b_px_color_base= 0x400;	/*pixel (this one refers COLOR, not COLOR CODE)*/

	return taitob_vh_start_core();
}

int taitob_vh_start_color_order1 (void)
{
	/*and this is the reversed layout used in: Crime City, Puzzle Bobble*/
	b_bg_color_base = 0x00;
	b_fg_color_base = 0x40;
	b_sp_color_base = 0x80;
	b_tx_color_base = 0xc0;
	b_px_color_base = 0x800; /*(this one refers COLOR, not COLOR CODE)*/

	return taitob_vh_start_core();
}

int taitob_vh_start_color_order2 (void)
{
	b_bg_color_base = 0x30;
	b_fg_color_base = 0x20;
	b_sp_color_base = 0x10;
	b_tx_color_base = 0x00;
	b_px_color_base = 0x100; /* my guess */ /*(this one refers COLOR, not COLOR CODE)*/

	return taitob_vh_start_core();
}

READ_HANDLER ( taitob_text_r )
{
	return READ_WORD(&b_textram[offset]);
}

WRITE_HANDLER( taitob_text_w_tm )
{
	int oldword = READ_WORD (&b_textram[offset]);
	int newword = COMBINE_WORD (oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD (&b_textram[offset],newword);
		tilemap_mark_tile_dirty(tx_tilemap, offset / 2);
	}
}

READ_HANDLER ( taitob_background_r )
{
	return READ_WORD(&b_backgroundram[offset]);
}

WRITE_HANDLER( taitob_background_w_tm )
{
	int oldword = READ_WORD (&b_backgroundram[offset]);
	int newword = COMBINE_WORD (oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD (&b_backgroundram[offset],newword);
		tilemap_mark_tile_dirty(bg_tilemap,(offset&0x1fff) / 2);
	}
}

READ_HANDLER ( taitob_foreground_r )
{
	return READ_WORD(&b_foregroundram[offset]);
}

WRITE_HANDLER( taitob_foreground_w_tm )
{
	int oldword = READ_WORD (&b_foregroundram[offset]);
	int newword = COMBINE_WORD (oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD (&b_foregroundram[offset],newword);
		tilemap_mark_tile_dirty(fg_tilemap,(offset&0x1fff) / 2);
	}
}



void taitob_mark_sprite_colors(void)
{
	int offs,color;
	unsigned int palette_map[256];
	unsigned int *pen_usage = Machine->gfx[1]->pen_usage;
	unsigned int elem_mask = Machine->gfx[1]->total_elements - 1;

	memset(palette_map, 0, sizeof(palette_map));

	/* Sprites */
	for (offs = 0;offs < 0x1980;offs += 16)
	{
		int tile;

		tile = READ_WORD(&videoram[offs]);
		color = b_sp_color_base + (READ_WORD(&videoram[offs+2]) & 0x3f);

		palette_map[color] |= pen_usage[tile & elem_mask];
	}

	/* Tell MAME about the color usage */
	for (color = 0; color < 256; color++)
	{
		int usage = palette_map[color];
		int i;

		if (usage)
		{
			if (usage & (1 << 0))
				palette_used_colors[ color * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
			for (i = 1; i < 16; i++)
				if (usage & (1 << i))
					palette_used_colors[ color * 16 + i] = PALETTE_COLOR_USED;
		}
	}
}

void taitob_mark_pixellayer_colors(void)
{
	int i;

	if (pixel_layer_colors[0])
		palette_used_colors[ b_px_color_base + 0 ] = PALETTE_COLOR_TRANSPARENT;

	/* Tell MAME about the color usage */
	for (i = 1; i < 256; i++)
	{
		if ( pixel_layer_colors[i] )	/*mark as used if histogram shows it*/
		{
			palette_used_colors[ b_px_color_base + i ] = PALETTE_COLOR_USED;
		}
	}
}


void taitob_draw_sprites (struct osd_bitmap *bitmap)
{
	/*
		Sprite format: (16 bytes per sprite)
              offset:             bits:
		0000: 0xxxxxxxxxxxxxxx: tile code - 0x0000 to 0x7fff in qzshowby

		0002: 0000000000xxxxxx: color (0x00 - 0x3f)
		      x000000000000000: flipy
		      0x00000000000000: flipx
		      00????????000000: unused ?

		0004: xxxxxx0000000000: doesn't matter - some games (eg nastar) fill this with sign bit, some (eg ashura) do not
		      000000xxxxxxxxxx: x-coordinate 10 bits signed (all zero for sprites forming up a big sprite, except for the first one)

		0006: xxxxxx0000000000: doesn't matter - same as x
		      000000xxxxxxxxxx: y-coordinate 10 bits signed (same as x)

		0008: xxxxxxxx00000000: sprite x-zoom level
		      00000000xxxxxxxx: sprite y-zoom level

			  0x00 - non scaled = 100%
			  0x80 - scaled to 50%
			  0xc0 - scaled to 25%
			  0xe0 - scaled to 12.5%
			  0xff - scaled to zero pixels size (off)

		Sprite zoom is used in Ashura Blaster just in the beginning
		where you can see a big choplifter and a japanese title.
		This japanese title is a scaled sprite.
		It is used in Crime City also at the end of the third level (in the garage)
		where there are four columns on the sides of the screen

		*Heaviest* usage is in Rambo 3 - almost every sprite in game is scaled

		000a: xxxxxxxx00000000: x-sprites number (big sprite) decremented by one
		      00000000xxxxxxxx: y-sprites number (big sprite) decremented by one

		000c - 000f: unused (unused in Ashura - checked)

	*/

	int x,y,xlatch=0,ylatch=0,x_no=0,y_no=0,x_num=0,y_num=0,big_sprite=0;
	int offs,code,color,flipx,flipy;
	unsigned int data, zoomx, zoomy, zx, zy, zoomxlatch=0, zoomylatch=0;

	for (offs = 0x1980-16; offs >=0; offs -= 16)
	{
		code = READ_WORD(&videoram[offs]);

		color = READ_WORD(&videoram[offs+2]);
		flipx = color & 0x4000;
		flipy = color & 0x8000;
#if 0
/*check the unknown bits*/
		if (color & 0x3fc0)
		{
			logerror("sprite color (taitob)=%4x ofs=%4x\n",color,offs);
			color = rand()&0x3f;
		}
#endif
		color = b_sp_color_base + (color & 0x3f);

		x = READ_WORD(&videoram[offs+4]) & 0x3ff;
		y = READ_WORD(&videoram[offs+6]) & 0x3ff;
		if (x >= 0x200)	x -= 0x400;
		if (y >= 0x200)	y -= 0x400;

		data = READ_WORD(&videoram[offs+0x0a]);
		if (data)
		{
			if (!big_sprite)
			{
				x_num = (data>>8) & 0xff;
				y_num = (data) & 0xff;
				x_no  = 0;
				y_no  = 0;
				xlatch = x;
				ylatch = y;
				data = READ_WORD(&videoram[offs+0x08]);
				zoomxlatch = (data>>8) & 0xff;
				zoomylatch = (data) & 0xff;
				big_sprite = 1;
			}
		}

		data = READ_WORD(&videoram[offs+0x08]);
		zoomx = (data>>8) & 0xff;
		zoomy = (data) & 0xff;
		zx = (0x100 - zoomx) / 16;
		zy = (0x100 - zoomy) / 16;

		if (big_sprite)
		{
			zoomx = zoomxlatch;
			zoomy = zoomylatch;

			x = xlatch + x_no * (0x100 - zoomx) / 16;
			y = ylatch + y_no * (0x100 - zoomy) / 16;
			zx = xlatch + (x_no+1) * (0x100 - zoomx) / 16 - x;
			zy = ylatch + (y_no+1) * (0x100 - zoomy) / 16 - y;
			y_no++;
			if (y_no>y_num)
			{
				y_no = 0;
				x_no++;
				if (x_no>x_num)
					big_sprite = 0;
			}
		}

		if ( zoomx || zoomy )
		{
			drawgfxzoom (bitmap,Machine->gfx[1],
				code,color,
				flipx,flipy,
				x,y,
				&Machine->visible_area,
				TRANSPARENCY_PEN,0,(zx << 16) / 16,(zy << 16) / 16);
		}
		else
		{
			drawgfx (bitmap,Machine->gfx[1],
				code,color,
				flipx,flipy,
				x,y,
				&Machine->visible_area,
				TRANSPARENCY_PEN,0);
		}
	}
}

void taitob_vh_screenrefresh_tm(struct osd_bitmap *bitmap,int full_refresh)
{

	/* Update tilemaps */
	tilemap_set_scrollx( bg_tilemap,0, -READ_WORD(&taitob_bscroll[0]) );
	tilemap_set_scrolly( bg_tilemap,0, -READ_WORD(&taitob_bscroll[2]) );
	tilemap_set_scrollx( fg_tilemap,0, -READ_WORD(&taitob_fscroll[0]) );
	tilemap_set_scrolly( fg_tilemap,0, -READ_WORD(&taitob_fscroll[2]) );

	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	palette_init_used_colors();
	taitob_mark_sprite_colors();
	if (palette_recalc ())
		full_refresh = 1;

	if (full_refresh)
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);
	tilemap_draw(bitmap,fg_tilemap,0);

	taitob_draw_sprites(bitmap);

	tilemap_draw(bitmap,tx_tilemap,0);
}



/*
* This is the same as taitob_draw_sprites(); only difference is that here we
* check bit 0 of color code and only draw a sprite if that bit is equal to
* 'priority' parameter
*/
void ashura_draw_sprites (struct osd_bitmap *bitmap, int priority)
{
	int x,y,xlatch=0,ylatch=0,x_no=0,y_no=0,x_num=0,y_num=0,big_sprite=0;
	int data,offs,code,color,flipx,flipy;
	unsigned int zoomx, zoomy, zx, zy, zoomxlatch=0, zoomylatch=0;

	for (offs = 0x1980-16; offs >= 0; offs -= 16)
	{
		code = READ_WORD(&videoram[offs]);

		color = READ_WORD(&videoram[offs+2]);
		flipx = color & 0x4000;
		flipy = color & 0x8000;
		//if (color & 0x3fc0) logerror("color sprite (crimec)=%x\n",color);
		color = b_sp_color_base + (color & 0x3f);

		x = READ_WORD(&videoram[offs+4]) & 0x3ff;
		y = READ_WORD(&videoram[offs+6]) & 0x3ff;
		if (x >= 0x200)	x -= 0x400;
		if (y >= 0x200)	y -= 0x400;

		data = READ_WORD(&videoram[offs+0x0a]);
		if (data)
		{
			if (!big_sprite)
			{
				x_num = (data>>8) & 0xff;
				y_num = (data) & 0xff;
				x_no  = 0;
				y_no  = 0;
				xlatch = x;
				ylatch = y;
				data = READ_WORD(&videoram[offs+0x08]);
				zoomxlatch = (data>>8) & 0xff;
				zoomylatch = (data) & 0xff;
				big_sprite = 1;
			}
		}

		data = READ_WORD(&videoram[offs+0x08]);
		zoomx = (data>>8) & 0xff;
		zoomy = (data) & 0xff;
		zx = (0x100 - zoomx) / 16;
		zy = (0x100 - zoomy) / 16;

		if (big_sprite)
		{
			zoomx = zoomxlatch;
			zoomy = zoomylatch;

			x = xlatch + x_no * (0x100 - zoomx) / 16;
			y = ylatch + y_no * (0x100 - zoomy) / 16;
			zx = xlatch + (x_no+1) * (0x100 - zoomx) / 16 - x;
			zy = ylatch + (y_no+1) * (0x100 - zoomy) / 16 - y;
			y_no++;
			if (y_no>y_num)
			{
				y_no = 0;
				x_no++;
				if (x_no>x_num)
					big_sprite = 0;
			}
		}

		if ( (color&1) == priority ) /*needed for ashura and crimec*/
		{
			if ( zoomx || zoomy )
			{
				drawgfxzoom (bitmap,Machine->gfx[1],
					code,color,
					flipx,flipy,
					x,y,
					&Machine->visible_area,
					TRANSPARENCY_PEN,0,(zx << 16) / 16,(zy << 16) / 16);
			}
			else
			{
				drawgfx (bitmap,Machine->gfx[1],
					code,color,
					flipx,flipy,
					x,y,
					&Machine->visible_area,
					TRANSPARENCY_PEN,0);
			}
		}
	}
}

void ashura_vh_screenrefresh_tm(struct osd_bitmap *bitmap,int full_refresh)
{
	/* Update tilemaps */
	tilemap_set_scrollx( bg_tilemap,0, -READ_WORD(&taitob_bscroll[0]) );
	tilemap_set_scrolly( bg_tilemap,0, -READ_WORD(&taitob_bscroll[2]) );
	tilemap_set_scrollx( fg_tilemap,0, -READ_WORD(&taitob_fscroll[0]) );
	tilemap_set_scrolly( fg_tilemap,0, -READ_WORD(&taitob_fscroll[2]) );

	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	palette_init_used_colors();
	taitob_mark_sprite_colors();
	if ( (video_control&0xef) == 0xef)
	{
		taitob_mark_pixellayer_colors();
	}
	if ( palette_recalc() )
	{
		memset(pixel_layer_dirty, 1, sizeof(pixel_layer_dirty));
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
	}


	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);

	ashura_draw_sprites(bitmap,1);

	tilemap_draw(bitmap,fg_tilemap,0);

	ashura_draw_sprites(bitmap,0);

	if ((video_control&0xef) == 0xef) /*masked out bit is screen flip*/
	{
	 	taitob_redraw_pixel_layer_dirty();
		copybitmap(bitmap,pixel_layer,0,0,0,0,&Machine->visible_area,TRANSPARENCY_PEN, Machine->pens[ b_px_color_base ] );
	}

	tilemap_draw(bitmap,tx_tilemap,0);
}



/*Crime City section*/

void crimec_vh_screenrefresh_tm(struct osd_bitmap *bitmap,int full_refresh)
{

	/* Update tilemaps */
	tilemap_set_scrollx( bg_tilemap,0, -READ_WORD(&taitob_bscroll[0]) );
	tilemap_set_scrolly( bg_tilemap,0, -READ_WORD(&taitob_bscroll[2]) );
	tilemap_set_scrollx( fg_tilemap,0, -READ_WORD(&taitob_fscroll[0]) );
	tilemap_set_scrolly( fg_tilemap,0, -READ_WORD(&taitob_fscroll[2]) );

	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	palette_init_used_colors();
	taitob_mark_sprite_colors();
	if ( (video_control&0xef) == 0xef)
	{
		taitob_mark_pixellayer_colors();
	}
	if ( palette_recalc() )
	{
		memset(pixel_layer_dirty, 1, sizeof(pixel_layer_dirty));
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
	}

	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);

	if (!(video_control&0x08))
		ashura_draw_sprites (bitmap,1);

	tilemap_draw(bitmap,fg_tilemap,0);

 /*a HACK !*/
	if ((video_control&0xef) == 0xef) /*masked out bit is screen flip*/
	{
		taitob_redraw_pixel_layer_dirty();
		copybitmap(bitmap,pixel_layer,0,0,0,0,&Machine->visible_area,TRANSPARENCY_PEN, Machine->pens[ b_px_color_base ] );
	}

	if (!(video_control&0x08))
		ashura_draw_sprites (bitmap,0);
	else
	{
		ashura_draw_sprites (bitmap,1);
		ashura_draw_sprites (bitmap,0);
	}

	tilemap_draw(bitmap,tx_tilemap,0);
}



/*Tetris section*/

void tetrist_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int scrollx, scrolly;

	palette_init_used_colors();
	taitob_mark_pixellayer_colors();

	if (palette_recalc())
		full_refresh = 1;

	if ( full_refresh )
	{
		memset(pixel_layer_dirty, 1, sizeof(pixel_layer_dirty));
	}

	taitob_redraw_pixel_layer_dirty();

	switch ( (video_control>>6) & 3 )
	{
	/*is MSB - pixel layer enable ? */

	case 0: /* 00xx xxxx */
		break;

	case 1: /* 01xx xxxx */
		break;

	case 2: /* 10xx xxxx */
		scrollx = 0;
		scrolly = -256;
		copyscrollbitmap(bitmap,pixel_layer,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
		break;

	case 3: /* 11xx xxxx */
		scrollx = 0;
		scrolly = 0;
		copyscrollbitmap(bitmap,pixel_layer,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
		break;
	}
}


void hitice_vh_screenrefresh_tm(struct osd_bitmap *bitmap,int full_refresh)
{
	int tx_scrolly;

	/* Update tilemaps */
	tilemap_set_scrollx( bg_tilemap,0, -READ_WORD(&taitob_bscroll[0]) );
	tilemap_set_scrolly( bg_tilemap,0, -READ_WORD(&taitob_bscroll[2]) );
	tilemap_set_scrollx( fg_tilemap,0, -READ_WORD(&taitob_fscroll[0]) );
	tilemap_set_scrolly( fg_tilemap,0, -READ_WORD(&taitob_fscroll[2]) );

/*this is more paging than scrolling, but I see no other way to implement it*/
	switch(text_video_control)
	{
	case 0x08:
		tx_scrolly = 0;
		break;
	case 0x09:
		tx_scrolly = 32*1*8;
		break;
	case 0x0a:
		tx_scrolly = 32*2*8;
		break;
	default: /*not used by any game so far*/
		tx_scrolly = 32*3*8;
		usrintf_showmessage("Text layer scroll-paging unknown mode: %i",text_video_control);
		break;
	}
	tilemap_set_scrollx( tx_tilemap,0, 0 );
	tilemap_set_scrolly( tx_tilemap,0, tx_scrolly );

	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	palette_init_used_colors();
	taitob_mark_sprite_colors();
	if (palette_recalc ())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);


	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);
	tilemap_draw(bitmap,fg_tilemap,0);

	taitob_draw_sprites(bitmap);

	tilemap_draw(bitmap,tx_tilemap,0);
}


void rambo3_vh_screenrefresh_tm(struct osd_bitmap *bitmap,int full_refresh)
{
	int tx_scrolly;

	/* Update tilemaps */
	tilemap_set_scrollx( bg_tilemap,0, -READ_WORD(&taitob_bscroll[0]) );
	tilemap_set_scrolly( bg_tilemap,0, -READ_WORD(&taitob_bscroll[2]) );
	tilemap_set_scrollx( fg_tilemap,0, -READ_WORD(&taitob_fscroll[0]) );
	tilemap_set_scrolly( fg_tilemap,0, -READ_WORD(&taitob_fscroll[2]) );

/*this is more paging than scrolling, but I see no other way to implement it*/
	switch(text_video_control)
	{
	case 0x08:
		tx_scrolly = 0;
		break;
	case 0x09:
		tx_scrolly = 32*1*8;
		break;
	case 0x0a:
		tx_scrolly = 32*2*8;
		break;
	default: /*not used by any game so far*/
		tx_scrolly = 32*3*8;
		usrintf_showmessage("Text layer scroll-paging unknown mode: %i",text_video_control);
		break;
	}
	tilemap_set_scrollx( tx_tilemap,0, 0 );
	tilemap_set_scrolly( tx_tilemap,0, tx_scrolly );

	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	palette_init_used_colors();
	taitob_mark_sprite_colors();
	if (palette_recalc ())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);
	tilemap_draw(bitmap,fg_tilemap,0);

	taitob_draw_sprites (bitmap);

	tilemap_draw(bitmap,tx_tilemap,0);

}


void puzbobb_vh_screenrefresh_tm(struct osd_bitmap *bitmap,int full_refresh)
{
	/* Update tilemaps */
	tilemap_set_scrollx( bg_tilemap,0, -READ_WORD(&taitob_bscroll[0]) );
	tilemap_set_scrolly( bg_tilemap,0, -READ_WORD(&taitob_bscroll[2]) );
	tilemap_set_scrollx( fg_tilemap,0, -READ_WORD(&taitob_fscroll[0]) );
	tilemap_set_scrolly( fg_tilemap,0, -READ_WORD(&taitob_fscroll[2]) );

	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	palette_init_used_colors();
	taitob_mark_sprite_colors();
	if (palette_recalc ())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);

	//if (!(video_control&0x08))  //sprites 1
	//	ashura_draw_sprites (bitmap,1);

	tilemap_draw(bitmap,fg_tilemap,0);

	//if (!(video_control&0x08))
	//	ashura_draw_sprites (bitmap,0); //sprites 0
	//else
	//{
	//	ashura_draw_sprites (bitmap,1); //both
	//	ashura_draw_sprites (bitmap,0);
	//}
	taitob_draw_sprites (bitmap); //both

	tilemap_draw(bitmap,tx_tilemap,0);

}


void qzshowby_vh_screenrefresh_tm(struct osd_bitmap *bitmap,int full_refresh)
{
	/* Update tilemaps */
	tilemap_set_scrollx( bg_tilemap,0, -READ_WORD(&taitob_bscroll[0]) );
	tilemap_set_scrolly( bg_tilemap,0, -READ_WORD(&taitob_bscroll[2]) );
	tilemap_set_scrollx( fg_tilemap,0, -READ_WORD(&taitob_fscroll[0]) );
	tilemap_set_scrolly( fg_tilemap,0, -READ_WORD(&taitob_fscroll[2]) );

	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	palette_init_used_colors();
	taitob_mark_sprite_colors();
	if (palette_recalc ())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);


	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);

	//if (!(video_control&0x08))  //sprites 1
	//	ashura_draw_sprites (bitmap,1);

	tilemap_draw(bitmap,fg_tilemap,0);

	//if (!(video_control&0x08))
	//	ashura_draw_sprites (bitmap,0); //sprites 0
	//else
	//{
	//	ashura_draw_sprites (bitmap,1); //both
	//	ashura_draw_sprites (bitmap,0);
	//}

	taitob_draw_sprites (bitmap); //both

	tilemap_draw(bitmap,tx_tilemap,0);
}



void masterw_vh_screenrefresh_tm(struct osd_bitmap *bitmap,int full_refresh)
{

	/* Update tilemaps */
	tilemap_set_scrollx( bg_tilemap,0, -READ_WORD(&taitob_bscroll[0]) );
	tilemap_set_scrolly( bg_tilemap,0, -READ_WORD(&taitob_bscroll[2]) );
	tilemap_set_scrollx( fg_tilemap,0, -READ_WORD(&taitob_fscroll[0]) );
	tilemap_set_scrolly( fg_tilemap,0, -READ_WORD(&taitob_fscroll[2]) );

	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	palette_init_used_colors();
	taitob_mark_sprite_colors();
	if (palette_recalc ())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);


	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);

	if (( video_control&0x01) == 0x01)
	{
		int scrollx = 80;
		int scrolly = 0;
		copyscrollbitmap(bitmap,pixel_layer,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN, Machine->pens[ b_px_color_base ] );
	}

	ashura_draw_sprites(bitmap,1);

	tilemap_draw(bitmap,fg_tilemap,0);

	ashura_draw_sprites(bitmap,0);

	tilemap_draw(bitmap,tx_tilemap,0);
}



void silentd_vh_screenrefresh_tm(struct osd_bitmap *bitmap,int full_refresh)
{
int i;
	tilemap_set_scroll_rows( bg_tilemap , 1024);
#if 1
	for (i=0; i<256; i++)
	{
		tilemap_set_scrollx( bg_tilemap, i, -READ_WORD(&taitob_bscroll[i*4]) );
	}
#endif

	/* Update tilemaps */
	//tilemap_set_scrollx( bg_tilemap,0, -READ_WORD(&taitob_bscroll[0x30]) );
	tilemap_set_scrolly( bg_tilemap,0, -READ_WORD(&taitob_bscroll[2]) );
	tilemap_set_scrollx( fg_tilemap,0, -READ_WORD(&taitob_fscroll[0]) );
	tilemap_set_scrolly( fg_tilemap,0, -READ_WORD(&taitob_fscroll[2]) );


	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);
	tilemap_update(tx_tilemap);

	palette_init_used_colors();
	taitob_mark_sprite_colors();
	if (palette_recalc ())
		full_refresh = 1;

	if (full_refresh)
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	/* Draw playfields */
	tilemap_render(ALL_TILEMAPS);
	tilemap_draw(bitmap,bg_tilemap,0);
	tilemap_draw(bitmap,fg_tilemap,0);

	taitob_draw_sprites(bitmap);

	tilemap_draw(bitmap,tx_tilemap,0);
}

