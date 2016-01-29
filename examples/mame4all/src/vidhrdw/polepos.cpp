#ifndef ROAD_CORE_INCLUDE

#include "driver.h"
#include "vidhrdw/generic.h"

UINT8 *polepos_view_memory;
UINT8 *polepos_road_memory;
UINT8 *polepos_sprite_memory;
UINT8 *polepos_alpha_memory;

/* modified vertical position built from three nibbles (12 bit)
 * of ROMs 136014-142, 136014-143, 136014-144
 * The value RVP (road vertical position, lower 12 bits) is added
 * to this value and the upper 10 bits of the result are used to
 * address the playfield video memory (AB0 - AB9).
 */
static UINT16 polepos_vertical_position_modifier[256];

static UINT16 view_hscroll;
static UINT16 road_vscroll;

static const UINT8 *road_control;
static const UINT8 *road_bits1;
static const UINT8 *road_bits2;

static struct osd_bitmap *view_bitmap;
static UINT8 *view_dirty;


static void draw_road_core_8(struct osd_bitmap *bitmap, int sx, int sy, int dx, int dy);
static void draw_road_core_16(struct osd_bitmap *bitmap, int sx, int sy, int dx, int dy);


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Pole Position has three 256x4 palette PROMs (one per gun)
  and a lot ;-) of 256x4 lookup table PROMs.
  The palette PROMs are connected to the RGB output this way:

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
		-- 470 ohm resistor  -- RED/GREEN/BLUE
		-- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/

void polepos_vh_convert_color_prom(UINT8 *palette, UINT16 *colortable, const UINT8 *color_prom)
{
	int i, j;

	/*******************************************************
	 * Color PROMs
	 * Sheet 15B: middle, 136014-137,138,139
	 * Inputs: MUX0 ... MUX3, ALPHA/BACK, SPRITE/BACK, 128V, COMPBLANK
	 *
	 * Note that we only decode the lower 128 colors because
	 * the upper 128 are all black and used during the
	 * horizontal and vertical blanking periods.
	 *******************************************************/
	for (i = 0; i < 128; i++)
	{
		int bit0,bit1,bit2,bit3;

		/* Sheet 15B: 136014-0137 red component */
		bit0 = (color_prom[0x000 + i] >> 0) & 1;
		bit1 = (color_prom[0x000 + i] >> 1) & 1;
		bit2 = (color_prom[0x000 + i] >> 2) & 1;
		bit3 = (color_prom[0x000 + i] >> 3) & 1;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* Sheet 15B: 136014-0138 green component */
		bit0 = (color_prom[0x100 + i] >> 0) & 1;
		bit1 = (color_prom[0x100 + i] >> 1) & 1;
		bit2 = (color_prom[0x100 + i] >> 2) & 1;
		bit3 = (color_prom[0x100 + i] >> 3) & 1;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* Sheet 15B: 136014-0139 blue component */
		bit0 = (color_prom[0x200 + i] >> 0) & 1;
		bit1 = (color_prom[0x200 + i] >> 1) & 1;
		bit2 = (color_prom[0x200 + i] >> 2) & 1;
		bit3 = (color_prom[0x200 + i] >> 3) & 1;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
	}

	/*******************************************************
	 * Alpha colors (colors 0x000-0x1ff)
	 * Sheet 15B: top left, 136014-140
	 * Inputs: SHFT0, SHFT1 and CHA8* ... CHA13*
	 *******************************************************/
	for (i = 0; i < 64*4; i++)
	{
		int color = color_prom[0x300 + i];
		colortable[0x0000 + i] = (color != 15) ? (0x020 + color) : 0;
		colortable[0x0100 + i] = (color != 15) ? (0x060 + color) : 0;
	}

	/*******************************************************
	 * View colors (colors 0x200-0x3ff)
	 * Sheet 13A: left, 136014-141
	 * Inputs: SHFT2, SHFT3 and CHA8 ... CHA13
	 *******************************************************/
	for (i = 0; i < 64*4; i++)
	{
		int color = color_prom[0x400 + i];
		colortable[0x0200 + i] = 0x000 + color;
		colortable[0x0300 + i] = 0x040 + color;
	}

	/*******************************************************
	 * Sprite colors (colors 0x400-0xbff)
	 * Sheet 14B: right, 136014-146
	 * Inputs: CUSTOM0 ... CUSTOM3 and DATA0 ... DATA5
	 *******************************************************/
	for (i = 0; i < 64*16; i++)
	{
		int color = color_prom[0xc00 + i];
		colortable[0x0400 + i] = (color != 15) ? (0x010 + color) : 0;
		colortable[0x0800 + i] = (color != 15) ? (0x050 + color) : 0;
	}

	/*******************************************************
	 * Road colors (colors 0xc00-0x13ff)
	 * Sheet 13A: bottom left, 136014-145
	 * Inputs: R1 ... R6 and CHA0 ... CHA3
	 *******************************************************/
	for (i = 0; i < 64*16; i++)
	{
		int color = color_prom[0x800 + i];
		colortable[0x0c00 + i] = 0x000 + color;
		colortable[0x1000 + i] = 0x040 + color;
	}

	/* 136014-142, 136014-143, 136014-144 Vertical position modifiers */
	for (i = 0; i < 256; i++)
	{
		j = color_prom[0x500 + i] + (color_prom[0x600 + i] << 4) + (color_prom[0x700 + i] << 8);
		polepos_vertical_position_modifier[i] = j;
	}

	road_control = &color_prom[0x2000];
	road_bits1 = &color_prom[0x4000];
	road_bits2 = &color_prom[0x6000];
}


