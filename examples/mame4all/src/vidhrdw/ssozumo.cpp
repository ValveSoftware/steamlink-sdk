/***************************************************************************

Syusse Oozumou
(c) 1984 Technos Japan (Licensed by Data East)

Driver by Takahiro Nogi (nogi@kt.rim.or.jp) 1999/10/04

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *ssozumo_videoram2, *ssozumo_colorram2;
size_t ssozumo_videoram2_size;
unsigned char *ssozumo_scroll;

#define TOTAL_COLORS(gfxn)	(Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
#define COLOR(gfxn,offs)	(colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])

/**************************************************************************/

void ssozumo_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable, const unsigned char *color_prom)
{
	int	bit0, bit1, bit2, bit3;
	int	i;

	for (i = 0 ; i < 64 ; i++)
	{
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[0] >> 4) & 0x01;
		bit1 = (color_prom[0] >> 5) & 0x01;
		bit2 = (color_prom[0] >> 6) & 0x01;
		bit3 = (color_prom[0] >> 7) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[64] >> 0) & 0x01;
		bit1 = (color_prom[64] >> 1) & 0x01;
		bit2 = (color_prom[64] >> 2) & 0x01;
		bit3 = (color_prom[64] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}
}


WRITE_HANDLER( ssozumo_paletteram_w )
{
	int	bit0, bit1, bit2, bit3, val;
	int	r, g, b;
	int	offs2;

	paletteram[offset] = data;
	offs2 = offset & 0x0f;

	val = paletteram[offs2];
	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;
	r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

	val = paletteram[offs2 | 0x10];
	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;
	g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

	val = paletteram[offs2 | 0x20];
	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;
	b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

	palette_change_color(offs2 + 64, r, g, b);
}


int ssozumo_vh_start(void)
{
	if ((dirtybuffer = (unsigned char*)malloc(videoram_size)) == 0)
	{
		return 1;
	}
	memset(dirtybuffer, 1, videoram_size);

	if ((tmpbitmap = bitmap_alloc(Machine->drv->screen_width, 2 * Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}

	return 0;
}


void ssozumo_vh_stop(void)
{
	free(dirtybuffer);
	bitmap_free(tmpbitmap);
}


void ssozumo_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int	offs;
	int	sx, sy;
	int	scrolly;


	if (palette_recalc())
		memset(dirtybuffer,1,videoram_size);

	/* Draw the background layer*/
	for (offs = (videoram_size - 1) ; offs >= 0 ; offs--)
	{
		if (dirtybuffer[offs])
		{
			dirtybuffer[offs] = 0;

			sx = (15 - offs / 32);
			sy = (offs % 32);

			drawgfx(tmpbitmap, Machine->gfx[1],
					videoram[offs] + ((colorram[offs] & 0x08) << 5),
					(colorram[offs] & 0x30) >> 4,
					0, sy >= 16,
					(16 * sx), (16 * sy),
					0, TRANSPARENCY_NONE, 0);
		}
	}

	/* Draw the front layer */
	scrolly = -*ssozumo_scroll;
	copyscrollbitmap(bitmap, tmpbitmap, 0, 0, 1, &scrolly, &Machine->visible_area, TRANSPARENCY_NONE, 0);

	for (offs = (ssozumo_videoram2_size - 1) ; offs >= 0 ; offs--)
	{
		sx = (31 - offs / 32);
		sy = (offs % 32);

		drawgfx(bitmap,Machine->gfx[0],
				ssozumo_videoram2[offs] + 256 * (ssozumo_colorram2[offs] & 0x07),
				(ssozumo_colorram2[offs] & 0x30) >> 4,
				0, 0,
				(8 * sx), (8 * sy),
				&Machine->visible_area, TRANSPARENCY_PEN, 0);
	}

	/* Draw the sprites layer */
	for (offs = 0 ; offs < spriteram_size ; offs += 4)
	{
		if (spriteram[offs] & 0x01)
		{
			drawgfx(bitmap, Machine->gfx[2],
					spriteram[offs+1] + ((spriteram[offs] & 0xf0) << 4),
					(spriteram[offs] & 0x08) >> 3,
					spriteram[offs] & 0x04,spriteram[offs] & 0x02,
					239 - spriteram[offs+3], (240 - spriteram[offs+2]) & 0xff,
					&Machine->visible_area, TRANSPARENCY_PEN, 0);
		}
	}
}
