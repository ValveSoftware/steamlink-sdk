/***************************************************************************

	vidhrdw/hydra.c

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

/* Note: if this is set to 1, it must also be set in the driver */
#define HIGH_RES 0

#define XCHARS 42
#define YCHARS 30

#define XDIM (XCHARS*(8 << HIGH_RES))
#define YDIM (YCHARS*8)


#define DEBUG_VIDEO 0



/*************************************
 *
 *	Globals
 *
 *************************************/

struct rectangle hydra_mo_area;
UINT32 hydra_mo_priority_offset;
INT32 hydra_pf_xoffset;



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


struct mo_sort_entry
{
	struct mo_sort_entry *next;
	int entry;
};



/*************************************
 *
 *	Statics
 *
 *************************************/

static struct atarigen_pf_state pf_state;
static UINT16 current_control;



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static const UINT8 *update_palette(void);

static void pf_color_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param);
static void pf_render_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param);

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

int hydra_vh_start(void)
{
	static struct atarigen_pf_desc pf_desc =
	{
		8 << HIGH_RES, 8,	/* width/height of each tile */
		64, 64				/* number of tiles in each direction */
	};


	/* reset statics */
	memset(&pf_state, 0, sizeof(pf_state));
	current_control = 0;

	/* add the top bit to the playfield graphics */
	if (Machine->gfx[0])
	{
		const UINT8 *src = &memory_region(REGION_GFX1)[0x80000];
		unsigned int *pen_usage = Machine->gfx[0]->pen_usage;
		int n, h, w;
		UINT8 *dst = Machine->gfx[0]->gfxdata;

		for (n = 0; n < Machine->gfx[0]->total_elements; n ++)
		{
			unsigned int usage = 0;

			for (h = 0; h < 8; h++)
			{
				UINT8 bits = *src++;

				for (w = 0; w < 8; w++, dst += (1 << HIGH_RES), bits <<= 1)
				{
					dst[0] = (dst[0] & 0x0f) | ((bits >> 3) & 0x10);
					if (HIGH_RES)
						dst[1] = (dst[1] & 0x0f) | ((bits >> 3) & 0x10);
					usage |= 1 << dst[0];
				}
			}

			/* update the tile's pen usage */
			if (pen_usage)
				*pen_usage++ = usage;
		}
	}

	/* decode the motion objects */
	if (atarigen_rle_init(REGION_GFX3, 0x200))
		return 1;

	/* initialize the playfield */
	if (atarigen_pf_init(&pf_desc))
	{
		atarigen_rle_free();
		return 1;
	}

	return 0;
}



/*************************************
 *
 *	Video system shutdown
 *
 *************************************/

void hydra_vh_stop(void)
{
	atarigen_rle_free();
	atarigen_pf_free();
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( hydra_playfieldram_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfieldram[offset], newword);
		atarigen_pf_dirty[(offset / 2) & 0xfff] = 0xff;
	}
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

WRITE_HANDLER( hydra_mo_control_w )
{
	//logerror("MOCONT = %d (scan = %d)\n", data, cpu_getscanline());

	/* set the control value */
	current_control = data;
}


