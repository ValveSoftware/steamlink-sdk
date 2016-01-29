/***************************************************************************

  vidhrdw/klax.c

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

int klax_vh_start(void)
{
	static struct atarigen_mo_desc mo_desc =
	{
		256,                 /* maximum number of MO's */
		8,                   /* number of bytes per MO entry */
		2,                   /* number of bytes between MO words */
		0,                   /* ignore an entry if this word == 0xffff */
		0, 0, 0xff,          /* link = (data[linkword] >> linkshift) & linkmask */
		0                    /* render in reverse link order */
	};

	static struct atarigen_pf_desc pf_desc =
	{
		8, 8,				/* width/height of each tile */
		64, 32,				/* number of tiles in each direction */
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

void klax_vh_stop(void)
{
	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( klax_playfieldram_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfieldram[offset], newword);
		atarigen_pf_dirty[(offset & 0xfff) / 2] = 1;
	}
}



/*************************************
 *
 *	Latch write handler
 *
 *************************************/

WRITE_HANDLER( klax_latch_w )
{
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void klax_scanline_update(int scanline)
{
	/* update the MOs from the SLIP table */
	atarigen_mo_update_slip_512(atarigen_spriteram, 0, scanline, &atarigen_playfieldram[0xf80]);
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void klax_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	/* remap if necessary */
	if (update_palette())
		memset(atarigen_pf_dirty, 1, atarigen_playfieldram_size / 4);

	/* update playfield */
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* render the motion objects */
	atarigen_mo_process(mo_render_callback, bitmap);

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
	UINT16 mo_map[16], pf_map[16];
	int i, j;

	/* reset color tracking */
	memset(mo_map, 0, sizeof(mo_map));
	memset(pf_map, 0, sizeof(pf_map));
	palette_init_used_colors();

	/* update color usage for the playfield */
	atarigen_pf_process(pf_color_callback, pf_map, &Machine->visible_area);

	/* update color usage for the mo's */
	atarigen_mo_process(mo_color_callback, mo_map);

	/* rebuild the playfield palette */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = pf_map[i];
		if (used)
			for (j = 0; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x100 + i * 16 + j] = PALETTE_COLOR_USED;
	}

	/* rebuild the motion object palette */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = mo_map[i];
		if (used)
		{
			palette_used_colors[0x000 + i * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
			for (j = 1; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x000 + i * 16 + j] = PALETTE_COLOR_USED;
		}
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
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 31)
		{
			int offs = x * 32 + y;
			int data1 = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int data2 = READ_WORD(&atarigen_playfieldram[offs * 2 + 0x1000]);
			int code = data1 & 0x1fff;
			int color = (data2 >> 8) & 15;

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
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 31)
		{
			int offs = x * 32 + y;

			/* update only if dirty */
			if (atarigen_pf_dirty[offs])
			{
				int data1 = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int data2 = READ_WORD(&atarigen_playfieldram[offs * 2 + 0x1000]);
				int color = (data2 >> 8) & 15;
				int hflip = data1 & 0x8000;
				int code = data1 & 0x1fff;

				drawgfx(atarigen_pf_bitmap, gfx, code, color, hflip, 0, 8 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = 0;
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
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	int x, y;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 31)
		{
			int offs = x * 32 + y;
			int data2 = READ_WORD(&atarigen_playfieldram[offs * 2 + 0x1000]);
			int color = (data2 >> 8) & 15;

			/* overdraw if the color is 15 */
			if (color == 15)
			{
				int data1 = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int hflip = data1 & 0x8000;
				int code = data1 & 0x1fff;

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
	int code = data[1] & 0x0fff;
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
	const struct GfxElement *gfx = Machine->gfx[1];
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	struct rectangle pf_clip;

	/* extract data from the various words */
	int code = data[1] & 0x0fff;
	int xpos = data[2] >> 7;
	int ypos = 512 - (data[3] >> 7);
	int color = data[2] & 0x000f;
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

	/* draw the motion object */
	atarigen_mo_draw_8x8(bitmap, gfx, code, color, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_PEN, 0);

	/* overrender the playfield */
	atarigen_pf_process(pf_overrender_callback, bitmap, &pf_clip);
}
