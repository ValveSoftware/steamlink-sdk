/***************************************************************************

  vidhrdw/mcr68.c

  Xenophobe video hardware very similar to Rampage.

  Colour 8 in sprites indicates transparency in closed area.
  Each tile has an attribute to indicate tile drawn on top of sprite.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


#define LOW_BYTE(x) (READ_WORD(x) & 0xff)


UINT8 mcr68_sprite_clip;
INT8 mcr68_sprite_xoffset;


#define DEBUG_VIDEO		0


#if DEBUG_VIDEO
static int show_bg_colors;
static int show_colors;

static void mcr68_debug();
static void zwackery_debug();
#endif



/*************************************
 *
 *	Palette RAM writes
 *
 *************************************/

WRITE_HANDLER( mcr68_paletteram_w )
{
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	int r, g, b;

	WRITE_WORD(&paletteram[offset], newword);

	r = (newword >> 6) & 7;
	b = (newword >> 3) & 7;
	g = (newword >> 0) & 7;

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

WRITE_HANDLER( mcr68_videoram_w )
{
	int oldword = READ_WORD(&videoram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		dirtybuffer[offset & ~3] = 1;
		WRITE_WORD(&videoram[offset], newword);
	}
}



/*************************************
 *
 *	Background update
 *
 *************************************/

static void mcr68_update_background(struct osd_bitmap *bitmap, int overrender)
{
	int offs;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 4; offs >= 0; offs -= 4)
	{
		/* this works for overrendering as well, since the sprite code will mark */
		/* intersecting tiles for us */
		if (dirtybuffer[offs])
		{
			int mx = (offs / 4) % 32;
			int my = (offs / 4) / 32;
			int attr = LOW_BYTE(&videoram[offs + 2]);
			int color = (attr & 0x30) >> 4;
			int code = LOW_BYTE(&videoram[offs]) + 256 * (attr & 0x03) + 1024 * ((attr >> 6) & 0x03);

			if (!overrender)
				drawgfx(bitmap, Machine->gfx[0], code, color ^ 3, attr & 0x04, attr & 0x08,
						16 * mx, 16 * my, &Machine->visible_area, TRANSPARENCY_NONE, 0);
			else if (Machine->gfx[0]->total_elements < 0x1000 && (attr & 0x80))
				drawgfx(bitmap, Machine->gfx[0], code, color ^ 3, attr & 0x04, attr & 0x08,
						16 * mx, 16 * my, &Machine->visible_area, TRANSPARENCY_PEN, 0);
			else
				continue;

			/* only clear the dirty flag if we're not overrendering */
			dirtybuffer[offs] = 0;
		}
	}
}



/*************************************
 *
 *	Sprite update
 *
 *************************************/

static void mcr68_update_sprites(struct osd_bitmap *bitmap, int priority)
{
	struct rectangle sprite_clip = Machine->visible_area;
	int offs;

	/* adjust for clipping */
	sprite_clip.min_x += mcr68_sprite_clip;
	sprite_clip.max_x -= mcr68_sprite_clip;

	/* loop over sprite RAM */
	for (offs = 0; offs < spriteram_size; offs += 8)
	{
		int code, color, flipx, flipy, x, y, sx, sy, xcount, ycount, flags;

		flags = LOW_BYTE(&spriteram[offs + 2]);
		code = LOW_BYTE(&spriteram[offs + 4]) + 256 * ((flags >> 3) & 0x01) + 512 * ((flags >> 6) & 0x03);

		/* skip if zero */
		if (code == 0)
			continue;

		/* also skip if this isn't the priority we're drawing right now */
		if (((flags >> 2) & 1) != priority)
			continue;

		/* extract the bits of information */
		color = ~flags & 0x03;
		flipx = flags & 0x10;
		flipy = flags & 0x20;
		x = LOW_BYTE(&spriteram[offs + 6]) * 2 + mcr68_sprite_xoffset;
		y = (241 - LOW_BYTE(&spriteram[offs])) * 2;

		/* allow sprites to clip off the left side */
		if (x > 0x1f0) x -= 0x200;

		/* draw the sprite */
		drawgfx(bitmap, Machine->gfx[1], code, color, flipx, flipy, x, y,
				&sprite_clip, TRANSPARENCY_PEN, 0);

		/* sprites use color 0 for background pen and 8 for the 'under tile' pen.
			The color 8 is used to cover over other sprites. */
		if (Machine->gfx[1]->pen_usage[code] & 0x0100)
		{
			struct rectangle clip;

			clip.min_x = x;
			clip.max_x = x + 31;
			clip.min_y = y;
			clip.max_y = y + 31;

			copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &clip, TRANSPARENCY_THROUGH, Machine->pens[8 + color * 16]);
		}

		/* mark tiles underneath as dirty for overrendering */
		if (priority == 0)
		{
			sx = x / 16;
			sy = y / 16;
			xcount = (x & 15) ? 3 : 2;
			ycount = (y & 15) ? 3 : 2;

			for (y = sy; y < sy + ycount; y++)
				for (x = sx; x < sx + xcount; x++)
					if (x >= 0 && x < 32 && y >= 0 && y < 30)
						dirtybuffer[(32 * y + x) * 4] = 1;
		}
	}
}



