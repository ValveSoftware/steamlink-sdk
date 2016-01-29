/***************************************************************************

	vidhrdw.c

	Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int flipscreen = 0;
static int palettebank;



void tagteam_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2;


		/* red component */
		bit0 = (*color_prom >> 0) & 0x01;
		bit1 = (*color_prom >> 1) & 0x01;
		bit2 = (*color_prom >> 2) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (*color_prom >> 3) & 0x01;
		bit1 = (*color_prom >> 4) & 0x01;
		bit2 = (*color_prom >> 5) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = (*color_prom >> 6) & 0x01;
		bit2 = (*color_prom >> 7) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		color_prom++;
	}
}

READ_HANDLER( tagteam_mirrorvideoram_r )
{
	int x,y;

	/* swap x and y coordinates */
	x = offset / 32;
	y = offset % 32;
	offset = 32 * y + x;

	return videoram_r(offset);
}

READ_HANDLER( tagteam_mirrorcolorram_r )
{
	int x,y;

	/* swap x and y coordinates */
	x = offset / 32;
	y = offset % 32;
	offset = 32 * y + x;

	return colorram_r(offset);
}

WRITE_HANDLER( tagteam_mirrorvideoram_w )
{
	int x,y;

	/* swap x and y coordinates */
	x = offset / 32;
	y = offset % 32;
	offset = 32 * y + x;

	videoram_w(offset,data);
}

WRITE_HANDLER( tagteam_mirrorcolorram_w )
{
	int x,y;

	/* swap x and y coordinates */
	x = offset / 32;
	y = offset % 32;
	offset = 32 * y + x;

	colorram_w(offset,data);
}

WRITE_HANDLER( tagteam_control_w )
{
//logerror("%04x: control = %02x\n",cpu_get_pc(),data);

	/* bit 7 is the palette bank */
	palettebank = (data & 0x80) >> 7;
}


/***************************************************************************

Draw the game screen in the given osd_bitmap.
Do NOT call osd_update_display() from this function, it will be called by
the main emulation engine.

***************************************************************************/
static void drawchars(struct osd_bitmap *bitmap,int color)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. If the background is on, */
	/* draw characters as sprites */


	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy;

		if (dirtybuffer[offs])
		{
			dirtybuffer[offs] = 0;

			sx = 31 - offs % 32;
			sy = offs / 32;

			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
			}

			/*Someday when the proms are properly figured out, we can remove
			the color hack*/
			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + 256 * colorram[offs],
					2*color,	/* guess */
					flipscreen,flipscreen,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the temporary bitmap to the screen */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
}

static void drawsprites(struct osd_bitmap *bitmap,int color)
{
	int offs;

	/* Draw the sprites */
	for (offs = 0;offs < 0x20;offs += 4)
	{
		int sx,sy,flipx,flipy;
		int spritebank;

		if (!(videoram[offs + 0] & 0x01)) continue;

		sx = 240 - videoram[offs + 3];
		sy = 240 - videoram[offs + 2];

		flipx = videoram[offs + 0] & 0x04;
		flipy = videoram[offs + 0] & 0x02;
		spritebank = (videoram[offs] & 0x30) << 4;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;

			flipx = !flipx;
			flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[1],
				videoram[offs + 1] + 256 * spritebank,
				1+2*color,	/* guess */
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);

		sy += (flipscreen ? -256 : 256);

		// Wrap around
		drawgfx(bitmap,Machine->gfx[1],
				videoram[offs + 0x20] + 256 * spritebank,
				color,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

void tagteam_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	drawchars(bitmap,palettebank);

	drawsprites(bitmap,palettebank);
}

