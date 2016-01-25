/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *meteor_scrollram;


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void meteor_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* draw the characters as sprites because they could be overlapping */

	fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);


	for (offs = 0; offs < videoram_size; offs++)
	{
		int code,sx,sy,col;


		sy = 8 * (offs / 32) -  (meteor_scrollram[offs]       & 0x0f);
		sx = 8 * (offs % 32) + ((meteor_scrollram[offs] >> 4) & 0x0f);

		code = videoram[offs] + ((colorram[offs] & 0x01) << 8);
		col  = (~colorram[offs] >> 4) & 0x07;

		drawgfx(bitmap,Machine->gfx[0],
				code,
				col,
				0,0,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
