/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* machine/nitedrvr.c */
extern int nitedrvr_gear;
extern int nitedrvr_track;

/* local */
unsigned char *nitedrvr_hvc;

WRITE_HANDLER( nitedrvr_hvc_w )
{
	nitedrvr_hvc[offset & 0x3f] = data;

//	if ((offset & 0x30) == 0x30)
//		;		/* Watchdog called here */

	return;
}

static void nitedrvr_draw_block(struct osd_bitmap *bitmap, int bx, int by, int ex, int ey)
{
	int x,y;

	for (y=by; y<ey; y++)
	{
		for (x=bx; x<ex; x++)
		{
			if ((y<256) && (x<256))
				plot_pixel(bitmap, x, y, Machine->pens[1]);
		}
	}

	return;
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void nitedrvr_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,roadway;
	char gear_buf[] =  {0x07,0x05,0x01,0x12,0x00,0x00}; /* "GEAR  " */
	char track_buf[] = {0x0e,0x0f,0x16,0x09,0x03,0x05, /* "NOVICE" */
						0x05,0x18,0x10,0x05,0x12,0x14, /* "EXPERT" */
						0x00,0x00,0x00,0x10,0x12,0x0f};/* "   PRO" */

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int charcode;
			int sx,sy;

			dirtybuffer[offs]=0;

			charcode = videoram[offs] & 0x3f;

			sx = 8 * (offs % 32);
			sy = 16 * (offs / 32);
			drawgfx(tmpbitmap,Machine->gfx[0],
					charcode, 0,
					0,0,sx,sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* Draw roadway */
	for (roadway=0; roadway < 16; roadway++)
	{
		int bx, by, ex, ey;

		bx = nitedrvr_hvc[roadway];
		by = nitedrvr_hvc[roadway + 16];
		ex = bx + ((nitedrvr_hvc[roadway + 32] & 0xf0) >> 4);
		ey = by + (16 - (nitedrvr_hvc[roadway + 32] & 0x0f));

		nitedrvr_draw_block(bitmap,bx,by,ex,ey);
	}

	/* Draw the "car backlight" */
	nitedrvr_draw_block(bitmap,64,232,192,256);

	/* gear shift indicator - not a part of the original game!!! */
	gear_buf[5]=0x30 + nitedrvr_gear;
	for (offs = 0; offs < 6; offs++)
		drawgfx(bitmap,Machine->gfx[0],
				gear_buf[offs],0,
				0,0,(offs)*8,31*8,
				&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* track indicator - not a part of the original game!!! */
	for (offs = 0; offs < 6; offs++)
		drawgfx(bitmap,Machine->gfx[0],
				track_buf[offs + 6*nitedrvr_track],0,
				0,0,(offs+26)*8,31*8,
				&Machine->visible_area,TRANSPARENCY_NONE,0);
}
