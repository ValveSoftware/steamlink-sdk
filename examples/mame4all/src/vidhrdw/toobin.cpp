/***************************************************************************

  vidhrdw/toobin.c

  Functions to emulate the video hardware of the machine.

****************************************************************************

	Playfield encoding
	------------------
		2 16-bit words are used

		Word 1:
			Bits  4-7  = priority of the image
			Bits  0-3  = palette

		Word 2:
			Bit  15    = vertical flip
			Bit  14    = horizontal flip
			Bits  0-13 = image index


	Motion Object encoding
	----------------------
		4 16-bit words are used

		Word 1:
			Bit  15    = absolute/relative positioning (1 = absolute)
			Bits  6-14 = Y position
			Bits  3-5  = height in tiles
			Bits  0-2  = width in tiles

		Word 2:
			Bit  15    = vertical flip
			Bit  14    = horizontal flip
			Bits  0-13 = image index

		Word 3:
			Bits 12-15 = priority (only upper 2 bits used)
			Bits  0-7  = link to the next image to display

		Word 4:
			Bits  6-15 = X position
			Bits  0-3  = palette


	Alpha layer encoding
	--------------------
		1 16-bit word is used

		Word 1:
			Bit  12-15 = palette
			Bit  11    = horizontal flip
			Bits  0-10 = image index

***************************************************************************/

#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"

#define XCHARS 64
#define YCHARS 48

#define XDIM (XCHARS*8)
#define YDIM (YCHARS*8)



/*************************************
 *
 *	Globals we own
 *
 *************************************/

UINT8 *toobin_intensity;
UINT8 *toobin_moslip;



/*************************************
 *
 *	Statics
 *
 *************************************/

static struct atarigen_pf_state pf_state;
static UINT8 last_intensity;



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static const UINT8 *update_palette(void);

static void pf_color_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_render_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);

static void mo_color_callback(const UINT16 *data, const struct rectangle *clip, void *param);
static void mo_render_callback(const UINT16 *data, const struct rectangle *clip, void *param);



/*************************************
 *
 *	Video system start
 *
 *************************************/

int toobin_vh_start(void)
{
	static struct atarigen_mo_desc mo_desc =
	{
		256,                 /* maximum number of MO's */
		8,                   /* number of bytes per MO entry */
		2,                   /* number of bytes between MO words */
		2,                   /* ignore an entry if this word == 0xffff */
		2, 0, 0xff,          /* link = (data[linkword] >> linkshift) & linkmask */
		0                    /* render in reverse link order */
	};

	static struct atarigen_pf_desc pf_desc =
	{
		8, 8,				/* width/height of each tile */
		128, 64				/* number of tiles in each direction */
	};

	/* reset statics */
	last_intensity = 0;

	/* initialize the playfield */
	if (atarigen_pf_init(&pf_desc))
		return 1;

	/* initialize the motion objects */
	if (atarigen_mo_init(&mo_desc))
	{
		atarigen_pf_free();
		return 1;
	}

	return 0;
}



/*************************************
 *
 *	Video system shutdown
 *
 *************************************/

void toobin_vh_stop(void)
{
	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( toobin_playfieldram_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfieldram[offset], newword);
		atarigen_pf_dirty[offset / 4] = 1;
	}
}



/*************************************
 *
 *	Palette RAM write handler
 *
 *************************************/

