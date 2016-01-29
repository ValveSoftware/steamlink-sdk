/***************************************************************************

  vidhrdw/mcr12.c

  Functions to emulate the video hardware of the MCR I and MCR II machines.

  Journey is an MCR/II game with a MCR/III sprite board so it has it's own
  routines.

***************************************************************************/

#include "driver.h"
#include "machine/mcr.h"
#include "vidhrdw/generic.h"


#ifndef INCLUDE_DRAW_CORE

static UINT8 last_cocktail_flip;
static UINT8 *spritebitmap;
static UINT32 spritebitmap_width;
static UINT32 spritebitmap_height;

static void render_one_sprite(int code, int sx, int sy, int hflip, int vflip);
static void render_sprite_tile_8(struct osd_bitmap *bitmap, const UINT16 *pens, int sx, int sy);
static void render_sprite_tile_16(struct osd_bitmap *bitmap, const UINT16 *pens, int sx, int sy);

INT8 mcr12_sprite_xoffs;
INT8 mcr12_sprite_xoffs_flip;

static UINT8 xtiles, ytiles;


/*************************************
 *
 *	Common video startup/shutdown
 *
 *************************************/

int mcr12_vh_start(void)
{
	const struct GfxElement *gfx = Machine->gfx[1];

	/* allocate a temporary bitmap for the sprite rendering */
	spritebitmap_width = Machine->drv->screen_width + 2 * 32;
	spritebitmap_height = Machine->drv->screen_height + 2 * 32;
	spritebitmap = (UINT8*)malloc(spritebitmap_width * spritebitmap_height);
	if (!spritebitmap)
		return 1;
	memset(spritebitmap, 0, spritebitmap_width * spritebitmap_height);

	/* if we're swapped in X/Y, the sprite data will be swapped */
	/* but that's not what we want, so we swap it back here */
	if (gfx && (Machine->orientation & ORIENTATION_SWAP_XY))
	{
		UINT8 *base = gfx->gfxdata;
		int c, x, y;
		for (c = 0; c < gfx->total_elements; c++)
		{
			for (y = 0; y < gfx->height; y++)
				for (x = y; x < gfx->width; x++)
				{
					int temp = base[y * gfx->line_modulo + x];
					base[y * gfx->line_modulo + x] = base[x * gfx->line_modulo + y];
					base[x * gfx->line_modulo + y] = temp;
				}
			base += gfx->char_modulo;
		}
	}

	/* compute tile counts */
	xtiles = Machine->drv->screen_width / 16;
	ytiles = Machine->drv->screen_height / 16;
	last_cocktail_flip = 0;

	/* start up the generic system */
	if (generic_vh_start())
	{
		free(spritebitmap);
		spritebitmap = NULL;
		return 1;
	}
	return 0;
}


void mcr12_vh_stop(void)
{
	generic_vh_stop();

	if (spritebitmap)
		free(spritebitmap);
	spritebitmap = NULL;
}



/*************************************
 *
 *	MCR2 palette writes
 *
 *************************************/

WRITE_HANDLER( mcr2_paletteram_w )
{
	int r, g, b;

	paletteram[offset] = data;

	/* bit 2 of the red component is taken from bit 0 of the address */
	r = ((offset & 1) << 2) + (data >> 6);
	g = (data >> 0) & 7;
	b = (data >> 3) & 7;

	/* up to 8 bits */
	r = (r << 5) | (r << 2) | (r >> 1);
	g = (g << 5) | (g << 2) | (g >> 1);
	b = (b << 5) | (b << 2) | (b >> 1);

	palette_change_color(offset / 2, r, g, b);
}



/*************************************
 *
 *	Videoram writes
 *
 *************************************/

WRITE_HANDLER( mcr1_videoram_w )
{
	if (videoram[offset] != data)
	{
		dirtybuffer[offset] = 1;
		videoram[offset] = data;
	}
}


WRITE_HANDLER( mcr2_videoram_w )
{
	if (videoram[offset] != data)
	{
		dirtybuffer[offset & ~1] = 1;
		videoram[offset] = data;
	}
}



/*************************************
 *
 *	Background updates
 *
 *************************************/

