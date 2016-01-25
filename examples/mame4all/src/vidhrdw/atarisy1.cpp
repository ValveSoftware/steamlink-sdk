/***************************************************************************

	Atari System 1 hardware

****************************************************************************/


#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"

#define XCHARS 42
#define YCHARS 30

#define XDIM (XCHARS*8)
#define YDIM (YCHARS*8)



/*************************************
 *
 *	Constants
 *
 *************************************/

/* the color and remap PROMs are mapped as follows */
#define PROM1_BANK_4			0x80		/* active low */
#define PROM1_BANK_3			0x40		/* active low */
#define PROM1_BANK_2			0x20		/* active low */
#define PROM1_BANK_1			0x10		/* active low */
#define PROM1_OFFSET_MASK		0x0f		/* postive logic */

#define PROM2_BANK_6_OR_7		0x80		/* active low */
#define PROM2_BANK_5			0x40		/* active low */
#define PROM2_PLANE_5_ENABLE	0x20		/* active high */
#define PROM2_PLANE_4_ENABLE	0x10		/* active high */
#define PROM2_PF_COLOR_MASK		0x0f		/* negative logic */
#define PROM2_BANK_7			0x08		/* active low, plus PROM2_BANK_6_OR_7 low as well */
#define PROM2_MO_COLOR_MASK		0x07		/* negative logic */

#define OVERRENDER_PRIORITY		1
#define OVERRENDER_SPECIAL		2



/*************************************
 *
 *	Macros
 *
 *************************************/

/* these macros make accessing the indirection table easier, plus this is how the data
   is stored for the pfmapped array */
#define PACK_LOOKUP_DATA(bank,color,offset,bpp) \
		(((((bpp) - 4) & 7) << 24) | \
		 (((color) & 255) << 16) | \
		 (((bank) & 15) << 12) | \
		 (((offset) & 15) << 8))

#define LOOKUP_BPP(data)			(((data) >> 24) & 7)
#define LOOKUP_COLOR(data) 			(((data) >> 16) & 0xff)
#define LOOKUP_GFX(data) 			(((data) >> 12) & 15)
#define LOOKUP_CODE(data) 			((data) & 0x0fff)



/*************************************
 *
 *	Structures
 *
 *************************************/

struct pf_overrender_data
{
	struct osd_bitmap *bitmap;
	int type;
};



/*************************************
 *
 *	Globals we own
 *
 *************************************/

UINT8 *atarisys1_bankselect;
UINT8 *atarisys1_prioritycolor;



/*************************************
 *
 *	Statics
 *
 *************************************/

/* playfield parameters */
static struct atarigen_pf_state pf_state;
static UINT16 priority_pens;

/* indirection tables */
static UINT32 pf_lookup[256];
static UINT32 mo_lookup[256];

/* INT3 tracking */
static void *int3_timer[YDIM];
static void *int3off_timer;

/* graphics bank tracking */
static UINT8 bank_gfx[3][8];
static unsigned int *pen_usage[MAX_GFX_ELEMENTS];

