/***************************************************************************

  Dec0 Video emulation - Bryan McPhail, mish@tendril.co.uk

*********************************************************************

	Each game uses the MXC-06 chip to produce sprites.

	Sprite data:  The unknown bits seem to be unused.

	Byte 0:
		Bit 0 : Y co-ord hi bit
		Bit 1,2: ?
		Bit 3,4 : Sprite height (1x, 2x, 4x, 8x)
		Bit 5  - X flip
		Bit 6  - Y flip
		Bit 7  - Only display Sprite if set
	Byte 1: Y-coords
	Byte 2:
		Bit 0,1,2,3: Hi bits of sprite number
		Bit 4,5,6,7: (Probably unused MSB's of sprite)
	Byte 3: Low bits of sprite number
	Byte 4:
		Bit 0 : X co-ords hi bit
		Bit 1,2: ??
		Bit 3: Sprite flash (sprite is displayed every other frame)
		Bit 4,5,6,7:  - Colour
	Byte 5: X-coords

**********************************************************************

  Palette data

    0x000 - character palettes (Sprites on Midnight R)
    0x200 - sprite palettes (Characters on Midnight R)
    0x400 - tiles 1
  	0x600 - tiles 2

	Bad Dudes, Robocop, Heavy Barrel, Hippodrome - 24 bit rgb
	Sly Spy, Midnight Resistance - 12 bit rgb

  Tile data

  	4 bit palette select, 12 bit tile select

**********************************************************************

 All games contain three BAC06 background generator chips, usual (software)
configuration is 2 chips of 16*16 tiles, 1 of 8*8.

 Playfield control registers:
   bank 0:
   0:
		bit 0 (0x1) set = 8*8 tiles, else 16*16 tiles
		Bit 1 (0x2) unknown
		bit 2 (0x4) set enables rowscroll
		bit 3 (0x8) set enables colscroll
		bit 7 (0x80) set in playfield 1 is reverse screen (set via dip-switch)
		bit 7 (0x80) in other playfields unknown
   2: unknown (00 in bg, 03 in fg+text - maybe controls pf transparency?)
   4: unknown (always 00)
   6: playfield shape: 00 = 4x1, 01 = 2x2, 02 = 1x4 (low 4 bits only)

   bank 1:
   0: horizontal scroll
   2: vertical scroll
   4: Style of colscroll (low 4 bits, top 4 bits do nothing)
   6: Style of rowscroll (low 4 bits, top 4 bits do nothing)

Rowscroll/Colscroll styles:
	0: 256 scroll registers (Robocop)
	1: 128 scroll registers
	2:  64 scroll registers
	3:  32 scroll registers (Heavy Barrel, Midres)
	4:  16 scroll registers (Bad Dudes, Sly Spy)
	5:   8 scroll registers (Hippodrome)
	6:   4 scroll registers (Heavy Barrel)
	7:   2 scroll registers (Heavy Barrel, used on other games but registers kept at 0)
	8:   1 scroll register (ie, none)

	Values above are *multiplied* by playfield shape.

Playfield priority (Bad Dudes, etc):
	In the bottommost playfield, pens 8-15 can have priority over the next playfield.
	In that next playfield, pens 8-15 can have priority over sprites.

Bit 0:  Playfield inversion
Bit 1:  Enable playfield mixing (for palettes 8-15 only)
Bit 2:  Enable playfield/sprite mixing (for palettes 8-15 only)

Priority word (Midres):
	Bit 0 set = Playfield 3 drawn over Playfield 2
			~ = Playfield 2 drawn over Playfield 3
	Bit 1 set = Sprites are drawn inbetween playfields
			~ = Sprites are on top of playfields
	Bit 2
	Bit 3 set = ...

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

#define TEXTRAM_SIZE	0x2000	/* Size of text layer */
#define TILERAM_SIZE	0x800	/* Size of background and foreground */

/* Video */
unsigned char *dec0_pf1_data,*dec0_pf2_data,*dec0_pf3_data;
static unsigned char *dec0_pf1_dirty,*dec0_pf3_dirty,*dec0_pf2_dirty;
static struct osd_bitmap *dec0_pf1_bitmap;
static int dec0_pf1_current_shape;
static struct osd_bitmap *dec0_pf2_bitmap;
static int dec0_pf2_current_shape;
static struct osd_bitmap *dec0_pf3_bitmap;
static int dec0_pf3_current_shape;
static struct osd_bitmap *dec0_tf2_bitmap;
static struct osd_bitmap *dec0_tf3_bitmap;

unsigned char *dec0_pf1_rowscroll,*dec0_pf2_rowscroll,*dec0_pf3_rowscroll;
unsigned char *dec0_pf1_colscroll,*dec0_pf2_colscroll,*dec0_pf3_colscroll;
static unsigned char dec0_pf1_control_0[8];
static unsigned char dec0_pf1_control_1[8];
static unsigned char dec0_pf2_control_0[8];
static unsigned char dec0_pf2_control_1[8];
static unsigned char dec0_pf3_control_0[8];
static unsigned char dec0_pf3_control_1[8];
static unsigned char *dec0_spriteram;

static int dec0_pri;

/******************************************************************************/

WRITE_HANDLER( dec0_update_sprites_w )
{
	memcpy(dec0_spriteram,spriteram,0x800);
}

/******************************************************************************/

static void update_24bitcol(int offset)
{
	int r,g,b;

	r = (READ_WORD(&paletteram[offset]) >> 0) & 0xff;
	g = (READ_WORD(&paletteram[offset]) >> 8) & 0xff;
	b = (READ_WORD(&paletteram_2[offset]) >> 0) & 0xff;

	palette_change_color(offset / 2,r,g,b);
}

WRITE_HANDLER( dec0_paletteram_rg_w )
{
	COMBINE_WORD_MEM(&paletteram[offset],data);
	update_24bitcol(offset);
}

