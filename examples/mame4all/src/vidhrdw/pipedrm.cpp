/***************************************************************************

  vidhrdw/pipedrm.c

  Functions to emulate the video hardware of the machine.

****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


/*************************************
 *
 *	Statics
 *
 *************************************/

extern UINT8 pipedrm_video_control;

static struct osd_bitmap *background[2];
static UINT8 *background_dirty[2];
static UINT8 *local_videoram[2];

static UINT8 scroll_regs[4];



/*************************************
 *
 *	Prototypes
 *
 *************************************/

void pipedrm_vh_stop(void);



/*************************************
 *
 *	Video system start
 *
 *************************************/

int pipedrm_vh_start(void)
{
	/* allocate videoram */
	local_videoram[0] = (UINT8*)malloc(videoram_size);
	local_videoram[1] = (UINT8*)malloc(videoram_size);

	/* allocate background bitmaps */
	background[0] = bitmap_alloc(64*8, 64*4);
	background[1] = bitmap_alloc(64*8, 64*4);

	/* allocate dirty buffers */
	background_dirty[0] = (UINT8*)malloc(64*64);
	background_dirty[1] = (UINT8*)malloc(64*64);

	/* handle errors */
	if (!local_videoram[0] || !local_videoram[1] || !background[0] || !background[1] || !background_dirty[0] || !background_dirty[1])
	{
		pipedrm_vh_stop();
		return 1;
	}

	/* reset the system */
	memset(background_dirty[0], 1, 64*64);
	memset(background_dirty[1], 1, 64*64);
	return 0;
}



/*************************************
 *
 *	Video system shutdown
 *
 *************************************/

void pipedrm_vh_stop(void)
{
	/* free the dirty buffers */
	if (background_dirty[1])
		free(background_dirty[1]);
	background_dirty[1] = NULL;

	if (background_dirty[0])
		free(background_dirty[0]);
	background_dirty[0] = NULL;

	/* free the background bitmaps */
	if (background[1])
		bitmap_free(background[1]);
	background[1] = NULL;

	if (background[0])
		bitmap_free(background[0]);
	background[0] = NULL;

	/* free videoram */
	if (local_videoram[1])
		free(local_videoram[1]);
	local_videoram[1] = NULL;

	if (local_videoram[0])
		free(local_videoram[0]);
	local_videoram[0] = NULL;
}



/*************************************
 *
 *		Write handlers
 *
 *************************************/

WRITE_HANDLER( pipedrm_videoram_w )
{
	int which = (pipedrm_video_control >> 3) & 1;
	if (local_videoram[which][offset] != data)
	{
		local_videoram[which][offset] = data;
		background_dirty[which][offset & 0xfff] = 1;
	}
}


WRITE_HANDLER( hatris_videoram_w )
{
	int which = (~pipedrm_video_control >> 1) & 1;
	if (local_videoram[which][offset] != data)
	{
		local_videoram[which][offset] = data;
		background_dirty[which][offset & 0xfff] = 1;
	}
}


WRITE_HANDLER( pipedrm_scroll_regs_w )
{
	scroll_regs[offset] = data;
}


READ_HANDLER( pipedrm_videoram_r )
{
	int which = (pipedrm_video_control >> 3) & 1;
	return local_videoram[which][offset];
}


READ_HANDLER( hatris_videoram_r )
{
	int which = (~pipedrm_video_control >> 1) & 1;
	return local_videoram[which][offset];
}



/*************************************
 *
 *		Palette marking
 *
 *************************************/

static void mark_background_colors(void)
{
	int mask1 = Machine->gfx[0]->total_elements - 1;
	int mask2 = Machine->gfx[1]->total_elements - 1;
	int colormask = Machine->gfx[0]->total_colors - 1;
	UINT16 used_colors[128];
	int code, color, offs;

	/* reset all color codes */
	memset(used_colors, 0, sizeof(used_colors));

	/* loop over tiles */
	for (offs = 0; offs < 64*64; offs++)
	{
		/* consider background 0 */
		color = local_videoram[0][offs] & colormask;
		code = (local_videoram[0][offs + 0x1000] << 8) | local_videoram[0][offs + 0x2000];
		used_colors[color] |= Machine->gfx[0]->pen_usage[code & mask1];

		/* consider background 1 */
		color = local_videoram[1][offs] & colormask;
		code = (local_videoram[1][offs + 0x1000] << 8) | local_videoram[1][offs + 0x2000];
		used_colors[color] |= Machine->gfx[1]->pen_usage[code & mask2] & 0x7fff;
	}

	/* fill in the final table */
	for (offs = 0; offs <= colormask; offs++)
	{
		UINT16 used = used_colors[offs];
		if (used)
		{
			for (color = 0; color < 15; color++)
				if (used & (1 << color))
					palette_used_colors[offs * 16 + color] = PALETTE_COLOR_USED;
			if (used & 0x8000)
				palette_used_colors[offs * 16 + 15] = PALETTE_COLOR_USED;
			else
				palette_used_colors[offs * 16 + 15] = PALETTE_COLOR_TRANSPARENT;
		}
	}
}


