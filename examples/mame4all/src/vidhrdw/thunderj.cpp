/***************************************************************************

  vidhrdw/thunderj.c

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
			Bits  0-9  = link to the next motion object

		Word 2:
			Bits  0-14 = image index

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
static struct atarigen_pf_state pf2_state;

static UINT16 alpha_bank;
static UINT16 palette_bank;
static UINT32 *start_end;

#if DEBUG_VIDEO
static UINT8 show_colors;
static UINT8 special_pen;
#endif



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static const UINT8 *update_palette(void);

static void pf_color_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf2_color_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_render_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf2_render_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);
static void pf2_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *data);

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

int thunderj_vh_start(void)
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
	memset(&pf2_state, 0, sizeof(pf2_state));
	
	/* allocate memory */
	start_end = (UINT32*)malloc(sizeof(UINT32) * 512);
	if (!start_end)
		return 1;
	
	/* initialize the playfield */
	if (atarigen_pf_init(&pf_desc))
	{
		free(start_end);
		return 1;
	}
	
	/* initialize the second playfield */
	if (atarigen_pf2_init(&pf_desc))
	{
		atarigen_pf_free();
		free(start_end);
		return 1;
	}
	
	/* initialize the motion objects */
	if (atarigen_mo_init(&mo_desc))
	{
		atarigen_pf2_free();
		atarigen_pf_free();
		free(start_end);
		return 1;
	}
	
	return 0;
}



/*************************************
 *
 *	Video system shutdown
 *
 *************************************/

void thunderj_vh_stop(void)
{
	atarigen_pf2_free();
	atarigen_pf_free();
	atarigen_mo_free();
	if (start_end)
		free(start_end);
	start_end = NULL;
}



/*************************************
 *
 *	Alpha banking
 *
 *************************************/

void thunderj_set_alpha_bank(int bank)
{
	alpha_bank = bank * 0x200;
}



/*************************************
 *
 *	Playfield RAM write handlers
 *
 *************************************/

WRITE_HANDLER( thunderj_colorram_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram_color[offset]);
	int newword = COMBINE_WORD(oldword, data);

	/* only update if changed */
	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfieldram_color[offset], newword);
		
		oldword ^= newword;
		
		/* low byte affects pf1 */
		if (oldword & 0x00ff)
			atarigen_pf_dirty[(offset / 2) & 0xfff] = 1;

		/* upper byte affects pf2 */
		if (oldword & 0xff00)
			atarigen_pf2_dirty[(offset / 2) & 0xfff] = 1;
	}
}


WRITE_HANDLER( thunderj_playfieldram_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	/* only update if changed */
	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfieldram[offset], newword);
		atarigen_pf_dirty[(offset / 2) & 0xfff] = 1;
	}
	
	/* handle the latch, but only write the lower byte */
	if (atarigen_video_control_state.latch2 != -1)
		thunderj_colorram_w(offset, atarigen_video_control_state.latch2 | 0xff000000);
}


WRITE_HANDLER( thunderj_playfield2ram_w )
{
	int oldword = READ_WORD(&atarigen_playfield2ram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	/* only update if changed */
	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfield2ram[offset], newword);
		atarigen_pf2_dirty[(offset / 2) & 0xfff] = 1;
	}
	
	/* handle the latch, but only write the upper byte */
	if (atarigen_video_control_state.latch1 != -1)
		thunderj_colorram_w(offset, atarigen_video_control_state.latch1 | 0x00ff0000);
}



/*************************************
 *
 *	Scroll register parser
 *
 *************************************/