WRITE_HANDLER( dec0_paletteram_b_w )
{
	COMBINE_WORD_MEM(&paletteram_2[offset],data);
	update_24bitcol(offset);
}

/******************************************************************************/

/* pf23priority: 1 -> pf2 transparent, pf3 not transparent */
/*               0 -> pf2 not transparent, pf3 transparent */
static void dec0_update_palette(int pf23priority)
{
	int offs;
	int color,code,i;
	int colmask[16];
	int pal_base;


	palette_init_used_colors();

	pal_base = Machine->drv->gfxdecodeinfo[0].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0; offs < TEXTRAM_SIZE;offs += 2)
	{
		code = READ_WORD(&dec0_pf1_data[offs]);
		color = (code & 0xf000) >> 12;
		code &= 0x0fff;
		colmask[color] |= Machine->gfx[0]->pen_usage[code];
	}

	for (color = 0;color < 16;color++)
	{
		if (colmask[color] & (1 << 0))
			palette_used_colors[pal_base + 16 * color] = PALETTE_COLOR_TRANSPARENT;
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0; offs < TILERAM_SIZE;offs += 2)
	{
		code = READ_WORD(&dec0_pf2_data[offs]);
		color = (code & 0xf000) >> 12;
		code &= 0x0fff;
		colmask[color] |= Machine->gfx[1]->pen_usage[code];
	}

	for (color = 0;color < 16;color++)
	{
		if (colmask[color] & (1 << 0))
			palette_used_colors[pal_base + 16 * color] = pf23priority ? PALETTE_COLOR_USED : PALETTE_COLOR_TRANSPARENT;
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	pal_base = Machine->drv->gfxdecodeinfo[2].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0; offs < TILERAM_SIZE;offs += 2)
	{
		code = READ_WORD(&dec0_pf3_data[offs]);
		color = (code & 0xf000) >> 12;
		code &= 0x0fff;
		colmask[color] |= Machine->gfx[2]->pen_usage[code];
	}

	for (color = 0;color < 16;color++)
	{
		if (colmask[color] & (1 << 0))
			palette_used_colors[pal_base + 16 * color] = pf23priority ? PALETTE_COLOR_TRANSPARENT : PALETTE_COLOR_USED;
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	pal_base = Machine->drv->gfxdecodeinfo[3].color_codes_start;
	palette_used_colors[pal_base] = PALETTE_COLOR_TRANSPARENT; /* Always set this, for priorities to work */
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,multi;

		y = READ_WORD(&dec0_spriteram[offs]);
		if ((y&0x8000) == 0) continue;

		x = READ_WORD(&dec0_spriteram[offs+4]);
		color = (x & 0xf000) >> 12;

		multi = (1 << ((y & 0x1800) >> 11)) - 1;	/* 1x, 2x, 4x, 8x height */
											/* multi = 0   1   3   7 */

		x = x & 0x01ff;
		if (x >= 256) x -= 512;
		x = 240 - x;
		if (x>256) continue; /* Speedup + save colours */

		sprite = READ_WORD (&dec0_spriteram[offs+2]) & 0x0fff;

		sprite &= ~multi;

		while (multi >= 0)
		{
			colmask[color] |= Machine->gfx[3]->pen_usage[sprite + multi];

			multi--;
		}
	}

	for (color = 0;color < 16;color++)
	{
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}


	if (palette_recalc())
	{
		memset(dec0_pf1_dirty,1,TEXTRAM_SIZE);
		memset(dec0_pf2_dirty,1,TILERAM_SIZE);
		memset(dec0_pf3_dirty,1,TILERAM_SIZE);
	}
}

/******************************************************************************/

static void dec0_drawsprites(struct osd_bitmap *bitmap,int pri_mask,int pri_val)
{
	int offs;

	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash;

		y = READ_WORD(&dec0_spriteram[offs]);
		if ((y&0x8000) == 0) continue;

		x = READ_WORD(&dec0_spriteram[offs+4]);
		colour = x >> 12;
		if ((colour & pri_mask) != pri_val) continue;

		flash=x&0x800;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x1800) >> 11)) - 1;	/* 1x, 2x, 4x, 8x height */
											/* multi = 0   1   3   7 */

		sprite = READ_WORD (&dec0_spriteram[offs+2]) & 0x0fff;

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 256) x -= 512;
		if (y >= 256) y -= 512;
		x = 240 - x;
		y = 240 - y;

		if (x>256) continue; /* Speedup */

		sprite &= ~multi;
		if (fy)
			inc = -1;
		else
		{
			sprite += multi;
			inc = 1;
		}

		while (multi >= 0)
		{
			drawgfx(bitmap,Machine->gfx[3],
					sprite - multi * inc,
					colour,
					fx,fy,
					x,y - 16 * multi,
					&Machine->visible_area,TRANSPARENCY_PEN,0);

			multi--;
		}
	}
}

/******************************************************************************/