WRITE_HANDLER( toobin_paletteram_w )
{
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	WRITE_WORD(&paletteram[offset], newword);

	{
		int red =   (((newword >> 10) & 31) * 224) >> 5;
		int green = (((newword >>  5) & 31) * 224) >> 5;
		int blue =  (((newword      ) & 31) * 224) >> 5;

		if (red) red += 38;
		if (green) green += 38;
		if (blue) blue += 38;

		if (!(newword & 0x8000))
		{
			red = (red * last_intensity) >> 5;
			green = (green * last_intensity) >> 5;
			blue = (blue * last_intensity) >> 5;
		}

		palette_change_color((offset / 2) & 0x3ff, red, green, blue);
	}
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void toobin_scanline_update(int scanline)
{
	int link = READ_WORD(&toobin_moslip[0]) & 0xff;

	/* update the playfield */
	if (scanline == 0)
	{
		pf_state.hscroll = (READ_WORD(&atarigen_hscroll[0]) >> 6) & 0x3ff;
		pf_state.vscroll = (READ_WORD(&atarigen_vscroll[0]) >> 6) & 0x1ff;
		atarigen_pf_update(&pf_state, scanline);
	}

	/* update the motion objects */
	if (scanline < YDIM)
		atarigen_mo_update(atarigen_spriteram, link, scanline);
}



/*************************************
 *
 *	MO SLIP write handler
 *
 *************************************/

WRITE_HANDLER( toobin_moslip_w )
{
	COMBINE_WORD_MEM(&toobin_moslip[offset], data);
	toobin_scanline_update(cpu_getscanline());
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void toobin_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	/* update the palette */
	if (update_palette())
		memset(atarigen_pf_dirty, 1, atarigen_playfieldram_size / 4);

	/* render the playfield */
	memset(atarigen_pf_visit, 0, 128*64);
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* render the motion objects */
	atarigen_mo_process(mo_render_callback, bitmap);

	/* render the alpha layer */
	{
		const struct GfxElement *gfx = Machine->gfx[2];
		int sx, sy, offs;

		for (sy = 0; sy < YCHARS; sy++)
			for (sx = 0, offs = sy * 64; sx < XCHARS; sx++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int code = data & 0x3ff;

				/* if there's a non-zero code, draw the tile */
				if (code)
				{
					int color = (data >> 12) & 15;
					int hflip = data & 0x400;

					/* draw the character */
					drawgfx(bitmap, gfx, code, color, hflip, 0, 8 * sx, 8 * sy, 0, TRANSPARENCY_PEN, 0);
				}
			}
	}

	/* update onscreen messages */
	atarigen_update_messages();
}



/*************************************
 *
 *	Palette management
 *
 *************************************/

static const UINT8 *update_palette(void)
{
	UINT16 al_map[16], pf_map[16], mo_map[16];
	int intensity, i, j;

	/* compute the intensity and modify the palette if it's different */
	intensity = 0x1f - (READ_WORD(&toobin_intensity[0]) & 0x1f);
	if (intensity != last_intensity)
	{
		last_intensity = intensity;
		for (i = 0; i < 256+256+64; i++)
		{
			int newword = READ_WORD(&paletteram[i*2]);
			int red =   (((newword >> 10) & 31) * 224) >> 5;
			int green = (((newword >>  5) & 31) * 224) >> 5;
			int blue =  (((newword      ) & 31) * 224) >> 5;

			if (red) red += 38;
			if (green) green += 38;
			if (blue) blue += 38;

			if (!(newword & 0x8000))
			{
				red = (red * last_intensity) >> 5;
				green = (green * last_intensity) >> 5;
				blue = (blue * last_intensity) >> 5;
			}

			palette_change_color(i, red, green, blue);
		}
	}

	/* reset color tracking */
	memset(mo_map, 0, sizeof(mo_map));
	memset(pf_map, 0, sizeof(pf_map));
	memset(al_map, 0, sizeof(al_map));
	palette_init_used_colors();

	/* update color usage for the playfield */
	atarigen_pf_process(pf_color_callback, pf_map, &Machine->visible_area);

	/* update color usage for the mo's */
	atarigen_mo_process(mo_color_callback, mo_map);

	/* update color usage for the alphanumerics */
	{
		const unsigned int *usage = Machine->gfx[2]->pen_usage;
		int offs, sx, sy;

		for (sy = 0; sy < YCHARS; sy++)
			for (sx = 0, offs = sy * 64; sx < XCHARS; sx++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int color = (data >> 12) & 0x000f;
				int code = data & 0x03ff;
				al_map[color] |= usage[code];
			}
	}

	/* rebuild the playfield palette */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = pf_map[i];
		if (used)
			for (j = 0; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x000 + i * 16 + j] = PALETTE_COLOR_USED;
	}

	/* rebuild the motion object palette */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = mo_map[i];
		if (used)
		{
			palette_used_colors[0x100 + i * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
			for (j = 1; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x100 + i * 16 + j] = PALETTE_COLOR_USED;
		}
	}

	/* rebuild the alphanumerics palette */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = al_map[i];
		if (used)
			for (j = 0; j < 4; j++)
				if (used & (1 << j))
					palette_used_colors[0x200 + i * 4 + j] = PALETTE_COLOR_USED;
	}

	return palette_recalc();
}



/*************************************
 *
 *	Playfield palette
 *
 *************************************/

static void pf_color_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const unsigned int *usage = Machine->gfx[0]->pen_usage;
	UINT16 *colormap = (UINT16 *)param;
	int x, y;

	/* standard loop over tiles */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 127)
		{
			int offs = y * 128 + x;
			int data1 = READ_WORD(&atarigen_playfieldram[offs * 4]);
			int data2 = READ_WORD(&atarigen_playfieldram[offs * 4 + 2]);
			int color = data1 & 0x000f;
			int code = data2 & 0x3fff;

			/* mark the colors used by this tile */
			colormap[color] |= usage[code];

			/* also mark unvisited tiles dirty */
			if (!atarigen_pf_visit[offs]) atarigen_pf_dirty[offs] = 0xff;
		}
}



/*************************************
 *
 *	Playfield rendering
 *
 *************************************/

static void pf_render_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const struct GfxElement *gfx = Machine->gfx[0];
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	int x, y;

	/* standard loop over tiles */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 127)
		{
			int offs = y * 128 + x;

			/* update only if dirty */
			if (atarigen_pf_dirty[offs])
			{
				int data1 = READ_WORD(&atarigen_playfieldram[offs * 4]);
				int data2 = READ_WORD(&atarigen_playfieldram[offs * 4 + 2]);
				int color = data1 & 0x000f;
/*				int priority = (data1 >> 4) & 3;*/
				int vflip = data2 & 0x8000;
				int hflip = data2 & 0x4000;
				int code = data2 & 0x3fff;

				drawgfx(atarigen_pf_bitmap, gfx, code, color, hflip, vflip, 8 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = 0;
			}

			/* track the tiles we've visited */
			atarigen_pf_visit[offs] = 1;
		}

	/* then blast the result */
	x = -state->hscroll;
	y = -state->vscroll;
	copyscrollbitmap(bitmap, atarigen_pf_bitmap, 1, &x, 1, &y, clip, TRANSPARENCY_NONE, 0);
}