/***************************************************************************

  Video initialization/shutdown

***************************************************************************/

int polepos_vh_start(void)
{
	/* allocate view bitmap */
	view_bitmap = bitmap_alloc(64*8, 16*8);
	if (!view_bitmap)
		return 1;

	/* allocate view dirty buffer */
	view_dirty = (UINT8*)malloc(64*16);
	if (!view_dirty)
	{
		bitmap_free(view_bitmap);
		return 1;
	}

	return 0;
}

void polepos_vh_stop(void)
{
	bitmap_free(view_bitmap);
	free(view_dirty);
}


/***************************************************************************

  Sprite memory

***************************************************************************/

READ_HANDLER( polepos_sprite_r )
{
	return READ_WORD(&polepos_sprite_memory[offset]);
}

WRITE_HANDLER( polepos_sprite_w )
{
	COMBINE_WORD_MEM(&polepos_sprite_memory[offset], data);
}

READ_HANDLER( polepos_z80_sprite_r )
{
	return polepos_sprite_r(offset << 1) & 0xff;
}

WRITE_HANDLER( polepos_z80_sprite_w )
{
	polepos_sprite_w(offset << 1, data | 0xff000000);
}


/***************************************************************************

  Road memory

***************************************************************************/

READ_HANDLER( polepos_road_r )
{
	return READ_WORD(&polepos_road_memory[offset]);
}

WRITE_HANDLER( polepos_road_w )
{
	COMBINE_WORD_MEM(&polepos_road_memory[offset], data);
}

READ_HANDLER( polepos_z80_road_r )
{
	return polepos_road_r(offset << 1) & 0xff;
}

WRITE_HANDLER( polepos_z80_road_w )
{
	polepos_road_w(offset << 1, data | 0xff000000);
}

WRITE_HANDLER( polepos_road_vscroll_w )
{
	road_vscroll = data;
}


/***************************************************************************

  View memory

***************************************************************************/

READ_HANDLER( polepos_view_r )
{
	return READ_WORD(&polepos_view_memory[offset]);
}

WRITE_HANDLER( polepos_view_w )
{
	int oldword = READ_WORD(&polepos_view_memory[offset]);
	int newword = COMBINE_WORD(oldword, data);
	if (oldword != newword)
	{
		WRITE_WORD(&polepos_view_memory[offset], newword);
		if (offset < 0x800)
			view_dirty[offset / 2] = 1;
	}
}

READ_HANDLER( polepos_z80_view_r )
{
	return polepos_view_r(offset << 1) & 0xff;
}

WRITE_HANDLER( polepos_z80_view_w )
{
	polepos_view_w(offset << 1, data | 0xff000000);
}

WRITE_HANDLER( polepos_view_hscroll_w )
{
	view_hscroll = data;
}


/***************************************************************************

  Alpha memory

***************************************************************************/

READ_HANDLER( polepos_alpha_r )
{
	return READ_WORD(&polepos_alpha_memory[offset]);
}

WRITE_HANDLER( polepos_alpha_w )
{
	int oldword = READ_WORD(&polepos_alpha_memory[offset]);
	int newword = COMBINE_WORD(oldword, data);
	if (oldword != newword)
		WRITE_WORD(&polepos_alpha_memory[offset], newword);
}

