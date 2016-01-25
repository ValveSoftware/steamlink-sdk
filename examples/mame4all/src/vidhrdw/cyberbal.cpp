/***************************************************************************

	vidhrdw/cyberbal.c

	Functions to emulate the video hardware of the machine.

****************************************************************************

	Playfield encoding
	------------------
		1 16-bit word is used

		Word 1:
			Bit  15    = horizontal flip
			Bits 11-14 = palette
			Bits  0-12 = image index


	Motion Object encoding
	----------------------
		4 16-bit words are used

		Word 1:
			Bit  15    = horizontal flip
			Bits  0-14 = image index

		Word 2:
			Bits  7-15 = Y position
			Bits  0-3  = height in tiles

		Word 3:
			Bits  3-10 = link to the next image to display

		Word 4:
			Bits  6-14 = X position
			Bit   4    = use current X position + 16 for next sprite
			Bits  0-3  = palette


	Alpha layer encoding
	--------------------
		1 16-bit word is used

		Word 1:
			Bit  15    = horizontal flip
			Bit  12-14 = palette
			Bits  0-11 = image index

***************************************************************************/

#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"

#define XCHARS 42
#define YCHARS 30

#define XDIM (XCHARS*16)
#define YDIM (YCHARS*8)



/*************************************
 *
 *	Structures
 *
 *************************************/

struct mo_params
{
	int xhold;
	struct osd_bitmap *bitmap;
};



/*************************************
 *
 *	Globals we own
 *
 *************************************/

UINT8 *cyberbal_playfieldram_1;
UINT8 *cyberbal_playfieldram_2;



/*************************************
 *
 *	Statics
 *
 *************************************/

static struct atarigen_pf_state pf_state;
static UINT8 current_slip;
static UINT8 *active_palette;



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

int cyberbal_vh_start(void)
{
	static struct atarigen_mo_desc mo_desc =
	{
		256,				/* maximum number of MO's */
		8,					/* number of bytes per MO entry */
		2,					/* number of bytes between MO words */
		0,					/* ignore an entry if this word == 0xffff */
		2, 3, 0xff,			/* link = (data[linkword] >> linkshift) & linkmask */
		0					/* reverse order */
	};

	static struct atarigen_pf_desc pf_desc =
	{
		16, 8,				/* width/height of each tile */
		64, 64				/* number of tiles in each direction */
	};

	/* reset statics */
	memset(&pf_state, 0, sizeof(pf_state));
	current_slip = 0;

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

void cyberbal_vh_stop(void)
{
	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Palette tweaker
 *
 *************************************/

INLINE void set_palette_entry(int entry, UINT16 value)
{
	int r, g, b;

	r = ((value >> 9) & 0x3e) | ((value >> 15) & 1);
	g = ((value >> 4) & 0x3e) | ((value >> 15) & 1);
	b = ((value << 1) & 0x3e) | ((value >> 15) & 1);

	r = (r << 2) | (r >> 4);
	g = (g << 2) | (g >> 4);
	b = (b << 2) | (b >> 4);

	palette_change_color(entry, r, g, b);
}



/*************************************
 *
 *	Screen switcher
 *
 *************************************/

void cyberbal_set_screen(int which)
{
	int i;

	/* update the video memory areas */
	atarigen_playfieldram = which ? cyberbal_playfieldram_2 : cyberbal_playfieldram_1;
	atarigen_playfieldram_size = 0x2000;
	atarigen_alpharam = atarigen_playfieldram + atarigen_playfieldram_size;
	atarigen_alpharam_size = 0x1000;
	atarigen_spriteram = atarigen_alpharam + atarigen_alpharam_size;
	atarigen_spriteram_size = 0x1000;

	/* pick the active palette */
	active_palette = which ? paletteram_2 : paletteram;

	/* re-init the palette */
	for (i = 0; i < 2048; i++)
		set_palette_entry(i, READ_WORD(&active_palette[i * 2]));

	/* invalidate the screen */
	memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 2);
}



/*************************************
 *
 *	Playfield RAM write handlers
 *
 *************************************/

WRITE_HANDLER( cyberbal_playfieldram_1_w )
{
	int oldword = READ_WORD(&cyberbal_playfieldram_1[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&cyberbal_playfieldram_1[offset], newword);
		if (cyberbal_playfieldram_1 == atarigen_playfieldram)
			atarigen_pf_dirty[offset / 2] = 0xff;
	}
}


WRITE_HANDLER( cyberbal_playfieldram_2_w )
{
	int oldword = READ_WORD(&cyberbal_playfieldram_2[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&cyberbal_playfieldram_2[offset], newword);
		if (cyberbal_playfieldram_2 == atarigen_playfieldram)
			atarigen_pf_dirty[offset / 2] = 0xff;
	}
}



/*************************************
 *
 *	Palette RAM write handlers
 *
 *************************************/

WRITE_HANDLER( cyberbal_paletteram_1_w )
{
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&paletteram[offset], newword);
		if (paletteram == active_palette)
			set_palette_entry(offset / 2, newword);
	}
}

