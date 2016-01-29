/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



#define MAX_STARS 250
#define STARS_COLOR_BASE 32

unsigned char *galaga_starcontrol;
static unsigned int stars_scroll;

struct star
{
	int x,y,col,set;
};
static struct star stars[MAX_STARS];
static int total_stars;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Galaga has one 32x8 palette PROM and two 256x4 color lookup table PROMs
  (one for characters, one for sprites). Only the first 128 bytes of the
  lookup tables seem to be used.
  The palette PROM is connected to the RGB output this way:

  bit 7 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

***************************************************************************/
void galaga_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < 32;i++)
	{
		int bit0,bit1,bit2;


		bit0 = (color_prom[31-i] >> 0) & 0x01;
		bit1 = (color_prom[31-i] >> 1) & 0x01;
		bit2 = (color_prom[31-i] >> 2) & 0x01;
		palette[3*i] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = (color_prom[31-i] >> 3) & 0x01;
		bit1 = (color_prom[31-i] >> 4) & 0x01;
		bit2 = (color_prom[31-i] >> 5) & 0x01;
		palette[3*i + 1] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = 0;
		bit1 = (color_prom[31-i] >> 6) & 0x01;
		bit2 = (color_prom[31-i] >> 7) & 0x01;
		palette[3*i + 2] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
	}

	color_prom += 32;

	/* characters */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = 15 - (*(color_prom++) & 0x0f);

	color_prom += 128;

	/* sprites */
	for (i = 0;i < TOTAL_COLORS(1);i++)
	{
		if (i % 4 == 0) COLOR(1,i) = 0;	/* preserve transparency */
		else COLOR(1,i) = 15 - ((*color_prom & 0x0f)) + 0x10;

		color_prom++;
	}

	color_prom += 128;


	/* now the stars */
	for (i = 32;i < 32 + 64;i++)
	{
		int bits;
		int map[4] = { 0x00, 0x88, 0xcc, 0xff };

		bits = ((i-32) >> 0) & 0x03;
		palette[3*i] = map[bits];
		bits = ((i-32) >> 2) & 0x03;
		palette[3*i + 1] = map[bits];
		bits = ((i-32) >> 4) & 0x03;
		palette[3*i + 2] = map[bits];
	}
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int galaga_vh_start(void)
{
	int generator;
	int x,y;
	int set = 0;


	if (generic_vh_start() != 0)
		return 1;


	/* precalculate the star background */
	/* this comes from the Galaxian hardware, Galaga is probably different */
	total_stars = 0;
	generator = 0;

	for (y = 0;y <= 255;y++)
	{
		for (x = 511;x >= 0;x--)
		{
			int bit1,bit2;


			generator <<= 1;
			bit1 = (~generator >> 17) & 1;
			bit2 = (generator >> 5) & 1;

			if (bit1 ^ bit2) generator |= 1;

			if (((~generator >> 16) & 1) && (generator & 0xff) == 0xff)
			{
				int color;

				color = (~(generator >> 8)) & 0x3f;
				if (color && total_stars < MAX_STARS)
				{
					stars[total_stars].x = x;
					stars[total_stars].y = y;
					stars[total_stars].col = Machine->pens[color + STARS_COLOR_BASE];
					stars[total_stars].set = set;
					if (++set > 3)
						set = 0;

					total_stars++;
				}
			}
		}
	}

	return 0;
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void galaga_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
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
			int sx,sy,mx,my;


			dirtybuffer[offs] = 0;

		/* Even if Galaga's screen is 28x36, the memory layout is 32x32. We therefore */
		/* have to convert the memory coordinates into screen coordinates. */
		/* Note that 32*32 = 1024, while 28*36 = 1008: therefore 16 bytes of Video RAM */
		/* don't map to a screen position. We don't check that here, however: range */
		/* checking is performed by drawgfx(). */

			mx = offs % 32;
			my = offs / 32;

			if (my <= 1)
			{
				sx = my + 34;
				sy = mx - 2;
			}
			else if (my >= 30)
			{
				sx = my - 30;
				sy = mx - 2;
			}
			else
			{
				sx = mx + 2;
				sy = my - 2;
			}

			if (flip_screen)
			{
				sx = 35 - sx;
				sy = 27 - sy;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs],
					colorram[offs],
					flip_screen,flip_screen,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);


	/* Draw the sprites. */
	for (offs = 0;offs < spriteram_size;offs += 2)
	{
		if ((spriteram_3[offs + 1] & 2) == 0)
		{
			int code,color,flipx,flipy,sx,sy,sfa,sfb;


			code = spriteram[offs];
			color = spriteram[offs + 1];
			flipx = spriteram_3[offs] & 1;
			flipy = spriteram_3[offs] & 2;
			sx = spriteram_2[offs + 1] - 40 + 0x100*(spriteram_3[offs + 1] & 1);
			sy = 28*8 - spriteram_2[offs];
			sfa = 0;
			sfb = 16;

			/* this constraint fell out of the old, pre-rotated code automatically */
			/* we need to explicitly add it because of the way we flip Y */
			if (sy <= -16)
				continue;

			if (flip_screen)
			{
				flipx = !flipx;
				flipy = !flipy;
				sfa = 16;
				sfb = 0;
			}

			if ((spriteram_3[offs] & 0x0c) == 0x0c)		/* double width, double height */
			{
				drawgfx(bitmap,Machine->gfx[1],
						code+2,color,flipx,flipy,sx+sfa,sy-sfa,
						&Machine->visible_area,TRANSPARENCY_THROUGH,Machine->pens[0]);
				drawgfx(bitmap,Machine->gfx[1],
						code,color,flipx,flipy,sx+sfa,sy-sfb,
						&Machine->visible_area,TRANSPARENCY_THROUGH,Machine->pens[0]);

				drawgfx(bitmap,Machine->gfx[1],
						code+3,color,flipx,flipy,sx+sfb,sy-sfa,
						&Machine->visible_area,TRANSPARENCY_THROUGH,Machine->pens[0]);
				drawgfx(bitmap,Machine->gfx[1],
						code+1,color,flipx,flipy,sx+sfb,sy-sfb,
						&Machine->visible_area,TRANSPARENCY_THROUGH,Machine->pens[0]);
			}
			else if (spriteram_3[offs] & 8)	/* double width */
			{
				drawgfx(bitmap,Machine->gfx[1],
						code+2,color,flipx,flipy,sx,sy-sfa,
						&Machine->visible_area,TRANSPARENCY_THROUGH,Machine->pens[0]);
				drawgfx(bitmap,Machine->gfx[1],
						code,color,flipx,flipy,sx,sy-sfb,
						&Machine->visible_area,TRANSPARENCY_THROUGH,Machine->pens[0]);
			}
			else if (spriteram_3[offs] & 4)	/* double height */
			{
				drawgfx(bitmap,Machine->gfx[1],
						code,color,flipx,flipy,sx+sfa,sy,
						&Machine->visible_area,TRANSPARENCY_THROUGH,Machine->pens[0]);
				drawgfx(bitmap,Machine->gfx[1],
						code+1,color,flipx,flipy,sx+sfb,sy,
						&Machine->visible_area,TRANSPARENCY_THROUGH,Machine->pens[0]);
			}
			else	/* normal */
				drawgfx(bitmap,Machine->gfx[1],
						code,color,flipx,flipy,sx,sy,
						&Machine->visible_area,TRANSPARENCY_THROUGH,Machine->pens[0]);
		}
	}


	/* draw the stars */
	if (galaga_starcontrol[5] & 1)
	{
		int bpen;


		bpen = Machine->pens[0];
		for (offs = 0;offs < total_stars;offs++)
		{
			int x,y;
			int set;
			int starset[4][2] = {{0,3},{0,1},{2,3},{2,1}};

			set = ((galaga_starcontrol[4] << 1) | galaga_starcontrol[3]) & 3;
			if ((stars[offs].set == starset[set][0]) ||
				(stars[offs].set == starset[set][1]))
			{
				x = ((stars[offs].x + stars_scroll) % 512) / 2 + 16;
				y = (stars[offs].y + (stars_scroll + stars[offs].x) / 512) % 256;

				if (y >= Machine->visible_area.min_y &&
					y <= Machine->visible_area.max_y)
				{
					if (read_pixel(bitmap, x, y) == bpen)
						plot_pixel(bitmap, x, y, stars[offs].col);
				}
			}
		}
	}
}



void galaga_vh_interrupt(void)
{
	/* this function is called by galaga_interrupt_1() */
	int s0,s1,s2;
	int speeds[8] = { 2, 3, 4, 0, -4, -3, -2, 0 };


	s0 = galaga_starcontrol[0] & 1;
	s1 = galaga_starcontrol[1] & 1;
	s2 = galaga_starcontrol[2] & 1;

	stars_scroll -= speeds[s0 + s1*2 + s2*4];
}
