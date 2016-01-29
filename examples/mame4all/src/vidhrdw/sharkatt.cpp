/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int color_plane = 0;

/***************************************************************************
 sharkatt_vtcsel_w

 TODO:  This writes to a TMS9927 VTAC.  Do we care?
 **************************************************************************/
WRITE_HANDLER( sharkatt_vtcsel_w )
{
}

/***************************************************************************
 sharkatt_color_plane_w
 **************************************************************************/
WRITE_HANDLER( sharkatt_color_plane_w )
{
	/* D0-D3 = WS0-WS3, D4-D5 = RS0-RS1 */
	/* RS = CPU Memory Plane Read Multiplex Select */
	color_plane = (data & 0x3F);
}

/***************************************************************************
 sharkatt_color_map_w
 **************************************************************************/
WRITE_HANDLER( sharkatt_color_map_w )
{
    int vals[4] = {0x00,0x55,0xAA,0xFF};
    int r,g,b;

    r = vals[(data & 0x03) >> 0];
    g = vals[(data & 0x0C) >> 2];
    b = vals[(data & 0x30) >> 4];
	palette_change_color (offset,r,g,b);
}

/***************************************************************************
 sharkatt_videoram_w
 **************************************************************************/
WRITE_HANDLER( sharkatt_videoram_w )
{
	int i,x,y;


	videoram[offset] = data;

	x = offset / 32;
	y = 8 * (offset % 32);

	for (i = 0;i < 8;i++)
	{
		int col;

		col = Machine->pens[color_plane & 0x0F];

		if (data & 0x80)
		{
			plot_pixel2(tmpbitmap, Machine->scrbitmap, x, y, col);
		}
		else
		{
			plot_pixel2(tmpbitmap, Machine->scrbitmap, x, y, Machine->pens[0]);
		}

		y++;
		data <<= 1;
	}
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void sharkatt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	if (palette_recalc())
	{
		int offs;

		for (offs = 0;offs < videoram_size;offs++)
			sharkatt_videoram_w(offs,videoram[offs]);
	}

	if (full_refresh)
		/* copy the character mapped graphics */
		copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
}