static void mcr1_update_background(struct osd_bitmap *bitmap)
{
	int offs;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1; offs >= 0; offs--)
	{
		int dirty = dirtybuffer[offs];
		if (dirty)
		{
			int mx = offs % 32;
			int my = offs / 32;
			int sx = 16 * mx;
			int sy = 16 * my;

			int code = videoram[offs];

			/* adjust for cocktail mode */
			if (mcr_cocktail_flip)
			{
				sx = (xtiles - 1) * 16 - sx;
				sy = (ytiles - 1) * 16 - sy;
			}

			/* draw the tile */
			drawgfx(bitmap, Machine->gfx[0], code, 0, mcr_cocktail_flip, mcr_cocktail_flip,
					sx, sy, &Machine->visible_area, TRANSPARENCY_NONE, 0);

			/* if there's live sprite data here, draw the sprite data */
			if (dirty & 2)
			{
				/* draw the sprite */
				if (bitmap->depth == 8)
					render_sprite_tile_8(bitmap, &Machine->pens[16], sx, sy);
				else
					render_sprite_tile_16(bitmap, &Machine->pens[16], sx, sy);
			}

			/* shift off the low bit of the dirty buffer */
			dirtybuffer[offs] = dirty >> 1;
		}
	}
}


static void mcr2_update_background(struct osd_bitmap *bitmap, int check_sprites)
{
	int offs;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 2; offs >= 0; offs -= 2)
	{
		int dirty = dirtybuffer[offs];
		if (dirty)
		{
			int mx = (offs / 2) % 32;
			int my = (offs / 2) / 32;
			int sx = 16 * mx;
			int sy = 16 * my;

			int attr = videoram[offs + 1];
			int code = videoram[offs] + 256 * (attr & 0x01);
			int hflip = attr & 0x02;
			int vflip = attr & 0x04;
			int color = (attr & 0x18) >> 3;

			/* adjust for cocktail mode */
			if (mcr_cocktail_flip)
			{
				sx = (xtiles - 1) * 16 - sx;
				sy = (ytiles - 1) * 16 - sy;
				hflip = !hflip;
				vflip = !vflip;
			}

			/* draw the tile */
			drawgfx(bitmap, Machine->gfx[0], code, color, hflip, vflip,
					sx, sy, &Machine->visible_area, TRANSPARENCY_NONE, 0);

			/* if there's live sprite data here, draw the sprite data */
			if (check_sprites && (dirty & 2))
			{
				color = (attr & 0xc0) >> 6;

				/* draw the sprite */
				if (bitmap->depth == 8)
					render_sprite_tile_8(bitmap, &Machine->pens[color * 16], sx, sy);
				else
					render_sprite_tile_16(bitmap, &Machine->pens[color * 16], sx, sy);
			}

			/* shift off the low bit of the dirty buffer */
			dirtybuffer[offs] = dirty >> 1;
		}
	}
}



/*************************************
 *
 *	Common sprite update
 *
 *************************************/

static void mcr12_update_sprites(int scale)
{
	int offs;

	/* render the sprites into the bitmap, ORing together */
	for (offs = 0; offs < spriteram_size; offs += 4)
	{
		int code, x, y, sx, sy, xcount, ycount, xtile, ytile, hflip, vflip;

		/* skip if zero */
		if (spriteram[offs] == 0)
			continue;

		/* extract the bits of information */
		code = spriteram[offs + 1] & 0x3f;
		hflip = spriteram[offs + 1] & 0x40;
		vflip = spriteram[offs + 1] & 0x80;
		x = (spriteram[offs + 2] - 4) * 2;
		y = (240 - spriteram[offs]) * 2;

		/* apply cocktail mode */
		if (mcr_cocktail_flip)
		{
			hflip = !hflip;
			vflip = !vflip;
			x = 466 - x + mcr12_sprite_xoffs_flip;
			y = 450 - y;
		}
		else
			x += mcr12_sprite_xoffs;

		/* wrap and clip */
		if (x > Machine->visible_area.max_x)
			x -= 512;
		if (y > Machine->visible_area.max_y)
			y -= 512;
		if (x <= -32 || y <= -32)
			continue;

		/* draw the sprite into the sprite bitmap */
		render_one_sprite(code, x + 32, y + 32, hflip, vflip);

		/* determine which tiles we will overdraw with this sprite */
		sx = x / 16;
		sy = y / 16;
		xcount = (x & 15) ? 3 : 2;
		ycount = (y & 15) ? 3 : 2;

		/* loop over dirty tiles and set the sprite bit */
		for (ytile = sy; ytile < sy + ycount; ytile++)
			for (xtile = sx; xtile < sx + xcount; xtile++)
				if (xtile >= 0 && xtile < xtiles && ytile >= 0 && ytile < ytiles)
				{
					int off;
					if (!mcr_cocktail_flip)
						off = 32 * ytile + xtile;
					else
						off = 32 * (ytiles - 1 - ytile) + (xtiles - 1 - xtile);
					dirtybuffer[off << scale] |= 2;
				}
	}
}



