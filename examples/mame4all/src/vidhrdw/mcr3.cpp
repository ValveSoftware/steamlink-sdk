/***************************************************************************

  vidhrdw/mcr3.c

	Functions to emulate the video hardware of an mcr3-style machine.

***************************************************************************/

#include "driver.h"
#include "artwork.h"
#include "machine/mcr.h"
#include "vidhrdw/generic.h"


#ifndef MIN
#define MIN(x,y) (x)<(y)?(x):(y)
#endif

/* These are used to align Discs of Tron with the backdrop */
#define DOTRON_X_START 144
#define DOTRON_Y_START 40
#define DOTRON_HORIZON 138



/*************************************
 *
 *	Global variables
 *
 *************************************/

/* Spy Hunter hardware extras */
UINT8 spyhunt_sprite_color_mask;
INT16 spyhunt_scrollx, spyhunt_scrolly;
INT16 spyhunt_scroll_offset;
UINT8 spyhunt_draw_lamps;
UINT8 spyhunt_lamp[8];

UINT8 *spyhunt_alpharam;
size_t spyhunt_alpharam_size;



/*************************************
 *
 *	Local variables
 *
 *************************************/

/* Spy Hunter-specific scrolling background */
static struct osd_bitmap *spyhunt_backbitmap;

/* Discs of Tron artwork globals */
static UINT8 dotron_palettes[3][3*256];
static UINT8 light_status;

static UINT8 last_cocktail_flip;



/*************************************
 *
 *	Palette RAM writes
 *
 *************************************/

WRITE_HANDLER( mcr3_paletteram_w )
{
	int r, g, b;

	paletteram[offset] = data;
	offset &= 0x7f;

	/* high bit of red comes from low bit of address */
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
 *	Video RAM writes
 *
 *************************************/

WRITE_HANDLER( mcr3_videoram_w )
{
	if (videoram[offset] != data)
	{
		dirtybuffer[offset & ~1] = 1;
		videoram[offset] = data;
	}
}



/*************************************
 *
 *	Background update
 *
 *************************************/

static void mcr3_update_background(struct osd_bitmap *bitmap, UINT8 color_xor)
{
	int offs;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 2; offs >= 0; offs -= 2)
	{
		if (dirtybuffer[offs])
		{
			int mx = (offs / 2) % 32;
			int my = (offs / 2) / 32;
			int attr = videoram[offs + 1];
			int color = ((attr & 0x30) >> 4) ^ color_xor;
			int code = videoram[offs] + 256 * (attr & 0x03);

			if (!mcr_cocktail_flip)
				drawgfx(bitmap, Machine->gfx[0], code, color, attr & 0x04, attr & 0x08,
						16 * mx, 16 * my, &Machine->visible_area, TRANSPARENCY_NONE, 0);
			else
				drawgfx(bitmap, Machine->gfx[0], code, color, !(attr & 0x04), !(attr & 0x08),
						16 * (31 - mx), 16 * (29 - my), &Machine->visible_area, TRANSPARENCY_NONE, 0);

			dirtybuffer[offs] = 0;
		}
	}
}



/*************************************
 *
 *	Sprite update
 *
 *************************************/

