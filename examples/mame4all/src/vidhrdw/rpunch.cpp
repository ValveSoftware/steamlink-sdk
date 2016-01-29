/***************************************************************************

  vidhrdw/rpunch.c

  Functions to emulate the video hardware of the machine.

****************************************************************************/

#ifndef INCLUDE_DRAW_CORE

#include "driver.h"
#include "vidhrdw/generic.h"


#define BITMAP_WIDTH	304
#define BITMAP_HEIGHT	224
#define BITMAP_XOFFSET	4


/*************************************
 *
 *	Statics
 *
 *************************************/

UINT8 *rpunch_bitmapram;
size_t rpunch_bitmapram_size;
static UINT32 *rpunch_bitmapsum;

int rpunch_sprite_palette;

static struct tilemap *background[2];

static UINT16 videoflags;
static UINT8 crtc_register;
static void *crtc_timer;
static UINT8 bins, gins;

static const UINT16 *callback_videoram;
static UINT8 callback_gfxbank;
static UINT8 callback_colorbase;
static UINT16 callback_imagebase;
static UINT16 callback_imagemask;


/*************************************
 *
 *	Prototypes
 *
 *************************************/

void rpunch_vh_stop(void);

static void draw_bitmap_8(struct osd_bitmap *bitmap);
static void draw_bitmap_16(struct osd_bitmap *bitmap);



/*************************************
 *
 *	Tilemap callbacks
 *
 *************************************/

static void get_bg_tile_info(int tile_index)
{
	int data = callback_videoram[tile_index];
	SET_TILE_INFO(callback_gfxbank,
			callback_imagebase | (data & callback_imagemask),
			callback_colorbase | ((data >> 13) & 7));
}



/*************************************
 *
 *	Video system start
 *
 *************************************/

static void crtc_interrupt_gen(int param)
{
	cpu_cause_interrupt(0, 1);
	if (param != 0)
		crtc_timer = timer_pulse(TIME_IN_HZ(Machine->drv->frames_per_second * param), 0, crtc_interrupt_gen);
}


int rpunch_vh_start(void)
{
	int i;

	/* allocate tilemaps for the backgrounds and a bitmap for the direct-mapped bitmap */
	background[0] = tilemap_create(get_bg_tile_info,tilemap_scan_cols,TILEMAP_OPAQUE,     8,8,64,64);
	background[1] = tilemap_create(get_bg_tile_info,tilemap_scan_cols,TILEMAP_TRANSPARENT,8,8,64,64);

	/* allocate a bitmap sum */
	rpunch_bitmapsum = (UINT32*)malloc(BITMAP_HEIGHT * sizeof(UINT32));

	/* if anything failed, clean up and return an error */
	if (!background[0] || !background[1] || !rpunch_bitmapsum)
	{
		rpunch_vh_stop();
		return 1;
	}

	/* configure the tilemaps */
	background[1]->transparent_pen = 15;

	/* reset the sums and bitmap */
	for (i = 0; i < BITMAP_HEIGHT; i++)
		rpunch_bitmapsum[i] = (BITMAP_WIDTH/4) * 0xffff;
	if (rpunch_bitmapram)
		memset(rpunch_bitmapram, 0xff, rpunch_bitmapram_size);

	/* reset the timer */
	crtc_timer = NULL;
	return 0;
}



/*************************************
 *
 *	Video system shutdown
 *
 *************************************/

void rpunch_vh_stop(void)
{
	if (rpunch_bitmapsum)
		free(rpunch_bitmapsum);
	rpunch_bitmapsum = NULL;
}



/*************************************
 *
 *		Write handlers
 *
 *************************************/

WRITE_HANDLER(rpunch_bitmap_w)
{
	if (rpunch_bitmapram)
	{
		int oldword = READ_WORD(&rpunch_bitmapram[offset]);
		int newword = COMBINE_WORD(oldword, data);
		if (oldword != newword)
		{
			int row = offset / 256;
			int col = 2 * (offset % 256) - BITMAP_XOFFSET;
			WRITE_WORD(&rpunch_bitmapram[offset], data);
			if (row < BITMAP_HEIGHT && col >= 0 && col < BITMAP_WIDTH)
				rpunch_bitmapsum[row] += newword - oldword;
		}
	}
}


