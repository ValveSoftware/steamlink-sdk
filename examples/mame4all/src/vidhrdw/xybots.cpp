/***************************************************************************

  vidhrdw/xybots.c

  Functions to emulate the video hardware of the machine.

****************************************************************************

	Playfield encoding
	------------------
		1 16-bit word is used

		Word 1:
			Bit  15    = horizontal flip
			Bits 12-14 = color
			Bits  0-11 = image


	Motion Object encoding
	----------------------
		4 16-bit words are used

		Word 1:
			Bits  0-13 = index of the image (0-16384)
			Bit  15    = horizontal flip

		Word 2:
			Bits  0-3  = priority

		Word 3:
			Bits  0-2  = height of the sprite / 8 (ranges from 1-8)
			Bits  7-14 = Y position of the sprite

		Word 4:
			Bits  0-3  = image palette
			Bits  7-14 = X position of the sprite


	Alpha layer encoding
	--------------------
		1 16-bit word is used

		Word 1:
			Bit  15    = transparent/opaque
			Bit  10-13 = color
			Bits  0-9  = index of the character

***************************************************************************/


#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"

#define XCHARS 42
#define YCHARS 30

#define XDIM (XCHARS*8)
#define YDIM (YCHARS*8)



/*************************************
 *
 *	Structures
 *
 *************************************/

struct pf_overrender_data
{
	struct osd_bitmap *bitmap;
	int mo_priority;
};



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static const UINT8 *update_palette(void);

static void pf_color_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_render_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_check_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);

static void mo_color_callback(const UINT16 *data, const struct rectangle *clip, void *param);
static void mo_render_callback(const UINT16 *data, const struct rectangle *clip, void *param);



/*************************************
 *
 *	Video system start
 *
 *************************************/

