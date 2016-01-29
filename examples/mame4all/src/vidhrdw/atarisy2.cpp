/***************************************************************************

	Atari System 2 hardware

****************************************************************************/


#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"

#define XCHARS 64
#define YCHARS 48

#define XDIM (XCHARS*8)
#define YDIM (YCHARS*8)



/*************************************
 *
 *	Constants
 *
 *************************************/

#define PFRAM_SIZE		0x4000
#define ANRAM_SIZE		0x1800
#define MORAM_SIZE		0x0800



/*************************************
 *
 *	Structures
 *
 *************************************/

struct mo_data
{
	struct osd_bitmap *bitmap;
	int xhold;
};


struct pf_overrender_data
{
	struct osd_bitmap *bitmap;
	int mo_priority;
};



/*************************************
 *
 *	Globals we own
 *
 *************************************/

UINT8 *atarisys2_slapstic;



/*************************************
 *
 *	Statics
 *
 *************************************/

static UINT8 *playfieldram;
static UINT8 *alpharam;

static UINT8 videobank;

static struct atarigen_pf_state pf_state;
static UINT16 latched_vscroll;



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static void pf_render_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_check_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);

static void mo_render_callback(const UINT16 *data, const struct rectangle *clip, void *param);



/*************************************
 *
 *	Video system start
 *
 *************************************/

int atarisys2_vh_start(void)
{
	static struct atarigen_mo_desc mo_desc =
	{
		256,                 /* maximum number of MO's */
		8,                   /* number of bytes per MO entry */
		2,                   /* number of bytes between MO words */
		3,                   /* ignore an entry if this word == 0xffff */
		3, 3, 0xff,          /* link = (data[linkword] >> linkshift) & linkmask */
		0                    /* render in reverse link order */
	};

	static struct atarigen_pf_desc pf_desc =
	{
		8, 8,				/* width/height of each tile */
		128, 64				/* number of tiles in each direction */
	};

	/* allocate banked memory */
	alpharam = (UINT8*)calloc(0x8000, 1);
	if (!alpharam)
		return 1;

	spriteram = alpharam + ANRAM_SIZE;
	playfieldram = alpharam + 0x4000;

	/* reset the videoram banking */
	videoram = alpharam;
	videobank = 0;

	/*
	 * if we are palette reducing, do the simple thing by marking everything used except for
	 * the transparent sprite and alpha colors; this should give some leeway for machines
	 * that can't give up all 256 colors
	 */
	if (palette_used_colors)
	{
		int i;
		memset(palette_used_colors, PALETTE_COLOR_USED, Machine->drv->total_colors * sizeof(UINT8));
		for (i = 0; i < 4; i++)
			palette_used_colors[15 + i * 16] = PALETTE_COLOR_TRANSPARENT;
		for (i = 0; i < 8; i++)
			palette_used_colors[64 + i * 4] = PALETTE_COLOR_TRANSPARENT;
	}

	/* initialize the playfield */
	if (atarigen_pf_init(&pf_desc))
	{
		free(alpharam);
		return 1;
	}

	/* initialize the motion objects */
	if (atarigen_mo_init(&mo_desc))
	{
		atarigen_pf_free();
		free(alpharam);
		return 1;
	}

	return 0;
}



/*************************************
 *
 *	Video system shutdown
 *
 *************************************/

