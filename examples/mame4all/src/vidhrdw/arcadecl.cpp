/***************************************************************************

	Atari Arcade Classics hardware (prototypes)

	Note: this video hardware has some similarities to Shuuz & company
	The sprite offset registers are stored to 3EFF80

****************************************************************************/


#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"

#define XCHARS 43
#define YCHARS 30

#define XDIM (XCHARS*8)
#define YDIM (YCHARS*8)



/*************************************
 *
 *	Statics
 *
 *************************************/

static int *color_usage;



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static const UINT8 *update_palette(void);

static void mo_color_callback(const UINT16 *data, const struct rectangle *clip, void *param);
static void mo_render_callback(const UINT16 *data, const struct rectangle *clip, void *param);



/*************************************
 *
 *	Video system start
 *
 *************************************/

int arcadecl_vh_start(void)
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

	/* allocate color usage */
	color_usage = (int*)malloc(sizeof(int) * 256);
	if (!color_usage)
		return 1;
	color_usage[0] = XDIM * YDIM;
	memset(atarigen_playfieldram, 0, 0x20000);

	/* initialize the playfield */
	if (atarigen_pf_init(&pf_desc))
	{
		free(color_usage);
		return 1;
	}

	/* initialize the motion objects */
	if (atarigen_mo_init(&mo_desc))
	{
		atarigen_pf_free();
		free(color_usage);
		return 1;
	}

	return 0;
}



/*************************************
 *
 *	Video system shutdown
 *
 *************************************/

void arcadecl_vh_stop(void)
{
	/* free data */
	if (color_usage)
		free(color_usage);
	color_usage = 0;

	atarigen_pf_free();
	atarigen_mo_free();
}



/*************************************
 *
 *	Playfield RAM write handler
 *
 *************************************/

WRITE_HANDLER( arcadecl_playfieldram_w )
{
	int oldword = READ_WORD(&atarigen_playfieldram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	int x, y;

	if (oldword != newword)
	{
		WRITE_WORD(&atarigen_playfieldram[offset], newword);

		/* track color usage */
		x = offset % 512;
		y = offset / 512;
		if (x < XDIM && y < YDIM)
		{
			color_usage[(oldword >> 8) & 0xff]--;
			color_usage[oldword & 0xff]--;
			color_usage[(newword >> 8) & 0xff]++;
			color_usage[newword & 0xff]++;
		}

		/* mark scanlines dirty */
		atarigen_pf_dirty[y] = 1;
	}
}



/*************************************
 *
 *	Periodic scanline updater
 *
 *************************************/

void arcadecl_scanline_update(int scanline)
{
	/* doesn't appear to use SLIPs */
	if (scanline < YDIM)
		atarigen_mo_update(atarigen_spriteram, 0, scanline);
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

void arcadecl_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	/* remap if necessary */
	if (update_palette())
		memset(atarigen_pf_dirty, 1, YDIM);

	/* update the cached bitmap */
	{
		int x, y;

		for (y = 0; y < YDIM; y++)
			if (atarigen_pf_dirty[y])
			{
				int xx = 0;
				const UINT8 *src = &atarigen_playfieldram[512 * y];

				/* regenerate the line */
				for (x = 0; x < XDIM/2; x++)
				{
					int bits = READ_WORD(src);
					src += 2;
					plot_pixel(atarigen_pf_bitmap, xx++, y, Machine->pens[bits >> 8]);
					plot_pixel(atarigen_pf_bitmap, xx++, y, Machine->pens[bits & 0xff]);
				}
				atarigen_pf_dirty[y] = 0;
			}
	}

	/* copy the cached bitmap */
	copybitmap(bitmap, atarigen_pf_bitmap, 0, 0, 0, 0, NULL, TRANSPARENCY_NONE, 0);

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
	UINT16 mo_map[16];
	int i, j;

	/* reset color tracking */
	memset(mo_map, 0, sizeof(mo_map));
	palette_init_used_colors();

	/* update color usage for the mo's */
	atarigen_mo_process(mo_color_callback, mo_map);

	/* rebuild the playfield palette */
	for (i = 0; i < 256; i++)
		if (color_usage[i])
			palette_used_colors[0x000 + i] = PALETTE_COLOR_USED;

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

	return palette_recalc();
}



/*************************************
 *
 *	Motion object palette
 *
 *************************************/

static void mo_color_callback(const UINT16 *data, const struct rectangle *clip, void *param)
{
	const UINT32 *usage = Machine->gfx[0]->pen_usage;
	UINT16 *colormap = (UINT16*)param;
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
	const struct GfxElement *gfx = Machine->gfx[0];
	struct osd_bitmap *bitmap = (struct osd_bitmap *)param;
	struct rectangle pf_clip;

	/* extract data from the various words */
	int hflip = data[1] & 0x8000;
	int code = data[1] & 0x7fff;
	int xpos = (data[2] >> 7) + 4;
	int color = data[2] & 0x000f;
/*	int priority = (data[2] >> 3) & 1;*/
	int ypos = YDIM - (data[3] >> 7);
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
}