/*************************************
 *
 *	Playfield overrendering
 *
 *************************************/

static void pf_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const struct GfxElement *gfx = Machine->gfx[0];
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	int x, y;

	/* standard loop over tiles */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
	{
		int sy = (8 * y - state->vscroll) & 0x1ff;
		if (sy >= YDIM) sy -= 0x200;

		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 127)
		{
			int offs = y * 128 + x;
			int data1 = READ_WORD(&atarigen_playfieldram[offs * 4]);
			int priority = (data1 >> 4) & 3;

			/* overrender if there is a non-zero priority for this tile */
			/* not perfect, but works for the most obvious cases */
			if (priority)
			{
				int data2 = READ_WORD(&atarigen_playfieldram[offs * 4 + 2]);
				int color = data1 & 0x000f;
				int vflip = data2 & 0x8000;
				int hflip = data2 & 0x4000;
				int code = data2 & 0x3fff;
				int sx = (8 * x - state->hscroll) & 0x1ff;
				if (sx >= XDIM) sx -= 0x400;

				drawgfx(bitmap, gfx, code, color, hflip, vflip, sx, sy, clip, TRANSPARENCY_PENS, 0x000000ff);
			}
		}
	}
}



/*************************************
 *
 *	Motion object palette
 *
 *************************************/

static void mo_color_callback(const UINT16 *data, const struct rectangle *clip, void *param)
{
	const unsigned int *usage = Machine->gfx[1]->pen_usage;
	UINT16 *colormap = (UINT16 *)param;
	int hsize = (data[0] & 7) + 1;
	int vsize = ((data[0] >> 3) & 7) + 1;
	int code = data[1] & 0x3fff;
	int color = data[3] & 0x000f;
	int tiles = hsize * vsize;
	UINT16 temp = 0;
	int i;

	for (i = 0; i < tiles; i++)
		temp |= usage[code++];
	colormap[color] |= temp;
}



/*************************************
 *
 *	Motion object rendering
 *
 *************************************/

static void mo_render_callback(const UINT16 *data, const struct rectangle *clip, void *param)
{
	const struct GfxElement *gfx = Machine->gfx[1];
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	struct rectangle pf_clip;

	/* extract data from the various words */
	int absolute = data[0] & 0x8000;
	int ypos = -(data[0] >> 6);
	int hsize = (data[0] & 7) + 1;
	int vsize = ((data[0] >> 3) & 7) + 1;
	int vflip = data[1] & 0x8000;
	int hflip = data[1] & 0x4000;
	int code = data[1] & 0x3fff;
	int xpos = data[3] >> 6;
	int color = data[3] & 0x000f;
//	int priority = (data[3] >> 4) & 3;

	/* adjust for height */
	ypos -= vsize * 16;

	/* adjust position if relative */
	if (!absolute)
	{
		xpos -= pf_state.hscroll;
		ypos -= pf_state.vscroll;
	}

	/* adjust the final coordinates */
	xpos &= 0x3ff;
	ypos &= 0x1ff;
	if (xpos >= XDIM) xpos -= 0x400;
	if (ypos >= YDIM) ypos -= 0x200;

	/* determine the bounding box */
	atarigen_mo_compute_clip_16x16(pf_clip, xpos, ypos, hsize, vsize, clip);

	/* draw the motion object */
	{
		int tilex, tiley, screeny, screendx, screendy;
		int screenx = xpos;
		int starty = ypos;
		int tile = code;

		/* adjust for h flip */
		if (hflip)
			screenx += (hsize - 1) * 16, screendx = -16;
		else
			screendx = 16;

		/* adjust for v flip */
		if (vflip)
			starty += (vsize - 1) * 16, screendy = -16;
		else
			screendy = 16;

		/* loop over the height */
		for (tilex = 0; tilex < hsize; tilex++, screenx += screendx)
		{
			/* clip the x coordinate */
			if (screenx <= clip->min_x - 16)
			{
				tile += vsize;
				continue;
			}
			else if (screenx > clip->max_x)
				break;

			/* loop over the width */
			for (tiley = 0, screeny = starty; tiley < vsize; tiley++, screeny += screendy, tile++)
			{
				/* clip the y coordinate */
				if (screeny <= clip->min_y - 16 || screeny > clip->max_y)
					continue;

				/* draw the sprite */
				drawgfx(bitmap, gfx, tile, color, hflip, vflip, screenx, screeny, clip, TRANSPARENCY_PEN, 0);
			}
		}
	}

	/* overrender the playfield */
	atarigen_pf_process(pf_overrender_callback, bitmap, &pf_clip);
}
