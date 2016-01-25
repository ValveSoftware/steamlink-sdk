/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



/* This is strange; it's unlikely that the sprites actually have a hardware */
/* clipping region, but I haven't found another way to have them masked by */
/* the characters at the top and bottom of the screen. */
static struct rectangle spritevisiblearea =
{
	0*8, 32*8-1,
	4*8, 29*8-1
};



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Ping Pong has a 32 bytes palette PROM and two 256 bytes color lookup table
  PROMs (one for sprites, one for characters).
  I don't know for sure how the palette PROM is connected to the RGB output,
  but it's probably the usual:

  bit 7 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

***************************************************************************/
void pingpong_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])

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

	/* color_prom now points to the beginning of the char lookup table */

	/* sprites */
	for (i = 0;i < TOTAL_COLORS(1);i++)
	{
		int code;
		int bit0,bit1,bit2,bit3;

		/* the bits of the color code are in reverse order - 0123 instead of 3210 */
		code = *(color_prom++) & 0x0f;
		bit0 = (code >> 0) & 1;
		bit1 = (code >> 1) & 1;
		bit2 = (code >> 2) & 1;
		bit3 = (code >> 3) & 1;
		code = (bit0 << 3) | (bit1 << 2) | (bit2 << 1) | (bit3 << 0);
		COLOR(1,i) = code;
	}

	/* characters */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = (*(color_prom++) & 0x0f) + 0x10;
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.

***************************************************************************/
void pingpong_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,flipx,flipy,tchar,color;


			sx = offs % 32;
			sy = offs / 32;

			dirtybuffer[offs] = 0;

			flipx = colorram[offs] & 0x40;
			flipy = colorram[offs] & 0x80;
			color = colorram[offs] & 0x1F;
			tchar = (videoram[offs] + ((colorram[offs] & 0x20)<<3));

			drawgfx(tmpbitmap,Machine->gfx[0],
					tchar,
					color,
					flipx,flipy,
					8 * sx,8 * sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int sx,sy,flipx,flipy,color,schar;


		sx = spriteram[offs + 3];
		sy = 241 - spriteram[offs + 1];

		flipx = spriteram[offs] & 0x40;
		flipy = spriteram[offs] & 0x80;
		color = spriteram[offs] & 0x1F;
		schar = spriteram[offs + 2] & 0x7F;

		drawgfx(bitmap,Machine->gfx[1],
				schar,
				color,
				flipx,flipy,
				sx,sy,
				&spritevisiblearea,TRANSPARENCY_COLOR,0);
	}
}
