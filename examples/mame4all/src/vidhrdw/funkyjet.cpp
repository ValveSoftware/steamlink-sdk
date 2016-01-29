/***************************************************************************

   Funky Jet Video emulation - Bryan McPhail, mish@tendril.co.uk

   Cut & paste from Super Burgertime for now! :)

***************************************************************************/
 
#include "driver.h"
#include "vidhrdw/generic.h"

#define TILERAM_SIZE	0x2000

unsigned char *funkyjet_pf2_data,*funkyjet_pf1_data,*funkyjet_pf1_row;
static unsigned char *funkyjet_pf1_dirty,*funkyjet_pf2_dirty;
static struct osd_bitmap *funkyjet_pf1_bitmap;
static struct osd_bitmap *funkyjet_pf2_bitmap;

static unsigned char funkyjet_control_0[16];
static int offsetx[4],offsety[4];

/******************************************************************************/

static void funkyjet_update_palette(void)
{
	int offs,color,code,i,pal_base;
	int colmask[16];
    unsigned int *pen_usage;

	palette_init_used_colors();

	pen_usage=Machine->gfx[1]->pen_usage;
	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0; offs < TILERAM_SIZE;offs += 2)
	{
		code = READ_WORD(&funkyjet_pf2_data[offs]);
		color = (code & 0xf000) >> 12;
		code &= 0x0fff;
		colmask[color] |= pen_usage[code];
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

	pen_usage=Machine->gfx[0]->pen_usage;
	pal_base = Machine->drv->gfxdecodeinfo[0].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0; offs < TILERAM_SIZE;offs += 2)
	{
		code = READ_WORD(&funkyjet_pf1_data[offs]);
		color = (code & 0xf000) >> 12;
		code &= 0x0fff;
		colmask[color] |= pen_usage[code];
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

	/* Sprites */
	pen_usage=Machine->gfx[2]->pen_usage;
	pal_base = Machine->drv->gfxdecodeinfo[2].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;

	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,multi;

		sprite = READ_WORD (&spriteram[offs+2]) & 0x1fff;
		if (!sprite) continue;

		y = READ_WORD(&spriteram[offs]);
		x = READ_WORD(&spriteram[offs+4]);
		color = (x >>9) &0xf;

		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

		sprite &= ~multi;

		while (multi >= 0)
		{
			colmask[color] |= pen_usage[(sprite + multi)&0x1fff];
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
		memset(funkyjet_pf2_dirty,1,TILERAM_SIZE);
		memset(funkyjet_pf1_dirty,1,TILERAM_SIZE);
	}
}

static void funkyjet_drawsprites(struct osd_bitmap *bitmap)
{
	int offs;

	for (offs = 0;offs < 0x800;offs += 8)
	{
		int x,y,sprite,colour,multi,fx,fy,inc,flash;

		sprite = READ_WORD (&spriteram[offs+2]) & 0x1fff;
		if (!sprite) continue;

		y = READ_WORD(&spriteram[offs]);
		flash=y&0x1000;
		if (flash && (cpu_getcurrentframe() & 1)) continue;

		x = READ_WORD(&spriteram[offs+4]);
		colour = (x >>9) & 0xf;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 320) x -= 512;
		if (y >= 256) y -= 512;
		y = 240 - y;
        x = 304 - x;

		if (x>320) continue;

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
			drawgfx(bitmap,Machine->gfx[2],
					sprite - multi * inc,
					colour,
					fx,fy,
					x,y - 16 * multi,
					&Machine->visible_area,TRANSPARENCY_PEN,0);

			multi--;
		}
	}
}

static void funkyjet_pf1_update(void)
{
	int offs,mx,my,color,tile;

	mx = -1;
	my = 0;
	for (offs = 0; offs < 0x2000 ;offs += 2)
	{
		mx++;
		if (mx == 64)
		{
			mx = 0;
			my++;
		}

		if (funkyjet_pf1_dirty[offs])
		{
			funkyjet_pf1_dirty[offs] = 0;
			tile = READ_WORD(&funkyjet_pf1_data[offs]);
			color = (tile & 0xf000) >> 12;

			drawgfx(funkyjet_pf1_bitmap,Machine->uifont, //gfx[0],
					tile & 0x0fff,
					color,
					0,0,
					8*mx,8*my,
					0,TRANSPARENCY_NONE,0);
		}
	}
}