WRITE_HANDLER(rpunch_videoram_w)
{
	int oldword = READ_WORD(&videoram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	if (oldword != newword)
	{
		int tilemap = offset >> 13;
		int tile_index = (offset / 2) & 0xfff;
		WRITE_WORD(&videoram[offset], newword);
		tilemap_mark_tile_dirty(background[tilemap],tile_index);
	}
}


WRITE_HANDLER(rpunch_videoreg_w)
{
	int newword = COMBINE_WORD(videoflags, data);
	if (videoflags != newword)
	{
		/* invalidate tilemaps */
		if ((newword ^ videoflags) & 0x0410)
			tilemap_mark_all_tiles_dirty(background[0]);
		if ((newword ^ videoflags) & 0x0820)
			tilemap_mark_all_tiles_dirty(background[1]);
		videoflags = newword;
	}
}


WRITE_HANDLER(rpunch_scrollreg_w)
{
	switch (offset / 2)
	{
		case 0:
			tilemap_set_scrolly(background[0], 0, data & 0x1ff);
			break;

		case 1:
			tilemap_set_scrollx(background[0], 0, (data & 0x1ff) - 8);
			break;

		case 2:
			tilemap_set_scrolly(background[1], 0, data & 0x1ff);
			break;

		case 3:
			tilemap_set_scrollx(background[1], 0, data & 0x1ff);
			break;
	}
}


WRITE_HANDLER(rpunch_crtc_data_w)
{
	if (!(data & 0x00ff0000))
	{
		data &= 0xff;
		switch (crtc_register)
		{
			/* only register we know about.... */
			case 0x0b:
				if (crtc_timer)
					timer_remove(crtc_timer);
				crtc_timer = timer_set(cpu_getscanlinetime(Machine->visible_area.max_y + 1), (data == 0xc0) ? 2 : 1, crtc_interrupt_gen);
				break;

			default:
				//logerror("CRTC register %02X = %02X\n", crtc_register, data & 0xff);
				break;
		}
	}
}


WRITE_HANDLER(rpunch_crtc_register_w)
{
	if (!(data & 0x00ff0000))
		crtc_register = data & 0xff;
}


WRITE_HANDLER(rpunch_ins_w)
{
	if (!(data & 0x00ff0000))
	{
		if (offset == 0)
		{
			gins = data & 0x3f;
			//logerror("GINS = %02X\n", data & 0x3f);
		}
		else
		{
			bins = data & 0x3f;
			//logerror("BINS = %02X\n", data & 0x3f);
		}
	}
}


/*************************************
 *
 *		Sprite routines
 *
 *************************************/

static void mark_sprite_palette(void)
{
	UINT16 used_colors[16];
	int offs, i, j;

	memset(used_colors, 0, sizeof(used_colors));
	for (offs = 0; offs < spriteram_size; offs += 8)
	{
		int data1 = READ_WORD(&spriteram[offs + 2]);
		int code = data1 & 0x7ff;

		if (code < 0x600)
		{
			int data0 = READ_WORD(&spriteram[offs + 0]);
			int data2 = READ_WORD(&spriteram[offs + 4]);
			int x = (data2 & 0x1ff) + 8;
			int y = 513 - (data0 & 0x1ff);
			int color = ((data1 >> 13) & 7) | ((videoflags & 0x0040) >> 3);

			if (x >= BITMAP_WIDTH) x -= 512;
			if (y >= BITMAP_HEIGHT) y -= 512;

			if (x > -16 && y > -32)
				used_colors[color] |= Machine->gfx[2]->pen_usage[code];
		}
	}

	for (i = 0; i < 16; i++)
	{
		UINT16 used = used_colors[i];
		if (used)
		{
			for (j = 0; j < 15; j++)
				if (used & (1 << j))
					palette_used_colors[rpunch_sprite_palette + i * 16 + j] = PALETTE_COLOR_USED;
			palette_used_colors[rpunch_sprite_palette + i * 16 + 15] = PALETTE_COLOR_TRANSPARENT;
		}
	}
}


static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;

	/* draw the sprites */
	for (offs = 0; offs < spriteram_size; offs += 8)
	{
		int data1 = READ_WORD(&spriteram[offs + 2]);
		int code = data1 & 0x7ff;

		if (code < 0x600)
		{
			int data0 = READ_WORD(&spriteram[offs + 0]);
			int data2 = READ_WORD(&spriteram[offs + 4]);
			int x = (data2 & 0x1ff) + 8;
			int y = 513 - (data0 & 0x1ff);
			int xflip = data1 & 0x1000;
			int color = ((data1 >> 13) & 7) | ((videoflags & 0x0040) >> 3);

			if (x >= BITMAP_WIDTH) x -= 512;
			if (y >= BITMAP_HEIGHT) y -= 512;

			drawgfx(bitmap, Machine->gfx[2],
					code, color + (rpunch_sprite_palette / 16), xflip, 0, x, y, 0, TRANSPARENCY_PEN, 15);
		}
	}
}


/*************************************
 *
 *		Main screen refresh
 *
 *************************************/