static void mark_sprite_palette(void)
{
	UINT16 used_colors[0x20];
	int offs, i;

	/* clear the color array */
	memset(used_colors, 0, sizeof(used_colors));

	/* find the used sprites */
	for (offs = 0; offs < spriteram_size; offs += 8)
	{
		int data2 = spriteram[offs + 4] | (spriteram[offs + 5] << 8);

		/* turns out the sprites are the same as in aerofgt.c */
		if (data2 & 0x80)
		{
			int data3 = spriteram[offs + 6] | (spriteram[offs + 7] << 8);
			int code = data3 & 0xfff;
			int color = data2 & 0x0f;
			int xtiles = ((data2 >> 8) & 7) + 1;
			int ytiles = ((data2 >> 12) & 7) + 1;
			int tiles = xtiles * ytiles;
			int t;

			/* normal case */
			for (t = 0; t < tiles; t++)
				used_colors[color] |= Machine->gfx[2]->pen_usage[code++];
		}
	}

	/* now mark the pens */
	for (offs = 0; offs < 0x20; offs++)
	{
		UINT16 used = used_colors[offs];
		if (used)
		{
			for (i = 0; i < 15; i++)
				if (used & (1 << i))
					palette_used_colors[1024 + offs * 16 + i] = PALETTE_COLOR_USED;
			palette_used_colors[1024 + offs * 16 + 15] = PALETTE_COLOR_TRANSPARENT;
		}
	}
}



/*************************************
 *
 *		Sprite routines
 *
 *************************************/

static void draw_sprites(struct osd_bitmap *bitmap, int draw_priority)
{
	UINT8 zoomtable[16] = { 0,7,14,20,25,30,34,38,42,46,49,52,54,57,59,61 };
	int offs;

	/* draw the sprites */
	for (offs = 0; offs < spriteram_size; offs += 8)
	{
		int data2 = spriteram[offs + 4] | (spriteram[offs + 5] << 8);
		int priority = (data2 >> 4) & 1;

		/* turns out the sprites are the same as in aerofgt.c */
		if ((data2 & 0x80) && priority == draw_priority)
		{
			int data0 = spriteram[offs + 0] | (spriteram[offs + 1] << 8);
			int data1 = spriteram[offs + 2] | (spriteram[offs + 3] << 8);
			int data3 = spriteram[offs + 6] | (spriteram[offs + 7] << 8);
			int code = data3 & 0xfff;
			int color = data2 & 0x0f;
			int y = (data0 & 0x1ff) - 6;
			int x = (data1 & 0x1ff) - 13;
			int yzoom = (data0 >> 12) & 15;
			int xzoom = (data1 >> 12) & 15;
			int zoomed = (xzoom | yzoom);
			int ytiles = ((data2 >> 12) & 7) + 1;
			int xtiles = ((data2 >> 8) & 7) + 1;
			int yflip = (data2 >> 15) & 1;
			int xflip = (data2 >> 11) & 1;
			int xt, yt;

			/* compute the zoom factor -- stolen from aerofgt.c */
			xzoom = 16 - zoomtable[xzoom] / 8;
			yzoom = 16 - zoomtable[yzoom] / 8;

			/* wrap around */
			if (x > Machine->visible_area.max_x) x -= 0x200;
			if (y > Machine->visible_area.max_y) y -= 0x200;

			/* normal case */
			if (!xflip && !yflip)
			{
				for (yt = 0; yt < ytiles; yt++)
					for (xt = 0; xt < xtiles; xt++, code++)
						if (!zoomed)
							drawgfx(bitmap, Machine->gfx[2], code, color, 0, 0,
									x + xt * 16, y + yt * 16, 0, TRANSPARENCY_PEN, 15);
						else
							drawgfxzoom(bitmap, Machine->gfx[2], code, color, 0, 0,
									x + xt * xzoom, y + yt * yzoom, 0, TRANSPARENCY_PEN, 15,
									0x1000 * xzoom, 0x1000 * yzoom);
			}

			/* xflipped case */
			else if (xflip && !yflip)
			{
				for (yt = 0; yt < ytiles; yt++)
					for (xt = 0; xt < xtiles; xt++, code++)
						if (!zoomed)
							drawgfx(bitmap, Machine->gfx[2], code, color, 1, 0,
									x + (xtiles - 1 - xt) * 16, y + yt * 16, 0, TRANSPARENCY_PEN, 15);
						else
							drawgfxzoom(bitmap, Machine->gfx[2], code, color, 1, 0,
									x + (xtiles - 1 - xt) * xzoom, y + yt * yzoom, 0, TRANSPARENCY_PEN, 15,
									0x1000 * xzoom, 0x1000 * yzoom);
			}

			/* yflipped case */
			else if (!xflip && yflip)
			{
				for (yt = 0; yt < ytiles; yt++)
					for (xt = 0; xt < xtiles; xt++, code++)
						if (!zoomed)
							drawgfx(bitmap, Machine->gfx[2], code, color, 0, 1,
									x + xt * 16, y + (ytiles - 1 - yt) * 16, 0, TRANSPARENCY_PEN, 15);
						else
							drawgfxzoom(bitmap, Machine->gfx[2], code, color, 0, 1,
									x + xt * xzoom, y + (ytiles - 1 - yt) * yzoom, 0, TRANSPARENCY_PEN, 15,
									0x1000 * xzoom, 0x1000 * yzoom);
			}

			/* x & yflipped case */
			else
			{
				for (yt = 0; yt < ytiles; yt++)
					for (xt = 0; xt < xtiles; xt++, code++)
						if (!zoomed)
							drawgfx(bitmap, Machine->gfx[2], code, color, 1, 1,
									x + (xtiles - 1 - xt) * 16, y + (ytiles - 1 - yt) * 16, 0, TRANSPARENCY_PEN, 15);
						else
							drawgfxzoom(bitmap, Machine->gfx[2], code, color, 1, 1,
									x + (xtiles - 1 - xt) * xzoom, y + (ytiles - 1 - yt) * yzoom, 0, TRANSPARENCY_PEN, 15,
									0x1000 * xzoom, 0x1000 * yzoom);
			}
		}
	}
}


