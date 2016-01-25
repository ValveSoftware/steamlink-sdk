/***************************************************************************

  vidhrdw/shuuz.c

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


#define DEBUG_VIDEO 0



/*************************************
 *
 *	Constants
 *
 *************************************/

#define OVERRENDER_STANDARD		0
#define OVERRENDER_PRIORITY		1



/*************************************
 *
 *	Structures
 *
 *************************************/

struct pf_overrender_data
{
	struct osd_bitmap *bitmap;
	int type, color;
};



/*************************************
 *
 *	Statics
 *
 *************************************/

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

int shuuz_vh_start(void)
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

void shuuz_vh_stop(void)
{
	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( shuuz_playfieldram_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	/* update the data if different */
	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfieldram[offset], newword);
		atarigen_pf_dirty[(offset & 0x1fff) / 2] = 1;
	}

	/* handle the latch, but only write the upper byte */
	if (offset < 0x2000 && atarigen_video_control_state.latch1 != -1)
		shuuz_playfieldram_w(offset + 0x2000, atarigen_video_control_state.latch1 | 0x00ff0000);
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void shuuz_scanline_update(int scanline)
{
	/* update the playfield */
	if (scanline == 0)
		atarigen_video_control_update(&atarigen_playfieldram[0x1f00]);

	/* update the MOs from the SLIP table */
	atarigen_mo_update_slip_512(atarigen_spriteram, atarigen_video_control_state.sprite_yscroll, scanline, &atarigen_playfieldram[0x1f80]);
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void shuuz_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
#if DEBUG_VIDEO
	debug();
#endif

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

	/* special case color 15 of motion object palette 15 */
	palette_used_colors[0x000 + 15 * 16 + 15] = PALETTE_COLOR_TRANSPARENT;

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
			int data2 = READ_WORD(&atarigen_playfieldram[offs * 2 + 0x2000]);
			int code = data1 & 0x3fff;
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
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = x * 64 + y;

			/* update only if dirty */
			if (atarigen_pf_dirty[offs])
			{
				int data1 = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int data2 = READ_WORD(&atarigen_playfieldram[offs * 2 + 0x2000]);
				int color = (data2 >> 8) & 15;
				int hflip = data1 & 0x8000;
				int code = data1 & 0x3fff;

				drawgfx(atarigen_pf_bitmap, gfx, code, color, hflip, 0, 8 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
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
	const struct pf_overrender_data *overrender_data = (const struct pf_overrender_data *)param;
	const struct GfxElement *gfx = Machine->gfx[0];
	struct osd_bitmap *bitmap = overrender_data->bitmap;
	int x, y;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = x * 64 + y;
			int data2 = READ_WORD(&atarigen_playfieldram[offs * 2 + 0x2000]);
			int color = (data2 >> 8) & 15;

			/* overdraw if the color is 15 */
			if (((color & 8) && color >= overrender_data->color) || overrender_data->type == OVERRENDER_PRIORITY)
			{
				int data1 = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int hflip = data1 & 0x8000;
				int code = data1 & 0x3fff;

				drawgfx(bitmap, gfx, code, color, hflip, 0, 8 * x, 8 * y, clip, TRANSPARENCY_NONE, 0);

#if DEBUG_VIDEO
				if (show_colors)
				{
					drawgfx(bitmap, Machine->uifont, "0123456789ABCDEF"[color], 1, 0, 0, 8 * x + 0, 8 * y, 0, TRANSPARENCY_PEN, 0);
					drawgfx(bitmap, Machine->uifont, "0123456789ABCDEF"[color], 1, 0, 0, 8 * x + 2, 8 * y, 0, TRANSPARENCY_PEN, 0);
					drawgfx(bitmap, Machine->uifont, "0123456789ABCDEF"[color], 0, 0, 0, 8 * x + 1, 8 * y, 0, TRANSPARENCY_PEN, 0);
				}
#endif
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
	const struct GfxElement *gfx = Machine->gfx[1];
	struct pf_overrender_data overrender_data;
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	struct rectangle pf_clip;

	/* extract data from the various words */
	int hflip = data[1] & 0x8000;
	int code = data[1] & 0x7fff;
	int xpos = (data[2] >> 7) - atarigen_video_control_state.sprite_xscroll;
	int color = data[2] & 0x000f;
	int ypos = -(data[3] >> 7) - atarigen_video_control_state.sprite_yscroll;
	int hsize = ((data[3] >> 4) & 7) + 1;
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

	/* standard priority case? */
	if (color != 15)
	{
		/* overrender the playfield */
		overrender_data.bitmap = bitmap;
		overrender_data.type = OVERRENDER_STANDARD;
		overrender_data.color = color;
		atarigen_pf_process(pf_overrender_callback, &overrender_data, &pf_clip);
	}

	/* high priority case? */
	else
	{
		/* overrender the playfield */
		overrender_data.bitmap = atarigen_pf_overrender_bitmap;
		overrender_data.type = OVERRENDER_PRIORITY;
		overrender_data.color = color;
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
		drawgfx(bitmap, Machine->uifont, "0123456789ABCDEF"[color], 0, 0, 0, tx, ty, 0, TRANSPARENCY_NONE, 0);
	}
#endif
}



/*************************************
 *
 *	Debugging
 *
 *************************************/

#if DEBUG_VIDEO

static void mo_print_callback(const UINT16 *data, const struct rectangle *clip, void *param)
{
	FILE *file = param;

	/* extract data from the various words */
	int hflip = data[1] & 0x8000;
	int code = data[1] & 0x7fff;
	int xpos = (data[2] >> 7) - 5;
	int color = data[2] & 0x000f;
	int ypos = YDIM - (data[3] >> 7);
	int hsize = ((data[3] >> 4) & 7) + 1;
	int vsize = (data[3] & 7) + 1;

	fprintf(file, "P=%04X C=%X F=%X  X=%03X Y=%03X S=%dx%d\n", code, color, hflip >> 15, xpos & 0xfff, ypos & 0xfff, hsize, vsize);
}

static void debug(void)
{
	int new_show_colors;
	int new_special;

	new_show_colors = keyboard_pressed(KEYCODE_CAPSLOCK);
	if (new_show_colors != show_colors)
	{
		show_colors = new_show_colors;
		memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 4);
	}

	if (keyboard_pressed(KEYCODE_Q)) new_special = 0;
	if (keyboard_pressed(KEYCODE_W)) new_special = 1;
	if (keyboard_pressed(KEYCODE_E)) new_special = 2;
	if (keyboard_pressed(KEYCODE_R)) new_special = 3;
	if (keyboard_pressed(KEYCODE_T)) new_special = 4;
	if (keyboard_pressed(KEYCODE_Y)) new_special = 5;
	if (keyboard_pressed(KEYCODE_U)) new_special = 6;
	if (keyboard_pressed(KEYCODE_I)) new_special = 7;

	if (keyboard_pressed(KEYCODE_A)) new_special = 8;
	if (keyboard_pressed(KEYCODE_S)) new_special = 9;
	if (keyboard_pressed(KEYCODE_D)) new_special = 10;
	if (keyboard_pressed(KEYCODE_F)) new_special = 11;
	if (keyboard_pressed(KEYCODE_G)) new_special = 12;
	if (keyboard_pressed(KEYCODE_H)) new_special = 13;
	if (keyboard_pressed(KEYCODE_J)) new_special = 14;
	if (keyboard_pressed(KEYCODE_K)) new_special = 15;

	if (keyboard_pressed(KEYCODE_9))
	{
		static int count;
		char name[50];
		FILE *f;
		int i;

		while (keyboard_pressed(KEYCODE_9)) { }

		sprintf(name, "Dump %d", ++count);
		f = fopen(name, "wt");

		fprintf(f, "\n\nMotion Object Palette:\n");
		for (i = 0x000; i < 0x100; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&paletteram[i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
		}

		fprintf(f, "\n\nPlayfield Palette:\n");
		for (i = 0x100; i < 0x200; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&paletteram[i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
		}

		fprintf(f, "\n\nMotion Objects Drawn:\n");
		atarigen_mo_process(mo_print_callback, f);

		fprintf(f, "\n\nMotion Objects\n");
		for (i = 0; i < 256; i++)
		{
			UINT16 *data = (UINT16 *)&atarigen_spriteram[i*8];
			int hflip = data[1] & 0x8000;
			int code = data[1] & 0x7fff;
			int xpos = (data[2] >> 7) - 5;
			int ypos = YDIM - (data[3] >> 7);
			int color = data[2] & 0x000f;
			int hsize = ((data[3] >> 4) & 7) + 1;
			int vsize = (data[3] & 7) + 1;
			fprintf(f, "   Object %03X: L=%03X P=%04X C=%X X=%03X Y=%03X W=%d H=%d F=%d LEFT=(%04X %04X %04X %04X)\n",
					i, data[0] & 0x3ff, code, color, xpos & 0x1ff, ypos & 0x1ff, hsize, vsize, hflip,
					data[0] & 0xfc00, data[1] & 0x0000, data[2] & 0x0070, data[3] & 0x0008);
		}

		fprintf(f, "\n\nPlayfield dump\n");
		for (i = 0; i < atarigen_playfieldram_size / 4; i++)
		{
			fprintf(f, "%X%04X ", READ_WORD(&atarigen_playfieldram[0x2000 + i*2]) >> 8, READ_WORD(&atarigen_playfieldram[i*2]));
			if ((i & 63) == 63) fprintf(f, "\n");
		}

		fprintf(f, "\n\nSprite RAM dump\n");
		for (i = 0; i < 0x3000 / 2; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&atarigen_spriteram[i*2]));
			if ((i & 31) == 31) fprintf(f, "\n");
		}

		fclose(f);
	}
}

#endif