/*************************************
 *
 *	General MCR/68k update
 *
 *************************************/

void mcr68_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
#if DEBUG_VIDEO
	mcr68_debug();
#endif

	/* update palette */
	if (palette_recalc())
		memset(dirtybuffer, 1, videoram_size);

	/* draw the background */
	mcr68_update_background(tmpbitmap, 0);

	/* copy it to the destination */
	copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);

	/* draw the low-priority sprites */
	mcr68_update_sprites(bitmap, 0);

    /* redraw tiles with priority over sprites */
	mcr68_update_background(bitmap, 1);

	/* draw the high-priority sprites */
	mcr68_update_sprites(bitmap, 1);
}



/*************************************
 *
 *	General MCR/68k debug
 *
 *************************************/

#if DEBUG_VIDEO
void mcr68_debug(void)
{
	static FILE *f;
	if (keyboard_pressed(KEYCODE_9))
	{
		int offs;

		if (!f) f = fopen("mcr.log", "w");
		if (f)
		{
			fprintf(f, "\n\n=================================================================\n");
			for (offs = 0; offs < videoram_size; offs += 4)
			{
				fprintf(f, "%02X%02X ", LOW_BYTE(&videoram[offs + 2]), LOW_BYTE(&videoram[offs + 0]));
				if (offs % (32 * 4) == 31 * 4) fprintf(f, "\n");
			}
			fprintf(f, "\n\n");
			for (offs = 0; offs < spriteram_size; offs += 8)
				fprintf(f, "Sprite %03d: %02X %02X %02X %02X\n", offs / 8,
					LOW_BYTE(&spriteram[offs + 0]),
					LOW_BYTE(&spriteram[offs + 2]),
					LOW_BYTE(&spriteram[offs + 4]),
					LOW_BYTE(&spriteram[offs + 6]));
		}
		fflush(f);
	}
}
#endif



/*************************************
 *
 *	Zwackery palette RAM writes
 *
 *************************************/

WRITE_HANDLER( zwackery_paletteram_w )
{
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	int r, g, b;

	WRITE_WORD(&paletteram[offset], newword);

	r = (~newword >> 10) & 31;
	b = (~newword >> 5) & 31;
	g = (~newword >> 0) & 31;

	/* up to 8 bits */
	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	palette_change_color(offset / 2, r, g, b);
}



/*************************************
 *
 *	Zwackery video RAM writes
 *
 *************************************/

WRITE_HANDLER( zwackery_videoram_w )
{
	int oldword = READ_WORD(&videoram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		dirtybuffer[offset & ~1] = 1;
		WRITE_WORD(&videoram[offset], newword);
	}
}



/*************************************
 *
 *	Zwackery video RAM writes
 *
 *************************************/