static void recompute_scrolls_and_update(int scanline)
{
	/* copy in the scroll values */
	pf_state.hscroll = atarigen_video_control_state.pf1_xscroll + (atarigen_video_control_state.pf2_xscroll & 7);
	pf_state.vscroll = atarigen_video_control_state.pf1_yscroll;
	pf2_state.hscroll = atarigen_video_control_state.pf2_xscroll + 4;
	pf2_state.vscroll = atarigen_video_control_state.pf2_yscroll;
	
	/* update the two playfields */
	atarigen_pf_update(&pf_state, scanline);
	atarigen_pf2_update(&pf2_state, scanline);
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void thunderj_scanline_update(int scanline)
{
	/* update the playfield */
	if (scanline == 0)
	{
		int new_bank;
		
		atarigen_video_control_update(&atarigen_alpharam[0xf00]);
		recompute_scrolls_and_update(scanline);

		/* check for a new palette bank */
		new_bank = atarigen_video_control_state.palette_bank * 0x400;
		if (new_bank != palette_bank)
		{
			palette_bank = new_bank;
			memset(atarigen_pf2_dirty, 0xff, atarigen_playfieldram_size / 2);
		}
	}
	
	/* update the MOs from the SLIP table */
	atarigen_mo_update_slip_512(atarigen_spriteram, atarigen_video_control_state.sprite_yscroll, scanline, &atarigen_alpharam[0xf80]);
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void thunderj_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;
	
#if DEBUG_VIDEO
	debug();
#endif

	/* update the palette */
	if (update_palette())
	{
		memset(atarigen_pf_dirty, 1, atarigen_playfieldram_size / 2);
		memset(atarigen_pf2_dirty, 1, atarigen_playfield2ram_size / 2);
	}

	/* set up the all-transparent overrender palette */
	for (i = 0; i < 16; i++)
		atarigen_overrender_colortable[i] = palette_transparent_pen;

	/* render the playfield */
	memset(atarigen_pf_visit, 0, 64*64);
#if DEBUG_VIDEO
	if (show_colors == 2)
		osd_clearbitmap(bitmap);
	else
#endif
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* render the playfield */
	memset(atarigen_pf2_visit, 0, 64*64);
#if DEBUG_VIDEO
	if (show_colors != 1)
#endif
	atarigen_pf2_process(pf2_render_callback, bitmap, &Machine->visible_area);

	/* render the motion objects */
	memset(start_end, 0xff, 512 * sizeof(UINT32));
	atarigen_mo_process(mo_render_callback, bitmap);
	
	/* redraw the alpha layer completely */
	{
		const struct GfxElement *gfx = Machine->gfx[2];
		int sx, sy, offs;

		for (sy = 0; sy < YCHARS; sy++)
			for (sx = 0, offs = sy * 64; sx < XCHARS; sx++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int code = data & 0x1ff;
				int opaque = data & 0x8000;
	
				if (data & 0x200) code += alpha_bank;
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
	UINT16 mo_map[16], al_map[64], pf_map[32];
	int i, j;

	/* reset color tracking */
	memset(mo_map, 0, sizeof(mo_map));
	memset(pf_map, 0, sizeof(pf_map));
	memset(al_map, 0, sizeof(al_map));
	palette_init_used_colors();
	
	/* update color usage for the playfields */
	atarigen_pf_process(pf_color_callback, pf_map, &Machine->visible_area);
	atarigen_pf2_process(pf2_color_callback, pf_map, &Machine->visible_area);

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
				int color = ((data >> 10) & 0xf) | ((data >> 9) & 0x20);
				int code = data & 0x1ff;
	
				if (data & 0x200) code += alpha_bank;
				al_map[color] |= usage[code];
			}
	}

	/* rebuild the playfield palettes */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = pf_map[i];
		if (used)
		{
			if (palette_bank == 0 || i == 0)
				palette_used_colors[0x200 + palette_bank + i * 16] = PALETTE_COLOR_TRANSPARENT;
			else if (used & 0x0001)
				palette_used_colors[0x200 + palette_bank + i * 16] = PALETTE_COLOR_USED;
			for (j = 1; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x200 + palette_bank + i * 16 + j] = PALETTE_COLOR_USED;
		}
	}
	for (i = 16; i < 32; i++)
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
			palette_used_colors[0x100 + palette_bank + i * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
			for (j = 1; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x100 + palette_bank + i * 16 + j] = PALETTE_COLOR_USED;
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
			int data2 = READ_WORD(&atarigen_playfieldram_color[offs * 2]);
			int code = data1 & 0x7fff;
			int color = 0x10 + (data2 & 0x0f);

			/* mark the colors used by this tile */
			colormap[color] |= usage[code];
			
			/* also mark unvisited tiles dirty */
			if (!atarigen_pf_visit[offs]) atarigen_pf_dirty[offs] = 1;
		}
}