READ_HANDLER( polepos_z80_alpha_r )
{
	return polepos_alpha_r(offset << 1) & 0xff;
}

WRITE_HANDLER( polepos_z80_alpha_w )
{
	polepos_alpha_w(offset << 1, data | 0xff000000);
}


/***************************************************************************

  Internal draw routines

***************************************************************************/

static void draw_view(struct osd_bitmap *bitmap)
{
	struct rectangle clip = Machine->visible_area;
	int x, y, offs;

	/* look for dirty tiles */
	for (x = offs = 0; x < 64; x++)
		for (y = 0; y < 16; y++, offs++)
			if (view_dirty[offs])
			{
				int word = READ_WORD(&polepos_view_memory[offs * 2]);
				int code = (word & 0xff) | ((word >> 6) & 0x100);
				int color = (word >> 8) & 0x3f;

				drawgfx(view_bitmap, Machine->gfx[1], code, color,
						0, 0, 8*x, 8*y, NULL, TRANSPARENCY_NONE, 0);
				view_dirty[offs] = 0;
			}

	/* copy the bitmap */
	x = -view_hscroll;
	clip.max_y = 127;
	copyscrollbitmap(bitmap, view_bitmap, 1, &x, 0, 0, &clip, TRANSPARENCY_NONE, 0);
}

static void draw_road(struct osd_bitmap *bitmap)
{
	int dx = 1, dy = (bitmap->line[1] - bitmap->line[0]) * 8 / bitmap->depth;
	int sx = 0, sy = 128, temp;

	/* adjust our parameters for the current orientation */
	if (Machine->orientation)
	{
		if (Machine->orientation & ORIENTATION_SWAP_XY)
		{
			temp = sx; sx = sy; sy = temp;
			temp = dx; dx = dy; dy = temp;
		}
		if (Machine->orientation & ORIENTATION_FLIP_X)
		{
			sx = bitmap->width - 1 - sx;
			if (!(Machine->orientation & ORIENTATION_SWAP_XY)) dx = -dx;
			else dy = -dy;
		}
		if (Machine->orientation & ORIENTATION_FLIP_Y)
		{
			sy = bitmap->height - 1 - sy;
			if (Machine->orientation & ORIENTATION_SWAP_XY) dx = -dx;
			else dy = -dy;
		}
	}

	/* 8-bit case */
	if (bitmap->depth == 8)
		draw_road_core_8(bitmap, sx, sy, dx, dy);
	else
		draw_road_core_16(bitmap, sx, sy, dx, dy);
}

static void draw_sprites(struct osd_bitmap *bitmap)
{
	UINT16 *posmem = (UINT16 *)&polepos_sprite_memory[0x700];
	UINT16 *sizmem = (UINT16 *)&polepos_sprite_memory[0xf00];
	int i;

	for (i = 0; i < 64; i++, posmem += 2, sizmem += 2)
	{
		const struct GfxElement *gfx = Machine->gfx[(sizmem[0] & 0x8000) ? 3 : 2];
		int vpos = (~posmem[0] & 0x1ff) + 4;
		int hpos = (posmem[1] & 0x3ff) - 0x40;
		int vsize = ((sizmem[0] >> 8) & 0x3f) + 1;
		int hsize = ((sizmem[1] >> 8) & 0x3f) + 1;
		int code = sizmem[0] & 0x7f;
		int hflip = sizmem[0] & 0x80;
		int color = sizmem[1] & 0x3f;

		if (vpos >= 128) color |= 0x40;
		drawgfxzoom(bitmap, gfx,
				 code, color, hflip, 0, hpos, vpos,
				 &Machine->visible_area, TRANSPARENCY_COLOR, 0, hsize << 11, vsize << 11);
	}
}

