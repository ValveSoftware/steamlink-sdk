/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int spritectrl[3];
static int flipscreen;

int crbaloon_collision;

/***************************************************************************

  Convert the color PROMs into a more useable format.

  Crazy Balloon has no PROMs, the color code directly maps to a color:
  all bits are inverted
  bit 3 HALF (intensity)
  bit 2 BLUE
  bit 1 GREEN
  bit 0 RED

***************************************************************************/
void crbaloon_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int intensity;


		intensity = (~i & 0x08) ? 0xff : 0x55;

		/* red component */
		*(palette++) = intensity * ((~i >> 0) & 1);
		/* green component */
		*(palette++) = intensity * ((~i >> 1) & 1);
		/* blue component */
		*(palette++) = intensity * ((~i >> 2) & 1);
	}

	for (i = 0;i < TOTAL_COLORS(0);i += 2)
	{
		COLOR(0,i) = 15;		/* black background */
		COLOR(0,i + 1) = i / 2;	/* colored foreground */
	}
}



WRITE_HANDLER( crbaloon_spritectrl_w )
{
	spritectrl[offset] = data;
}



WRITE_HANDLER( crbaloon_flipscreen_w )
{
	if (flipscreen != (data & 1))
	{
		flipscreen = data & 1;
		memset(dirtybuffer,1,videoram_size);
	}
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

 ***************************************************************************/

void crbaloon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,x,y;
	int bx,by;

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
			if (flipscreen == 0)
			{
				sx = 31 - sx;
				sy = 35 - sy;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs],
					colorram[offs] & 0x0f,
					flipscreen,flipscreen,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);


    /* Check Collision - Draw balloon in background colour, if no */
    /* collision occured, bitmap will be same as tmpbitmap        */

	bx = spritectrl[1];
	by = spritectrl[2];

	drawgfx(bitmap,Machine->gfx[1],
			spritectrl[0] & 0x0f,
			15,
			0,0,
			bx,by,
			&Machine->visible_area,TRANSPARENCY_PEN,0);

    crbaloon_collision = 0;

	for (x = bx; x < bx + Machine->gfx[1]->width; x++)
	{
		for (y = by; y < by + Machine->gfx[1]->height; y++)
        {
			if ((x < Machine->visible_area.min_x) ||
			    (x > Machine->visible_area.max_x) ||
			    (y < Machine->visible_area.min_y) ||
			    (y > Machine->visible_area.max_y))
			{
				continue;
			}

        	if (read_pixel(bitmap, x, y) != read_pixel(tmpbitmap, x, y))
        	{
				crbaloon_collision = -1;
				break;
			}
        }
	}


	/* actually draw the balloon */

	drawgfx(bitmap,Machine->gfx[1],
			spritectrl[0] & 0x0f,
			(spritectrl[0] & 0xf0) >> 4,
			0,0,
			bx,by,
			&Machine->visible_area,TRANSPARENCY_PEN,0);
}
