/*******************************************************************************

     Nova 2001 - Video Description:
     --------------------------------------------------------------------------

     Foreground Playfield Chars (static)
        character code index from first set of chars
     Foreground Playfield color modifier RAM
        Char colors are normally taken from the first set of 16,
        This is the pen for "color 1" for this tile (from the first 16 pens)

     Background Playfield Chars (scrolling)
        character code index from second set of chars
     Foreground Playfield color modifier RAM
        Char colors are normally taken from the second set of 16,
        This is the pen for "color 1" for this tile (from the second 16 pens)
     (Scrolling in controlled via the 8910 A and B port outputs)

     Sprite memory is made of 32 byte records:

        Sprite+0, 0x80 = Sprite Bank
        Sprite+0, 0x40 = Sprite Enable
        Sprite+0, 0x3f = Sprite Character Code
        Sprite+1, 0xff = X location
        Sprite+2, 0xff = Y location
        Sprite+3, 0x40 = Y Flip
        Sprite+3, 0x20 = X Flip
        Sprite+4, 0x0f = pen for "color 1" taken from the first 16 colors

        All the rest are unknown and/or uneccessary.

*******************************************************************************/

#include "vidhrdw/generic.h"

unsigned char *nova2001_videoram,*nova2001_colorram;
size_t nova2001_videoram_size;

static int nova2001_xscroll;
static int nova2001_yscroll;
static int flipscreen;



void nova2001_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i,j;


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int intensity;


		intensity = (*color_prom >> 0) & 0x03;
		/* red component */
		*(palette++) = (((*color_prom >> 0) & 0x0c) | intensity) * 0x11;
		/* green component */
		*(palette++) = (((*color_prom >> 2) & 0x0c) | intensity) * 0x11;
		/* blue component */
		*(palette++) = (((*color_prom >> 4) & 0x0c) | intensity) * 0x11;

		color_prom++;
	}

	/* Color #1 is used for palette animation.          */

	/* To handle this, color entries 0-15 are based on  */
	/* the primary 16 colors, while color entries 16-31 */
	/* are based on the secondary set.                  */

	/* The only difference among 0-15 and 16-31 is that */
	/* color #1 changes each time */

	for (i = 0;i < 16;i++)
	{
		for (j = 0;j < 16;j++)
		{
			if (j == 1)
			{
				colortable[16*i+1] = i;
				colortable[16*i+16*16+1] = i+16;
			}
			else
			{
				colortable[16*i+j] = j;
				colortable[16*i+16*16+j] = j+16;
			}
		}
	}
}



WRITE_HANDLER( nova2001_scroll_x_w )
{
	nova2001_xscroll = data;
}

WRITE_HANDLER( nova2001_scroll_y_w )
{
	nova2001_yscroll = data;
}



WRITE_HANDLER( nova2001_flipscreen_w )
{
	if ((~data & 0x01) != flipscreen)
	{
		flipscreen = ~data & 0x01;
		memset(dirtybuffer,1,videoram_size);
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void nova2001_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy;


			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
			}

			drawgfx(tmpbitmap,Machine->gfx[1],
					videoram[offs],
					colorram[offs] & 0x0f,
					flipscreen,flipscreen,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	{
		int scrollx,scrolly;

		if (flipscreen)
		{
			scrollx = nova2001_xscroll;
			scrolly = nova2001_yscroll;
		}
		else
		{
			scrollx = -nova2001_xscroll;
			scrolly = -nova2001_yscroll;
		}

	    copyscrollbitmap(bitmap,tmpbitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* Next, draw the sprites */
	for (offs = 0;offs < spriteram_size;offs += 32)
	{
		if (spriteram[offs+0] & 0x40)
		{
			int sx,sy,flipx,flipy;


			sx = spriteram[offs+1];
			sy = spriteram[offs+2];
			flipx = spriteram[offs+3] & 0x10;
			flipy = spriteram[offs+3] & 0x20;
			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(bitmap,Machine->gfx[2 + ((spriteram[offs+0] & 0x80) >> 7)],
					spriteram[offs+0] & 0x3f,
					spriteram[offs+3] & 0x0f,
					flipx,flipy,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}


	/* Finally, draw the foreground text */
	for (offs = nova2001_videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy;


		sx = offs % 32;
		sy = offs / 32;
		if (flipscreen)
		{
			sx = 31 - sx;
			sy = 31 - sy;
		}

		drawgfx(bitmap,Machine->gfx[0],
				nova2001_videoram[offs],
				nova2001_colorram[offs] & 0x0f,
				flipscreen,flipscreen,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