WRITE_HANDLER( zwackery_spriteram_w )
{
	/* yech -- Zwackery relies on the upper 8 bits of a spriteram read being $ff! */
	/* to make this happen we always write $ff in the upper 8 bits */
	int oldword = READ_WORD(&spriteram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	WRITE_WORD(&spriteram[offset], newword | 0xff00);
}



/*************************************
 *
 *	Zwackery color data conversion
 *
 *************************************/

void zwackery_convert_color_prom(unsigned char *palette, unsigned short *colortable, const unsigned char *color_prom)
{
	const UINT8 *colordatabase = (const UINT8 *)memory_region(REGION_GFX3);
	struct GfxElement *gfx0 = Machine->gfx[0];
	struct GfxElement *gfx2 = Machine->gfx[2];
	int code, y, x, ix;

	/* "colorize" each code */
	for (code = 0; code < gfx0->total_elements; code++)
	{
		const UINT8 *coldata = colordatabase + code * 32;
		UINT8 *gfxdata0 = gfx0->gfxdata + code * gfx0->char_modulo;
		UINT8 *gfxdata2 = gfx2->gfxdata + code * gfx2->char_modulo;

		/* assume 16 rows */
		for (y = 0; y < 16; y++)
		{
			const UINT8 *cd = coldata;
			UINT8 *gd0 = gfxdata0;
			UINT8 *gd2 = gfxdata2;

			/* 16 colums, in batches of 4 pixels */
			for (x = 0; x < 16; x += 4)
			{
				int pen0 = *cd++;
				int pen1 = *cd++;
				int tp0, tp1;

				/* every 4 pixels gets its own foreground/background colors */
				for (ix = 0; ix < 4; ix++, gd0++)
					*gd0 = *gd0 ? pen1 : pen0;

				/* for gfx 2, we convert all low-priority pens to 0 */
				tp0 = (pen0 & 0x80) ? pen0 : 0;
				tp1 = (pen1 & 0x80) ? pen1 : 0;
				for (ix = 0; ix < 4; ix++, gd2++)
					*gd2 = *gd2 ? tp1 : tp0;
			}

			/* advance */
			if (y % 4 == 3) coldata = cd;
			gfxdata0 += gfx0->line_modulo;
			gfxdata2 += gfx2->line_modulo;
		}
	}
}



/*************************************
 *
 *	Zwackery background update
 *
 *************************************/

static void zwackery_mark_background(void)
{
	const UINT8 *colordatabase = (const UINT8 *)memory_region(REGION_GFX3);
	int offs;

	/* for every character in the Video RAM, mark the colors */
	for (offs = videoram_size - 2; offs >= 0; offs -= 2)
	{
		int data = READ_WORD(&videoram[offs]);
		int color = (data >> 13) & 7;
		int code = data & 0x3ff;
		int i;

		/* get color data pointers */
		const UINT8 *coldata = colordatabase + code * 32;
		UINT8 *used_colors = &palette_used_colors[color << 8];

		/* each character uses up to 32 unique colors */
		for (i = 0; i < 32; i++)
			used_colors[*coldata++] = PALETTE_COLOR_VISIBLE;
	}
}


static void zwackery_update_background(struct osd_bitmap *bitmap, int overrender)
{
	int offs;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 2; offs >= 0; offs -= 2)
	{
		/* this works for overrendering as well, since the sprite code will mark */
		/* intersecting tiles for us */
		if (dirtybuffer[offs])
		{
			int data = READ_WORD(&videoram[offs]);
			int mx = (offs / 2) % 32;
			int my = (offs / 2) / 32;
			int color = (data >> 13) & 7;
			int code = data & 0x3ff;

			/* standard case: draw with no transparency */
			if (!overrender)
				drawgfx(bitmap, Machine->gfx[0], code, color, data & 0x0800, data & 0x1000,
						16 * mx, 16 * my, &Machine->visible_area, TRANSPARENCY_NONE, 0);

			/* overrender case: for non-zero colors, draw with transparency pen 0 */
			/* we use gfx[2] here, which was generated above to have all low-priority */
			/* colors set to pen 0 */
			else if (color != 0)
				drawgfx(bitmap, Machine->gfx[2], code, color, data & 0x0800, data & 0x1000,
						16 * mx, 16 * my, &Machine->visible_area, TRANSPARENCY_PEN, 0);

#if DEBUG_VIDEO
if (show_bg_colors)
{
	char c = "01234567"[color];
	drawgfx(bitmap, Machine->uifont, c, 1, 0, 0, 16 * mx + 0, 16 * my, 0, TRANSPARENCY_PEN, 0);
	drawgfx(bitmap, Machine->uifont, c, 1, 0, 0, 16 * mx + 4, 16 * my, 0, TRANSPARENCY_PEN, 0);
	drawgfx(bitmap, Machine->uifont, c, 0, 0, 0, 16 * mx + 2, 16 * my, 0, TRANSPARENCY_PEN, 0);
}
#endif

			/* only clear the dirty flag if we're not overrendering */
			dirtybuffer[offs] = 0;
		}
	}
}