void mcr3_update_sprites(struct osd_bitmap *bitmap, int color_mask, int code_xor, int dx, int dy)
{
	int offs;

	/* loop over sprite RAM */
	for (offs = 0; offs < spriteram_size; offs += 4)
	{
		int code, color, flipx, flipy, sx, sy, flags;

		/* skip if zero */
		if (spriteram[offs] == 0)
			continue;

		/* extract the bits of information */
		flags = spriteram[offs + 1];
		code = spriteram[offs + 2] + 256 * ((flags >> 3) & 0x01);
		color = ~flags & color_mask;
		flipx = flags & 0x10;
		flipy = flags & 0x20;
		sx = (spriteram[offs + 3] - 3) * 2;
		sy = (241 - spriteram[offs]) * 2;

		code ^= code_xor;

		sx += dx;
		sy += dy;

		/* draw the sprite */
		if (!mcr_cocktail_flip)
			drawgfx(bitmap, Machine->gfx[1], code, color, flipx, flipy, sx, sy,
					&Machine->visible_area, TRANSPARENCY_PEN, 0);
		else
			drawgfx(bitmap, Machine->gfx[1], code, color, !flipx, !flipy, 480 - sx, 452 - sy,
					&Machine->visible_area, TRANSPARENCY_PEN, 0);

		/* sprites use color 0 for background pen and 8 for the 'under tile' pen.
			The color 8 is used to cover over other sprites. */
		if (Machine->gfx[1]->pen_usage[code] & 0x0100)
		{
			struct rectangle clip;

			clip.min_x = sx;
			clip.max_x = sx + 31;
			clip.min_y = sy;
			clip.max_y = sy + 31;

			copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &clip, TRANSPARENCY_THROUGH, Machine->pens[8 + color * 16]);
		}
	}
}



/*************************************
 *
 *	Generic MCR3 redraw
 *
 *************************************/

void mcr3_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	/* mark everything dirty on a cocktail flip change */
	if (palette_recalc() || last_cocktail_flip != mcr_cocktail_flip)
		memset(dirtybuffer, 1, videoram_size);
	last_cocktail_flip = mcr_cocktail_flip;

	/* redraw the background */
	mcr3_update_background(tmpbitmap, 0);

	/* copy it to the destination */
	copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);

	/* draw the sprites */
	mcr3_update_sprites(bitmap, 0x03, 0, 0, 0);
}



/*************************************
 *
 *	MCR monoboard-specific redraw
 *
 *************************************/

void mcrmono_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	if (palette_recalc())
		memset(dirtybuffer, 1, videoram_size);

	/* redraw the background */
	mcr3_update_background(tmpbitmap, 3);

	/* copy it to the destination */
	copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);

	/* draw the sprites */
	mcr3_update_sprites(bitmap, 0x03, 0, 0, 0);
}



/*************************************
 *
 *	Spy Hunter-specific color PROM decoder
 *
 *************************************/

void spyhunt_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable, const unsigned char *color_prom)
{
	/* add some colors for the alpha RAM */
	palette[(8*16)*3+0] = 0;
	palette[(8*16)*3+1] = 0;
	palette[(8*16)*3+2] = 0;
	palette[(8*16+1)*3+0] = 0;
	palette[(8*16+1)*3+1] = 255;
	palette[(8*16+1)*3+2] = 0;
	palette[(8*16+2)*3+0] = 0;
	palette[(8*16+2)*3+1] = 0;
	palette[(8*16+2)*3+2] = 255;
	palette[(8*16+3)*3+0] = 255;
	palette[(8*16+3)*3+1] = 255;
	palette[(8*16+3)*3+2] = 255;

	/* put them into the color table */
	colortable[8*16+0] = 8*16;
	colortable[8*16+1] = 8*16+1;
	colortable[8*16+2] = 8*16+2;
	colortable[8*16+3] = 8*16+3;
}



/*************************************
 *
 *	Spy Hunter-specific video startup
 *
 *************************************/

int spyhunt_vh_start(void)
{
	/* allocate our own dirty buffer */
	dirtybuffer = (unsigned char*)malloc(videoram_size);
	if (!dirtybuffer)
		return 1;
	memset(dirtybuffer, 1, videoram_size);

	/* allocate a bitmap for the background */
	spyhunt_backbitmap = bitmap_alloc(64*64, 32*32);
	if (!spyhunt_backbitmap)
	{
		free(dirtybuffer);
		return 1;
	}

	/* reset the scrolling */
	spyhunt_scrollx = spyhunt_scrolly = 0;

	return 0;
}



/*************************************
 *
 *	Spy Hunter-specific video shutdown
 *
 *************************************/

void spyhunt_vh_stop(void)
{
	/* free the buffers */
	bitmap_free(spyhunt_backbitmap);
	free(dirtybuffer);
}