static void pf2_color_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const unsigned int *usage = Machine->gfx[0]->pen_usage;
	UINT16 *colormap = (UINT16 *)param;
	int x, y;
	
	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = x * 64 + y;
			int data1 = READ_WORD(&atarigen_playfield2ram[offs * 2]);
			int data2 = READ_WORD(&atarigen_playfieldram_color[offs * 2]);
			int code = data1 & 0x7fff;
			int color = (data2 >> 8) & 0x0f;

			/* mark the colors used by this tile */
			colormap[color] |= usage[code];
			
			/* also mark unvisited tiles dirty */
			if (!atarigen_pf2_visit[offs]) atarigen_pf2_dirty[offs] = 1;
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
				int data2 = READ_WORD(&atarigen_playfieldram_color[offs * 2]);
				int color = 0x10 + (data2 & 0x0f);
				int code = data1 & 0x7fff;
				int hflip = data1 & 0x8000;
				
				drawgfx(atarigen_pf_bitmap, gfx, code, color, hflip, 0, 8 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf_dirty[offs] = 0;

#if DEBUG_VIDEO
				if (show_colors == 1 || show_colors == -1)
				{
					char c;
					if (show_colors == 1)
						c = "0123456789ABCDEF"[(data2 >> 4) & 0x0f];
					else
						c = "0123456789ABCDEF"[(data2 >> 0) & 0x0f];
					drawgfx(atarigen_pf_bitmap, Machine->uifont, c, 1, 0, 0, 8 * x + 0, 8 * y, 0, TRANSPARENCY_PEN, 0);
					drawgfx(atarigen_pf_bitmap, Machine->uifont, c, 1, 0, 0, 8 * x + 2, 8 * y, 0, TRANSPARENCY_PEN, 0);
					drawgfx(atarigen_pf_bitmap, Machine->uifont, c, 0, 0, 0, 8 * x + 1, 8 * y, 0, TRANSPARENCY_PEN, 0);
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


static void pf2_render_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
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
			if (atarigen_pf2_dirty[offs])
			{
				int data1 = READ_WORD(&atarigen_playfield2ram[offs * 2]);
				int data2 = READ_WORD(&atarigen_playfieldram_color[offs * 2]);
				int color = (data2 >> 8) & 0x0f;
				int code = data1 & 0x7fff;
				int hflip = data1 & 0x8000;

				color += palette_bank >> 4;
				
				drawgfx(atarigen_pf2_bitmap, gfx, code, color, hflip, 0, 8 * x, 8 * y, 0, TRANSPARENCY_NONE, 0);
				atarigen_pf2_dirty[offs] = 0;

#if DEBUG_VIDEO
				if (show_colors == 2 || show_colors == -2)
				{
					char c;
					if (show_colors == 2)
						c = "0123456789ABCDEF"[(data2 >> 12) & 0x0f];
					else
						c = "0123456789ABCDEF"[(data2 >> 8) & 0x0f];
					drawgfx(atarigen_pf2_bitmap, Machine->uifont, c, 1, 0, 0, 8 * x + 0, 8 * y, 0, TRANSPARENCY_PEN, 0);
					drawgfx(atarigen_pf2_bitmap, Machine->uifont, c, 1, 0, 0, 8 * x + 2, 8 * y, 0, TRANSPARENCY_PEN, 0);
					drawgfx(atarigen_pf2_bitmap, Machine->uifont, c, 0, 0, 0, 8 * x + 1, 8 * y, 0, TRANSPARENCY_PEN, 0);
				}
#endif
			}
			
			/* track the tiles we've visited */
			atarigen_pf2_visit[offs] = 1;
		}

	/* then blast the result */
	x = -state->hscroll;
	y = -state->vscroll;
	copyscrollbitmap(bitmap, atarigen_pf2_bitmap, 1, &x, 1, &y, clip, TRANSPARENCY_PEN, palette_transparent_pen);
}



/*************************************
 *
 *	Playfield overrendering
 *
 *************************************/

static const UINT16 transparency_mask[4] = { 0xffff, 0x00ff, 0x00ff, 0x00ff };

static void pf_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const struct pf_overrender_data *overrender_data = (const struct pf_overrender_data *)param;

	struct osd_bitmap *bitmap = overrender_data->bitmap;
	const struct GfxElement *gfx = Machine->gfx[0];
	int mo_priority = overrender_data->mo_priority;
	int x, y;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
	{
		int sx = (8 * x - state->hscroll) & 0x1ff;
		if (sx >= XDIM) sx -= 0x200;

		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = x * 64 + y;
			int data2 = READ_WORD(&atarigen_playfieldram_color[offs * 2]);
			int priority = (data2 >> 4) & 3;
			
			if (priority > mo_priority)
			{
				int color = 0x10 + (data2 & 0x0f);
				int data1 = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int code = data1 & 0x7fff;
				int hflip = data1 & 0x8000;

				int sy = (8 * y - state->vscroll) & 0x1ff;
				if (sy >= YDIM) sy -= 0x200;

				if (mo_priority == -1)
					drawgfx(bitmap, gfx, code, color, hflip, 0, sx, sy, clip, TRANSPARENCY_THROUGH, palette_transparent_pen);
#if DEBUG_VIDEO
				else if (special_pen != -1)
					drawgfx(bitmap, gfx, code, color, hflip, 0, sx, sy, clip, TRANSPARENCY_PENS, ~(1 << special_pen));
#endif
				else
					drawgfx(bitmap, gfx, code, color, hflip, 0, sx, sy, clip, TRANSPARENCY_PENS, transparency_mask[priority]);
			}
		}
	}
}