/*************************************
 *
 *	Sprite update
 *
 *************************************/

static void zwackery_mark_sprites(void)
{
	UINT16 used[32];
	int offs, i;

	/* clear the usage array */
	memset(&used, 0, sizeof(used));

	/* loop over spriteram */
	for (offs = 0; offs < spriteram_size; offs += 8)
	{
		int code, color, flags;

		/* get the code and skip if zero */
		code = LOW_BYTE(&spriteram[offs + 4]);
		if (code == 0)
			continue;

		/* extract the flag bits and determine the color */
		flags = LOW_BYTE(&spriteram[offs + 2]);
		color = ((~flags >> 2) & 0x0f) | ((flags & 0x02) << 3);

		/* mark the appropriate pens */
		used[color] |= Machine->gfx[1]->pen_usage[code];
	}

	/* use the usage array to mark the global palette_used_colors */
	for (offs = 0; offs < 32; offs++)
	{
		UINT16 u = used[offs];
		if (u)
		{
			palette_used_colors[0x800 + offs * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
			for (i = 1; i < 16; i++)
				if (u & (1 << i))
					palette_used_colors[0x800 + offs * 16 + i] = PALETTE_COLOR_USED;
		}
	}
}


static void zwackery_update_sprites(struct osd_bitmap *bitmap, int priority)
{
	int offs;

	/* loop over sprite RAM */
	for (offs = 0; offs < spriteram_size; offs += 8)
	{
		int code, color, flipx, flipy, x, y, sx, sy, xcount, ycount, flags;

		/* get the code and skip if zero */
		code = LOW_BYTE(&spriteram[offs + 4]);
		if (code == 0)
			continue;

		/* extract the flag bits and determine the color */
		flags = LOW_BYTE(&spriteram[offs + 2]);
		color = ((~flags >> 2) & 0x0f) | ((flags & 0x02) << 3);

		/* for low priority, draw everything but color 7 */
		if (!priority)
		{
			if (color == 7)
				continue;
		}

		/* for high priority, only draw color 7 */
		else
		{
			if (color != 7)
				continue;
		}

		/* determine flipping and coordinates */
		flipx = ~flags & 0x40;
		flipy = flags & 0x80;
		x = (231 - LOW_BYTE(&spriteram[offs + 6])) * 2;
		y = (241 - LOW_BYTE(&spriteram[offs])) * 2;

		if (x <= -32) x += 512;

		/* draw the sprite */
		drawgfx(bitmap, Machine->gfx[1], code, color, flipx, flipy, x, y,
				&Machine->visible_area, TRANSPARENCY_PEN, 0);

#if DEBUG_VIDEO
if (show_colors)
{
	char c = "0123456789ABCDEF"[color];
	drawgfx(bitmap, Machine->uifont, c, 1, 0, 0, x + 8,  y + 8, 0, TRANSPARENCY_PEN, 0);
	drawgfx(bitmap, Machine->uifont, c, 1, 0, 0, x + 12, y + 8, 0, TRANSPARENCY_PEN, 0);
	drawgfx(bitmap, Machine->uifont, c, 0, 0, 0, x + 10, y + 8, 0, TRANSPARENCY_PEN, 0);
}
#endif

		/* sprites use color 0 for background pen and 8 for the 'under tile' pen.
			The color 8 is used to cover over other sprites. */
		if (Machine->gfx[1]->pen_usage[code] & 0x0100)
		{
			struct rectangle clip;

			clip.min_x = x;
			clip.max_x = x + 31;
			clip.min_y = y;
			clip.max_y = y + 31;

			copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &clip, TRANSPARENCY_THROUGH, Machine->pens[8 + color * 16]);
		}

		/* mark tiles underneath as dirty for overrendering */
		if (priority == 0)
		{
			sx = x / 16;
			sy = y / 16;
			xcount = (x & 15) ? 3 : 2;
			ycount = (y & 15) ? 3 : 2;

			for (y = sy; y < sy + ycount; y++)
				for (x = sx; x < sx + xcount; x++)
					dirtybuffer[(32 * (y & 31) + (x & 31)) * 2] = 1;
		}
	}
}



