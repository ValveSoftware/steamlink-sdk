/***************************************************************************

  vidhrdw/balsente.c

  Functions to emulate the video hardware of the machine.

****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

#ifndef INCLUDE_DRAW_CORE

/*************************************
 *
 *	External globals
 *
 *************************************/

extern UINT8 balsente_shooter;
extern UINT8 balsente_shooter_x;
extern UINT8 balsente_shooter_y;



/*************************************
 *
 *	Statics
 *
 *************************************/

static UINT8 *local_videoram;
static UINT8 *scanline_dirty;
static UINT8 *scanline_palette;

static UINT8 last_scanline_palette;
static UINT8 screen_refresh_counter;
static UINT8 palettebank_vis;



/*************************************
 *
 *	Prototypes
 *
 *************************************/

void balsente_vh_stop(void);

static void update_screen_8(struct osd_bitmap *bitmap, int full_refresh);
static void update_screen_16(struct osd_bitmap *bitmap, int full_refresh);


/*************************************
 *
 *	Video system start
 *
 *************************************/

int balsente_vh_start(void)
{
	/* reset the system */
	palettebank_vis = 0;

	/* allocate a local copy of video RAM */
	local_videoram = (UINT8*)malloc(256 * 256);
	if (!local_videoram)
	{
		balsente_vh_stop();
		return 1;
	}

	/* allocate a scanline dirty array */
	scanline_dirty = (UINT8*)malloc(256);
	if (!scanline_dirty)
	{
		balsente_vh_stop();
		return 1;
	}

	/* allocate a scanline palette array */
	scanline_palette = (UINT8*)malloc(256);
	if (!scanline_palette)
	{
		balsente_vh_stop();
		return 1;
	}

	/* mark everything dirty to start */
	memset(scanline_dirty, 1, 256);

	/* reset the scanline palette */
	memset(scanline_palette, 0, 256);
	last_scanline_palette = 0;

	return 0;
}



/*************************************
 *
 *	Video system shutdown
 *
 *************************************/

void balsente_vh_stop(void)
{
	/* free the local video RAM array */
	if (local_videoram)
		free(local_videoram);
	local_videoram = NULL;

	/* free the scanline dirty array */
	if (scanline_dirty)
		free(scanline_dirty);
	scanline_dirty = NULL;

	/* free the scanline dirty array */
	if (scanline_palette)
		free(scanline_palette);
	scanline_palette = NULL;
}



/*************************************
 *
 *	Video RAM write
 *
 *************************************/

WRITE_HANDLER( balsente_videoram_w )
{
	videoram[offset] = data;

	/* expand the two pixel values into two bytes */
	local_videoram[offset * 2 + 0] = data >> 4;
	local_videoram[offset * 2 + 1] = data & 15;

	/* mark the scanline dirty */
	scanline_dirty[offset / 128] = 1;
}



/*************************************
 *
 *	Palette banking
 *
 *************************************/

static void update_palette(void)
{
	int scanline = cpu_getscanline(), i;
	if (scanline > 255) scanline = 0;

	/* special case: the scanline is the same as last time, but a screen refresh has occurred */
	if (scanline == last_scanline_palette && screen_refresh_counter)
	{
		for (i = 0; i < 256; i++)
		{
			/* mark the scanline dirty if it was a different palette */
			if (scanline_palette[i] != palettebank_vis)
				scanline_dirty[i] = 1;
			scanline_palette[i] = palettebank_vis;
		}
	}

	/* fill in the scanlines up till now */
	else
	{
		for (i = last_scanline_palette; i != scanline; i = (i + 1) & 255)
		{
			/* mark the scanline dirty if it was a different palette */
			if (scanline_palette[i] != palettebank_vis)
				scanline_dirty[i] = 1;
			scanline_palette[i] = palettebank_vis;
		}

		/* remember where we left off */
		last_scanline_palette = scanline;
	}

	/* reset the screen refresh counter */
	screen_refresh_counter = 0;
}


WRITE_HANDLER( balsente_palette_select_w )
{
	/* only update if changed */
	if (palettebank_vis != (data & 3))
	{
		/* update the scanline palette */
		update_palette();
		palettebank_vis = data & 3;
	}

	//logerror("balsente_palette_select_w(%d) scanline=%d\n", data & 3, cpu_getscanline());
}



/*************************************
 *
 *	Palette RAM write
 *
 *************************************/

WRITE_HANDLER( balsente_paletteram_w )
{
	int r, g, b;

	paletteram[offset] = data & 0x0f;

	r = paletteram[(offset & ~3) + 0];
	g = paletteram[(offset & ~3) + 1];
	b = paletteram[(offset & ~3) + 2];
	palette_change_color(offset / 4, (r << 4) | r, (g << 4) | g, (b << 4) | b);
}



/*************************************
 *
 *		Main screen refresh
 *
 *************************************/