/*************************************
 *
 *		Main screen refresh
 *
 *************************************/

static void common_screenrefresh(int full_refresh)
{
	UINT16 saved_pens[64];
	int offs;

	/* update the palette usage */
	palette_init_used_colors();
	mark_background_colors();
	if (Machine->gfx[2])
		mark_sprite_palette();

	/* handle full refresh */
	if (palette_recalc() || full_refresh)
	{
		memset(background_dirty[0], 1, 64*64);
		memset(background_dirty[1], 1, 64*64);
	}

	/* update background 1 (opaque) */
	for (offs = 0; offs < 64*64; offs++)
		if (background_dirty[0][offs])
		{
			int color = local_videoram[0][offs];
			int code = (local_videoram[0][offs + 0x1000] << 8) | local_videoram[0][offs + 0x2000];
			int sx = offs % 64;
			int sy = offs / 64;
			drawgfx(background[0], Machine->gfx[0], code, color, 0, 0, sx * 8, sy * 4, 0, TRANSPARENCY_NONE, 0);
			background_dirty[0][offs] = 0;
		}

	/* mark the transparent pens transparent before drawing to background 2 */
	for (offs = 0; offs < 64; offs++)
	{
		saved_pens[offs] = Machine->gfx[0]->colortable[offs * 16 + 15];
		Machine->gfx[0]->colortable[offs * 16 + 15] = palette_transparent_pen;
	}

	/* update background 2 (transparent) */
	for (offs = 0; offs < 64*64; offs++)
		if (background_dirty[1][offs])
		{
			int color = local_videoram[1][offs];
			int code = (local_videoram[1][offs + 0x1000] << 8) | local_videoram[1][offs + 0x2000];
			int sx = offs % 64;
			int sy = offs / 64;
			drawgfx(background[1], Machine->gfx[1], code, color, 0, 0, sx * 8, sy * 4, 0, TRANSPARENCY_NONE, 0);
			background_dirty[1][offs] = 0;
		}

	/* restore the saved pens */
	for (offs = 0; offs < 64; offs++)
		Machine->gfx[0]->colortable[offs * 16 + 15] = saved_pens[offs];

}


void pipedrm_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int x, y;

	common_screenrefresh(full_refresh);

	/* draw the lower background */
	x = 0;
	y = -scroll_regs[1] - 7;
	copyscrollbitmap(bitmap, background[0], 1, &x, 1, &y, &Machine->visible_area, TRANSPARENCY_NONE, 0);

	/* draw the upper background */
	x = 0;
	y = -scroll_regs[3] - 7;
	copyscrollbitmap(bitmap, background[1], 1, &x, 1, &y, &Machine->visible_area, TRANSPARENCY_PEN, palette_transparent_pen);

	/* draw all the sprites (priority doesn't seem to matter) */
	if (Machine->gfx[2])
	{
		draw_sprites(bitmap, 0);
		draw_sprites(bitmap, 1);
	}
}


void hatris_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int x, y;

	common_screenrefresh(full_refresh);

	/* draw the lower background */
	x = 0;
	y = -scroll_regs[3] - 7;
	copyscrollbitmap(bitmap, background[0], 1, &x, 1, &y, &Machine->visible_area, TRANSPARENCY_NONE, 0);

	/* draw the upper background */
	x = 0;
	y = -scroll_regs[1] - 7;
	copyscrollbitmap(bitmap, background[1], 1, &x, 1, &y, &Machine->visible_area, TRANSPARENCY_PEN, palette_transparent_pen);
}