/*************************************
 *
 *	Zwackery MCR/68k update
 *
 *************************************/

void zwackery_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
#if DEBUG_VIDEO
	zwackery_debug();
#endif

	/* mark the palette */
	palette_init_used_colors();
	zwackery_mark_background();
	zwackery_mark_sprites();

	/* update palette */
	if (palette_recalc())
		memset(dirtybuffer, 1, videoram_size);

	/* draw the background */
	zwackery_update_background(tmpbitmap, 0);

	/* copy it to the destination */
	copybitmap(bitmap, tmpbitmap, 0, 0, 0, 0, &Machine->visible_area, TRANSPARENCY_NONE, 0);

	/* draw the low-priority sprites */
	zwackery_update_sprites(bitmap, 0);

	/* draw the background */
	zwackery_update_background(bitmap, 1);

	/* draw the high-priority sprites */
	zwackery_update_sprites(bitmap, 1);
}



/*************************************
 *
 *	Zwackery debug
 *
 *************************************/

#if DEBUG_VIDEO
void zwackery_debug(void)
{
	static int last_h_state;
	static int last_l_state;
	static FILE *f;
	int i;

	if (keyboard_pressed(KEYCODE_CAPSLOCK))
	{
		if (!show_bg_colors) memset(dirtybuffer, 1, videoram_size);
		show_bg_colors = 1;
	}
	else
	{
		if (show_bg_colors) memset(dirtybuffer, 1, videoram_size);
		show_bg_colors = 0;
	}

	if (keyboard_pressed(KEYCODE_9))
	{
		int offs;

		if (!f) f = fopen("mcr.log", "w");
		if (f)
		{
			fprintf(f, "\n\n=================================================================\n");
			for (offs = 0; offs < videoram_size; offs += 2)
			{
				fprintf(f, "%04X ", READ_WORD(&videoram[offs]));
				if (offs % (32 * 2) == 31 * 2) fprintf(f, "\n");
			}
			fprintf(f, "\n\n");
			for (offs = 0; offs < spriteram_size; offs += 8)
				fprintf(f, "Sprite %03d: %02X %02X %02X %02X\n", offs / 8,
					READ_WORD(&spriteram[offs + 0]),
					READ_WORD(&spriteram[offs + 2]),
					READ_WORD(&spriteram[offs + 4]),
					READ_WORD(&spriteram[offs + 6]));
		}
		fflush(f);
	}

	show_colors = keyboard_pressed(KEYCODE_8);

	if (keyboard_pressed(KEYCODE_H))
	{
		last_h_state = 1;
		for (i = 0; i < 2048; i++)
			if (i & 0x80) palette_change_color(i, 255, 255, 255);
	}
	else if (last_h_state)
	{
		last_h_state = 0;
		for (i = 0; i < 2048; i++)
			if (i & 0x80)
			{
				int word = READ_WORD(&paletteram[i * 2]);

				int r = (~word >> 10) & 31;
				int b = (~word >> 5) & 31;
				int g = (~word >> 0) & 31;

				/* up to 8 bits */
				r = (r << 3) | (r >> 2);
				g = (g << 3) | (g >> 2);
				b = (b << 3) | (b >> 2);

				palette_change_color(i, r, g, b);
			}
	}

	if (keyboard_pressed(KEYCODE_L))
	{
		last_l_state = 1;
		for (i = 0; i < 2048; i++)
			if (!(i & 0x80)) palette_change_color(i, 255, 255, 255);
	}
	else if (last_l_state)
	{
		last_l_state = 0;
		for (i = 0; i < 2048; i++)
			if (!(i & 0x80))
			{
				int word = READ_WORD(&paletteram[i * 2]);

				int r = (~word >> 10) & 31;
				int b = (~word >> 5) & 31;
				int g = (~word >> 0) & 31;

				/* up to 8 bits */
				r = (r << 3) | (r >> 2);
				g = (g << 3) | (g >> 2);
				b = (b << 3) | (b >> 2);

				palette_change_color(i, r, g, b);
			}
	}
}
#endif

