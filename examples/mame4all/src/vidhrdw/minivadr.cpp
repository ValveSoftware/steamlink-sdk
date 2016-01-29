/***************************************************************************

Minivader (Space Invaders's mini game)
(c)1990 Taito Corporation

Driver by Takahiro Nogi (nogi@kt.rim.or.jp) 1999/12/19 -

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



/*******************************************************************

	Palette Setting.

*******************************************************************/
static unsigned char minivadr_palette[] =
{
	0x00,0x00,0x00,			/* black */
	0xff,0xff,0xff			/* white */
};

void minivadr_init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette, minivadr_palette, sizeof(minivadr_palette));
}


/*******************************************************************

	Draw Pixel.

*******************************************************************/
WRITE_HANDLER( minivadr_videoram_w )
{
	int i;
	int x, y;
	int color;


	videoram[offset] = data;

	x = (offset % 32) * 8;
	y = (offset / 32);

	if (x >= Machine->visible_area.min_x &&
			x <= Machine->visible_area.max_x &&
			y >= Machine->visible_area.min_y &&
			y <= Machine->visible_area.max_y)
	{
		for (i = 0; i < 8; i++)
		{
			color = Machine->pens[((data >> i) & 0x01)];

			plot_pixel(Machine->scrbitmap, x + (7 - i), y, color);
		}
	}
}


void minivadr_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	if (full_refresh)
	{
		int offs;

		/* redraw bitmap */

		for (offs = 0; offs < videoram_size; offs++)
			minivadr_videoram_w(offs,videoram[offs]);
	}
}
