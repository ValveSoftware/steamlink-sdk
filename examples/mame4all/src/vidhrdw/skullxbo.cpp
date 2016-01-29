/***************************************************************************

	vidhrdw/skullxbo.c

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
			Bits  0-3  = height in tiles (ranges from 1-16)

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


#define DEBUG_VIDEO 0



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
 *	Statics
 *
 *************************************/

static struct atarigen_pf_state pf_state;

static UINT16 *scroll_list;

static UINT8 latch_byte;
static UINT16 mo_bank;

#if DEBUG_VIDEO
static UINT8 show_colors;
#endif



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

#if DEBUG_VIDEO
static int debug(void);
#endif



/*************************************
 *
 *	Video system start
 *
 *************************************/

int skullxbo_vh_start(void)
{
	static struct atarigen_mo_desc mo_desc =
	{
		256,                 /* maximum number of MO's */
		8,                   /* number of bytes per MO entry */
		2,                   /* number of bytes between MO words */
		0,                   /* ignore an entry if this word == 0xffff */
		0, 0, 0xff,          /* link = (data[linkword] >> linkshift) & linkmask */
		0                    /* reverse order */
	};

	static struct atarigen_pf_desc pf_desc =
	{
		16, 8,				/* width/height of each tile */
		64, 64				/* number of tiles in each direction */
	};

	/* reset statics */
	memset(&pf_state, 0, sizeof(pf_state));
	latch_byte = 0;
	mo_bank = 0;

	/* allocate the scroll list */
	scroll_list = (UINT16*)malloc(sizeof(UINT16) * 2 * YDIM);
	if (!scroll_list)
		return 1;

	/* initialize the playfield */
	if (atarigen_pf_init(&pf_desc))
	{
		free(scroll_list);
		return 1;
	}

	/* initialize the motion objects */
	if (atarigen_mo_init(&mo_desc))
	{
		free(scroll_list);
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

void skullxbo_vh_stop(void)
{
	if (scroll_list)
		free(scroll_list);
	scroll_list = NULL;

	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Video data latch
 *
 *************************************/

WRITE_HANDLER( skullxbo_hscroll_w )
{
	/* update the playfield state */
	pf_state.hscroll = (data >> 7) << 1;
	atarigen_pf_update(&pf_state, cpu_getscanline());
}


WRITE_HANDLER( skullxbo_vscroll_w )
{
	/* adjust for the scanline we're currently on */
	int scanline = cpu_getscanline();
	if (scanline >= YDIM) scanline -= YDIM;

	/* update the playfield state */
	pf_state.vscroll = ((data >> 7) - scanline) & 0x1ff;
	atarigen_pf_update(&pf_state, scanline);
}



/*************************************
 *
 *	Motion object bank handler
 *
 *************************************/

WRITE_HANDLER( skullxbo_mobmsb_w )
{
	mo_bank = (offset & 0x400) * 2;
}



/*************************************
 *
 *	Playfield latch write handler
 *
 *************************************/

WRITE_HANDLER( skullxbo_playfieldlatch_w )
{
	latch_byte = data & 0xff;
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( skullxbo_playfieldram_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	/* only update if changed */
	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfieldram[offset], newword);
		atarigen_pf_dirty[(offset & 0x1fff) / 2] = 0xff;
	}

	/* if we're writing the low byte, also write the upper */
	if (offset < 0x2000)
		skullxbo_playfieldram_w(offset + 0x2000, latch_byte);
}



/*************************************
 *
 *	Periodic playfield updater
 *
 *************************************/

void skullxbo_scanline_update(int scanline)
{
	UINT16 *base = (UINT16 *)&atarigen_alpharam[((scanline / 8) * 64 + XCHARS) * 2];
	int x;

	/* keep in range */
	if ((UINT8 *)base >= &atarigen_alpharam[atarigen_alpharam_size])
		return;

	/* update the MOs from the SLIP table */
	atarigen_mo_update_slip_512(&atarigen_spriteram[mo_bank], pf_state.vscroll, scanline, &atarigen_alpharam[0xf80]);

	/* update the current parameters */
	for (x = XCHARS; x < 64; x++)
	{
		UINT16 data = *base++;
		UINT16 command = data & 0x000f;

		if (command == 0x0d)
		{
			/* a new vscroll latches the offset into a counter; we must adjust for this */
			int offset = scanline;
			if (offset >= YDIM) offset -= YDIM;
			pf_state.vscroll = ((data >> 7) - offset) & 0x1ff;
			atarigen_pf_update(&pf_state, scanline);
		}
	}
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void skullxbo_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int i;

#if DEBUG_VIDEO
	debug();
#endif

	/* update the palette, and mark things dirty */
	if (update_palette())
		memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 4);

	/* set up the all-transparent overrender palette */
	for (i = 0; i < 32; i++)
		atarigen_overrender_colortable[i] = palette_transparent_pen;

	/* draw the playfield */
	memset(atarigen_pf_visit, 0, 64*64);
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* draw the motion objects */
	atarigen_mo_process(mo_render_callback, bitmap);

	/* draw the alphanumerics */
	{
		const struct GfxElement *gfx = Machine->gfx[2];
		int x, y, offs;

		for (y = 0; y < YCHARS; y++)
			for (x = 0, offs = y * 64; x < XCHARS; x++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int code = (data & 0x7ff) ^ 0x400;
				int opaque = data & 0x8000;
				int color = (data >> 11) & 15;
				drawgfx(bitmap, gfx, code, color, 0, 0, 16 * x, 8 * y, 0, opaque ? TRANSPARENCY_NONE : TRANSPARENCY_PEN, 0);
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
	UINT16 pf_map[16], al_map[16];
	UINT32 mo_map[16];
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
			int code = (data & 0x7ff) ^ 0x400;
			int color = (data >> 11) & 15;
			al_map[color] |= usage[code];
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
		UINT32 used = mo_map[i];
		if (used)
		{
			palette_used_colors[0x000 + i * 32 + 0] = PALETTE_COLOR_TRANSPARENT;
			for (j = 1; j < 32; j++)
				if (used & (1 << j))
					palette_used_colors[0x000 + i * 32 + j] = PALETTE_COLOR_USED;
		}
	}

	/* rebuild the alphanumerics palette */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = al_map[i];
		if (used)
			for (j = 0; j < 4; j++)
				if (used & (1 << j))
					palette_used_colors[0x300 + i * 4 + j] = PALETTE_COLOR_USED;
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
	const unsigned int *usage = Machine->gfx[1]->pen_usage;
	UINT16 *colormap = (UINT16 *)param;
	int x, y;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = x * 64 + y;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int data2 = READ_WORD(&atarigen_playfieldram[0x2000 + offs * 2]);
			int code = data & 0x7fff;
			int color = data2 & 15;
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
	const struct GfxElement *gfx = Machine->gfx[1];
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	int x, y;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = x * 64 + y;
			int data2 = READ_WORD(&atarigen_playfieldram[0x2000 + offs * 2]);
			int color = data2 & 15;

			if (atarigen_pf_dirty[offs] != color)
			{
				int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int code = data & 0x7fff;
				int hflip = data & 0x8000;

				drawgfx(atarigen_pf_bitmap, gfx, code, color, hflip, 0, 16 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = color;

#if DEBUG_VIDEO
				if (show_colors)
				{
					drawgfx(atarigen_pf_bitmap, Machine->uifont, "0123456789ABCDEF"[color], 1, 0, 0, 16 * x + 4, 8 * y, 0, TRANSPARENCY_PEN, 0);
					drawgfx(atarigen_pf_bitmap, Machine->uifont, "0123456789ABCDEF"[color], 1, 0, 0, 16 * x + 6, 8 * y, 0, TRANSPARENCY_PEN, 0);
					drawgfx(atarigen_pf_bitmap, Machine->uifont, "0123456789ABCDEF"[color], 0, 0, 0, 16 * x + 5, 8 * y, 0, TRANSPARENCY_PEN, 0);
				}
#endif
			}

			/* track the tiles we've visited */
			atarigen_pf_visit[offs] = 1;
		}

	/* then blast the result */
	x = -state->hscroll;
	y = -state->vscroll;
	copyscrollbitmap(bitmap, atarigen_pf_bitmap, 1, &x, 1, &y, clip, TRANSPARENCY_NONE, 0);

	/* also fill in the scroll list */
	for (y = clip->min_y; y <= clip->max_y; y++)
		if (y >= 0 && y < YDIM)
		{
			scroll_list[y * 2 + 0] = state->hscroll;
			scroll_list[y * 2 + 1] = state->vscroll;
		}
}



/*************************************
 *
 *	Playfield overrender check
 *
 *************************************/

static const UINT16 overrender_matrix[4] =
{
	0xf000,
	0xff00,
	0x0ff0,
	0x00f0
};

static void pf_check_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const unsigned int *usage = Machine->gfx[1]->pen_usage;
	struct pf_overrender_data *overrender_data = (struct pf_overrender_data *)param;
	int mo_priority = overrender_data->mo_priority;
	int x, y;

	/* bail if we've already decided */
	if (mo_priority == -1)
		return;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = x * 64 + y;
			int data2 = READ_WORD(&atarigen_playfieldram[0x2000 + offs * 2]);
			int color = data2 & 15;

			if (overrender_matrix[mo_priority] & (1 << color))
			{
				int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int code = data & 0x7fff;

				if (usage[code] & 0xff00)
				{
					overrender_data->mo_priority = -1;
					return;
				}
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
	struct osd_bitmap *bitmap = overrender_data->bitmap;
	int mo_priority = overrender_data->mo_priority;
	const struct GfxElement *gfx = Machine->gfx[1];
	int x, y;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
	{
		int sx = (16 * x - state->hscroll) & 0x3ff;
		if (sx >= XDIM) sx -= 0x400;

		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = x * 64 + y;
			int data2 = READ_WORD(&atarigen_playfieldram[0x2000 + offs * 2]);
			int color = data2 & 15;
			int sy = (8 * y - state->vscroll) & 0x1ff;
			if (sy >= YDIM) sy -= 0x200;

			if (overrender_matrix[mo_priority] & (1 << color))
			{
				int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int code = data & 0x7fff;
				int hflip = data & 0x8000;

				drawgfx(bitmap, gfx, code, color, hflip, 0, sx, sy, 0, TRANSPARENCY_PENS, 0x00ff);
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
	const unsigned int *usage = Machine->gfx[0]->pen_usage;
	UINT32 *colormap = (UINT32 *)param;
	int code = data[1] & 0x7fff;
	int color = data[2] & 0x000f;
	int hsize = ((data[3] >> 4) & 7) + 1;
	int vsize = (data[3] & 15) + 1;
	int tiles = hsize * vsize;
	UINT32 temp = 0;
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
	struct GfxElement *gfx = Machine->gfx[0];
	struct pf_overrender_data overrender_data;
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	struct rectangle pf_clip;

	/* determine the scroll offsets */
	int scroll_scanline = (clip->min_y < 0) ? 0 : (clip->min_y > YDIM) ? YDIM : clip->min_y;
	int xscroll = scroll_list[scroll_scanline * 2 + 0];
	int yscroll = scroll_list[scroll_scanline * 2 + 1];

	/* extract data from the various words */
	int code = data[1] & 0x7fff;
	int hflip = data[1] & 0x8000;
	int xpos = ((data[2] >> 7) << 1) - xscroll;
	int color = data[2] & 0x000f;
	int hsize = ((data[3] >> 4) & 7) + 1;
	int vsize = (data[3] & 15) + 1;
	int ypos = -(data[3] >> 7) - yscroll;
	int priority = (data[2] >> 4) & 3;

	/* adjust for height */
	ypos -= vsize * 8;

	/* adjust the final coordinates */
	xpos &= 0x3ff;
	ypos &= 0x1ff;
	if (xpos >= XDIM) xpos -= 0x400;
	if (ypos >= YDIM) ypos -= 0x200;

	/* determine the bounding box */
	atarigen_mo_compute_clip_16x8(pf_clip, xpos, ypos, hsize, vsize, clip);

	/* see if we're going to need to overrender */
	overrender_data.mo_priority = priority;
	atarigen_pf_process(pf_check_overrender_callback, &overrender_data, &pf_clip);

	/* simple case? */
	if (overrender_data.mo_priority == priority)
	{
		/* just draw -- we have dominion over all */
		atarigen_mo_draw_16x8(bitmap, gfx, code, color, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_PEN, 0);
	}

	/* otherwise, it gets a smidge trickier */
	else
	{
		/* draw an instance of the object in all transparent pens */
		atarigen_mo_draw_transparent_16x8(bitmap, gfx, code, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_PEN, 0);

		/* and then draw it normally on the temp bitmap */
		atarigen_mo_draw_16x8(atarigen_pf_overrender_bitmap, gfx, code, color, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_NONE, 0);

		/* overrender the playfield on top of that that */
		overrender_data.mo_priority = priority;
		overrender_data.bitmap = atarigen_pf_overrender_bitmap;
		atarigen_pf_process(pf_overrender_callback, &overrender_data, &pf_clip);

		/* finally, copy this chunk to the real bitmap */
		copybitmap(bitmap, atarigen_pf_overrender_bitmap, 0, 0, 0, 0, &pf_clip, TRANSPARENCY_THROUGH, palette_transparent_pen);
	}

#if DEBUG_VIDEO
	if (show_colors)
	{
		int tx = (pf_clip.min_x + pf_clip.max_x) / 2 - 3;
		int ty = (pf_clip.min_y + pf_clip.max_y) / 2 - 4;
		drawgfx(bitmap, Machine->uifont, ' ', 0, 0, 0, tx - 2, ty - 2, 0, TRANSPARENCY_NONE, 0);
		drawgfx(bitmap, Machine->uifont, ' ', 0, 0, 0, tx + 2, ty - 2, 0, TRANSPARENCY_NONE, 0);
		drawgfx(bitmap, Machine->uifont, ' ', 0, 0, 0, tx - 2, ty + 2, 0, TRANSPARENCY_NONE, 0);
		drawgfx(bitmap, Machine->uifont, ' ', 0, 0, 0, tx + 2, ty + 2, 0, TRANSPARENCY_NONE, 0);
		drawgfx(bitmap, Machine->uifont, "0123456789ABCDEF"[priority], 0, 0, 0, tx, ty, 0, TRANSPARENCY_NONE, 0);
	}
#endif
}



/*************************************
 *
 *	Debugging
 *
 *************************************/

#if DEBUG_VIDEO

static void mo_print(struct osd_bitmap *bitmap, struct rectangle *clip, UINT16 *data, void *param)
{
	int code = data[1] & 0x7fff;
	int hsize = ((data[3] >> 4) & 7) + 1;
	int vsize = (data[3] & 15) + 1;
	int xpos = (data[2] >> 7);
	int ypos = (data[3] >> 7);
	int color = data[2] & 15;
	int hflip = data[1] & 0x8000;
	int priority = (data[2] >> 4) & 3;

	FILE *f = (FILE *)param;
	fprintf(f, "P=%04X X=%03X Y=%03X SIZE=%Xx%X COL=%X FLIP=%X PRI=%X  -- DATA=%04X %04X %04X %04X\n",
			code, xpos, ypos, hsize, vsize, color, hflip >> 15, priority, data[0], data[1], data[2], data[3]);
}

static int debug(void)
{
	int hidebank = 0;
	int new_show_colors;

	new_show_colors = keyboard_pressed(KEYCODE_CAPSLOCK);
	if (new_show_colors != show_colors)
	{
		show_colors = new_show_colors;
		memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 4);
	}

	if (keyboard_pressed(KEYCODE_Q)) hidebank = 0;
	if (keyboard_pressed(KEYCODE_W)) hidebank = 1;
	if (keyboard_pressed(KEYCODE_E)) hidebank = 2;
	if (keyboard_pressed(KEYCODE_R)) hidebank = 3;
	if (keyboard_pressed(KEYCODE_T)) hidebank = 4;
	if (keyboard_pressed(KEYCODE_Y)) hidebank = 5;
	if (keyboard_pressed(KEYCODE_U)) hidebank = 6;
	if (keyboard_pressed(KEYCODE_I)) hidebank = 7;

	if (keyboard_pressed(KEYCODE_A)) hidebank = 8;
	if (keyboard_pressed(KEYCODE_S)) hidebank = 9;
	if (keyboard_pressed(KEYCODE_D)) hidebank = 10;
	if (keyboard_pressed(KEYCODE_F)) hidebank = 11;
	if (keyboard_pressed(KEYCODE_G)) hidebank = 12;
	if (keyboard_pressed(KEYCODE_H)) hidebank = 13;
	if (keyboard_pressed(KEYCODE_J)) hidebank = 14;
	if (keyboard_pressed(KEYCODE_K)) hidebank = 15;

	if (keyboard_pressed(KEYCODE_9))
	{
		static int count;
		char name[50];
		FILE *f;
		int i;

		while (keyboard_pressed(KEYCODE_9)) { }

		sprintf(name, "Dump %d", ++count);
		f = fopen(name, "wt");

		fprintf(f, "\n\nPalette RAM:\n");

		for (i = 0x000; i < 0x800; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&paletteram[i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
			if ((i & 255) == 255) fprintf(f, "\n");
		}

		fprintf(f, "\n\nMotion Objects (drawn)\n");
/*		atarigen_mo_process(mo_print, f);*/

		fprintf(f, "\n\nMotion Objects\n");
		for (i = 0; i < 0x200; i++)
		{
			fprintf(f, "   Object %02X:  P=%04X  Y=%04X  L=%04X  X=%04X\n",
					i,
					READ_WORD(&atarigen_spriteram[i*8+0]),
					READ_WORD(&atarigen_spriteram[i*8+2]),
					READ_WORD(&atarigen_spriteram[i*8+4]),
					READ_WORD(&atarigen_spriteram[i*8+6])
			);
		}

		fprintf(f, "\n\nPlayfield dump\n");
		for (i = 0; i < atarigen_playfieldram_size / 4; i++)
		{
			fprintf(f, "%01X%04X ", READ_WORD(&atarigen_playfieldram[0x2000 + i*2]), READ_WORD(&atarigen_playfieldram[i*2]));
			if ((i & 63) == 63) fprintf(f, "\n");
		}

		fprintf(f, "\n\nAlpha dump\n");
		for (i = 0; i < atarigen_alpharam_size / 2; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&atarigen_alpharam[i*2]));
			if ((i & 63) == 63) fprintf(f, "\n");
		}

		fclose(f);
	}

	return hidebank;
}

#endif