static void pf2_overrender_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const struct pf_overrender_data *overrender_data = (const struct pf_overrender_data *)param;

	struct osd_bitmap *bitmap = overrender_data->bitmap;
	const struct GfxElement *gfx = Machine->gfx[0];
	int mo_priority = overrender_data->mo_priority;
	int x, y;

	/* standard loop over tiles */
	for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
	{
		int sx = (8 * x - state->hscroll) & 0x1ff;
		if (sx >= XDIM) sx -= 0x200;

		for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		{
			int offs = x * 64 + y;
			int data2 = READ_WORD(&atarigen_playfieldram_color[offs * 2]);
			int priority = (data2 >> 12) & 3;
			
			if (priority > mo_priority)
			{
				int color = (data2 >> 8) & 0x0f;
				int data1 = READ_WORD(&atarigen_playfield2ram[offs * 2]);
				int code = data1 & 0x7fff;
				int hflip = data1 & 0x8000;

				int sy = (8 * y - state->vscroll) & 0x1ff;
				if (sy >= YDIM) sy -= 0x200;

				color += palette_bank >> 4;
			
				if (mo_priority == -1)
					drawgfx(bitmap, gfx, code, color, hflip, 0, sx, sy, clip, TRANSPARENCY_THROUGH, palette_transparent_pen);
#if DEBUG_VIDEO
				else if (special_pen != -1)
					drawgfx(bitmap, gfx, code, color, hflip, 0, sx, sy, clip, TRANSPARENCY_PENS, ~(1 << special_pen));
#endif
				else
					drawgfx(bitmap, gfx, code, color, hflip, 0, sx, sy, clip, TRANSPARENCY_PENS, transparency_mask[priority]);
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
	struct GfxElement *gfx = Machine->gfx[1];
	struct pf_overrender_data overrender_data;
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	struct rectangle pf_clip;

	/* extract data from the various words */
	int hflip = data[1] & 0x8000;
	int code = data[1] & 0x7fff;
	int xpos = (data[2] >> 7) - atarigen_video_control_state.sprite_xscroll;
	int priority = (data[2] >> 4) & 3;
	int special = (data[2] >> 6) & 1;
	int color = data[2] & 0x000f;
	int ypos = -(data[3] >> 7) - atarigen_video_control_state.sprite_yscroll;
	int hsize = ((data[3] >> 4) & 7) + 1;
	int vsize = (data[3] & 7) + 1;

	color += palette_bank >> 4;

	/* adjust for height */
	ypos -= vsize * 8;

	/* adjust the final coordinates */
	xpos &= 0x1ff;
	ypos &= 0x1ff;
	if (xpos >= XDIM) xpos -= 0x200;
	if (ypos >= YDIM) ypos -= 0x200;

	/* determine the bounding box */
	atarigen_mo_compute_clip_8x8(pf_clip, xpos, ypos, hsize, vsize, clip);
	
	/* special case? */
	if (special)
	{
		UINT32 temp = start_end[ypos & 0x1ff];
		UINT32 start = temp >> 16;
		UINT32 stop = temp & 0xffff;
		
		/* update the data */
		if (code == 2)
			start = xpos & 0x1ff;
		else if (code == 4)
			stop = xpos & 0x1ff;
		start_end[ypos & 0x1ff] = (start << 16) | stop;
		
		/* render if complete */
		if (start != 0xffff && stop != 0xffff)
		{
			struct rectangle temp_clip = *clip;
			int x;
			
			/* adjust coordinates */
			if (start >= XDIM) start -= 0x200;
			if (start > stop) stop += 0x200;

			/* set up a clipper */
			temp_clip.min_x = (start < temp_clip.min_x) ? temp_clip.min_x : start;
			temp_clip.max_x = (stop > temp_clip.max_x) ? temp_clip.max_x : stop;
			
			/* draw it */
			for (x = start; x < stop; x += 8)
			{
				atarigen_mo_draw_8x8(bitmap, gfx, 2, color, hflip, 0, x, ypos, 1, 1, (&temp_clip), TRANSPARENCY_PEN, 0);
			}
			start_end[ypos & 0x1ff] = 0xffffffff;
		}
	}

	/* simple case? */
	else if (priority == 3)
	{
		/* just draw -- we have dominion over all */
		atarigen_mo_draw_8x8(bitmap, gfx, code, color, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_PEN, 0);
	}

#if DEBUG_VIDEO
	else if (show_colors)
	{
		atarigen_mo_draw_8x8(bitmap, gfx, code, color, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_PEN, 0);
	}
#endif

	/* otherwise, it gets a smidge trickier */
	else
	{
		/* draw an instance of the object in all transparent pens */
		atarigen_mo_draw_transparent_8x8(bitmap, gfx, code, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_PEN, 0);

		/* and then draw it normally to the temp bitmap */
		atarigen_mo_draw_8x8(atarigen_pf_overrender_bitmap, gfx, code, color, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_NONE, 0);

		/* overrender the playfields on top of that */
		overrender_data.mo_priority = priority;
		overrender_data.bitmap = atarigen_pf_overrender_bitmap;
		atarigen_pf_process(pf_overrender_callback, &overrender_data, &pf_clip);
		atarigen_pf2_process(pf2_overrender_callback, &overrender_data, &pf_clip);

		/* special case the special pens */
		if (priority == 0 && (color & 0x0f) == 0)
		{
			/* draw an instance of the object's pen 1 in transparent pen */
			atarigen_mo_draw_transparent_8x8(atarigen_pf_overrender_bitmap, gfx, code, hflip, 0, xpos, ypos, hsize, vsize, clip, TRANSPARENCY_PENS, 0xfffd);
	
			/* overrender the playfields into those regions */
			/* note: the reversed order is intentional */
			overrender_data.mo_priority = -1;
			atarigen_pf2_process(pf2_overrender_callback, &overrender_data, &pf_clip);
			atarigen_pf_process(pf_overrender_callback, &overrender_data, &pf_clip);
		}

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
	
	new_show_colors = (keyboard_pressed(KEYCODE_LSHIFT)) ? 1 : keyboard_pressed(KEYCODE_RSHIFT) ? 2 : 0;
	if (new_show_colors)
		if (keyboard_pressed(KEYCODE_LCONTROL) || keyboard_pressed(KEYCODE_RCONTROL))
			new_show_colors = -new_show_colors;
	
	if (new_show_colors != show_colors)
	{
		show_colors = new_show_colors;
		memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 2);
		memset(atarigen_pf2_dirty, 0xff, atarigen_playfieldram_size / 2);
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

		fprintf(f, "\n\nAlpha Palette 2:\n");
		for (i = 0x400; i < 0x500; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&paletteram[i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
		}

		fprintf(f, "\n\nMotion Object Palette 2:\n");
		for (i = 0x500; i < 0x600; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&paletteram[i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
		}

		fprintf(f, "\n\nPlayfield Palette 2:\n");
		for (i = 0x600; i < 0x800; i++)
		{
			fprintf(f, "%04X ", READ_WORD(&paletteram[i*2]));
			if ((i & 15) == 15) fprintf(f, "\n");
		}

		fprintf(f, "\n\nMotion Objects\n");
		for (i = 0; i < 0x400; i++)
		{
			UINT16 *data = (UINT16 *)&atarigen_spriteram[i*8];
			int hflip = data[1] & 0x8000;
			int code = data[1] & 0x7fff;
			int xpos = (data[2] >> 7);
			int priority = (data[2] >> 4) & 7;
			int color = data[2] & 0x000f;
			int ypos = (data[3] >> 7);
			int hsize = ((data[3] >> 4) & 7) + 1;
			int vsize = (data[3] & 7) + 1;
			fprintf(f, "   Object %03X: L=%03X P=%04X C=%X X=%03X Y=%03X W=%d H=%d F=%d LEFT=(%04X %04X %04X %04X)\n",
					i, data[0] & 0x3ff, code, color, xpos & 0x1ff, ypos & 0x1ff, hsize, vsize, hflip,
					data[0] & 0xfc00, data[1] & 0x0000, data[2] & 0x0070, data[3] & 0x0000);
		}

		fprintf(f, "\n\nPlayfield 1 dump\n");
		for (i = 0; i < atarigen_playfieldram_size / 2; i++)
		{
			fprintf(f, "%X%04X ", READ_WORD(&atarigen_playfieldram_color[i*2]) & 0xff, READ_WORD(&atarigen_playfieldram[i*2]));
			if ((i & 63) == 63) fprintf(f, "\n");
		}

		fprintf(f, "\n\nPlayfield 2 dump\n");
		for (i = 0; i < atarigen_playfield2ram_size / 2; i++)
		{
			fprintf(f, "%X%04X ", (READ_WORD(&atarigen_playfieldram_color[i*2]) >> 8) & 0xff, READ_WORD(&atarigen_playfield2ram[i*2]));
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
}

#endif