/*************************************
 *
 *	Spy Hunter-specific redraw
 *
 *************************************/

void spyhunt_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	static const struct rectangle clip = { 0, 30*16-1, 0, 30*16-1 };
	int offs, scrollx, scrolly;

	if (palette_recalc())
		memset(dirtybuffer, 1, videoram_size);

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1; offs >= 0; offs--)
	{
		if (dirtybuffer[offs])
		{
			int code = videoram[offs];
			int vflip = code & 0x40;
			int mx = (offs >> 4) & 0x3f;
			int my = (offs & 0x0f) | ((offs >> 6) & 0x10);

			code = (code & 0x3f) | ((code & 0x80) >> 1);

			drawgfx(spyhunt_backbitmap, Machine->gfx[0], code, 0, 0, vflip,
					64 * mx, 32 * my, NULL, TRANSPARENCY_NONE, 0);

			dirtybuffer[offs] = 0;
		}
	}

	/* copy it to the destination */
	scrollx = -spyhunt_scrollx * 2 + spyhunt_scroll_offset;
	scrolly = -spyhunt_scrolly * 2;
	copyscrollbitmap(bitmap, spyhunt_backbitmap, 1, &scrollx, 1, &scrolly, &clip, TRANSPARENCY_NONE, 0);

	/* draw the sprites */
	mcr3_update_sprites(bitmap, spyhunt_sprite_color_mask, 0x80, -12, 0);

	/* render any characters on top */
	for (offs = spyhunt_alpharam_size - 1; offs >= 0; offs--)
	{
		int ch = spyhunt_alpharam[offs];
		if (ch)
		{
			int mx = offs / 32;
			int my = offs % 32;

			drawgfx(bitmap, Machine->gfx[2], ch, 0, 0, 0,
					16 * mx - 16, 16 * my, &clip, TRANSPARENCY_PEN, 0);
		}
	}

	/* lamp indicators */
	if (spyhunt_draw_lamps)
	{
		char buffer[32];

		sprintf(buffer, "%s  %s  %s  %s  %s",
				spyhunt_lamp[0] ? "OIL" : "   ",
				spyhunt_lamp[1] ? "MISSILE" : "       ",
				spyhunt_lamp[2] ? "VAN" : "   ",
				spyhunt_lamp[3] ? "SMOKE" : "     ",
				spyhunt_lamp[4] ? "GUNS" : "    ");
		for (offs = 0; offs < 30; offs++)
			drawgfx(bitmap, Machine->gfx[2], buffer[offs], 0, 0, 0,
					30 * 16, (29 - offs) * 16, &Machine->visible_area, TRANSPARENCY_NONE, 0);
	}
}



/*************************************
 *
 *	Discs of Tron-specific video startup
 *
 *************************************/

int dotron_vh_start(void)
{
	int i, x, y;

	/* do generic initialization to start */
	if (generic_vh_start())
		return 1;

	backdrop_load("dotron.png", 64, Machine->drv->total_colors-64);

	/* if we got it, compute palettes */
	if (artwork_backdrop)
	{
		/* from the horizon upwards, use the second palette */
		for (y = 0; y < DOTRON_HORIZON; y++)
			for (x = 0; x < artwork_backdrop->artwork->width; x++)
			{
				int newpixel = read_pixel(artwork_backdrop->orig_artwork, x, y) + 95;
				plot_pixel(artwork_backdrop->orig_artwork, x, y, newpixel);
			}

		backdrop_refresh(artwork_backdrop);

		/* create palettes with different levels of brightness */
		memcpy(dotron_palettes[0], artwork_backdrop->orig_palette, 3 * artwork_backdrop->num_pens_used);
		for (i = 0; i < artwork_backdrop->num_pens_used; i++)
		{
			/* only boost red and blue */
			dotron_palettes[1][i * 3 + 0] = MIN(artwork_backdrop->orig_palette[i * 3] * 2, 255);
			dotron_palettes[1][i * 3 + 1] = artwork_backdrop->orig_palette[i * 3 + 1];
			dotron_palettes[1][i * 3 + 2] = MIN(artwork_backdrop->orig_palette[i * 3 + 2] * 2, 255);
			dotron_palettes[2][i * 3 + 0] = MIN(artwork_backdrop->orig_palette[i * 3] * 3, 255);
			dotron_palettes[2][i * 3 + 1] = artwork_backdrop->orig_palette[i * 3 + 1];
			dotron_palettes[2][i * 3 + 2] = MIN(artwork_backdrop->orig_palette[i * 3 + 2] * 3, 255);
		}

		//logerror("Backdrop loaded.\n");
	}

	return 0;
}