void hydra_scanline_update(int scanline)
{
	UINT16 *base = (UINT16 *)&atarigen_alpharam[((scanline / 8) * 64 + 47) * 2];
	int i;

	//if (scanline == 0) logerror("-------\n");

	/* keep in range */
	if ((UINT8 *)base >= &atarigen_alpharam[atarigen_alpharam_size])
		return;

	/* update the current parameters */
	for (i = 0; i < 8; i++)
	{
		int word;

		word = base[i * 2 + 1];
		if (word & 0x8000)
			pf_state.hscroll = (((word >> 6) + hydra_pf_xoffset) & 0x1ff) << HIGH_RES;

		word = base[i * 2 + 2];
		if (word & 0x8000)
		{
			/* a new vscroll latches the offset into a counter; we must adjust for this */
			int offset = scanline + i;
			if (offset >= 256)
				offset -= 256;
			pf_state.vscroll = ((word >> 6) - offset) & 0x1ff;

			pf_state.param[0] = word & 7;
		}

		/* update the playfield with the new parameters */
		atarigen_pf_update(&pf_state, scanline + i);
	}
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void hydra_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	/* 	a note about this: I don't see how to compute the MO ROM checksums, so these
		are just the values Pit Fighter is expecting. Hydra never checks. */
	static UINT16 mo_checksum[16] =
	{
		0xc289, 0x3103, 0x2b8d, 0xe048, 0xc12e, 0x0ede, 0x2cd7, 0x7dc8,
		0x58fc, 0xb877, 0x9449, 0x59d4, 0x8b63, 0x241b, 0xa3de, 0x4724
	};

	struct mo_sort_entry sort_entry[256];
	struct mo_sort_entry *list_head[256];
	struct mo_sort_entry *current;

	struct mo_params modata;
	const struct GfxElement *gfx;
	int x, y, offs;

#if DEBUG_VIDEO
	int xorval = debug();
#endif

	/* special case: checksum the sprite ROMs */
	if (READ_WORD(&atarigen_spriteram[0]) == 0x000f)
	{
		for (x = 1; x < 5; x++)
			if (READ_WORD(&atarigen_spriteram[x * 2]) != 0)
				break;
		if (x == 5)
		{
			//logerror("Wrote checksums\n");
			for (x = 0; x < 16; x++)
				WRITE_WORD(&atarigen_spriteram[x * 2], mo_checksum[x]);
		}
	}

	/* update the palette, and mark things dirty */
	if (update_palette())
		memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 2);

	/* draw the playfield */
	memset(atarigen_pf_visit, 0, 64*64);
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* draw the motion objects */
	modata.xhold = 1000;
	modata.bitmap = bitmap;

	/* sort the motion objects into their proper priorities */
	memset(list_head, 0, sizeof(list_head));
	for (x = 0; x < 256; x++)
	{
		int priority = READ_WORD(&atarigen_spriteram[x * 16 + hydra_mo_priority_offset]) & 0xff;
		sort_entry[x].entry = x;
		sort_entry[x].next = list_head[priority];
		list_head[priority] = &sort_entry[x];
	}

	/* now loop back and process */
	for (x = 1; x < 256; x++)
		for (current = list_head[x]; current; current = current->next)
			mo_render_callback((const UINT16 *)&atarigen_spriteram[current->entry * 16], &hydra_mo_area, &modata);

	/* draw the alphanumerics */
	gfx = Machine->gfx[1];
	for (y = 0; y < YCHARS; y++)
		for (x = 0, offs = y * 64; x < XCHARS; x++, offs++)
		{
			int data = READ_WORD(&atarigen_alpharam[offs * 2]);
			int code = data & 0xfff;
			int color = (data >> 12) & 15;
			int opaque = data & 0x8000;
			drawgfx(bitmap, gfx, code, color, 0, 0, (8 << HIGH_RES) * x, 8 * y, 0, opaque ? TRANSPARENCY_NONE : TRANSPARENCY_PEN, 0);
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
	UINT16 mo_map[16+4], al_map[16];
	UINT32 pf_map[8];
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
	for (j = 0; j < 256; j++)
	{
		int priority = READ_WORD(&atarigen_spriteram[j * 16 + hydra_mo_priority_offset]) & 0xff;
		if (priority != 0)
			mo_color_callback((const UINT16 *)&atarigen_spriteram[j * 16], &Machine->visible_area, mo_map);
	}

	/* update color usage for the alphanumerics */
	usage = Machine->gfx[1]->pen_usage;
	for (y = 0; y < YCHARS; y++)
		for (x = 0, offs = y * 64; x < XCHARS; x++, offs++)
		{
			int data = READ_WORD(&atarigen_alpharam[offs * 2]);
			int code = data & 0xfff;
			int color = (data >> 12) & 15;
			al_map[color] |= usage[code];
		}

	/* rebuild the playfield palette */
	for (i = 0; i < 8; i++)
	{
		UINT32 used = pf_map[i];
		if (used)
			for (j = 0; j < 32; j++)
				if (used & (1 << j))
					palette_used_colors[0x300 + i * 32 + j] = PALETTE_COLOR_USED;
	}

	/* rebuild the motion object palette */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = mo_map[i];
		if (used)
		{
			for (j = 0; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x200 + i * 16 + j] = PALETTE_COLOR_USED;
		}
	}

	/* rebuild the alphanumerics palette */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = al_map[i];
		if (used)
		{
			if (i < 8)
				palette_used_colors[0x100 + i * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
			else if (used & 0x0001)
				palette_used_colors[0x100 + i * 16 + 0] = PALETTE_COLOR_USED;
			for (j = 1; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x100 + i * 16 + j] = PALETTE_COLOR_USED;
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
	const unsigned int *usage = Machine->gfx[0]->pen_usage;
	UINT32 *colormap = (UINT32 *)param;
	int bankbase = state->param[0] * 0x1000;
	int x, y;

	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = y * 64 + x;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int code = bankbase + (data & 0x0fff);
			int color = (data >> 12) & 7;
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
	int bankbase = state->param[0] * 0x1000;
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	int x, y;

	/* first update any tiles whose color is out of date */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = y * 64 + x;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int color = (data >> 12) & 7;

			if (atarigen_pf_dirty[offs] != state->param[0])
			{
				int code = bankbase + (data & 0x0fff);
				int hflip = data & 0x8000;

				drawgfx(atarigen_pf_bitmap, gfx, code, color, hflip, 0, (8 << HIGH_RES) * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = state->param[0];
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
	UINT16 *colormap = (UINT16 *)param;

	int scale = data[4];
	int code = data[0] & 0x7fff;
	if (scale > 0x0000 && code < atarigen_rle_count)
	{
		const struct atarigen_rle_descriptor *rle = &atarigen_rle_info[code];
		int colorentry = (data[1] & 0xff) >> 4;
		int colorshift = (data[1] & 0x0f);
		int usage = rle->pen_usage;
		int bpp = rle->bpp;

		if (bpp == 4)
		{
			colormap[colorentry + 0] |= usage << colorshift;
			if (colorshift)
				colormap[colorentry + 1] |= usage >> (16 - colorshift);
		}
		else if (bpp == 5)
		{
			colormap[colorentry + 0] |= usage << colorshift;
			colormap[colorentry + 1] |= usage >> (16 - colorshift);
			if (colorshift)
				colormap[colorentry + 2] |= usage >> (32 - colorshift);
		}
		else
		{
			colormap[colorentry + 0] |= usage << colorshift;
			colormap[colorentry + 1] |= usage >> (16 - colorshift);
			if (colorshift)
				colormap[colorentry + 2] |= usage >> (32 - colorshift);

			usage = rle->pen_usage_hi;
			colormap[colorentry + 2] |= usage << colorshift;
			colormap[colorentry + 3] |= usage >> (16 - colorshift);
			if (colorshift)
				colormap[colorentry + 4] |= usage >> (32 - colorshift);
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
	int scale = data[4];
	int code = data[0] & 0x7fff;
	if (scale > 0x0000 && code < atarigen_rle_count)
	{
		const struct mo_params *modata = (const struct mo_params *)param;
		struct osd_bitmap *bitmap = modata->bitmap;
		int hflip = data[0] & 0x8000;
		int color = data[1] & 0xff;
		int x = ((INT16)data[2] >> 6);
		int y = ((INT16)data[3] >> 6);

		atarigen_rle_render(bitmap, &atarigen_rle_info[code], color, hflip, 0, (x << HIGH_RES) + clip->min_x, y, scale << HIGH_RES, scale, clip);
	}
}



/*************************************
 *
 *	Debugging
 *
 *************************************/

#if DEBUG_VIDEO

static void mo_print_callback(struct osd_bitmap *bitmap, struct rectangle *clip, UINT16 *data, void *param)
{
	int code = (data[0] & 0x7fff);
	int vsize = (data[1] & 15) + 1;
	int xpos = (data[3] >> 7);
	int ypos = (data[1] >> 7);
	int color = data[3] & 15;
	int hflip = data[0] & 0x8000;

	FILE *f = (FILE *)param;
	fprintf(f, "P=%04X X=%03X Y=%03X SIZE=%X COL=%X FLIP=%X  -- DATA=%04X %04X %04X %04X\n",
			code, xpos, ypos, vsize, color, hflip >> 15, data[0], data[1], data[2], data[3]);
}

static int debug(void)
{
	int hidebank = -1;

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
/*		atarigen_mo_process(mo_print_callback, f);*/

		fprintf(f, "\n\nMotion Objects (control = %d)\n", current_control);
		for (i = 0; i < 0x100; i++)
		{
			fprintf(f, "   Object %03X:  P=%04X  ?=%04X  X=%04X  Y=%04X  S=%04X  ?=%04X  L=%04X  ?=%04X\n",
					i,
					READ_WORD(&atarigen_spriteram[i*16+0]),
					READ_WORD(&atarigen_spriteram[i*16+2]),
					READ_WORD(&atarigen_spriteram[i*16+4]),
					READ_WORD(&atarigen_spriteram[i*16+6]),
					READ_WORD(&atarigen_spriteram[i*16+8]),
					READ_WORD(&atarigen_spriteram[i*16+10]),
					READ_WORD(&atarigen_spriteram[i*16+12]),
					READ_WORD(&atarigen_spriteram[i*16+14])
			);
		}

		fprintf(f, "\n\nPlayfield dump\n");
		for (i = 0; i < atarigen_playfieldram_size / 2; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&atarigen_playfieldram[i*2]));
			if ((i & 63) == 63) fprintf(f, "\n");
		}

		fprintf(f, "\n\nAlpha dump\n");
		for (i = 0; i < atarigen_alpharam_size / 2; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&atarigen_alpharam[i*2]));
			if ((i & 63) == 63) fprintf(f, "\n");
		}

		fprintf(f, "\n\nMemory dump");
		for (i = 0xff0000; i < 0xffffff; i += 2)
		{
			if ((i & 31) == 0) fprintf(f, "\n%06X: ", i);
			fprintf(f, "%04X ", READ_WORD(&atarigen_spriteram[i - 0xff0000]));
		}

		fclose(f);
	}

	return hidebank;
}

#endif
