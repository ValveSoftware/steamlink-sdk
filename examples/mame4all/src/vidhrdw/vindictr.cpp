/***************************************************************************

	vidhrdw/vindictr.c

	Functions to emulate the video hardware of the machine.

****************************************************************************

	Playfield encoding
	------------------
		1 16-bit word is used

		Word 1:
			Bit  15    = horizontal flip
			Bits 13-15 = palette
			Bits  0-12 = image index


	Motion Object encoding
	----------------------
		4 16-bit words are used

		Word 1:
			Bits  0-14 = image index

		Word 2:
			Bits  7-15 = X position
			Bits  0-3  = palette

		Word 3:
			Bits  7-14 = Y position
			Bit   6    = horizontal flip
			Bits  3-5  = width in tiles (ranges from 1-8)
			Bits  0-2  = height in tiles (ranges from 1-8)

		Word 4:
			Bits  0-9  = link to the next image to display


	Alpha layer encoding
	--------------------
		1 16-bit word is used

		Word 1:
			Bit  15    = transparent/opaque
			Bit  10-13 = palette
			Bits  0-9  = image index

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
 *	Statics
 *
 *************************************/

struct mo_data
{
	struct osd_bitmap *bitmap;
	UINT8 color_xor;
};

static struct atarigen_pf_state pf_state;



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static const UINT8 *update_palette(void);

static void pf_color_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_render_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);

static void mo_color_callback(const UINT16 *data, const struct rectangle *clip, void *param);
static void mo_render_callback(const UINT16 *data, const struct rectangle *clip, void *param);



/*************************************
 *
 *	Video system start
 *
 *************************************/

int vindictr_vh_start(void)
{
	static struct atarigen_mo_desc mo_desc =
	{
		1024,                /* maximum number of MO's */
		8,                   /* number of bytes per MO entry */
		2,                   /* number of bytes between MO words */
		0,                   /* ignore an entry if this word == 0xffff */
		3, 0, 0x3ff,         /* link = (data[linkword] >> linkshift) & linkmask */
		0                    /* reverse order */
	};

	static struct atarigen_pf_desc pf_desc =
	{
		8, 8,				/* width/height of each tile */
		64, 64				/* number of tiles in each direction */
	};

	/* reset statics */
	memset(&pf_state, 0, sizeof(pf_state));

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

void vindictr_vh_stop(void)
{
	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( vindictr_playfieldram_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfieldram[offset], newword);
		atarigen_pf_dirty[offset / 2] = 0xff;
	}
}



/*************************************
 *
 *	Palette RAM write handler
 *
 *************************************/