static void dec0_pf1_update(void)
{
	int offs,mx,my,color,tile,quarter;
	int offsetx[4],offsety[4];

	switch (READ_WORD(&dec0_pf1_control_0[6])&0xf)
	{
		case 0:	/* 4x1 */
			offsetx[0] = 0;
			offsetx[1] = 256;
			offsetx[2] = 512;
			offsetx[3] = 768;
			offsety[0] = 0;
			offsety[1] = 0;
			offsety[2] = 0;
			offsety[3] = 0;
			if (dec0_pf1_current_shape != 0)
			{
				bitmap_free(dec0_pf1_bitmap);
				dec0_pf1_bitmap = bitmap_alloc(1024,256);
				dec0_pf1_current_shape = 0;
				memset(dec0_pf1_dirty,1,TEXTRAM_SIZE);
			}
			break;
		case 1:	/* 2x2 */
			offsetx[0] = 0;
			offsetx[1] = 0;
			offsetx[2] = 256;
			offsetx[3] = 256;
			offsety[0] = 0;
			offsety[1] = 256;
			offsety[2] = 0;
			offsety[3] = 256;
			if (dec0_pf1_current_shape != 1)
			{
				bitmap_free(dec0_pf1_bitmap);
				dec0_pf1_bitmap = bitmap_alloc(512,512);
				dec0_pf1_current_shape = 1;
				memset(dec0_pf1_dirty,1,TEXTRAM_SIZE);
			}
			break;
		case 2:	/* 1x4 */
			offsetx[0] = 0;
			offsetx[1] = 0;
			offsetx[2] = 0;
			offsetx[3] = 0;
			offsety[0] = 0;
			offsety[1] = 256;
			offsety[2] = 512;
			offsety[3] = 768;
			if (dec0_pf1_current_shape != 2)
			{
				bitmap_free(dec0_pf1_bitmap);
				dec0_pf1_bitmap = bitmap_alloc(256,1024);
				dec0_pf1_current_shape = 2;
				memset(dec0_pf1_dirty,1,TEXTRAM_SIZE);
			}
			break;
		default:
			//logerror("error: pf1_update with unknown shape %04x\n",READ_WORD(&dec0_pf1_control_0[6]));
			return;
	}

	for (quarter = 0;quarter < 4;quarter++)
	{
		mx = -1;
		my = 0;

		for (offs = 0x800 * quarter; offs < 0x800 * quarter + 0x800;offs += 2)
		{
			mx++;
			if (mx == 32)
			{
				mx = 0;
				my++;
			}

			if (dec0_pf1_dirty[offs])
			{
				dec0_pf1_dirty[offs] = 0;
				tile = READ_WORD(&dec0_pf1_data[offs]);
				color = (tile & 0xf000) >> 12;

				drawgfx(dec0_pf1_bitmap,Machine->gfx[0],
						tile & 0x0fff,
						color,
						0,0,
						8*mx + offsetx[quarter],8*my + offsety[quarter],
						0,TRANSPARENCY_NONE,0);
			}
		}
	}
}

static void dec0_pf2_update(int transparent, int special)
{
	int offs,mx,my,color,tile,quarter;
	int offsetx[4],offsety[4];
	static int last_transparent;

	if (transparent != last_transparent)
	{
		last_transparent = transparent;
		memset(dec0_pf2_dirty,1,TILERAM_SIZE);
	}

	switch (READ_WORD(&dec0_pf2_control_0[6])&0xf)
	{
		case 0:	/* 4x1 */
			offsetx[0] = 0;
			offsetx[1] = 256;
			offsetx[2] = 512;
			offsetx[3] = 768;
			offsety[0] = 0;
			offsety[1] = 0;
			offsety[2] = 0;
			offsety[3] = 0;
			if (dec0_pf2_current_shape != 0)
			{
				bitmap_free(dec0_pf2_bitmap);
				dec0_pf2_bitmap = bitmap_alloc(1024,256);
				bitmap_free(dec0_tf2_bitmap);
				dec0_tf2_bitmap = bitmap_alloc(1024,256);
				dec0_pf2_current_shape = 0;
				memset(dec0_pf2_dirty,1,TILERAM_SIZE);
			}
			break;
		case 1:	/* 2x2 */
			offsetx[0] = 0;
			offsetx[1] = 0;
			offsetx[2] = 256;
			offsetx[3] = 256;
			offsety[0] = 0;
			offsety[1] = 256;
			offsety[2] = 0;
			offsety[3] = 256;
			if (dec0_pf2_current_shape != 1)
			{
				bitmap_free(dec0_pf2_bitmap);
				dec0_pf2_bitmap = bitmap_alloc(512,512);
				bitmap_free(dec0_tf2_bitmap);
				dec0_tf2_bitmap = bitmap_alloc(512,512);
				dec0_pf2_current_shape = 1;
				memset(dec0_pf2_dirty,1,TILERAM_SIZE);
			}
			break;
		case 2:	/* 1x4 */
			offsetx[0] = 0;
			offsetx[1] = 0;
			offsetx[2] = 0;
			offsetx[3] = 0;
			offsety[0] = 0;
			offsety[1] = 256;
			offsety[2] = 512;
			offsety[3] = 768;
			if (dec0_pf2_current_shape != 2)
			{
				bitmap_free(dec0_pf2_bitmap);
				dec0_pf2_bitmap = bitmap_alloc(256,1024);
				bitmap_free(dec0_tf2_bitmap);
				dec0_tf2_bitmap = bitmap_alloc(256,1024);
				dec0_pf2_current_shape = 2;
				memset(dec0_pf2_dirty,1,TILERAM_SIZE);
			}
			break;
		default:
			//logerror("error: pf2_update with unknown shape %04x\n",READ_WORD(&dec0_pf2_control_0[6]));
			return;
	}

	for (quarter = 0;quarter < 4;quarter++)
	{
		mx = -1;
		my = 0;

		for (offs = 0x200 * quarter; offs < 0x200 * quarter + 0x200;offs += 2)
		{
			mx++;
			if (mx == 16)
			{
				mx = 0;
				my++;
			}

			if (dec0_pf2_dirty[offs])
			{
				tile = READ_WORD(&dec0_pf2_data[offs]);
				color = (tile & 0xf000) >> 12;

				/* 'Special' - Render foreground pens (8-15) to a seperate bitmap */
				if (special) {
					/* Blank tile */
					drawgfx(dec0_tf2_bitmap,Machine->gfx[3],
							0,
							0,
							0,0,
							16*mx + offsetx[quarter],16*my + offsety[quarter],
							0,TRANSPARENCY_NONE,0);

					if (color>7)
						drawgfx(dec0_tf2_bitmap,Machine->gfx[1],
								tile & 0x0fff,
								color,
								0,0,
								16*mx + offsetx[quarter],16*my + offsety[quarter],
								0,TRANSPARENCY_PENS,0xff);
				} else { /* Else, business as usual */
					dec0_pf2_dirty[offs] = 0;
					drawgfx(dec0_pf2_bitmap,Machine->gfx[1],
							tile & 0x0fff,
							color,
							0,0,
							16*mx + offsetx[quarter],16*my + offsety[quarter],
							0,TRANSPARENCY_NONE,0);
				}
			}
		}
	}
}

