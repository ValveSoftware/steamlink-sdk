/***************************************************************************

  vidhrdw.c

	Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int skydiver_lamps[8];
static int skydiver_width = 0;

WRITE_HANDLER( skydiver_width_w )
{
	skydiver_width = offset;
	//logerror("width: %02x\n", data);
}

WRITE_HANDLER( skydiver_sk_lamps_w )
{
	switch (offset)
	{
		case 0:	skydiver_lamps[0] = 0;	break;
		case 1:	skydiver_lamps[0] = 1;	break;	/* S */
		case 2:	skydiver_lamps[1] = 0;	break;
		case 3:	skydiver_lamps[1] = 1;	break;	/* K */
	}
}

WRITE_HANDLER( skydiver_yd_lamps_w )
{
	switch (offset)
	{
		case 0:	skydiver_lamps[2] = 0;	break;
		case 1:	skydiver_lamps[2] = 1;	break;	/* Y */
		case 2:	skydiver_lamps[3] = 0;	break;
		case 3:	skydiver_lamps[3] = 1;	break;	/* D */
	}
}

WRITE_HANDLER( skydiver_iver_lamps_w )
{
	switch (offset)
	{
		case 0:	skydiver_lamps[4] = 0;	break;
		case 1:	skydiver_lamps[4] = 1;	break;	/* I */
		case 2:	skydiver_lamps[5] = 0;	break;
		case 3:	skydiver_lamps[5] = 1;	break;	/* V */
		case 4:	skydiver_lamps[6] = 0;	break;
		case 5:	skydiver_lamps[6] = 1;	break;	/* E */
		case 6:	skydiver_lamps[7] = 0;	break;
		case 7:	skydiver_lamps[7] = 1;	break;	/* R */
	}
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void skydiver_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;
	int pic;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int charcode;
			int color;
			int sx,sy;

			dirtybuffer[offs]=0;

			charcode = videoram[offs] & 0x3F;
			color    = (videoram[offs] & 0xc0) >> 6;

			sx = 8 * (offs % 32);
			sy = 8 * (offs / 32);
			drawgfx(tmpbitmap,Machine->gfx[0],
				charcode, color,
				0,0,sx,sy,
				&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* Draw "SKYDIVER" lights? */
	{
		int light;

		for (light = 0; light < 8; light++)
		{
			char *text = "SKYDIVER";

			drawgfx(bitmap,Machine->gfx[0],
				text[light], skydiver_lamps[light] + 4,
				0,0,light*8,28*8,
				&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}


	/* Draw each one of our four motion objects */
	for (pic=3;pic>=0;pic--)
	{
		int sx,sy;
		int charcode;
		int xflip, yflip;
		int color;

		sx = 29*8 - spriteram[pic];
		sy = 30*8 - spriteram[pic*2 + 8];
		charcode = spriteram[pic*2 + 9];
		xflip = (charcode & 0x10) >> 4;
		yflip = (charcode & 0x08) >> 3;
		charcode = (charcode & 0x07) | ((charcode & 0x60) >> 2);
		color = pic & 0x01;

		drawgfx(bitmap,Machine->gfx[1+(charcode >= 0x10)],
			charcode, color,
			xflip,yflip,sx,sy,
			&Machine->visible_area,TRANSPARENCY_PEN,0);

	}
}