static void draw_alpha(struct osd_bitmap *bitmap)
{
	int x, y, offs, in;

	for (y = offs = 0; y < 32; y++)
		for (x = 0; x < 32; x++, offs++)
		{
			int word = READ_WORD(&polepos_alpha_memory[offs * 2]);
			int code = (word & 0xff) | ((word >> 6) & 0x100);
			int color = (word >> 8) & 0x3f;	/* 6 bits color */

			if (y >= 16) color |= 0x40;
			drawgfx(bitmap, Machine->gfx[0],
					 code, color, 0, 0, 8*x, 8*y,
					 &Machine->visible_area, TRANSPARENCY_COLOR, 0);
		}

	/* Now draw the shift if selected on the fake dipswitch */
	in = readinputport( 0 );

	if ( in & 8 ) {
		if ( ( in & 2 ) == 0 ) {
			/* L */
			drawgfx(bitmap, Machine->gfx[0],
					 0x15, 0, 0, 0, 30*8-1, 29*8,
					 &Machine->visible_area, TRANSPARENCY_PEN, 0);
			/* O */
			drawgfx(bitmap, Machine->gfx[0],
					 0x18, 0, 0, 0, 31*8-1, 29*8,
					 &Machine->visible_area, TRANSPARENCY_PEN, 0);
		} else {
			/* H */
			drawgfx(bitmap, Machine->gfx[0],
					 0x11, 0, 0, 0, 30*8-1, 29*8,
					 &Machine->visible_area, TRANSPARENCY_PEN, 0);
			/* I */
			drawgfx(bitmap, Machine->gfx[0],
					 0x12, 0, 0, 0, 31*8-1, 29*8,
					 &Machine->visible_area, TRANSPARENCY_PEN, 0);
		}
	}
}


/***************************************************************************

  Master refresh routine

***************************************************************************/

void polepos_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	draw_view(bitmap);
	draw_road(bitmap);
	draw_sprites(bitmap);
	draw_alpha(bitmap);
}


/***************************************************************************

  Road drawing generators

***************************************************************************/

#define ROAD_CORE_INCLUDE

#define NAME draw_road_core_8
#define TYPE UINT8
#include "polepos.cpp"
#undef TYPE
#undef NAME

#define NAME draw_road_core_16
#define TYPE UINT16
#include "polepos.cpp"
#undef TYPE
#undef NAME

#else

/***************************************************************************

  Road drawing routine

***************************************************************************/

static void NAME(struct osd_bitmap *bitmap, int sx, int sy, int dx, int dy)
{
	TYPE *base = &((TYPE *)bitmap->line[sy])[sx];
	int x, y, i;

	/* loop over the lower half of the screen */
	for (y = 128; y < 256; y++, base += dy)
	{
		int xoffs, yoffs, roadpal;
		UINT16 *colortable;
		TYPE *dest;

		/* first add the vertical position modifier and the vertical scroll */
		yoffs = ((polepos_vertical_position_modifier[y] + road_vscroll) >> 2) & 0x3fe;

		/* then use that as a lookup into the road memory */
		roadpal = READ_WORD(&polepos_road_memory[yoffs]) & 15;

		/* this becomes the palette base for the scanline */
		colortable = &Machine->remapped_colortable[0x1000 + (roadpal << 6)];

		/* now fetch the horizontal scroll offset for this scanline */
		xoffs = READ_WORD(&polepos_road_memory[0x700 + (y & 0x7f) * 2]) & 0x3ff;

		/* the road is drawn in 8-pixel chunks, so round downward and adjust the base */
		/* note that we assume there is at least 8 pixels of slop on the left/right */
		dest = base - (xoffs & 7) * dx;
		xoffs &= ~7;

		/* loop over 8-pixel chunks */
		for (x = 0; x < 256 / 8 + 1; x++, xoffs += 8)
		{
			/* if the 0x200 bit of the xoffset is set, a special pin on the custom */
			/* chip is set and the /CE and /OE for the road chips is disabled */
			if (xoffs & 0x200)
			{
				/* in this case, it looks like we just fill with 0 */
				for (i = 0; i < 8; i++, dest += dx)
					*dest = colortable[0];
			}

			/* otherwise, we clock in the bits and compute the road value */
			else
			{
				/* the road ROM offset comes from the current scanline and the X offset */
				int romoffs = ((y & 0x07f) << 6) + ((xoffs & 0x1ff) >> 3);

				/* fetch the current data from the road ROMs */
				int control = road_control[romoffs];
				int bits1 = road_bits1[romoffs];
				int bits2 = road_bits2[(romoffs & 0xfff) | ((romoffs >> 1) & 0x800)];

				/* extract the road value and the carry-in bit */
				int roadval = control & 0x3f;
				int carin = control >> 7;

				/* draw this 8-pixel chunk */
				for (i = 0; i < 8; i++, dest += dx, bits1 <<= 1, bits2 <<= 1)
				{
					int bits = ((bits1 >> 7) & 1) + ((bits2 >> 6) & 2);
					if (!carin && bits) bits++;
					*dest = colortable[roadval & 0x3f];
					roadval += bits;
				}
			}
		}
	}
}

#endif
