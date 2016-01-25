#include "driver.h"
#include "vidhrdw/generic.h"


WRITE_HANDLER( avalnche_videoram_w )
{
	videoram[offset] = data;

	if (offset >= 0x200)
	{
		int x,y,i;

		x = 8 * (offset % 32);
		y = offset / 32;

		for (i = 0;i < 8;i++)
			plot_pixel(Machine->scrbitmap,x+7-i,y,Machine->pens[(data >> i) & 1]);
	}
}

void avalnche_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	if (full_refresh)
	{
		int offs;


		for (offs = 0;offs < videoram_size; offs++)
			avalnche_videoram_w(offs,videoram[offs]);
	}
}