static void funkyjet_pf2_update(void)
{
	int offs,mx,my,color,tile,quarter;

	for (quarter = 0;quarter < 4;quarter++)
	{
		mx = -1;
		my = 0;

  		for (offs = 0x400 * quarter; offs < 0x400 * quarter + 0x400;offs += 2)
		{
			mx++;
			if (mx == 32)
			{
				mx = 0;
				my++;
			}

			if (funkyjet_pf2_dirty[offs])
			{
				funkyjet_pf2_dirty[offs] = 0;
				tile = READ_WORD(&funkyjet_pf2_data[offs]);
				color = (tile & 0xf000) >> 12;

				drawgfx(funkyjet_pf2_bitmap,Machine->gfx[1],
						tile & 0x0fff,
						color,
						0,0,
						16*mx + offsetx[quarter],16*my + offsety[quarter],
						0,TRANSPARENCY_NONE,0);
			}
		}
	}
}

/******************************************************************************/

void funkyjet_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int scrollx,scrolly;

	funkyjet_update_palette();
	funkyjet_pf1_update();
	funkyjet_pf2_update();

	/* Background */
	scrollx=-READ_WORD (&funkyjet_control_0[6]);
	scrolly=-READ_WORD (&funkyjet_control_0[8]);
 	copyscrollbitmap(bitmap,funkyjet_pf2_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* Sprites */
//	funkyjet_drawsprites(bitmap);

	/* Foreground */
	scrollx=-READ_WORD (&funkyjet_control_0[2]);
	scrolly=-READ_WORD (&funkyjet_control_0[4]);

	/* 'Fake' rowscroll, used only in the end game message */
	if (READ_WORD (&funkyjet_control_0[0xc])==0xc0)
		scrollx=-READ_WORD (&funkyjet_pf1_row[8]);
	copyscrollbitmap(bitmap,funkyjet_pf1_bitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);

	funkyjet_drawsprites(bitmap);
}

/******************************************************************************/

WRITE_HANDLER( funkyjet_pf2_data_w )
{
	int oldword = READ_WORD(&funkyjet_pf2_data[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&funkyjet_pf2_data[offset],newword);
		funkyjet_pf2_dirty[offset] = 1;
	}
}

WRITE_HANDLER( funkyjet_pf1_data_w )
{
	int oldword = READ_WORD(&funkyjet_pf1_data[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&funkyjet_pf1_data[offset],newword);
		funkyjet_pf1_dirty[offset] = 1;
	}
}

READ_HANDLER( funkyjet_pf1_data_r )
{
	return READ_WORD(&funkyjet_pf1_data[offset]);
}

READ_HANDLER( funkyjet_pf2_data_r )
{
	return READ_WORD(&funkyjet_pf2_data[offset]);
}

WRITE_HANDLER( funkyjet_control_0_w )
{
	COMBINE_WORD_MEM(&funkyjet_control_0[offset],data);
}

/******************************************************************************/

void funkyjet_vh_stop (void)
{
	bitmap_free(funkyjet_pf2_bitmap);
	bitmap_free(funkyjet_pf1_bitmap);
	free(funkyjet_pf1_dirty);
	free(funkyjet_pf2_dirty);
}

int funkyjet_vh_start(void)
{
	/* Allocate bitmaps */
	if ((funkyjet_pf1_bitmap = bitmap_alloc(512,512)) == 0) {
		funkyjet_vh_stop ();
		return 1;
	}

	if ((funkyjet_pf2_bitmap = bitmap_alloc(1024,512)) == 0) {
		funkyjet_vh_stop ();
		return 1;
	}

	funkyjet_pf1_dirty = (unsigned char*)malloc(TILERAM_SIZE);
	funkyjet_pf2_dirty = (unsigned char*)malloc(TILERAM_SIZE);
	memset(funkyjet_pf2_dirty,1,TILERAM_SIZE);
	memset(funkyjet_pf1_dirty,1,TILERAM_SIZE);

	offsetx[0] = 0;
	offsetx[1] = 0;
	offsetx[2] = 512;
	offsetx[3] = 512;
	offsety[0] = 0;
	offsety[1] = 256;
	offsety[2] = 0;
	offsety[3] = 256;

	return 0;
}
