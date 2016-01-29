/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *funkyb_row_scroll;

static int gfx_bank;


void funkybee_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


	/* first, the character/sprite palette */
	for (i = 0;i < 32;i++)
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


WRITE_HANDLER( funkybee_gfx_bank_w )
{
	if (data != (gfx_bank & 0x01))
	{
		gfx_bank = data & 0x01;
		memset(dirtybuffer, 1, videoram_size);
	}
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
static void draw_chars(struct osd_bitmap *_tmpbitmap, struct osd_bitmap *bitmap)
{
	int sx,sy;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (sy = 0x1f;sy >= 0;sy--)
	{
		int offs;


		offs = (sy << 8) | 0x1f;

		for (sx = 0x1f;sx >= 0;sx--,offs--)
		{
			if (dirtybuffer[offs])
			{
				dirtybuffer[offs] = 0;

				drawgfx(_tmpbitmap,Machine->gfx[gfx_bank],
						videoram[offs],
						colorram[offs] & 0x03,
						0,0,
						8*sx,8*sy,
						0,TRANSPARENCY_NONE,0);
			}
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int offs,scroll[32];


		for (offs = 0;offs < 28;offs++)
			scroll[offs] = -*funkyb_row_scroll;

		for (;offs < 32;offs++)
			scroll[offs] = 0;

		copyscrollbitmap(bitmap,_tmpbitmap,32,scroll,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
}


void funkybee_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	draw_chars(tmpbitmap, bitmap);


	/* draw the sprites */
	for (offs = 0x0f; offs >= 0; offs--)
	{
		int sx,sy,code,col,flipy,offs2;


		offs2 = 0x1e00 + offs;

		code  = videoram[offs2];
		sx    = videoram[offs2 + 0x10];
		sy    = 224 - colorram[offs2];
		col   = colorram[offs2 + 0x10];
		flipy = code & 0x01;

		drawgfx(bitmap,Machine->gfx[2+gfx_bank],
				(code >> 2) | ((code & 2) << 5),
				col,
				0,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}


	/* draw the two variable position columns */
	for (offs = 0x1f;offs >= 0;offs--)
	{
		drawgfx(bitmap,Machine->gfx[gfx_bank],
				videoram[offs+0x1c00],
				colorram[0x1f10] & 0x03,
				0,0,
				videoram[0x1f10],8*offs,
				0,TRANSPARENCY_PEN,0);

		drawgfx(bitmap,Machine->gfx[gfx_bank],
				videoram[offs+0x1d00],
				colorram[0x1f11] & 0x03,
				0,0,
				videoram[0x1f11],8*offs,
				0,TRANSPARENCY_PEN,0);
	}
}