int xybots_vh_start(void)
{
	static struct atarigen_mo_desc mo_desc =
	{
		64,                  /* maximum number of MO's */
		8,                   /* number of bytes per MO entry */
		2,                   /* number of bytes between MO words */
		0,                   /* ignore an entry if this word == 0xffff */
		-1, 0, 0x3f,         /* link = (data[linkword] >> linkshift) & linkmask */
		0                    /* render in reverse link order */
	};

	static struct atarigen_pf_desc pf_desc =
	{
		8, 8,				/* width/height of each tile */
		64, 64,				/* number of tiles in each direction */
		1					/* non-scrolling */
	};

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

void xybots_vh_stop(void)
{
	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( xybots_playfieldram_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfieldram[offset], newword);
		atarigen_pf_dirty[offset / 2] = 1;
	}
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void xybots_scanline_update(int scanline)
{
	if (scanline < YDIM)
		atarigen_mo_update(atarigen_spriteram, 0, scanline);
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void xybots_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int i;

	/* update the palette */
	if (update_palette())
		memset(atarigen_pf_dirty, 1, atarigen_playfieldram_size / 2);

	/* set up the all-transparent overrender palette */
	for (i = 0; i < 16; i++)
		atarigen_overrender_colortable[i] = palette_transparent_pen;

	/* render the playfield */
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* render the motion objects */
	atarigen_mo_process(mo_render_callback, bitmap);

	/* redraw the alpha layer completely */
	{
		const struct GfxElement *gfx = Machine->gfx[2];
		int sx, sy, offs;

		for (sy = 0; sy < YCHARS; sy++)
			for (sx = 0, offs = sy * 64; sx < XCHARS; sx++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int code = data & 0x3ff;
				int opaque = data & 0x8000;

				if (code || opaque)
				{
					int color = (data >> 12) & 7;

					drawgfx(bitmap, gfx, code, color, 0, 0, 8 * sx, 8 * sy, 0,
							opaque ? TRANSPARENCY_NONE : TRANSPARENCY_PEN, 0);
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
	UINT16 mo_map[48], al_map[8], pf_map[16];
	int i, j;

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
		int sx, sy, offs;

		for (sy = 0; sy < YCHARS; sy++)
			for (sx = 0, offs = sy * 64; sx < XCHARS; sx++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int color = (data >> 12) & 7;
				int code = data & 0x3ff;
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
					palette_used_colors[0x200 + i * 16 + j] = PALETTE_COLOR_USED;
	}

	/* rebuild the motion object palette */
	for (i = 0; i < 48; i++)
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
	for (i = 0; i < 8; i++)
	{
		UINT16 used = al_map[i];
		if (used)
			for (j = 0; j < 4; j++)
				if (used & (1 << j))
					palette_used_colors[0x000 + i * 4 + j] = PALETTE_COLOR_USED;
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
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = y * 64 + x;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int color = (data >> 11) & 15;
			int code = data & 0x1fff;

			/* mark the colors used by this tile */
			colormap[color] |= usage[code];
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
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = y * 64 + x;

			/* update only if dirty */
			if (atarigen_pf_dirty[offs])
			{
				int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int color = (data >> 11) & 15;
				int hflip = data & 0x8000;
				int code = data & 0x1fff;

				drawgfx(atarigen_pf_bitmap, gfx, code, color, hflip, 0, 8 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = 0;
			}
		}

	/* then blast the result */
	copybitmap(bitmap, atarigen_pf_bitmap, 0, 0, 0, 0, clip, TRANSPARENCY_NONE, 0);
}



/*************************************
 *
 *	Playfield overrender check
 *
 *************************************/

static void pf_check_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	struct pf_overrender_data *overrender_data = (struct pf_overrender_data *)param;
	int mo_priority = overrender_data->mo_priority;
	int x, y;

	/* if we've already decided, bail */
	if (mo_priority == -1)
		return;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = y * 64 + x;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int color = (data >> 11) & 15;

			/* this is the priority equation from the schematics */
			if (mo_priority > color)
			{
				overrender_data->mo_priority = -1;
				return;
			}
		}
}



/*************************************
 *
 *	Playfield overrendering
 *
 *************************************/

static void pf_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const struct pf_overrender_data *overrender_data = (const struct pf_overrender_data *)param;
	const struct GfxElement *gfx = Machine->gfx[0];
	struct osd_bitmap *bitmap = overrender_data->bitmap;
	int mo_priority = overrender_data->mo_priority;
	int x, y;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = y * 64 + x;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int color = (data >> 11) & 15;

			/* this is the priority equation from the schematics */
			if (mo_priority > color)
			{
				int hflip = data & 0x8000;
				int code = data & 0x1fff;

				drawgfx(bitmap, gfx, code, color, hflip, 0, 8 * x, 8 * y, clip, TRANSPARENCY_NONE, 0);
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
	int code = data[0] & 0x3fff;
	int color = data[3] & 7;
	int vsize = (data[2] & 7) + 1;
	UINT16 temp = 0;
	int i;

	/* sneaky -- an extra color bank is hidden after the playfield palette */
	if (data[3] & 8) color = 0x20 + (color ^ 7);

	for (i = 0; i < vsize; i++)
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
	struct GfxElement *gfx = Machine->gfx[1];
	struct pf_overrender_data overrender_data;
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	struct rectangle pf_clip;

	/* extract data from the various words */
	int code = data[0] & 0x3fff;
	int hflip = data[0] & 0x8000;
	int priority = ~data[1] & 15;
	int vsize = (data[2] & 7) + 1;
	int ypos = -(data[2] >> 7);
	int color = data[3] & 7;
	int xpos = data[3] >> 7;

	/* sneaky -- an extra color bank is hidden after the playfield palette */
	if (data[3] & 8) color = 0x20 + (color ^ 7);

	/* adjust for the height */
	ypos -= vsize * 8;

	/* adjust the final coordinates */
	xpos &= 0x1ff;
	ypos &= 0x1ff;
	if (xpos >= XDIM) xpos -= 0x200;
	if (ypos >= YDIM) ypos -= 0x200;

	/* clip the X coordinate */
	if (xpos <= -8 || xpos >= XDIM)
		return;

	/* determine the bounding box */
	atarigen_mo_compute_clip_8x8(pf_clip, xpos, ypos, 1, vsize, clip);

	/* see if we need to overrender */
	overrender_data.mo_priority = priority;
	atarigen_pf_process(pf_check_overrender_callback, &overrender_data, &pf_clip);

	/* if not, do it the easy way */
	if (overrender_data.mo_priority == priority)
	{
		atarigen_mo_draw_8x8_strip(bitmap, gfx, code, color, hflip, 0, xpos, ypos, vsize, clip, TRANSPARENCY_PEN, 0);
	}

	/* otherwise, make it tricky */
	else
	{
		/* draw an instance of the object in all transparent pens */
		atarigen_mo_draw_transparent_8x8_strip(bitmap, gfx, code, hflip, 0, xpos, ypos, vsize, clip, TRANSPARENCY_PEN, 0);

		/* and then draw it normally on the temp bitmap */
		atarigen_mo_draw_8x8_strip(atarigen_pf_overrender_bitmap, gfx, code, color, hflip, 0, xpos, ypos, vsize, clip, TRANSPARENCY_NONE, 0);

		/* overrender the playfield on top of that that */
		overrender_data.mo_priority = priority;
		overrender_data.bitmap = atarigen_pf_overrender_bitmap;
		atarigen_pf_process(pf_overrender_callback, &overrender_data, &pf_clip);

		/* finally, copy this chunk to the real bitmap */
		copybitmap(bitmap, atarigen_pf_overrender_bitmap, 0, 0, 0, 0, &pf_clip, TRANSPARENCY_THROUGH, palette_transparent_pen);
	}
}