void rpunch_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int x, penbase;

	/* update background 0 */
	callback_gfxbank = 0;
	callback_videoram = (const UINT16 *)&videoram[0];
	callback_colorbase = (videoflags & 0x0010) >> 1;
	callback_imagebase = (videoflags & 0x0400) << 3;
	callback_imagemask = callback_imagebase ? 0x0fff : 0x1fff;
	tilemap_update(background[0]);

	/* update background 1 */
	callback_gfxbank = 1;
	callback_videoram = (const UINT16 *)&videoram[videoram_size / 2];
	callback_colorbase = (videoflags & 0x0020) >> 2;
	callback_imagebase = (videoflags & 0x0800) << 2;
	callback_imagemask = callback_imagebase ? 0x0fff : 0x1fff;
	tilemap_update(background[1]);

	/* update the palette usage */
	palette_init_used_colors();
	mark_sprite_palette();
	if (rpunch_bitmapram)
	{
		penbase = 512 + (videoflags & 15);
		for (x = 0; x < 15; x++)
			palette_used_colors[penbase + x] = PALETTE_COLOR_USED;
		palette_used_colors[penbase + 15] = PALETTE_COLOR_TRANSPARENT;
	}

	/* handle full refresh */
	if (palette_recalc() || full_refresh)
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	/* update the tilemaps */
	tilemap_render(ALL_TILEMAPS);

	/* build the result */
	tilemap_draw(bitmap, background[0], 0);

	/* if we have a bitmap layer, it goes on top */
	if (rpunch_bitmapram)
	{
		tilemap_draw(bitmap, background[1], 0);
		draw_sprites(bitmap);
		if (bitmap->depth == 8)
			draw_bitmap_8(bitmap);
		else
			draw_bitmap_16(bitmap);
	}

	/* otherwise, the second background layer gets top billing */
	else
	{
		draw_sprites(bitmap);
		tilemap_draw(bitmap, background[1], 0);
	}
}



/*************************************
 *
 *		Depth-specific refresh
 *
 *************************************/

#define ADJUST_FOR_ORIENTATION(orientation, bitmap, dst, x, y, xadv)	\
	if (orientation)													\
	{																	\
		int dy = bitmap->line[1] - bitmap->line[0];						\
		int tx = x, ty = y, temp;										\
		if (orientation & ORIENTATION_SWAP_XY)							\
		{																\
			temp = tx; tx = ty; ty = temp;								\
			xadv = dy / (bitmap->depth / 8);							\
		}																\
		if (orientation & ORIENTATION_FLIP_X)							\
		{																\
			tx = bitmap->width - 1 - tx;								\
			if (!(orientation & ORIENTATION_SWAP_XY)) xadv = -xadv;		\
		}																\
		if (orientation & ORIENTATION_FLIP_Y)							\
		{																\
			ty = bitmap->height - 1 - ty;								\
			if ((orientation & ORIENTATION_SWAP_XY)) xadv = -xadv;		\
		}																\
		/* can't lookup line because it may be negative! */				\
		dst = (TYPE *)(bitmap->line[0] + dy * ty) + tx;					\
	}

#define INCLUDE_DRAW_CORE

#define DRAW_FUNC draw_bitmap_8
#define TYPE UINT8
#include "rpunch.cpp"
#undef TYPE
#undef DRAW_FUNC

#define DRAW_FUNC draw_bitmap_16
#define TYPE UINT16
#include "rpunch.cpp"
#undef TYPE
#undef DRAW_FUNC


#else


/*************************************
 *
 *		Core refresh routine
 *
 *************************************/

void DRAW_FUNC(struct osd_bitmap *bitmap)
{
	UINT16 *pens = &Machine->pens[512 + (videoflags & 15) * 16];
	int orientation = Machine->orientation;
	int x, y;

	/* draw any non-transparent scanlines from the VRAM directly */
	for (y = 0; y < BITMAP_HEIGHT; y++)
		if (rpunch_bitmapsum[y] != (BITMAP_WIDTH/4) * 0xffff)
		{
			UINT16 *src = (UINT16 *)&rpunch_bitmapram[y * 256 + BITMAP_XOFFSET/2];
			TYPE *dst = (TYPE *)bitmap->line[y];
			int xadv = 1;

			/* adjust in case we're oddly oriented */
			ADJUST_FOR_ORIENTATION(orientation, bitmap, dst, 0, y, xadv);

			/* redraw the scanline */
			for (x = 0; x < BITMAP_WIDTH/4; x++)
			{
				int data = *src++;
				if (data != 0xffff)
				{
					if ((data & 0xf000) != 0xf000)
						*dst = pens[data >> 12];
					dst += xadv;
					if ((data & 0x0f00) != 0x0f00)
						*dst = pens[(data >> 8) & 15];
					dst += xadv;
					if ((data & 0x00f0) != 0x00f0)
						*dst = pens[(data >> 4) & 15];
					dst += xadv;
					if ((data & 0x000f) != 0x000f)
						*dst = pens[data & 15];
					dst += xadv;
				}
				else
					dst += 4 * xadv;
			}
		}
}

#endif