/*************************************
 *
 *	Main refresh routines
 *
 *************************************/

void mcr1_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	/* mark everything dirty on a full refresh or cocktail flip change */
	if (palette_recalc() || full_refresh || last_cocktail_flip != mcr_cocktail_flip)
		memset(dirtybuffer, 1, videoram_size);
	last_cocktail_flip = mcr_cocktail_flip;

	/* update the sprites */
	mcr12_update_sprites(0);

	/* redraw everything, merging the bitmaps */
	mcr1_update_background(bitmap);
}


void mcr2_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	/* mark everything dirty on a full refresh or cocktail flip change */
	if (palette_recalc() || full_refresh || last_cocktail_flip != mcr_cocktail_flip)
		memset(dirtybuffer, 1, videoram_size);
	last_cocktail_flip = mcr_cocktail_flip;

	/* update the sprites */
	mcr12_update_sprites(1);

	/* redraw everything, merging the bitmaps */
	mcr2_update_background(bitmap, 1);
}



/*************************************
 *
 *	Journey-specific MCR2 redraw
 *
 *	Uses the MCR3 sprite drawing
 *
 *************************************/

extern void mcr3_update_sprites(struct osd_bitmap *bitmap, int color_mask, int code_xor, int dx, int dy);

void journey_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	/* mark everything dirty on a cocktail flip change */
	if (palette_recalc() || last_cocktail_flip != mcr_cocktail_flip)
		memset(dirtybuffer, 1, videoram_size);
	last_cocktail_flip = mcr_cocktail_flip;

	/* redraw the background */
	mcr2_update_background(tmpbitmap, 0);

	/* copy it to the destination */
	copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);

	/* draw the sprites */
	mcr3_update_sprites(bitmap, 0x03, 0, 0, 0);
}



/*************************************
 *
 *		Sprite drawing
 *
 *************************************/

static void render_one_sprite(int code, int sx, int sy, int hflip, int vflip)
{
	const struct GfxElement *gfx = Machine->gfx[1];
	UINT8 *src = gfx->gfxdata + gfx->char_modulo * code;
	int y, x;

	/* adjust for vflip */
	if (vflip)
		src += 31 * gfx->line_modulo;

	/* loop over lines in the sprite */
	for (y = 0; y < 32; y++, sy++)
	{
		UINT8 *dst = spritebitmap + spritebitmap_width * sy + sx;

		/* redraw the line */
		if (!hflip)
		{
			for (x = 0; x < 32; x++)
				*dst++ |= *src++;
		}
		else
		{
			src += 32;
			for (x = 0; x < 32; x++)
				*dst++ |= *--src;
			src += 32;
		}

		/* adjust for vflip */
		if (vflip)
			src -= 2 * gfx->line_modulo;
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

#define DRAW_FUNC render_sprite_tile_8
#define TYPE UINT8
#include "mcr12.cpp"
#undef TYPE
#undef DRAW_FUNC

#define DRAW_FUNC render_sprite_tile_16
#define TYPE UINT16
#include "mcr12.cpp"
#undef TYPE
#undef DRAW_FUNC


#else


/*************************************
 *
 *		Core refresh routine
 *
 *************************************/

void DRAW_FUNC(struct osd_bitmap *bitmap, const UINT16 *pens, int sx, int sy)
{
	int orientation = Machine->orientation;
	int x, y;

	/* draw any dirty scanlines from the VRAM directly */
	for (y = 0; y < 16; y++, sy++)
	{
		UINT8 *src = &spritebitmap[(sy + 32) * spritebitmap_width + (sx + 32)];
		TYPE *dst = &((TYPE *)bitmap->line[sy])[sx];
		int xadv = 1;

		/* adjust in case we're oddly oriented */
		ADJUST_FOR_ORIENTATION(orientation, bitmap, dst, sx, sy, xadv);

		/* redraw the sprite scanline, erasing as we go */
		for (x = 0; x < 16; x++, dst += xadv)
		{
			int pixel = *src;
			if (pixel & 7)
				*dst = pens[pixel];
			*src++ = 0;
		}
	}
}

#endif
