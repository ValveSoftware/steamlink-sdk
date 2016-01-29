/***************************************************************************

  vidhrdw/eprom.c

  Functions to emulate the video hardware of the machine.

****************************************************************************

	Playfield encoding
	------------------
		1 16-bit word is used

		Word 1:
			Bits 13-15 = palette
			Bits  0-12 = image number


	Motion Object encoding
	----------------------
		4 16-bit words are used

		Word 1:
			Bits  0-7  = link to the next motion object

		Word 2:
			Bits  0-11 = image index

		Word 3:
			Bits  7-15 = horizontal position
			Bits  0-3  = motion object palette

		Word 4:
			Bits  7-15 = vertical position
			Bits  4-6  = horizontal size of the object, in tiles
			Bit   3    = horizontal flip
			Bits  0-2  = vertical size of the object, in tiles


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


#define DEBUG_VIDEO 0



/*************************************
 *
 *	Globals we own
 *
 *************************************/

UINT8 *eprom_playfieldpalram;
size_t eprom_playfieldpalram_size;



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

#if DEBUG_VIDEO
static int show_colors;
static int special_pen;
#endif



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

#if DEBUG_VIDEO
static void debug(void);
#endif



/*************************************
 *
 *	Video system start
 *
 *************************************/

int eprom_vh_start(void)
{
	static struct atarigen_mo_desc mo_desc =
	{
		1024,                /* maximum number of MO's */
		8,                   /* number of bytes per MO entry */
		2,                   /* number of bytes between MO words */
		0,                   /* ignore an entry if this word == 0xffff */
		0, 0, 0x3ff,         /* link = (data[linkword] >> linkshift) & linkmask */
		1                    /* reverse order */
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

void eprom_vh_stop(void)
{
	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( eprom_playfieldram_w )
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
 *	Playfield palette RAM write handler
 *
 *************************************/

WRITE_HANDLER( eprom_playfieldpalram_w )
{
	int oldword = READ_WORD(&eprom_playfieldpalram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&eprom_playfieldpalram[offset], newword);
		atarigen_pf_dirty[offset / 2] = 1;
	}
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void eprom_scanline_update(int scanline)
{
	/* update the playfield */
	if (scanline == 0)
	{
		pf_state.hscroll = (READ_WORD(&atarigen_alpharam[0xf00]) >> 7) & 0x1ff;
		pf_state.vscroll = (READ_WORD(&atarigen_alpharam[0xf02]) >> 7) & 0x1ff;
		atarigen_pf_update(&pf_state, scanline);
	}

	/* update the MOs from the SLIP table */
	atarigen_mo_update_slip_512(atarigen_spriteram, pf_state.vscroll, scanline, &atarigen_alpharam[0xf80]);
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void eprom_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;

#if DEBUG_VIDEO
	debug();
#endif

	/* update the palette */
	if (update_palette())
		memset(atarigen_pf_dirty, 1, atarigen_playfieldram_size / 2);

	/* set up the all-transparent overrender palette */
	for (i = 0; i < 16; i++)
		atarigen_overrender_colortable[i] = palette_transparent_pen;

	/* render the playfield */
	memset(atarigen_pf_visit, 0, 64*64);
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* render the motion objects */
	atarigen_mo_process(mo_render_callback, bitmap);

	/* redraw the alpha layer completely */
	{
		const struct GfxElement *gfx = Machine->gfx[1];
		int sx, sy, offs;

		for (sy = 0; sy < YCHARS; sy++)
			for (sx = 0, offs = sy * 64; sx < XCHARS; sx++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int code = data & 0x3ff;
				int opaque = data & 0x8000;

				if (code || opaque)
				{
					int color = ((data >> 10) & 0xf) | ((data >> 9) & 0x20);
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
	UINT16 mo_map[16], al_map[64], pf_map[16];
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
		int sx, sy, offs;

		for (sy = 0; sy < YCHARS; sy++)
			for (sx = 0, offs = sy * 64; sx < XCHARS; sx++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int color = ((data >> 10) & 0xf) | ((data >> 9) & 0x20);
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
	for (i = 0; i < 64; i++)
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
			int offs = x * 64 + y;
			int data1 = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int data2 = READ_WORD(&eprom_playfieldpalram[offs * 2]);
			int code = data1 & 0x7fff;
			int color = (data2 >> 8) & 15;

			/* mark the colors used by this tile */
			colormap[color] |= usage[code];

			/* also mark unvisited tiles dirty */
			if (!atarigen_pf_visit[offs]) atarigen_pf_dirty[offs] = 1;
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
			int offs = x * 64 + y;

			/* update only if dirty */
			if (atarigen_pf_dirty[offs])
			{
				int data1 = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int data2 = READ_WORD(&eprom_playfieldpalram[offs * 2]);
				int color = (data2 >> 8) & 15;
				int code = data1 & 0x7fff;
				int hflip = data1 & 0x8000;

				drawgfx(atarigen_pf_bitmap, gfx, code, 0x10 + color, hflip, 0, 8 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = 0;

#if DEBUG_VIDEO
				if (show_colors)
				{
					drawgfx(atarigen_pf_bitmap, Machine->uifont, "0123456789ABCDEF"[color], 1, 0, 0, 8 * x + 0, 8 * y, 0, TRANSPARENCY_PEN, 0);
					drawgfx(atarigen_pf_bitmap, Machine->uifont, "0123456789ABCDEF"[color], 1, 0, 0, 8 * x + 2, 8 * y, 0, TRANSPARENCY_PEN, 0);
					drawgfx(atarigen_pf_bitmap, Machine->uifont, "0123456789ABCDEF"[color], 0, 0, 0, 8 * x + 1, 8 * y, 0, TRANSPARENCY_PEN, 0);
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
	const struct GfxElement *gfx = Machine->gfx[0];
	int x, y;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
	{
		int sx = (8 * x - state->hscroll) & 0x1ff;
		if (sx >= XDIM) sx -= 0x200;

		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = x * 64 + y;
			int data2 = READ_WORD(&eprom_playfieldpalram[offs * 2]);
			int color = (data2 >> 8) & 15;
			int sy = (8 * y - state->vscroll) & 0x1ff;
			if (sy >= YDIM) sy -= 0x200;

			/* update only if dirty */
			if (color >= 13 + overrender_data->mo_priority)
			{
				int data1 = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int code = data1 & 0x7fff;
				int hflip = data1 & 0x8000;

				drawgfx(bitmap, gfx, code, 0x10 + color, hflip, 0, sx, sy, clip, TRANSPARENCY_PENS, 0xff00);

#if DEBUG_VIDEO
				if (show_colors)
				{
					drawgfx(atarigen_pf_bitmap, Machine->uifont, "0123456789ABCDEF"[color], 1, 0, 0, sx + 0, sy, 0, TRANSPARENCY_PEN, 0);
					drawgfx(atarigen_pf_bitmap, Machine->uifont, "0123456789ABCDEF"[color], 1, 0, 0, sx + 2, sy, 0, TRANSPARENCY_PEN, 0);
					drawgfx(atarigen_pf_bitmap, Machine->uifont, "0123456789ABCDEF"[color], 0, 0, 0, sx + 1, sy, 0, TRANSPARENCY_PEN, 0);
				}
#endif
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
	UINT16 *colormap = (UINT16 *)param;
	int code = data[1] & 0x7fff;
	int color = data[2] & 0x000f;
	int hsize = ((data[3] >> 4) & 7) + 1;
	int vsize = (data[3] & 7) + 1;
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
	struct GfxElement *gfx = Machine->gfx[0];
	struct pf_overrender_data overrender_data;
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	struct rectangle pf_clip;

	/* extract data from the various words */
	int code = data[1] & 0x7fff;
	int xpos = (data[2] >> 7) - pf_state.hscroll;
	int priority = (data[2] >> 4) & 3;
	int color = data[2] & 0x000f;
	int ypos = -(data[3] >> 7) - pf_state.vscroll;
	int hsize = ((data[3] >> 4) & 7) + 1;
	int hflip = data[3] & 0x0008;
	int vsize = (data[3] & 7) + 1;

	/* adjust for height */
	ypos -= vsize * 8;

	/* adjust the final coordinates */
	xpos &= 0x1ff;
	ypos &= 0x1ff;
	if (xpos >= XDIM) xpos -= 0x200;
	if (ypos >= YDIM) ypos -= 0x200;

	/* determine the bounding box */
	atarigen_mo_compute_clip_8x8(pf_clip, xpos, ypos, hsize, vsize, clip);

	/* simple case? */
	if (priority == 3)
	{
		/* just draw -- we have dominion over all */
		atarigen_mo_draw_8x8(bitmap, gfx, code, color, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_PEN, 0);
	}

	/* otherwise, it gets a smidge trickier */
	else
	{
		/* draw an instance of the object in all transparent pens */
		atarigen_mo_draw_transparent_8x8(bitmap, gfx, code, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_PEN, 0);

		/* and then draw it normally on the temp bitmap */
		atarigen_mo_draw_8x8(atarigen_pf_overrender_bitmap, gfx, code, color, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_NONE, 0);

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

static void debug(void)
{
	int new_show_colors;

	new_show_colors = keyboard_pressed(KEYCODE_CAPSLOCK);
	if (new_show_colors != show_colors)
	{
		show_colors = new_show_colors;
		memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 2);
	}

	special_pen = -1;
	if (keyboard_pressed(KEYCODE_Q)) special_pen = 0;
	if (keyboard_pressed(KEYCODE_W)) special_pen = 1;
	if (keyboard_pressed(KEYCODE_E)) special_pen = 2;
	if (keyboard_pressed(KEYCODE_R)) special_pen = 3;
	if (keyboard_pressed(KEYCODE_T)) special_pen = 4;
	if (keyboard_pressed(KEYCODE_Y)) special_pen = 5;
	if (keyboard_pressed(KEYCODE_U)) special_pen = 6;
	if (keyboard_pressed(KEYCODE_I)) special_pen = 7;

	if (keyboard_pressed(KEYCODE_A)) special_pen = 8;
	if (keyboard_pressed(KEYCODE_S)) special_pen = 9;
	if (keyboard_pressed(KEYCODE_D)) special_pen = 10;
	if (keyboard_pressed(KEYCODE_F)) special_pen = 11;
	if (keyboard_pressed(KEYCODE_G)) special_pen = 12;
	if (keyboard_pressed(KEYCODE_H)) special_pen = 13;
	if (keyboard_pressed(KEYCODE_J)) special_pen = 14;
	if (keyboard_pressed(KEYCODE_K)) special_pen = 15;

	if (keyboard_pressed(KEYCODE_9))
	{
		static int count;
		char name[50];
		FILE *f;
		int i;

		while (keyboard_pressed(KEYCODE_9)) { }

		sprintf(name, "Dump %d", ++count);
		f = fopen(name, "wt");

		fprintf(f, "\n\nAlpha Palette:\n");
		for (i = 0x000; i < 0x100; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&paletteram[i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
		}

		fprintf(f, "\n\nMotion Object Palette:\n");
		for (i = 0x100; i < 0x200; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&paletteram[i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
		}

		fprintf(f, "\n\nPlayfield Palette:\n");
		for (i = 0x200; i < 0x400; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&paletteram[i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
		}

		fprintf(f, "\n\nMotion Object Config:\n");
		for (i = 0x00; i < 0x40; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&atarigen_playfieldram[0xf00 + i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
		}

		fprintf(f, "\n\nMotion Object SLIPs:\n");
		for (i = 0x00; i < 0x40; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&atarigen_playfieldram[0xf80 + i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
		}

		fprintf(f, "\n\nMotion Objects\n");
		for (i = 0; i < 0x400; i++)
		{
			UINT16 *data = (UINT16 *)&atarigen_spriteram[i*8];
			int code = data[1] & 0x7fff;
			int hsize = ((data[3] >> 4) & 7) + 1;
			int vsize = (data[3] & 7) + 1;
			int xpos = (data[2] >> 7);
			int ypos = (data[3] >> 7) - vsize * 8;
			int color = data[2] & 15;
			int hflip = data[3] & 0x0008;
			fprintf(f, "   Object %03X: L=%03X P=%04X C=%X X=%03X Y=%03X W=%d H=%d F=%d LEFT=(%04X %04X %04X %04X)\n",
					i, data[0] & 0x3ff, code, color, xpos & 0x1ff, ypos & 0x1ff, hsize, vsize, hflip,
					data[0] & 0xfc00, data[1] & 0x0000, data[2] & 0x0070, data[3] & 0x0000);
		}

		fprintf(f, "\n\nPlayfield dump\n");
		for (i = 0; i < atarigen_playfieldram_size / 2; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&atarigen_playfieldram[i*2]));
			if ((i & 63) == 63) fprintf(f, "\n");
		}

		fclose(f);
	}
}

#endif