static void dec0_pf3_update(int transparent, int special)
{
	int offs,mx,my,color,tile,quarter;
	int offsetx[4],offsety[4];
	static int last_transparent;

	if (transparent != last_transparent)
	{
		last_transparent = transparent;
		memset(dec0_pf3_dirty,1,TILERAM_SIZE);
	}

	switch (READ_WORD(&dec0_pf3_control_0[6])&0xf)
	{
		case 0:	/* 4x1 */
			offsetx[0] = 0;
			offsetx[1] = 256;
			offsetx[2] = 512;
			offsetx[3] = 768;
			offsety[0] = 0;
			offsety[1] = 0;
			offsety[2] = 0;
			offsety[3] = 0;
			if (dec0_pf3_current_shape != 0)
			{
				bitmap_free(dec0_pf3_bitmap);
				dec0_pf3_bitmap = bitmap_alloc(1024,256);
				bitmap_free(dec0_tf3_bitmap);
				dec0_tf3_bitmap = bitmap_alloc(1024,256);
				dec0_pf3_current_shape = 0;
				memset(dec0_pf3_dirty,1,TILERAM_SIZE);
			}
			break;
		case 1:	/* 2x2 */
			offsetx[0] = 0;
			offsetx[1] = 0;
			offsetx[2] = 256;
			offsetx[3] = 256;
			offsety[0] = 0;
			offsety[1] = 256;
			offsety[2] = 0;
			offsety[3] = 256;
			if (dec0_pf3_current_shape != 1)
			{
				bitmap_free(dec0_pf3_bitmap);
				dec0_pf3_bitmap = bitmap_alloc(512,512);
				bitmap_free(dec0_tf3_bitmap);
				dec0_tf3_bitmap = bitmap_alloc(512,512);
				dec0_pf3_current_shape = 1;
				memset(dec0_pf3_dirty,1,TILERAM_SIZE);
			}
			break;
		case 2:	/* 1x4 */
			offsetx[0] = 0;
			offsetx[1] = 0;
			offsetx[2] = 0;
			offsetx[3] = 0;
			offsety[0] = 0;
			offsety[1] = 256;
			offsety[2] = 512;
			offsety[3] = 768;
			if (dec0_pf3_current_shape != 2)
			{
				bitmap_free(dec0_pf3_bitmap);
				dec0_pf3_bitmap = bitmap_alloc(256,1024);
				bitmap_free(dec0_tf3_bitmap);
				dec0_tf3_bitmap = bitmap_alloc(256,1024);
				dec0_pf3_current_shape = 2;
				memset(dec0_pf3_dirty,1,TILERAM_SIZE);
			}
			break;
		default:
			//logerror("error: pf3_update with unknown shape %04x\n",READ_WORD(&dec0_pf3_control_0[6]));
			return;
	}

	for (quarter = 0;quarter < 4;quarter++)
	{
		mx = -1;
		my = 0;

		for (offs = 0x200 * quarter; offs < 0x200 * quarter + 0x200;offs += 2)
		{
			mx++;
			if (mx == 16)
			{
				mx = 0;
				my++;
			}

			if (dec0_pf3_dirty[offs])
			{
				tile = READ_WORD(&dec0_pf3_data[offs]);
				color = (tile & 0xf000) >> 12;

				/* 'Special' - Render foreground pens (8-15) to a seperate bitmap */
				if (special) {
					/* Blank tile */
					drawgfx(dec0_tf3_bitmap,Machine->gfx[3],
							0,
							0,
							0,0,
							16*mx + offsetx[quarter],16*my + offsety[quarter],
							0,TRANSPARENCY_NONE,0);

					if (color>7)
						drawgfx(dec0_tf3_bitmap,Machine->gfx[2],
							tile & 0x0fff,
							color,
							0,0,
							16*mx + offsetx[quarter],16*my + offsety[quarter],
							0,TRANSPARENCY_PENS,0xff);
				} else { /* Else, business as usual */
					dec0_pf3_dirty[offs] = 0;
					drawgfx(dec0_pf3_bitmap,Machine->gfx[2],
							tile & 0x0fff,
							color,
							0,0,
							16*mx + offsetx[quarter],16*my + offsety[quarter],
							0,TRANSPARENCY_NONE,0);
				}
			}
		}
	}
}

/******************************************************************************/

