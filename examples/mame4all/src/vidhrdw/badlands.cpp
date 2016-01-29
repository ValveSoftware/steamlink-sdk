/***************************************************************************

	Atari Bad Lands hardware

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

static struct atarigen_pf_state pf_state;



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static void pf_render_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);

static void mo_render_callback(const UINT16 *data, const struct rectangle *clip, void *param);



/*************************************
 *
 *	Generic video system start
 *
 *************************************/

int badlands_vh_start(void)
{
	static struct atarigen_mo_desc mo_desc =
	{
		32,                  /* maximum number of MO's */
		4,                   /* number of bytes per MO entry */
		0x80,                /* number of bytes between MO words */
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

	/* initialize statics */
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

	/*
	 * if we are palette reducing, do the simple thing by marking everything used except for
	 * the transparent sprite and alpha colors; this should give some leeway for machines
	 * that can't give up all 256 colors
	 */
	if (palette_used_colors)
	{
		int i;

		memset(palette_used_colors, PALETTE_COLOR_USED, Machine->drv->total_colors * sizeof(UINT8));
		for (i = 0; i < 8; i++)
			palette_used_colors[0x80 + i * 16] = PALETTE_COLOR_TRANSPARENT;
	}

	return 0;
}



/*************************************
 *
 *	Video system shutdown
 *
 *************************************/

void badlands_vh_stop(void)
{
	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void badlands_scanline_update(int scanline)
{
	/* update motion objects */
	if (scanline == 0)
		atarigen_mo_update(atarigen_spriteram, 0, scanline);
}



/*************************************
 *
 *	Playfield bank write handler
 *
 *************************************/

WRITE_HANDLER( badlands_pf_bank_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		pf_state.param[0] = data & 1;
		atarigen_pf_update(&pf_state, cpu_getscanline());
	}
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( badlands_playfieldram_w )
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
 *	Main refresh
 *
 *************************************/

void badlands_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int i;

	/* remap if necessary */
	if (palette_recalc())
		memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 2);

	/* set up the all-transparent overrender palette */
	for (i = 0; i < 16; i++)
		atarigen_overrender_colortable[i] = palette_transparent_pen;

	/* draw the playfield */
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* render the motion objects */
	atarigen_mo_process(mo_render_callback, bitmap);

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
	int bank = state->param[0] * 0x1000;
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	int x, y;

	/* standard loop over tiles */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = y * 64 + x;

			/* update only if dirty */
			if (atarigen_pf_dirty[offs] != state->param[0])
			{
				int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int code = data & 0x1fff;
				int color = data >> 13;
				if (code & 0x1000) code += bank;

				drawgfx(atarigen_pf_bitmap, gfx, code, color, 0, 0, 8 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = state->param[0];
			}
		}

	/* then blast the result */
	copybitmap(bitmap, atarigen_pf_bitmap, 0, 0, 0, 0, clip, TRANSPARENCY_NONE, 0);
}



/*************************************
 *
 *	Playfield overrendering
 *
 *************************************/

static void pf_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const struct GfxElement *gfx = Machine->gfx[0];
	int bank = state->param[0] * 0x1000;
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	int x, y;

	/* standard loop over tiles */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = y * 64 + x;
/*			int priority_offs = y * 64 + XCHARS + x / 2;*/
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int color = data >> 13;
			int code = data & 0x1fff;
			if (code & 0x1000) code += bank;

			drawgfx(bitmap, gfx, code, color, 0, 0, 8 * x, 8 * y, clip, TRANSPARENCY_PENS, 0x00ff);
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
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	struct rectangle pf_clip;

	/* extract data from the various words */
	int ypos = -(data[1] >> 7);
	int vsize = (data[1] & 0x000f) + 1;
	int code = data[0] & 0x0fff;
	int xpos = data[3] >> 7;
	int color = data[3] & 0x0007;
	int priority = (data[3] >> 3) & 1;

	/* adjust for height */
	ypos -= vsize * 8;

	/* adjust the final coordinates */
	xpos &= 0x1ff;
	ypos &= 0x1ff;
	if (xpos >= XDIM) xpos -= 0x200;
	if (ypos >= YDIM) ypos -= 0x200;

	/* clip the X coordinate */
	if (xpos <= -16 || xpos >= XDIM)
		return;

	/* determine the bounding box */
	atarigen_mo_compute_clip_16x16(pf_clip, xpos, ypos, 1, vsize, clip);

	/* simple case? */
	if (priority == 1)
	{
		/* draw the motion object */
		atarigen_mo_draw_16x8_strip(bitmap, gfx, code, color, 0, 0, xpos, ypos, vsize, clip, TRANSPARENCY_PEN, 0);
	}

	/* otherwise, make it tricky */
	else
	{
		/* draw an instance of the object in all transparent pens */
		atarigen_mo_draw_transparent_16x8_strip(bitmap, gfx, code, 0, 0, xpos, ypos, vsize, clip, TRANSPARENCY_PEN, 0);

		/* and then draw it normally on the temp bitmap */
		atarigen_mo_draw_16x8_strip(atarigen_pf_overrender_bitmap, gfx, code, color, 0, 0, xpos, ypos, vsize, clip, TRANSPARENCY_NONE, 0);

		/* overrender the playfield on top of that that */
		atarigen_pf_process(pf_overrender_callback, atarigen_pf_overrender_bitmap, &pf_clip);

		/* finally, copy this chunk to the real bitmap */
		copybitmap(bitmap, atarigen_pf_overrender_bitmap, 0, 0, 0, 0, &pf_clip, TRANSPARENCY_THROUGH, palette_transparent_pen);
	}
}