/*************************************
 *
 *	Discs of Tron light management
 *
 *************************************/

void dotron_change_light(int light)
{
	light_status = light;
}


static void dotron_change_palette(int which)
{
	UINT8 *new_palette;
	int i, offset;

	/* get the palette indices */
	offset = artwork_backdrop->start_pen + 95;
	new_palette = dotron_palettes[which];

	/* update the palette entries */
	for (i = 0; i < artwork_backdrop->num_pens_used; i++)
		palette_change_color(i + offset, new_palette[i * 3], new_palette[i * 3 + 1], new_palette[i * 3 + 2]);
}



/*************************************
 *
 *	Discs of Tron-specific redraw
 *
 *************************************/

void dotron_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	struct rectangle sclip;
	int offs;

	/* handle background lights */
	if (artwork_backdrop != NULL)
	{
		int light = light_status & 1;
		if ((light_status & 2) && (cpu_getcurrentframe() & 1)) light++;	/* strobe */
		dotron_change_palette(light);
		/* This is necessary because Discs of Tron modifies the palette */
		if (backdrop_black_recalc())
			memset(dirtybuffer, 1, videoram_size);

	}

	if (full_refresh || palette_recalc())
	{
		if (artwork_backdrop)
		{
			backdrop_refresh(artwork_backdrop);
			copybitmap(tmpbitmap, artwork_backdrop->artwork, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);
			copybitmap(bitmap, artwork_backdrop->artwork, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);
			osd_mark_dirty(0,0,bitmap->width, bitmap->height, 0);
		}
		memset(dirtybuffer, 1 ,videoram_size);
	}

	/* Screen clip, because our backdrop is a different resolution than the game */
	sclip.min_x = DOTRON_X_START + 0;
	sclip.max_x = DOTRON_X_START + 32*16 - 1;
	sclip.min_y = DOTRON_Y_START + 0;
	sclip.max_y = DOTRON_Y_START + 30*16 - 1;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 2; offs >= 0; offs -= 2)
	{
		if (dirtybuffer[offs])
		{
			int attr = videoram[offs+1];
			int code = videoram[offs] + 256 * (attr & 0x03);
			int color = (attr & 0x30) >> 4;
			int mx = ((offs / 2) % 32) * 16;
			int my = ((offs / 2) / 32) * 16;

			/* center for the backdrop */
			mx += DOTRON_X_START;
			my += DOTRON_Y_START;

			drawgfx(tmpbitmap, Machine->gfx[0], code, color, attr & 0x04, attr & 0x08,
					mx, my, &sclip, TRANSPARENCY_NONE, 0);

			if (artwork_backdrop != NULL)
			{
				struct rectangle bclip;

				bclip.min_x = mx;
				bclip.max_x = mx + 16 - 1;
				bclip.min_y = my;
				bclip.max_y = my + 16 - 1;

				draw_backdrop(tmpbitmap, artwork_backdrop->artwork, 0, 0, &bclip);
			}

			dirtybuffer[offs] = 0;
		}
	}

	/* copy the resulting bitmap to the screen */
	copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, 0, TRANSPARENCY_NONE, 0);

	/* draw the sprites */
	mcr3_update_sprites(bitmap, 0x03, 0, DOTRON_X_START, DOTRON_Y_START);
}