void dec0_pf1_draw(struct osd_bitmap *bitmap)
{
	int offs,lines,height,scrolly,scrollx;

	scrollx = - READ_WORD(&dec0_pf1_control_1[0]);
	scrolly = - READ_WORD(&dec0_pf1_control_1[2]);

	/* We check for column scroll and use that if needed, otherwise use row scroll,
	   I am 99% sure they are never needed at same time ;) */
	if (READ_WORD(&dec0_pf1_colscroll[0])) /* This is NOT a good check for col scroll, I can't find real bit */
	{
		int cscrolly[64];

		for (offs = 0;offs < 32;offs++)
		cscrolly[offs] = -READ_WORD(&dec0_pf1_control_1[2]) - READ_WORD(&dec0_pf1_colscroll[2*offs]);

		copyscrollbitmap(bitmap,dec0_pf1_bitmap,1,&scrollx,32,cscrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
	}

	/* Row scroll enable bit (unsure if this enables/disables col scroll too) */
	else if (READ_WORD(&dec0_pf1_control_0[0])&0x4)
	{
		int rscrollx[1024];

		/* Playfield shape */
		switch (READ_WORD(&dec0_pf1_control_0[6])&0xf)
		{
			case 0:	height=1; break; /* 4x1, 256 rows */
			case 1:	height=2; break; /* 2x2, 512 rows */
			case 2:	height=4; break; /* 1x4, 1024 rows */
			default: height=2; break; /* Never happens (I hope) */
		}

		/* Rowscroll style */
		switch (READ_WORD(&dec0_pf1_control_1[6])&0xf)
		{
			case 0: lines=256; break; /* 256 horizontal scroll registers (Robocop) */
			case 1: lines=128; break; /* 128 horizontal scroll registers (Not used?) */
			case 2: lines=64; break; /* 128 horizontal scroll registers (Not used?) */
			case 3: lines=32; break; /* 32 horizontal scroll registers (Heavy Barrel title screen) */
			case 4: lines=16; break; /* 16 horizontal scroll registers (Bad Dudes, Sly Spy) */
			case 5: lines=8; break; /* 8 horizontal scroll registers (Not used?) */
			case 6: lines=4; break; /* 4 horizontal scroll registers (Not used?) */
			case 7: lines=2; break; /* 2 horizontal scroll registers (Not used?) */
			case 8: lines=1; break; /* Appears to be no row-scroll - so maybe only bottom 3 bits are style */
			default: lines=1; break; /* Just in case */
		}

		for (offs = 0; offs < lines*height; offs++)
			rscrollx[offs] = scrollx - READ_WORD(&dec0_pf1_rowscroll[offs<<1]);
		copyscrollbitmap(bitmap,dec0_pf1_bitmap,lines*height,rscrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
	}
	else /* Scroll registers not enabled */
		copyscrollbitmap(bitmap,dec0_pf1_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
}

/* trans=0 - bottom playfield, trans=1 - top playfield, trans=2 - special foreground section */
void dec0_pf2_draw(struct osd_bitmap *bitmap, int trans)
{
	int offs,lines,height,width,scrolly,scrollx;

	scrollx = - READ_WORD(&dec0_pf2_control_1[0]);
	scrolly = - READ_WORD(&dec0_pf2_control_1[2]);

	/* Row scroll enable bit */
	if (READ_WORD(&dec0_pf2_control_0[0])&0x4)
	{
		int rscrollx[1024];

		/* Playfield shape */
		switch (READ_WORD(&dec0_pf2_control_0[6])&0xf)
		{
			case 0:	height=1; break; /* 4x1, 256 rows */
			case 1:	height=2; break; /* 2x2, 512 rows */
			case 2:	height=4; break; /* 1x4, 1024 rows */
			default: height=2; break; /* Never happens (I hope) */
		}

		/* Rowscroll style */
		switch (READ_WORD(&dec0_pf2_control_1[6])&0xf)
		{
			case 0: lines=256; break; /* 256 horizontal scroll registers (Robocop) */
			case 1: lines=128; break; /* 128 horizontal scroll registers (Not used?) */
			case 2: lines=64; break; /* 128 horizontal scroll registers (Not used?) */
			case 3: lines=32; break; /* 32 horizontal scroll registers (Heavy Barrel title screen) */
			case 4: lines=16; break; /* 16 horizontal scroll registers (Bad Dudes, Sly Spy) */
			case 5: lines=8; break; /* 8 horizontal scroll registers (Not used?) */
			case 6: lines=4; break; /* 4 horizontal scroll registers (Not used?) */
			case 7: lines=2; break; /* 2 horizontal scroll registers (Not used?) */
			case 8: lines=1; break; /* Appears to be no row-scroll */
			default: lines=1; break; /* Just in case */
		}

		for (offs = 0; offs < lines*height; offs++)
			rscrollx[offs] = scrollx - READ_WORD(&dec0_pf2_rowscroll[offs<<1]);

		if (trans==2)
			copyscrollbitmap(bitmap,dec0_tf2_bitmap,lines*height,rscrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
		else if (trans==1)
			copyscrollbitmap(bitmap,dec0_pf2_bitmap,lines*height,rscrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
		else
			copyscrollbitmap(bitmap,dec0_pf2_bitmap,lines*height,rscrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
	else /* Column scroll enable bit */
	if (READ_WORD(&dec0_pf2_control_0[0])&0x8)
	{
		int rscrollx[1024];

		/* Playfield shape */
		switch (READ_WORD(&dec0_pf2_control_0[6])&0xf)
		{
			case 0:	width=1; break; /* 4x1, 256 rows */
			case 1:	width=2; break; /* 2x2, 512 rows */
			case 2:	width=4; break; /* 1x4, 1024 rows */
			default: width=2; break; /* Never happens (I hope) */
		}

		/* Rowscroll style */
		switch (READ_WORD(&dec0_pf2_control_1[4])&0xf)
		{
			case 0: lines=64; break; /* 256 horizontal scroll registers (Robocop) */
			case 1: lines=32; break; /* 128 horizontal scroll registers (Not used?) */
			case 2: lines=16; break; /* 128 horizontal scroll registers (Not used?) */
			case 3: lines=8; break; /* 32 horizontal scroll registers (Heavy Barrel title screen) */
			case 4: lines=4; break; /* 16 horizontal scroll registers (Bad Dudes, Sly Spy) */
			case 5: lines=2; break; /* 8 horizontal scroll registers (Not used?) */
			case 6: lines=1; break; /* 4 horizontal scroll registers (Not used?) */
			case 7: lines=2; break; /* 2 horizontal scroll registers (Not used?) */
			case 8: lines=1; break; /* Appears to be no row-scroll */
			default: lines=1; break; /* Just in case */
		}

		for (offs = 0; offs < lines; offs++)
			rscrollx[offs] = scrolly - READ_WORD(&dec0_pf2_colscroll[offs<<1]);

		if (trans==2)
			copyscrollbitmap(bitmap,dec0_tf2_bitmap,1,&scrollx,lines,rscrollx,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
		else if (trans==1)
			copyscrollbitmap(bitmap,dec0_pf2_bitmap,1,&scrollx,lines,rscrollx,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
		else
			copyscrollbitmap(bitmap,dec0_pf2_bitmap,1,&scrollx,lines,rscrollx,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
	else { /* Scroll registers not enabled */
		if (trans==2)
			copyscrollbitmap(bitmap,dec0_tf2_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
		else if (trans==1)
			copyscrollbitmap(bitmap,dec0_pf2_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
		else
			copyscrollbitmap(bitmap,dec0_pf2_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
}

void dec0_pf3_draw(struct osd_bitmap *bitmap, int trans)
{
	int offs,lines,height,scrolly,scrollx;

	scrollx = - READ_WORD(&dec0_pf3_control_1[0]);
	scrolly = - READ_WORD(&dec0_pf3_control_1[2]);

	/* Colscroll not supported for this playfield (does anything use it?) */

	/* Row scroll enable bit (unsure if this enables/disables col scroll too) */
	if (READ_WORD(&dec0_pf3_control_0[0])&0x4)
	{
		int rscrollx[1024];

		/* Playfield shape */
		switch (READ_WORD(&dec0_pf3_control_0[6])&0xf)
		{
			case 0:	height=1; break; /* 4x1, 256 rows */
			case 1:	height=2; break; /* 2x2, 512 rows */
			case 2:	height=4; break; /* 1x4, 1024 rows */
			default: height=2; break; /* Never happens (I hope) */
		}

		/* Rowscroll style */
		switch (READ_WORD(&dec0_pf3_control_1[6])&0xf)
		{
			case 0: lines=256; break; /* 256 horizontal scroll registers (Robocop) */
			case 1: lines=128; break; /* 128 horizontal scroll registers (Not used?) */
			case 2: lines=64; break; /* 128 horizontal scroll registers (Not used?) */
			case 3: lines=32; break; /* 32 horizontal scroll registers (Heavy Barrel title screen) */
			case 4: lines=16; break; /* 16 horizontal scroll registers (Bad Dudes, Sly Spy) */
			case 5: lines=8; break; /* 8 horizontal scroll registers (Not used?) */
			case 6: lines=4; break; /* 4 horizontal scroll registers (Not used?) */
			case 7: lines=2; break; /* 2 horizontal scroll registers (Not used?) */
			case 8: lines=1; break; /* Appears to be no row-scroll - so maybe only bottom 3 bits are style */
			default: lines=1; break; /* Just in case */
		}

		for (offs = 0; offs < lines*height; offs++)
			rscrollx[offs] = scrollx - READ_WORD(&dec0_pf3_rowscroll[offs<<1]);

		if (trans==2)
			copyscrollbitmap(bitmap,dec0_tf3_bitmap,lines*height,rscrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
		else if (trans==1)
			copyscrollbitmap(bitmap,dec0_pf3_bitmap,lines*height,rscrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
		else
			copyscrollbitmap(bitmap,dec0_pf3_bitmap,lines*height,rscrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
	else { /* Scroll registers not enabled */
		if (trans==2)
			copyscrollbitmap(bitmap,dec0_tf3_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
		else if (trans==1)
			copyscrollbitmap(bitmap,dec0_pf3_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);
		else
			copyscrollbitmap(bitmap,dec0_pf3_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
}

/******************************************************************************/

void hbarrel_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	dec0_update_palette(dec0_pri & 0x01);

	dec0_pf1_update();
	dec0_pf3_update(0,0);
	dec0_pf2_update(1,0);
	dec0_pf3_draw(bitmap,0);
	dec0_drawsprites(bitmap,0x08,0x08);
	dec0_pf2_draw(bitmap,1);

	/* HB always keeps pf2 on top of pf3, no need explicitly support priority register */

	dec0_drawsprites(bitmap,0x08,0x00);
	dec0_pf1_draw(bitmap);
}

/******************************************************************************/

void baddudes_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	/* WARNING: priority inverted wrt all the other games */
	dec0_update_palette(~dec0_pri & 0x01);
	dec0_pf1_update();

	/* WARNING: inverted wrt Midnight Resistance */
	if ((dec0_pri & 0x01) == 0)
	{
		dec0_pf2_update(0,1); /* Foreground pens of pf2 only */
		dec0_pf2_update(0,0);
		dec0_pf3_update(1,1);
		dec0_pf3_update(1,0);

		dec0_pf2_draw(bitmap,0);
		dec0_pf3_draw(bitmap,1);

		if (dec0_pri & 2)
			dec0_pf2_draw(bitmap,2); /* Foreground pens only */

		dec0_drawsprites(bitmap,0x00,0x00);

		if (dec0_pri & 4)
			dec0_pf3_draw(bitmap,2); /* Foreground pens only */
	}
	else
	{
		dec0_pf3_update(0,1);
		dec0_pf3_update(0,0);
		dec0_pf2_update(1,1);
		dec0_pf2_update(1,0);

		dec0_pf3_draw(bitmap,0);
		dec0_pf2_draw(bitmap,1);

		if (dec0_pri & 2)
			dec0_pf3_draw(bitmap,2);

		dec0_drawsprites(bitmap,0x00,0x00);

		if (dec0_pri & 4)
			dec0_pf2_draw(bitmap,2);
	}

	dec0_pf1_draw(bitmap);
}

/******************************************************************************/

void robocop_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	dec0_update_palette(dec0_pri & 0x01);
	dec0_pf1_update();

	if (dec0_pri & 0x01)
	{
		int trans;

		dec0_pf2_update(0,0);
		dec0_pf3_update(1,0);

		/* WARNING: inverted wrt Midnight Resistance */
		/* Robocop uses it only for the title screen, so this might be just */
		/* completely wrong. The top 8 bits of the register might mean */
		/* something (they are 0x80 in midres, 0x00 here) */
		if (dec0_pri & 0x04)
			trans = 0x08;
		else trans = 0x00;

		dec0_pf2_draw(bitmap,0);

		if (dec0_pri & 0x02)
			dec0_drawsprites(bitmap,0x08,trans);

		dec0_pf3_draw(bitmap,1);

		if (dec0_pri & 0x02)
			dec0_drawsprites(bitmap,0x08,trans ^ 0x08);
		else
			dec0_drawsprites(bitmap,0x00,0x00);
	}
	else
	{
		int trans;

		dec0_pf3_update(0,0);
		dec0_pf2_update(1,0);

		/* WARNING: inverted wrt Midnight Resistance */
		/* Robocop uses it only for the title screen, so this might be just */
		/* completely wrong. The top 8 bits of the register might mean */
		/* something (they are 0x80 in midres, 0x00 here) */
		if (dec0_pri & 0x04)
			trans = 0x08;
		else trans = 0x00;

		dec0_pf3_draw(bitmap,0);

		if (dec0_pri & 0x02)
			dec0_drawsprites(bitmap,0x08,trans);

		dec0_pf2_draw(bitmap,1);

		if (dec0_pri & 0x02)
			dec0_drawsprites(bitmap,0x08,trans ^ 0x08);
		else
			dec0_drawsprites(bitmap,0x00,0x00);
	}

	dec0_pf1_draw(bitmap);

}

/******************************************************************************/

void birdtry_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	dec0_update_palette(dec0_pri & 0x01);

	/* This game doesn't have the extra playfield chip on the game board */
	dec0_pf1_update();
	dec0_pf2_update(0,0);
	dec0_pf2_draw(bitmap,0);
	dec0_drawsprites(bitmap,0x00,0x00);
	dec0_pf1_draw(bitmap);
}

/******************************************************************************/

void hippodrm_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	dec0_update_palette(dec0_pri & 0x01);
	dec0_pf1_update();

	if (dec0_pri & 0x01)
	{
		dec0_pf2_update(0,0);
		dec0_pf3_update(1,0);

		dec0_pf2_draw(bitmap,0);
		dec0_pf3_draw(bitmap,1);
	}
	else
	{
		dec0_pf3_update(0,0);
		dec0_pf2_update(1,0);

		dec0_pf3_draw(bitmap,0);
		dec0_pf2_draw(bitmap,1);
	}

	dec0_drawsprites(bitmap,0x00,0x00);
	dec0_pf1_draw(bitmap);
}

/******************************************************************************/

void slyspy_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	dec0_update_palette(0);

	dec0_pf1_update();
	dec0_pf3_update(0,0);
	dec0_pf2_update(1,1);
	dec0_pf2_update(1,0);

	dec0_pf3_draw(bitmap,0);
	dec0_pf2_draw(bitmap,1);

	dec0_drawsprites(bitmap,0x00,0x00);

	if (dec0_pri&0x80)
		dec0_pf2_draw(bitmap,2);

	dec0_pf1_draw(bitmap);
}

/******************************************************************************/

void midres_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	dec0_update_palette(dec0_pri & 0x01);
	dec0_pf1_update();

if (dec0_pri & 0x01)
{
	int trans;

	dec0_pf2_update(0,0);
	dec0_pf3_update(1,0);

	if (dec0_pri & 0x04)
		trans = 0x00;
	else trans = 0x08;

	dec0_pf2_draw(bitmap,0);

	if (dec0_pri & 0x02)
		dec0_drawsprites(bitmap,0x08,trans);

	dec0_pf3_draw(bitmap,1);

	if (dec0_pri & 0x02)
		dec0_drawsprites(bitmap,0x08,trans ^ 0x08);
	else
		dec0_drawsprites(bitmap,0x00,0x00);
}
else
{
	int trans;

	dec0_pf3_update(0,0);
	dec0_pf2_update(1,0);

	if (dec0_pri & 0x04)
		trans = 0x00;
	else trans = 0x08;

	dec0_pf3_draw(bitmap,0);

	if (dec0_pri & 0x02)
		dec0_drawsprites(bitmap,0x08,trans);

	dec0_pf2_draw(bitmap,1);

	if (dec0_pri & 0x02)
		dec0_drawsprites(bitmap,0x08,trans ^ 0x08);
	else
		dec0_drawsprites(bitmap,0x00,0x00);
	}

	dec0_pf1_draw(bitmap);
}

/******************************************************************************/

WRITE_HANDLER( dec0_pf1_control_0_w )
{
	COMBINE_WORD_MEM(&dec0_pf1_control_0[offset],data);
}

WRITE_HANDLER( dec0_pf1_control_1_w )
{
	COMBINE_WORD_MEM(&dec0_pf1_control_1[offset],data);
}

WRITE_HANDLER( dec0_pf1_rowscroll_w )
{
	COMBINE_WORD_MEM(&dec0_pf1_rowscroll[offset],data);
}

WRITE_HANDLER( dec0_pf1_colscroll_w )
{
	COMBINE_WORD_MEM(&dec0_pf1_colscroll[offset],data);
}

WRITE_HANDLER( dec0_pf1_data_w )
{
	int oldword = READ_WORD(&dec0_pf1_data[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&dec0_pf1_data[offset],newword);
		dec0_pf1_dirty[offset] = 1;
	}
}

READ_HANDLER( dec0_pf1_data_r )
{
	return READ_WORD(&dec0_pf1_data[offset]);
}

WRITE_HANDLER( dec0_pf2_control_0_w )
{
	COMBINE_WORD_MEM(&dec0_pf2_control_0[offset],data);
}

WRITE_HANDLER( dec0_pf2_control_1_w )
{
	COMBINE_WORD_MEM(&dec0_pf2_control_1[offset],data);
}

WRITE_HANDLER( dec0_pf2_rowscroll_w )
{
	COMBINE_WORD_MEM(&dec0_pf2_rowscroll[offset],data);
}

WRITE_HANDLER( dec0_pf2_colscroll_w )
{
	COMBINE_WORD_MEM(&dec0_pf2_colscroll[offset],data);
}

WRITE_HANDLER( dec0_pf2_data_w )
{
	int oldword = READ_WORD(&dec0_pf2_data[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&dec0_pf2_data[offset],newword);
		dec0_pf2_dirty[offset] = 1;
	}
}

READ_HANDLER( dec0_pf2_data_r )
{
	return READ_WORD(&dec0_pf2_data[offset]);
}

WRITE_HANDLER( dec0_pf3_control_0_w )
{
	COMBINE_WORD_MEM(&dec0_pf3_control_0[offset],data);
}

WRITE_HANDLER( dec0_pf3_control_1_w )
{
	COMBINE_WORD_MEM(&dec0_pf3_control_1[offset],data);
}

WRITE_HANDLER( dec0_pf3_rowscroll_w )
{
	COMBINE_WORD_MEM(&dec0_pf3_rowscroll[offset],data);
}

WRITE_HANDLER( dec0_pf3_colscroll_w )
{
	COMBINE_WORD_MEM(&dec0_pf3_colscroll[offset],data);
}

READ_HANDLER( dec0_pf3_colscroll_r )
{
	return READ_WORD(&dec0_pf3_colscroll[offset]);
}

WRITE_HANDLER( dec0_pf3_data_w )
{
	int oldword = READ_WORD(&dec0_pf3_data[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&dec0_pf3_data[offset],newword);
		dec0_pf3_dirty[offset] = 1;
	}
}

READ_HANDLER( dec0_pf3_data_r )
{
	return READ_WORD(&dec0_pf3_data[offset]);
}

WRITE_HANDLER( dec0_priority_w )
{
  	dec0_pri = COMBINE_WORD(dec0_pri,data);
}

WRITE_HANDLER( dec0_pf3_control_8bit_w )
{
	static int buffer[0x20];
	int myword;

	buffer[offset]=data;

	/* Rearrange little endian bytes from H6280 into big endian words for 68k */
	offset&=0xffe;
	myword=buffer[offset] + (buffer[offset+1]<<8);

	if (offset<0x10) dec0_pf3_control_0_w(offset,myword);
	else dec0_pf3_control_1_w(offset-0x10,myword);
}

WRITE_HANDLER( dec0_pf3_data_8bit_w )
{
	if (offset&1) { /* MSB has changed */
		int lsb=READ_WORD(&dec0_pf3_data[offset&0x7fe]);
		int newword=(lsb&0xff) | (data<<8);
		WRITE_WORD(&dec0_pf3_data[offset&0x7fe],newword);
		dec0_pf3_dirty[offset&0x7fe] = 1;
	}
	else { /* LSB has changed */
		int msb=READ_WORD(&dec0_pf3_data[offset&0x7fe]);
		int newword=(msb&0xff00) | data;
		WRITE_WORD(&dec0_pf3_data[offset&0x7fe],newword);
		dec0_pf3_dirty[offset&0x7fe] = 1;
	}
}

READ_HANDLER( dec0_pf3_data_8bit_r )
{
	if (offset&1) /* MSB */
		return READ_WORD(&dec0_pf3_data[offset&0x7fe])>>8;

	return READ_WORD(&dec0_pf3_data[offset&0x7fe])&0xff;;
}

/******************************************************************************/

void dec0_nodma_vh_stop (void)
{
	bitmap_free(dec0_pf3_bitmap);
	bitmap_free(dec0_pf2_bitmap);
	bitmap_free(dec0_pf1_bitmap);
	bitmap_free(dec0_tf2_bitmap);
	bitmap_free(dec0_tf3_bitmap);
	free(dec0_pf3_dirty);
	free(dec0_pf2_dirty);
	free(dec0_pf1_dirty);
}

void dec0_vh_stop (void)
{
	free(dec0_spriteram);
	dec0_nodma_vh_stop();
}

int dec0_nodma_vh_start (void)
{
	/* Allocate bitmaps */
	if ((dec0_pf1_bitmap = bitmap_alloc(512,512)) == 0) {
		dec0_vh_stop ();
		return 1;
	}
	dec0_pf1_current_shape = 1;

	if ((dec0_pf2_bitmap = bitmap_alloc(512,512)) == 0) {
		dec0_vh_stop ();
		return 1;
	}
	dec0_pf2_current_shape = 1;

	if ((dec0_pf3_bitmap = bitmap_alloc(512,512)) == 0) {
		dec0_vh_stop ();
		return 1;
	}
	dec0_pf3_current_shape = 1;

	if ((dec0_tf2_bitmap = bitmap_alloc(512,512)) == 0) {
		dec0_vh_stop ();
		return 1;
	}

	if ((dec0_tf3_bitmap = bitmap_alloc(512,512)) == 0) {
		dec0_vh_stop ();
		return 1;
	}

	dec0_pf1_dirty = (unsigned char*)malloc(TEXTRAM_SIZE);
	dec0_pf3_dirty = (unsigned char*)malloc(TILERAM_SIZE);
	dec0_pf2_dirty = (unsigned char*)malloc(TILERAM_SIZE);

	memset(dec0_pf1_dirty,1,TEXTRAM_SIZE);
	memset(dec0_pf2_dirty,1,TILERAM_SIZE);
	memset(dec0_pf3_dirty,1,TILERAM_SIZE);

	dec0_spriteram=spriteram;

	return 0;
}

int dec0_vh_start (void)
{
	dec0_nodma_vh_start();
	dec0_spriteram=(unsigned char*)malloc(0x800);

	return 0;
}

/******************************************************************************/