void balsente_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	UINT8 palette_used[4];
	int x, y, i;

	/* update the remaining scanlines */
	screen_refresh_counter++;
	update_palette();

	/* determine which palette banks were used */
	palette_used[0] = palette_used[1] = palette_used[2] = palette_used[3] = 0;
	for (i = 0; i < 240; i++)
		palette_used[scanline_palette[i]] = 1;

	/* make sure color 1024 is white for our crosshair */
	palette_change_color(1024, 0xff, 0xff, 0xff);

	/* set the used status of all the palette entries */
	for (x = 0; x < 4; x++)
		if (palette_used[x])
			memset(&palette_used_colors[x * 256], PALETTE_COLOR_USED, 256);
		else
			memset(&palette_used_colors[x * 256], PALETTE_COLOR_UNUSED, 256);
	palette_used_colors[1024] = balsente_shooter ? PALETTE_COLOR_USED : PALETTE_COLOR_UNUSED;

	/* recompute the palette, and mark all scanlines dirty if we need to redraw */
	if (palette_recalc())
		memset(scanline_dirty, 1, 256);

	/* do the core redraw */
	if (bitmap->depth == 8)
		update_screen_8(bitmap, full_refresh);
	else
		update_screen_16(bitmap, full_refresh);

	/* draw a crosshair */
	if (balsente_shooter)
	{
		int beamx = balsente_shooter_x;
		int beamy = balsente_shooter_y - 12;

		int xoffs = beamx - 3;
		int yoffs = beamy - 3;

		for (y = -3; y <= 3; y++, yoffs++, xoffs++)
		{
			if (yoffs >= 0 && yoffs < 240 && beamx >= 0 && beamx < 256)
			{
				plot_pixel(bitmap, beamx, yoffs, Machine->pens[1024]);
				scanline_dirty[yoffs] = 1;
			}
			if (xoffs >= 0 && xoffs < 256 && beamy >= 0 && beamy < 240)
				plot_pixel(bitmap, xoffs, beamy, Machine->pens[1024]);
		}
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

#define DRAW_FUNC update_screen_8
#define TYPE UINT8
#include "balsente.cpp"
#undef TYPE
#undef DRAW_FUNC

#define DRAW_FUNC update_screen_16
#define TYPE UINT16
#include "balsente.cpp"
#undef TYPE
#undef DRAW_FUNC


#else


/*************************************
 *
 *		Core refresh routine
 *
 *************************************/

void DRAW_FUNC(struct osd_bitmap *bitmap, int full_refresh)
{
	int orientation = Machine->orientation;
	int x, y, i;

	/* draw any dirty scanlines from the VRAM directly */
	for (y = 0; y < 240; y++)
	{
		UINT16 *pens = &Machine->pens[scanline_palette[y] * 256];
		if (scanline_dirty[y] || full_refresh)
		{
			UINT8 *src = &local_videoram[y * 256];
			TYPE *dst = (TYPE *)bitmap->line[y];
			int xadv = 1;

			/* adjust in case we're oddly oriented */
			ADJUST_FOR_ORIENTATION(orientation, bitmap, dst, 0, y, xadv);

			/* redraw the scanline */
			for (x = 0; x < 256; x++, dst += xadv)
				*dst = pens[*src++];
			scanline_dirty[y] = 0;
		}
	}

	/* draw the sprite images */
	for (i = 0; i < 40; i++)
	{
		UINT8 *sprite = spriteram + ((0xe0 + i * 4) & 0xff);
		UINT8 *src;
		int flags = sprite[0];
		int image = sprite[1] | ((flags & 3) << 8);
		int ypos = sprite[2] + 17;
		int xpos = sprite[3];

		/* get a pointer to the source image */
		src = &memory_region(REGION_GFX1)[64 * image];
		if (flags & 0x80) src += 4 * 15;

		/* loop over y */
		for (y = 0; y < 16; y++, ypos = (ypos + 1) & 255)
		{
			if (ypos >= 16 && ypos < 240)
			{
				UINT16 *pens = &Machine->pens[scanline_palette[y] * 256];
				UINT8 *old = &local_videoram[ypos * 256 + xpos];
				TYPE *dst = &((TYPE *)bitmap->line[ypos])[xpos];
				int currx = xpos, xadv = 1;

				/* adjust in case we're oddly oriented */
				ADJUST_FOR_ORIENTATION(orientation, bitmap, dst, xpos, ypos, xadv);

				/* mark this scanline dirty */
				scanline_dirty[ypos] = 1;

				/* standard case */
				if (!(flags & 0x40))
				{
					/* loop over x */
					for (x = 0; x < 4; x++, dst += xadv * 2, old += 2)
					{
						int ipixel = *src++;
						int left = ipixel & 0xf0;
						int right = (ipixel << 4) & 0xf0;
						int pen;

						/* left pixel */
						if (left && currx >= 0 && currx < 256)
						{
							/* combine with the background */
							pen = left | old[0];
							dst[0] = pens[pen];
						}
						currx++;

						/* right pixel */
						if (right && currx >= 0 && currx < 256)
						{
							/* combine with the background */
							pen = right | old[1];
							dst[xadv] = pens[pen];
						}
						currx++;
					}
				}

				/* hflip case */
				else
				{
					src += 4;

					/* loop over x */
					for (x = 0; x < 4; x++, dst += xadv * 2, old += 2)
					{
						int ipixel = *--src;
						int left = (ipixel << 4) & 0xf0;
						int right = ipixel & 0xf0;
						int pen;

						/* left pixel */
						if (left && currx >= 0 && currx < 256)
						{
							/* combine with the background */
							pen = left | old[0];
							dst[0] = pens[pen];
						}
						currx++;

						/* right pixel */
						if (right && currx >= 0 && currx < 256)
						{
							/* combine with the background */
							pen = right | old[1];
							dst[xadv] = pens[pen];
						}
						currx++;
					}
					src += 4;
				}
			}
			else
				src += 4;
			if (flags & 0x80) src -= 2 * 4;
		}
	}
}

#endif