void atarisys2_vh_stop(void)
{
	/* free memory */
	if (alpharam)
		free(alpharam);
	alpharam = playfieldram = spriteram = 0;

	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Scroll/playfield bank write
 *
 *************************************/

WRITE_HANDLER( atarisys2_hscroll_w )
{
	int oldword = READ_WORD(&atarigen_hscroll[offset]);
	int newword = COMBINE_WORD(oldword, data);
	WRITE_WORD(&atarigen_hscroll[offset], newword);

	/* update the playfield parameters - hscroll is clocked on the following scanline */
	pf_state.hscroll = (newword >> 6) & 0x03ff;
	pf_state.param[0] = newword & 0x000f;
	atarigen_pf_update(&pf_state, cpu_getscanline() + 1);

	/* mark the playfield dirty for those games that handle it */
	if (oldword != newword && (Machine->drv->video_attributes & VIDEO_SUPPORTS_DIRTY))
		osd_mark_dirty(Machine->visible_area.min_x, Machine->visible_area.min_y,
		                Machine->visible_area.max_x, Machine->visible_area.max_y, 0);
}


WRITE_HANDLER( atarisys2_vscroll_w )
{
	int oldword = READ_WORD(&atarigen_vscroll[offset]);
	int newword = COMBINE_WORD(oldword, data);
	WRITE_WORD(&atarigen_vscroll[offset], newword);

	/* if bit 4 is zero, the scroll value is clocked in right away */
	latched_vscroll = (newword >> 6) & 0x01ff;
	if (!(newword & 0x10)) pf_state.vscroll = latched_vscroll;

	/* update the playfield parameters */
	pf_state.param[1] = newword & 0x000f;
	atarigen_pf_update(&pf_state, cpu_getscanline() + 1);

	/* mark the playfield dirty for those games that handle it */
	if (oldword != newword && (Machine->drv->video_attributes & VIDEO_SUPPORTS_DIRTY))
		osd_mark_dirty(Machine->visible_area.min_x, Machine->visible_area.min_y,
		                Machine->visible_area.max_x, Machine->visible_area.max_y, 0);
}



/*************************************
 *
 *	Palette RAM write handler
 *
 *************************************/

WRITE_HANDLER( atarisys2_paletteram_w )
{
	static const int intensity_table[16] =
	{
		#define ZB 115
		#define Z3 78
		#define Z2 37
		#define Z1 17
		#define Z0 9
		0, ZB+Z0, ZB+Z1, ZB+Z1+Z0, ZB+Z2, ZB+Z2+Z0, ZB+Z2+Z1, ZB+Z2+Z1+Z0,
		ZB+Z3, ZB+Z3+Z0, ZB+Z3+Z1, ZB+Z3+Z1+Z0,ZB+ Z3+Z2, ZB+Z3+Z2+Z0, ZB+Z3+Z2+Z1, ZB+Z3+Z2+Z1+Z0
	};
	static const int color_table[16] =
		{ 0x0, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xe, 0xf, 0xf };

	int inten, red, green, blue;

	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	int indx = offset / 2;

	WRITE_WORD(&paletteram[offset], newword);

	inten = intensity_table[newword & 15];
	red = (color_table[(newword >> 12) & 15] * inten) >> 4;
	green = (color_table[(newword >> 8) & 15] * inten) >> 4;
	blue = (color_table[(newword >> 4) & 15] * inten) >> 4;
	palette_change_color(indx, red, green, blue);
}



/*************************************
 *
 *	Video RAM bank read/write handlers
 *
 *************************************/

READ_HANDLER( atarisys2_slapstic_r )
{
	slapstic_tweak(offset / 2);

	/* an extra tweak for the next opcode fetch */
	videobank = slapstic_tweak(0x1234);
	videoram = alpharam + videobank * 0x2000;

	return READ_WORD(&atarisys2_slapstic[offset]);
}


WRITE_HANDLER( atarisys2_slapstic_w )
{
	slapstic_tweak(offset / 2);

	/* an extra tweak for the next opcode fetch */
	videobank = slapstic_tweak(0x1234);
	videoram = alpharam + videobank * 0x2000;
}



/*************************************
 *
 *	Video RAM read/write handlers
 *
 *************************************/

READ_HANDLER( atarisys2_videoram_r )
{
	return READ_WORD(&videoram[offset]);
}


WRITE_HANDLER( atarisys2_videoram_w )
{
	int oldword = READ_WORD(&videoram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	WRITE_WORD(&videoram[offset], newword);

	/* mark the playfield dirty if we write to it */
	if (videobank >= 2)
		if ((oldword & 0x3fff) != (newword & 0x3fff))
		{
			int offs = (&videoram[offset] - playfieldram) / 2;
			atarigen_pf_dirty[offs] = 0xff;
		}

	/* force an update if the link of object 0 changes */
	if (videobank == 0 && offset == 0x1806)
		atarigen_mo_update(spriteram, 0, cpu_getscanline() + 1);
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void atarisys2_scanline_update(int scanline)
{
	/* update the playfield */
	if (scanline == 0)
	{
		pf_state.vscroll = latched_vscroll;
		atarigen_pf_update(&pf_state, scanline);
	}

	/* update the motion objects */
	if (scanline < YDIM)
		atarigen_mo_update(spriteram, 0, scanline);
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void atarisys2_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	struct mo_data modata;
	int i;

	/* recalc the palette if necessary */
	if (palette_recalc())
		memset(atarigen_pf_dirty, 0xff, PFRAM_SIZE / 2);

	/* set up the all-transparent overrender palette */
	for (i = 0; i < 16; i++)
		atarigen_overrender_colortable[i] = palette_transparent_pen;

	/* render the playfield */
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* render the motion objects */
	modata.xhold = 0;
	modata.bitmap = bitmap;
	atarigen_mo_process(mo_render_callback, &modata);

	/* render the alpha layer */
	{
		const struct GfxElement *gfx = Machine->gfx[2];
		int sx, sy, offs;

		for (sy = 0; sy < YCHARS; sy++)
			for (sx = 0, offs = sy * 64; sx < XCHARS; sx++, offs++)
			{
				int data = READ_WORD(&alpharam[offs * 2]);
				int code = data & 0x3ff;

				/* if there's a non-zero code, draw the tile */
				if (code)
				{
					int color = (data >> 13) & 7;

					/* draw the character */
					drawgfx(bitmap, gfx, code, color, 0, 0, 8 * sx, 8 * sy, 0, TRANSPARENCY_PEN, 0);
				}
			}
	}

	/* update onscreen messages */
	atarigen_update_messages();
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
			int data = READ_WORD(&playfieldram[offs * 2]);
			int pfbank = state->param[(data >> 10) & 1];

			/* update only if dirty */
			if (atarigen_pf_dirty[offs] != pfbank)
			{
				int code = (pfbank << 10) + (data & 0x3ff);
				int color = (data >> 11) & 7;

				drawgfx(atarigen_pf_bitmap, gfx, code, color, 0, 0, 8 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = pfbank;
			}
		}

	/* then blast the result */
	x = -state->hscroll;
	y = -state->vscroll;
	copyscrollbitmap(bitmap, atarigen_pf_bitmap, 1, &x, 1, &y, clip, TRANSPARENCY_NONE, 0);
}



/*************************************
 *
 *	Playfield overrender check
 *
 *************************************/

static void pf_check_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	struct pf_overrender_data *overrender_data = (struct pf_overrender_data *)param;
	const struct GfxElement *gfx = Machine->gfx[0];
	int mo_priority = overrender_data->mo_priority;
	int x, y;

	/* if we've already decided, bail */
	if (mo_priority == -1)
		return;

	/* standard loop over tiles */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 127)
		{
			int offs = y * 128 + x;
			int data = READ_WORD(&playfieldram[offs * 2]);
			int pf_priority = ((~data >> 13) & 6) | 1;

			if ((mo_priority + pf_priority) & 4)
			{
				int pfbank = state->param[(data >> 10) & 1];
				int code = (pfbank << 10) + (data & 0x3ff);
				if (gfx->pen_usage[code] & 0xff00)
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
	const struct GfxElement *gfx = Machine->gfx[0];
	struct osd_bitmap *bitmap = overrender_data->bitmap;
	int mo_priority = overrender_data->mo_priority;
	int x, y;

	/* standard loop over tiles */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
	{
		int sy = (8 * y - state->vscroll) & 0x1ff;
		if (sy >= YDIM) sy -= 0x200;

		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 127)
		{
			int offs = y * 128 + x;
			int data = READ_WORD(&playfieldram[offs * 2]);
			int pf_priority = ((~data >> 13) & 6) | 1;

			if ((mo_priority + pf_priority) & 4)
			{
				int pfbank = state->param[(data >> 10) & 1];
				int code = (pfbank << 10) + (data & 0x3ff);
				int color = (data >> 11) & 7;
				int sx = (8 * x - state->hscroll) & 0x1ff;
				if (sx >= XDIM) sx -= 0x400;

				drawgfx(bitmap, gfx, code, color, 0, 0, sx, sy, clip, TRANSPARENCY_PENS, 0x00ff);
			}
		}
	}
}



/*************************************
 *
 *	Motion object rendering
 *
 *************************************/

static void mo_render_callback(const UINT16 *data, const struct rectangle *clip, void *param)
{
	struct GfxElement *gfx = Machine->gfx[1];
	struct mo_data *modata = (struct mo_data *)param;
	struct osd_bitmap *bitmap = modata->bitmap;
	struct pf_overrender_data overrender_data;
	struct rectangle pf_clip;

	/* extract data from the various words */
	int ypos = -(data[0] >> 6);
	int hold = data[1] & 0x8000;
	int hflip = data[1] & 0x4000;
	int vsize = ((data[1] >> 11) & 7) + 1;
	int code = (data[1] & 0x7ff) + ((data[0] & 7) << 11);
	int xpos = (data[2] >> 6);
	int color = (data[3] >> 12) & 3;
	int priority = (data[3] >> 13) & 6;

	/* adjust for height */
	ypos -= vsize * 16;

	/* adjust x position for holding */
	if (hold)
		xpos = modata->xhold;
	modata->xhold = xpos + 16;

	/* adjust the final coordinates */
	xpos &= 0x3ff;
	ypos &= 0x1ff;
	if (xpos >= XDIM) xpos -= 0x400;
	if (ypos >= YDIM) ypos -= 0x200;

	/* clip the X coordinate */
	if (xpos <= -16 || xpos >= XDIM)
		return;

	/* determine the bounding box */
	atarigen_mo_compute_clip_16x16(pf_clip, xpos, ypos, 1, vsize, clip);

	/* determine if we need to overrender */
	overrender_data.mo_priority = priority;
	atarigen_pf_process(pf_check_overrender_callback, &overrender_data, &pf_clip);

	/* if not, do it simply */
	if (overrender_data.mo_priority == priority)
	{
		atarigen_mo_draw_16x16_strip(bitmap, gfx, code, color, hflip, 0, xpos, ypos, vsize, clip, TRANSPARENCY_PEN, 15);
	}

	/* otherwise, make it tricky */
	else
	{
		/* draw an instance of the object in all transparent pens */
		atarigen_mo_draw_transparent_16x16_strip(bitmap, gfx, code, hflip, 0, xpos, ypos, vsize, clip, TRANSPARENCY_PEN, 15);

		/* and then draw it normally on the temp bitmap */
		atarigen_mo_draw_16x16_strip(atarigen_pf_overrender_bitmap, gfx, code, color, hflip, 0, xpos, ypos, vsize, clip, TRANSPARENCY_NONE, 0);

		/* overrender the playfield on top of that that */
		overrender_data.mo_priority = priority;
		overrender_data.bitmap = atarigen_pf_overrender_bitmap;
		atarigen_pf_process(pf_overrender_callback, &overrender_data, &pf_clip);

		/* finally, copy this chunk to the real bitmap */
		copybitmap(bitmap, atarigen_pf_overrender_bitmap, 0, 0, 0, 0, &pf_clip, TRANSPARENCY_THROUGH, palette_transparent_pen);
	}
}