WRITE_HANDLER( vindictr_paletteram_w )
{
	static const int ztable[16] =
		{ 0x0, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11 };

	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	int i, r, g, b;

	WRITE_WORD(&paletteram[offset], newword);

	i = ztable[(newword >> 12) & 15];
	r = ((newword >> 8) & 15) * i;
	g = ((newword >> 4) & 15) * i;
	b = ((newword >> 0) & 15) * i;

	offset /= 2;
	palette_change_color(offset, r, g, b);
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void vindictr_scanline_update(int scanline)
{
	UINT16 *base = (UINT16 *)&atarigen_alpharam[((scanline / 8) * 64 + XCHARS) * 2];
	int x;

	/* update the playfield with the previous parameters */
	atarigen_pf_update(&pf_state, scanline);

	/* update the MOs from the SLIP table */
	atarigen_mo_update_slip_512(atarigen_spriteram, pf_state.vscroll, scanline, &atarigen_alpharam[0xf80]);

	/* update the current parameters */
	if ((UINT8 *)base < &atarigen_alpharam[0xf80])
		for (x = XCHARS; x < 64; x++)
		{
			UINT16 data = *base++;
			UINT16 command = data & 0x7e00;

			if (command == 0x7400)
				pf_state.param[0] = data & 7;
			else if (command == 0x7600)
				pf_state.hscroll = data & 0x1ff;
			else if (command == 0x7800)
				;
			else if (command == 0x7a00)
				;
			else if (command == 0x7c00)
				;
			else if (command == 0x7e00)
			{
				/* a new vscroll latches the offset into a counter; we must adjust for this */
				int offset = scanline + 8;
				if (offset >= 240)
					offset -= 240;
				pf_state.vscroll = (data - offset) & 0x1ff;
			}
		}
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void vindictr_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	/* update the palette, and mark things dirty */
	if (update_palette())
		memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 2);

	/* draw the playfield */
	memset(atarigen_pf_visit, 0, 64*64);
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* draw the motion objects */
	atarigen_mo_process(mo_render_callback, bitmap);

	/* draw the alphanumerics */
	{
		const struct GfxElement *gfx = Machine->gfx[1];
		int x, y, offs;

		for (y = 0; y < YCHARS; y++)
			for (x = 0, offs = y * 64; x < XCHARS; x++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int code = data & 0x3ff;
				int opaque = data & 0x8000;

				if (code || opaque)
				{
					int color = ((data >> 10) & 0xf) | ((data >> 9) & 0x20);
					drawgfx(bitmap, gfx, code, color, 0, 0, 8 * x, 8 * y, 0, opaque ? TRANSPARENCY_NONE : TRANSPARENCY_PEN, 0);
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
	UINT16 pf_map[16], al_map[64], mo_map[16];
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
		const unsigned int *usage = Machine->gfx[1]->pen_usage;
		int x, y, offs;

		for (y = 0; y < YCHARS; y++)
			for (x = 0, offs = y * 64; x < XCHARS; x++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int code = data & 0x3ff;
				int color = ((data >> 10) & 0xf) | ((data >> 9) & 0x20);
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
	for (i = 0; i < 16; i++)
	{
		UINT16 used = mo_map[i];
		if (used)
		{
			palette_used_colors[0x100 + i * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
			palette_used_colors[0x100 + i * 16 + 1] = PALETTE_COLOR_TRANSPARENT;
			for (j = 2; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x100 + i * 16 + j] = PALETTE_COLOR_USED;
		}
	}

	/* rebuild the alphanumerics palette */
	for (i = 0; i < 64; i++)
	{
		UINT16 used = al_map[i];
		if (used)
			for (j = 0; j < 4; j++)
				if (used & (1 << j))
					palette_used_colors[0x000 + i * 4 + j] = PALETTE_COLOR_USED;
	}

	/* recalc */
	return palette_recalc();
}



/*************************************
 *
 *	Playfield palette
 *
 *************************************/

static void pf_color_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const unsigned int *usage = &Machine->gfx[0]->pen_usage[state->param[0] * 0x1000];
	UINT16 *colormap = (UINT16 *)param;
	int x, y;

	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = x * 64 + y;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int code = data & 0xfff;
			int color = (data >> 11) & 14;
			colormap[color] |= usage[code];
			colormap[color ^ 1] |= usage[code];

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
	int bank = state->param[0];
	int x, y;

	/* first update any tiles whose color is out of date */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = x * 64 + y;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);

			/* update only if dirty */
			if (atarigen_pf_dirty[offs] != bank)
			{
				int color = 16 + ((data >> 11) & 14);
				int code = bank * 0x1000 + (data & 0xfff);
				int hflip = data & 0x8000;

				drawgfx(atarigen_pf_bitmap, gfx, code, color, hflip, 0, 8 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = bank;
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
	const struct mo_data *modata = (const struct mo_data *)param;
	struct osd_bitmap *bitmap = modata->bitmap;
	int color_xor = modata->color_xor;
	int bank = state->param[0];
	int x, y;

	/* first update any tiles whose color is out of date */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
	{
		int sy = (8 * y - state->vscroll) & 0x1ff;
		if (sy >= YDIM) sy -= 0x200;

		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = x * 64 + y;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int color = 16 + ((data >> 11) & 14);
			int code = bank * 0x1000 + (data & 0xfff);
			int hflip = data & 0x8000;
			int sx = (8 * x - state->hscroll) & 0x1ff;
			if (sx >= XDIM) sx -= 0x200;

			drawgfx(bitmap, gfx, code, color ^ color_xor, hflip, 0, sx, sy, 0, TRANSPARENCY_THROUGH, palette_transparent_pen);
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
	const unsigned int *usage = Machine->gfx[0]->pen_usage;
	UINT16 *colormap = (UINT16 *)param;
	int code = data[0] & 0x7fff;
	int hsize = ((data[2] >> 3) & 7) + 1;
	int vsize = (data[2] & 7) + 1;
	int color = data[1] & 0x000f;
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
	const struct GfxElement *gfx = Machine->gfx[0];
	const unsigned int *usage = gfx->pen_usage;
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	unsigned int total_usage = 0;
	struct rectangle pf_clip;
	int x, y, sx, sy;

	/* extract data from the various words */
	int code = data[0] & 0x7fff;
	int color = data[1] & 0x000f;
	int ypos = -pf_state.vscroll - (data[2] >> 7);
	int hflip = data[2] & 0x0040;
	int hsize = ((data[2] >> 3) & 7) + 1;
	int vsize = (data[2] & 7) + 1;
	int xpos = -pf_state.hscroll + (data[1] >> 7);
	int xadv;

	/* adjust for height */
	ypos -= vsize * 8;

	/* adjust the final coordinates */
	xpos &= 0x1ff;
	ypos &= 0x1ff;
	if (xpos >= XDIM) xpos -= 0x200;
	if (ypos >= YDIM) ypos -= 0x200;

	/* determine the bounding box */
	atarigen_mo_compute_clip_8x8(pf_clip, xpos, ypos, hsize, vsize, clip);

	/* adjust for h flip */
	if (hflip)
		xpos += (hsize - 1) * 8, xadv = -8;
	else
		xadv = 8;

	/* loop over the height */
	for (y = 0, sy = ypos; y < vsize; y++, sy += 8)
	{
		/* clip the Y coordinate */
		if (sy <= clip->min_y - 8)
		{
			code += hsize;
			continue;
		}
		else if (sy > clip->max_y)
			break;

		/* loop over the width */
		for (x = 0, sx = xpos; x < hsize; x++, sx += xadv, code++)
		{
			/* clip the X coordinate */
			if (sx <= -8 || sx >= XDIM)
				continue;

			/* draw the sprite */
			drawgfx(bitmap, gfx, code, color, hflip, 0, sx, sy, clip, TRANSPARENCY_PEN, 0);
			total_usage |= usage[code];
		}
	}

	/* overrender the playfield */
	if (total_usage & 0x0002)
	{
		struct mo_data modata;
		modata.bitmap = bitmap;
		modata.color_xor = (color == 0) ? 0 : 1;
		atarigen_pf_process(pf_overrender_callback, &modata, &pf_clip);
	}
}