READ_HANDLER( cyberbal_paletteram_1_r )
{
	return READ_WORD(&paletteram[offset]);
}


WRITE_HANDLER( cyberbal_paletteram_2_w )
{
	int oldword = READ_WORD(&paletteram_2[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&paletteram_2[offset], newword);
		if (paletteram_2 == active_palette)
			set_palette_entry(offset / 2, newword);
	}
}

READ_HANDLER( cyberbal_paletteram_2_r )
{
	return READ_WORD(&paletteram_2[offset]);
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void cyberbal_scanline_update(int scanline)
{
	UINT16 *base = (UINT16 *)&atarigen_alpharam[((scanline / 8) * 64 + 47) * 2];

	/* keep in range */
	if ((UINT8 *)base >= &atarigen_alpharam[atarigen_alpharam_size])
		return;

	/* update the playfield with the previous parameters */
	atarigen_pf_update(&pf_state, scanline);

	/* update the MOs with the previous parameters */
	atarigen_mo_update(atarigen_spriteram, current_slip, scanline);

	/* update the current parameters */
	if (!(base[3] & 1))
		pf_state.param[0] = (base[3] >> 1) & 7;
	if (!(base[4] & 1))
		pf_state.hscroll = 2 * (((base[4] >> 7) + 4) & 0x1ff);
	if (!(base[5] & 1))
	{
		/* a new vscroll latches the offset into a counter; we must adjust for this */
		int offset = scanline + 8;
		if (offset >= 256)
			offset -= 256;
		pf_state.vscroll = ((base[5] >> 7) - offset) & 0x1ff;
	}
	if (!(base[6] & 1))
		pf_state.param[1] = (base[6] >> 1) & 0xff;
	if (!(base[7] & 1))
		current_slip = (base[7] >> 3) & 0xff;
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void cyberbal_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	struct mo_params modata;
	const struct GfxElement *gfx;
	int x, y, offs;

	/* update the palette, and mark things dirty */
	if (update_palette())
		memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 2);

	/* draw the playfield */
	memset(atarigen_pf_visit, 0, 64*64);
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* draw the motion objects */
	modata.xhold = 1000;
	modata.bitmap = bitmap;
	atarigen_mo_process(mo_render_callback, &modata);

	/* draw the alphanumerics */
	gfx = Machine->gfx[2];
	for (y = 0; y < YCHARS; y++)
		for (x = 0, offs = y * 64; x < XCHARS; x++, offs++)
		{
			int data = READ_WORD(&atarigen_alpharam[offs * 2]);
			int code = data & 0xfff;
			int hflip = data & 0x8000;
			int color = (data >> 12) & 7;
			drawgfx(bitmap, gfx, code, color, hflip, 0, 16 * x, 8 * y, 0, TRANSPARENCY_PEN, 0);
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
	UINT16 pf_map[16 * 8], mo_map[16], al_map[8];
	const unsigned int *usage;
	int i, j, x, y, offs;

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
	usage = Machine->gfx[2]->pen_usage;
	for (y = 0; y < YCHARS; y++)
		for (x = 0, offs = y * 64; x < XCHARS; x++, offs++)
		{
			int data = READ_WORD(&atarigen_alpharam[offs * 2]);
			int code = data & 0xfff;
			int color = (data >> 12) & 7;
			al_map[color] |= usage[code];
		}

	/* rebuild the playfield palette */
	for (i = 0; i < 16 * 8; i++)
	{
		UINT16 used = pf_map[i];
		if (used)
			for (j = 0; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[i * 16 + j] = PALETTE_COLOR_USED;
	}

	/* rebuild the motion object palette */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = mo_map[i];
		if (used)
		{
			palette_used_colors[0x600 + i * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
			for (j = 1; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x600 + i * 16 + j] = PALETTE_COLOR_USED;
		}
	}

	/* rebuild the alphanumerics palette */
	for (i = 0; i < 8; i++)
	{
		UINT16 used = al_map[i];
		if (used)
		{
			palette_used_colors[0x780 + i * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
			for (j = 1; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x780 + i * 16 + j] = PALETTE_COLOR_USED;
		}
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
	UINT16 *colormap = (UINT16 *)param + 16 * state->param[0];
	const unsigned int *usage = Machine->gfx[0]->pen_usage;
	int x, y;

	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = y * 64 + x;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int code = data & 0x1fff;
			int color = (data >> 11) & 15;
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
	int colorbase = 16 * state->param[0];
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	int x, y;

	/* first update any tiles whose color is out of date */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = y * 64 + x;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int color = colorbase + ((data >> 11) & 15);

			if (atarigen_pf_dirty[offs] != color)
			{
				int code = data & 0x1fff;
				int hflip = data & 0x8000;

				drawgfx(atarigen_pf_bitmap, gfx, code, color, hflip, 0, 16 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = color;
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
 *	Motion object palette
 *
 *************************************/

static void mo_color_callback(const UINT16 *data, const struct rectangle *clip, void *param)
{
	const unsigned int *usage = Machine->gfx[1]->pen_usage;
	UINT16 *colormap = (UINT16 *)param;
	UINT16 temp = 0;
	int y;

	int code = data[0] & 0x7fff;
	int vsize = (data[1] & 15) + 1;
	int color = data[3] & 0x0f;

	for (y = 0; y < vsize; y++)
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
	struct mo_params *modata = (struct mo_params *)param;
	struct osd_bitmap *bitmap = modata->bitmap;

	/* extract data from the various words */
	int hflip = data[0] & 0x8000;
	int code = data[0] & 0x7fff;
	int ypos = -(data[1] >> 7);
	int vsize = (data[1] & 0x000f) + 1;
	int xpos = (data[3] >> 6) - 4;
	int hold_next = data[3] & 0x0010;
	int color = data[3] & 0x000f;

	/* adjust for height */
	ypos -= vsize * 8;

	/* if we've got a hold position from the last MO, use that */
	if (modata->xhold != 1000)
		xpos = modata->xhold;

	/* if we've got a hold position for the next MO, set it now */
	if (hold_next)
		modata->xhold = xpos + 16;
	else
		modata->xhold = 1000;

	/* adjust the final coordinates */
	xpos &= 0x3ff;
	ypos &= 0x1ff;
	if (xpos >= XDIM) xpos -= 0x400;
	if (ypos >= YDIM) ypos -= 0x200;

	/* draw the motion object */
	atarigen_mo_draw_16x8_strip(bitmap, gfx, code, color, hflip, 0, xpos, ypos, vsize, clip, TRANSPARENCY_PEN, 0);
}
