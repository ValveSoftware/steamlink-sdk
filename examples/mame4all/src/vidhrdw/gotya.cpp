#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *gotya_scroll;
unsigned char *gotya_foregroundram;

static int scroll_bit_8;


/***************************************************************************

  Convert the color PROMs into a more useable format.

  I'm using Pac Man resistor values

***************************************************************************/
void gotya_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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

	color_prom += 0x18;
	/* color_prom now points to the beginning of the lookup table */

	/* character lookup table */
	/* sprites use the same color lookup table as characters */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = *(color_prom++) & 0x07;
}


WRITE_HANDLER( gotya_video_control_w )
{
	/* bit 0 - scroll bit 8
	   bit 1 - flip screen
	   bit 2 - sound disable ??? */

	scroll_bit_8 = data & 0x01;

	flip_screen_w(offset, data & 0x02);
}


int gotya_vh_start(void)
{
	if ((dirtybuffer = (unsigned char*)malloc(videoram_size)) == 0)
	{
		return 1;
	}

	/* the background area is twice as wide as the screen (actually twice as tall, */
	/* because this is a vertical game) */
	if ((tmpbitmap = bitmap_alloc(2*256,Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer);
		return 1;
	}

	return 0;
}


static void draw_status_row(struct osd_bitmap *bitmap, int sx, int col)
{
	int row;

	if (flip_screen)
	{
		sx = 35 - sx;
	}

	for (row = 29;row >= 0;row--)
	{
		int sy;

		if (flip_screen)
		{
			sy = row;
		}
		else
		{
			sy = 31 - row;
		}

		drawgfx(bitmap,Machine->gfx[0],
				gotya_foregroundram[row * 32 + col],
				gotya_foregroundram[row * 32 + col + 0x10] & 0x0f,
				flip_screen, flip_screen,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
}


void gotya_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	if (full_refresh)
	{
		memset(dirtybuffer,1,videoram_size);
	}


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
    for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
	        int sx,sy;


			dirtybuffer[offs] = 0;

			sx = 31 - (offs % 32);
			sy = 31 - ((offs & 0x03ff) / 32);

			if (flip_screen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
			}

			if (offs < 0x0400)
			{
				sx = sx + 32;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs],
					colorram[offs] & 0x0f,
					flip_screen,flip_screen,
					8*sx, 8*sy,
					0,TRANSPARENCY_NONE,0);
		}
    }


	/* copy the background graphics */
	{
		int scroll;


		scroll = *gotya_scroll + (scroll_bit_8 * 256) + 16;

		copyscrollbitmap(bitmap,tmpbitmap,1,&scroll,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* draw the sprites */
	for (offs = 2; offs < 0x0e; offs += 2)
	{
		int code,col,sx,sy;


		code = spriteram[offs + 0x01] >> 2;
		col  = spriteram[offs + 0x11] & 0x0f;
		sx   = 256 - spriteram[offs + 0x10] + (spriteram[offs + 0x01] & 0x01) * 256;
		sy   =       spriteram[offs + 0x00];

		if (flip_screen)
		{
			sy = 240 - sy;
		}

		drawgfx(bitmap,Machine->gfx[1],
				code,col,
				flip_screen,flip_screen,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}


	/* draw the status lines */
	draw_status_row(bitmap, 0,  1);
	draw_status_row(bitmap, 1,  0);
	draw_status_row(bitmap, 2,  2);		/* these two are blank, but I dont' know if the data comes */
	draw_status_row(bitmap, 33, 13);	/* from RAM or 'hardcoded' into the hardware. Likely the latter */
	draw_status_row(bitmap, 35, 14);
	draw_status_row(bitmap, 34, 15);
}