/* basic form of a graphics bank */
static struct GfxLayout objlayout =
{
	8,8,	/* 8*8 sprites */
	4096,	/* 4096 of them */
	6,		/* 6 bits per pixel */
	{ 5*8*0x08000, 4*8*0x08000, 3*8*0x08000, 2*8*0x08000, 1*8*0x08000, 0*8*0x08000 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8		/* every sprite takes 8 consecutive bytes */
};



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

static int decode_gfx(void);
static int get_bank(UINT8 prom1, UINT8 prom2, int bpp);

void atarisys1_scanline_update(int scanline);



/*************************************
 *
 *	Generic video system start
 *
 *************************************/

int atarisys1_vh_start(void)
{
	static struct atarigen_mo_desc mo_desc =
	{
		64,                  /* maximum number of MO's */
		2,                   /* number of bytes per MO entry */
		0x80,                /* number of bytes between MO words */
		1,                   /* ignore an entry if this word == 0xffff */
		3, 0, 0x3f,          /* link = (data[linkword] >> linkshift) & linkmask */
		0                    /* render in reverse link order */
	};

	static struct atarigen_pf_desc pf_desc =
	{
		8, 8,				/* width/height of each tile */
		64, 64				/* number of tiles in each direction */
	};

	int i, e;

	/* first decode the graphics */
	if (decode_gfx())
		return 1;

	/* reset the statics */
	memset(&pf_state, 0, sizeof(pf_state));
	memset(int3_timer, 0, sizeof(int3_timer));

	/* initialize the pen usage array */
	for (e = 0; e < MAX_GFX_ELEMENTS; e++)
		if (Machine->gfx[e])
		{
			pen_usage[e] = Machine->gfx[e]->pen_usage;

			/* if this element has 6bpp data, create a special new usage array for it */
			if (Machine->gfx[e]->color_granularity == 64)
			{
				const struct GfxElement *gfx = Machine->gfx[e];

				/* allocate storage */
				pen_usage[e] = (unsigned int*)malloc(gfx->total_elements * 2 * sizeof(int));
				if (pen_usage[e])
				{
					unsigned int *entry;
					int x, y;

					/* scan each entry, marking which pens are used */
					memset(pen_usage[e], 0, gfx->total_elements * 2 * sizeof(int));
					for (i = 0, entry = pen_usage[e]; i < gfx->total_elements; i++, entry += 2)
					{
						UINT8 *dp = gfx->gfxdata + i * gfx->char_modulo;
						for (y = 0; y < gfx->height; y++)
						{
							for (x = 0; x < gfx->width; x++)
							{
								int color = dp[x];
								entry[(color >> 5) & 1] |= 1 << (color & 31);
							}
							dp += gfx->line_modulo;
						}
					}
				}
			}
		}

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

void atarisys1_vh_stop(void)
{
	int i;

	/* free any extra pen usage */
	for (i = 0; i < MAX_GFX_ELEMENTS; i++)
	{
		if (pen_usage[i] && Machine->gfx[i] && pen_usage[i] != Machine->gfx[i]->pen_usage)
			free(pen_usage[i]);
		pen_usage[i] = 0;
	}

	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Graphics bank selection
 *
 *************************************/

WRITE_HANDLER( atarisys1_bankselect_w )
{
	int oldword = READ_WORD(&atarisys1_bankselect[offset]);
	int newword = COMBINE_WORD(oldword, data);
	int scanline = cpu_getscanline();
	int diff = oldword ^ newword;

	/* update memory */
	WRITE_WORD(&atarisys1_bankselect[offset], newword);

	/* sound CPU reset */
	if (diff & 0x0080)
	{
		cpu_set_reset_line(1, (newword & 0x0080) ? CLEAR_LINE : ASSERT_LINE);
		if (!(newword & 0x0080)) atarigen_sound_reset();
	}

	/* motion object bank select */
	atarisys1_scanline_update(scanline);

	/* playfield bank select */
	if (diff & 0x04)
	{
		pf_state.param[0] = (newword & 0x04) ? 0x80 : 0x00;
		atarigen_pf_update(&pf_state, cpu_getscanline() + 1);
	}
}



/*************************************
 *
 *	Playfield horizontal scroll
 *
 *************************************/

WRITE_HANDLER( atarisys1_hscroll_w )
{
	int oldword = READ_WORD(&atarigen_hscroll[offset]);
	int newword = COMBINE_WORD(oldword, data);
	WRITE_WORD(&atarigen_hscroll[offset], newword);

	/* set the new scroll value and update the playfield status */
	pf_state.hscroll = newword & 0x1ff;
	atarigen_pf_update(&pf_state, cpu_getscanline() + 1);
}



/*************************************
 *
 *	Playfield vertical scroll
 *
 *************************************/

WRITE_HANDLER( atarisys1_vscroll_w )
{
	int scanline = cpu_getscanline() + 1;

	int oldword = READ_WORD(&atarigen_vscroll[offset]);
	int newword = COMBINE_WORD(oldword, data);
	WRITE_WORD(&atarigen_vscroll[offset], newword);

	/* because this latches a new value into the scroll base,
	   we need to adjust for the scanline */
	pf_state.vscroll = newword & 0x1ff;
	if (scanline < YDIM) pf_state.vscroll -= scanline;
	atarigen_pf_update(&pf_state, scanline);
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( atarisys1_playfieldram_w )
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
 *	Sprite RAM write handler
 *
 *************************************/

WRITE_HANDLER( atarisys1_spriteram_w )
{
	int oldword = READ_WORD(&atarigen_spriteram[offset]);
	int newword = COMBINE_WORD(oldword, data);

	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_spriteram[offset], newword);

		/* if modifying a timer, beware */
		if (((offset & 0x180) == 0x000 && READ_WORD(&atarigen_spriteram[offset | 0x080]) == 0xffff) ||
		    ((offset & 0x180) == 0x080 && newword == 0xffff))
		{
			/* if the timer is in the active bank, update the display list */
			if ((offset >> 9) == ((READ_WORD(&atarisys1_bankselect[0]) >> 3) & 7))
			{
				//logerror("Caught timer mod!\n");
				atarisys1_scanline_update(cpu_getscanline());
			}
		}
	}
}



/*************************************
 *
 *	MO interrupt handlers
 *
 *************************************/

static void int3off_callback(int param)
{
	/* clear the state */
	atarigen_scanline_int_ack_w(0, 0);

	/* make this timer go away */
	int3off_timer = 0;
}


void atarisys1_int3_callback(int param)
{
	/* update the state */
	atarigen_scanline_int_gen();

	/* set a timer to turn it off */
	if (int3off_timer)
		timer_remove(int3off_timer);
	int3off_timer = timer_set(cpu_getscanlineperiod(), 0, int3off_callback);

	/* set ourselves up to go off next frame */
	int3_timer[param] = timer_set(TIME_IN_HZ(Machine->drv->frames_per_second), param, atarisys1_int3_callback);
}



/*************************************
 *
 *	MO interrupt state read
 *
 *************************************/

READ_HANDLER( atarisys1_int3state_r )
{
	return atarigen_scanline_int_state ? 0x0080 : 0x0000;
}



/*************************************
 *
 *	Periodic MO updater
 *
 *************************************/

void atarisys1_scanline_update(int scanline)
{
	int bank = ((READ_WORD(&atarisys1_bankselect[0]) >> 3) & 7) * 0x200;
	UINT8 *base = &atarigen_spriteram[bank];
	UINT8 spritevisit[64];
	UINT8 timer[YDIM];
	int link = 0;
	int i;

	/* only process if we're still onscreen */
	if (scanline < YDIM)
	{
		/* generic update first */
		if (!scanline)
			atarigen_mo_update(base, 0, 0);
		else
			atarigen_mo_update(base, 0, scanline + 1);
	}

	/* visit all the sprites and look for timers */
	memset(spritevisit, 0, sizeof(spritevisit));
	memset(timer, 0, sizeof(timer));
	while (!spritevisit[link])
	{
		int data2 = READ_WORD(&base[link * 2 + 0x080]);

		/* a codeure of 0xffff is really an interrupt - gross! */
		if (data2 == 0xffff)
		{
			int data1 = READ_WORD(&base[link * 2 + 0x000]);
			int vsize = (data1 & 15) + 1;
			int ypos = (256 - (data1 >> 5) - vsize * 8) & 0x1ff;

			/* only generate timers on visible scanlines */
			if (ypos < YDIM)
				timer[ypos] = 1;
		}

		/* link to the next object */
		spritevisit[link] = 1;
		link = READ_WORD(&atarigen_spriteram[bank + link * 2 + 0x180]) & 0x3f;
	}

	/* update our interrupt timers */
	for (i = 0; i < YDIM; i++)
	{
		if (timer[i] && !int3_timer[i])
			int3_timer[i] = timer_set(cpu_getscanlinetime(i), i, atarisys1_int3_callback);
		else if (!timer[i] && int3_timer[i])
		{
			timer_remove(int3_timer[i]);
			int3_timer[i] = 0;
		}
	}
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void atarisys1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;

	/* update the palette */
	if (update_palette())
		memset(atarigen_pf_dirty, 0xff, atarigen_playfieldram_size / 2);

	/* set up the all-transparent overrender palette */
	for (i = 0; i < 16; i++)
		atarigen_overrender_colortable[i] = palette_transparent_pen;

	/* render the playfield */
	memset(atarigen_pf_visit, 0, 64*64);
	atarigen_pf_process(pf_render_callback, bitmap, &Machine->visible_area);

	/* render the motion objects */
	priority_pens = READ_WORD(&atarisys1_prioritycolor[0]) & 0xff;
	atarigen_mo_process(mo_render_callback, bitmap);

	/* redraw the alpha layer completely */
	{
		const struct GfxElement *gfx = Machine->gfx[0];
		int sx, sy, offs;

		for (sy = 0; sy < YCHARS; sy++)
			for (sx = 0, offs = sy*64; sx < XCHARS; sx++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int opaque = data & 0x2000;
				int code = data & 0x3ff;

				if (code || opaque)
				{
					int color = (data >> 10) & 7;
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
	UINT16 al_map[8], pfmo_map[32];
	int i, j;

	/* reset color tracking */
	memset(pfmo_map, 0, sizeof(pfmo_map));
	memset(al_map, 0, sizeof(al_map));
	palette_init_used_colors();

	/* always remap the transluscent colors */
	memset(&palette_used_colors[0x300], PALETTE_COLOR_USED, 16);

	/* update color usage for the playfield */
	atarigen_pf_process(pf_color_callback, pfmo_map, &Machine->visible_area);

	/* update color usage for the mo's */
	atarigen_mo_process(mo_color_callback, pfmo_map);

	/* update color usage for the alphanumerics */
	{
		const unsigned int *usage = Machine->gfx[0]->pen_usage;
		int sx, sy, offs;

		for (sy = 0; sy < YCHARS; sy++)
			for (sx = 0, offs = sy * 64; sx < XCHARS; sx++, offs++)
			{
				int data = READ_WORD(&atarigen_alpharam[offs * 2]);
				int color = (data >> 10) & 7;
				int code = data & 0x3ff;
				al_map[color] |= usage[code];
			}
	}

	/* determine the final playfield palette */
	for (i = 16; i < 32; i++)
	{
		UINT16 used = pfmo_map[i];
		if (used)
			for (j = 0; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x100 + i * 16 + j] = PALETTE_COLOR_USED;
	}

	/* determine the final motion object palette */
	for (i = 0; i < 16; i++)
	{
		UINT16 used = pfmo_map[i];
		if (used)
		{
			palette_used_colors[0x100 + i * 16] = PALETTE_COLOR_TRANSPARENT;
			for (j = 1; j < 16; j++)
				if (used & (1 << j))
					palette_used_colors[0x100 + i * 16 + j] = PALETTE_COLOR_USED;
		}
	}

	/* determine the final alpha palette */
	for (i = 0; i < 8; i++)
	{
		UINT16 used = al_map[i];
		if (used)
			for (j = 0; j < 4; j++)
				if (used & (1 << j))
					palette_used_colors[0x000 + i * 4 + j] = PALETTE_COLOR_USED;
	}

	/* recalculate the palette */
	return palette_recalc();
}



/*************************************
 *
 *	Playfield palette
 *
 *************************************/

static void pf_color_callback(const struct rectangle *clip, const struct rectangle *tiles, const struct atarigen_pf_state *state, void *param)
{
	const UINT32 *lookup_table = &pf_lookup[state->param[0]];
	UINT16 *colormap = (UINT16*)param;
	int x, y;

	/* standard loop over tiles */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = y * 64 + x;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int lookup = lookup_table[(data >> 8) & 0x7f];
			const unsigned int *usage = pen_usage[LOOKUP_GFX(lookup)];
			int bpp = LOOKUP_BPP(lookup);
			int code = LOOKUP_CODE(lookup) | (data & 0xff);
			int color = LOOKUP_COLOR(lookup);
			unsigned int bits;

			/* based on the depth, we need to tweak our pen mapping */
			if (bpp == 0)
				colormap[color] |= usage[code];
			else if (bpp == 1)
			{
				bits = usage[code];
				colormap[color * 2] |= bits;
				colormap[color * 2 + 1] |= bits >> 16;
			}
			else
			{
				bits = usage[code * 2];
				colormap[color * 4] |= bits;
				colormap[color * 4 + 1] |= bits >> 16;
				bits = usage[code * 2 + 1];
				colormap[color * 4 + 2] |= bits;
				colormap[color * 4 + 3] |= bits >> 16;
			}

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
	int bank = state->param[0];
	const UINT32 *lookup_table = &pf_lookup[bank];
	struct osd_bitmap *bitmap = (struct osd_bitmap*)param;
	int x, y;

	/* standard loop over tiles */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = y * 64 + x;

			/* update only if dirty */
			if (atarigen_pf_dirty[offs] != bank)
			{
				int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
				int lookup = lookup_table[(data >> 8) & 0x7f];
				const struct GfxElement *gfx = Machine->gfx[LOOKUP_GFX(lookup)];
				int code = LOOKUP_CODE(lookup) | (data & 0xff);
				int color = LOOKUP_COLOR(lookup);
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
	const UINT32 *lookup_table = &pf_lookup[state->param[0]];
	const struct pf_overrender_data *overrender_data = (const struct pf_overrender_data *)param;
	struct osd_bitmap *bitmap = overrender_data->bitmap;
	int type = overrender_data->type;
	int x, y;

	/* standard loop over tiles */
	for (y = tiles->min_y; y != tiles->max_y; y = (y + 1) & 63)
	{
		int sy = (8 * y - state->vscroll) & 0x1ff;
		if (sy >= YDIM) sy -= 0x200;

		for (x = tiles->min_x; x != tiles->max_x; x = (x + 1) & 63)
		{
			int offs = y * 64 + x;
			int data = READ_WORD(&atarigen_playfieldram[offs * 2]);
			int lookup = lookup_table[(data >> 8) & 0x7f];
			const struct GfxElement *gfx = Machine->gfx[LOOKUP_GFX(lookup)];
			int code = LOOKUP_CODE(lookup) | (data & 0xff);
			int color = LOOKUP_COLOR(lookup);
			int hflip = data & 0x8000;

			int sx = (8 * x - state->hscroll) & 0x1ff;
			if (sx >= XDIM) sx -= 0x200;

			/* overrender based on the type */
			if (type == OVERRENDER_PRIORITY)
			{
				int bpp = LOOKUP_BPP(lookup);
				if (color == (16 >> bpp))
					drawgfx(bitmap, gfx, code, color, hflip, 0, sx, sy, clip, TRANSPARENCY_PENS, ~priority_pens);
			}
			else
				drawgfx(bitmap, gfx, code, color, hflip, 0, sx, sy, clip, TRANSPARENCY_PEN, 0);
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
	UINT16 *colormap = (UINT16*)param;
	UINT16 temp = 0;
	int i;

	UINT32 lookup = mo_lookup[data[1] >> 8];
	const unsigned int *usage = pen_usage[LOOKUP_GFX(lookup)];
	int code = LOOKUP_CODE(lookup) | (data[1] & 0xff);
	int color = LOOKUP_COLOR(lookup);
	int vsize = (data[0] & 0x000f) + 1;
	int bpp = LOOKUP_BPP(lookup);

	if (bpp == 0)
	{
		for (i = 0; i < vsize; i++)
			temp |= usage[code++];
		colormap[color] |= temp;
	}

	/* in theory we should support all 3 possible depths, but motion objects are all 4bpp */
}



/*************************************
 *
 *	Motion object rendering
 *
 *************************************/

static void mo_render_callback(const UINT16 *data, const struct rectangle *clip, void *param)
{
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	struct pf_overrender_data overrender_data;
	struct rectangle pf_clip;

	/* extract data from the various words */
	UINT32 lookup = mo_lookup[data[1] >> 8];
	struct GfxElement *gfx = Machine->gfx[LOOKUP_GFX(lookup)];
	int code = LOOKUP_CODE(lookup) | (data[1] & 0xff);
	int color = LOOKUP_COLOR(lookup);
	int xpos = data[2] >> 5;
	int ypos = 256 - (data[0] >> 5);
	int hflip = data[0] & 0x8000;
	int vsize = (data[0] & 0x000f) + 1;
	int priority = data[2] >> 15;

	/* adjust for height */
	ypos -= vsize * 8;

	/* adjust the final coordinates */
	xpos &= 0x1ff;
	ypos &= 0x1ff;
	if (xpos >= XDIM) xpos -= 0x200;
	if (ypos >= YDIM) ypos -= 0x200;

	/* bail if X coordinate is out of range */
	if (xpos <= -8 || xpos >= XDIM)
		return;

	/* determine the bounding box */
	atarigen_mo_compute_clip_8x8(pf_clip, xpos, ypos, 1, vsize, clip);

	/* standard priority case? */
	if (!priority)
	{
		/* draw the motion object */
		atarigen_mo_draw_8x8_strip(bitmap, gfx, code, color, hflip, 0, xpos, ypos, vsize, clip, TRANSPARENCY_PEN, 0);

		/* do we have a priority color active? */
		if (priority_pens)
		{
			overrender_data.bitmap = bitmap;
			overrender_data.type = OVERRENDER_PRIORITY;

			/* overrender the playfield */
			atarigen_pf_process(pf_overrender_callback, &overrender_data, &pf_clip);
		}
	}

	/* high priority case? */
	else
	{
		/* draw the sprite in bright pink on the real bitmap */
		atarigen_mo_draw_transparent_8x8_strip(bitmap, gfx, code, hflip, 0, xpos, ypos, vsize, clip, TRANSPARENCY_PEN, 0);

		/* also draw the sprite normally on the temp bitmap */
		atarigen_mo_draw_8x8_strip(atarigen_pf_overrender_bitmap, gfx, code, 0x20, hflip, 0, xpos, ypos, vsize, clip, TRANSPARENCY_NONE, 0);

		/* now redraw the playfield tiles over top of the sprite */
		overrender_data.bitmap = atarigen_pf_overrender_bitmap;
		overrender_data.type = OVERRENDER_SPECIAL;
		atarigen_pf_process(pf_overrender_callback, &overrender_data, &pf_clip);

		/* finally, copy this chunk to the real bitmap */
		copybitmap(bitmap, atarigen_pf_overrender_bitmap, 0, 0, 0, 0, &pf_clip, TRANSPARENCY_THROUGH, palette_transparent_pen);
	}
}



/*************************************
 *
 *	Graphics decoding
 *
 *************************************/

static int decode_gfx(void)
{
	UINT8 *prom1 = &memory_region(REGION_PROMS)[0x000];
	UINT8 *prom2 = &memory_region(REGION_PROMS)[0x200];
	int obj, i;

	/* reset the globals */
	memset(&bank_gfx, 0, sizeof(bank_gfx));

	/* loop for two sets of objects */
	for (obj = 0; obj < 2; obj++)
	{
		UINT32 *table = (obj == 0) ? pf_lookup : mo_lookup;

		/* loop for 256 objects in the set */
		for (i = 0; i < 256; i++, prom1++, prom2++)
		{
			int bank, bpp, color, offset;

			/* determine the bpp */
			bpp = 4;
			if (*prom2 & PROM2_PLANE_4_ENABLE)
			{
				bpp = 5;
				if (*prom2 & PROM2_PLANE_5_ENABLE)
					bpp = 6;
			}

			/* determine the color */
			if (obj == 0)
				color = (16 + (~*prom2 & PROM2_PF_COLOR_MASK)) >> (bpp - 4); /* playfield */
			else
				color = (~*prom2 & PROM2_MO_COLOR_MASK) >> (bpp - 4);	/* motion objects (high bit ignored) */

			/* determine the offset */
			offset = *prom1 & PROM1_OFFSET_MASK;

			/* determine the bank */
			bank = get_bank(*prom1, *prom2, bpp);
			if (bank < 0)
				return 1;

			/* set the value */
			if (bank == 0)
				*table++ = 0;
			else
				*table++ = PACK_LOOKUP_DATA(bank, color, offset, bpp);
		}
	}
	return 0;
}



/*************************************
 *
 *	Graphics bank mapping
 *
 *************************************/

static int get_bank(UINT8 prom1, UINT8 prom2, int bpp)
{
	int bank_offset[8] = { 0, 0x00000, 0x30000, 0x60000, 0x90000, 0xc0000, 0xe0000, 0x100000 };
	int bank_index, i, gfx_index;

	/* determine the bank index */
	if ((prom1 & PROM1_BANK_1) == 0)
		bank_index = 1;
	else if ((prom1 & PROM1_BANK_2) == 0)
		bank_index = 2;
	else if ((prom1 & PROM1_BANK_3) == 0)
		bank_index = 3;
	else if ((prom1 & PROM1_BANK_4) == 0)
		bank_index = 4;
	else if ((prom2 & PROM2_BANK_5) == 0)
		bank_index = 5;
	else if ((prom2 & PROM2_BANK_6_OR_7) == 0)
	{
		if ((prom2 & PROM2_BANK_7) == 0)
			bank_index = 7;
		else
			bank_index = 6;
	}
	else
		return 0;

	/* find the bank */
	if (bank_gfx[bpp - 4][bank_index])
		return bank_gfx[bpp - 4][bank_index];

	/* if the bank is out of range, call it 0 */
	if (bank_offset[bank_index] >= memory_region_length(REGION_GFX2))
		return 0;

	/* don't have one? let's make it ... first find any empty slot */
	for (gfx_index = 0; gfx_index < MAX_GFX_ELEMENTS; gfx_index++)
		if (Machine->gfx[gfx_index] == NULL)
			break;
	if (gfx_index == MAX_GFX_ELEMENTS)
		return -1;

	/* tweak the structure for the number of bitplanes we have */
	objlayout.planes = bpp;
	for (i = 0; i < bpp; i++)
		objlayout.planeoffset[i] = (bpp - i - 1) * 0x8000 * 8;

	/* decode the graphics */
	Machine->gfx[gfx_index] = decodegfx(&memory_region(REGION_GFX2)[bank_offset[bank_index]], &objlayout);
	if (!Machine->gfx[gfx_index])
		return -1;

	/* set the color information */
	Machine->gfx[gfx_index]->colortable = &Machine->remapped_colortable[256];
	Machine->gfx[gfx_index]->total_colors = 48 >> (bpp - 4);

	/* set the entry and return it */
	return bank_gfx[bpp - 4][bank_index] = gfx_index;
}
