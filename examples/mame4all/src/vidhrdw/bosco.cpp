/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


#define MAX_STARS 250
#define STARS_COLOR_BASE 32

unsigned char *bosco_staronoff;
unsigned char *bosco_starblink;
static unsigned int stars_scrollx;
static unsigned int stars_scrolly;
static unsigned char bosco_scrollx,bosco_scrolly;
static unsigned char bosco_starcontrol;
static int flipscreen;
static int displacement;


struct star
{
	int x,y,col,set;
};
static struct star stars[MAX_STARS];
static int total_stars;

#define VIDEO_RAM_SIZE 0x400

unsigned char *bosco_videoram2,*bosco_colorram2;
unsigned char *bosco_radarx,*bosco_radary,*bosco_radarattr;
size_t bosco_radarram_size;
											/* to speed up video refresh */
static unsigned char *dirtybuffer2;	/* keep track of modified portions of the screen */
											/* to speed up video refresh */
static struct osd_bitmap *tmpbitmap1;



static struct rectangle spritevisiblearea =
{
	0*8+3, 28*8-1,
	0*8, 28*8-1
};

static struct rectangle spritevisibleareaflip =
{
	8*8, 36*8-1-3,
	0*8, 28*8-1
};


static struct rectangle radarvisiblearea =
{
	28*8, 36*8-1,
	0*8, 28*8-1
};

static struct rectangle radarvisibleareaflip =
{
	0*8, 8*8-1,
	0*8, 28*8-1
};



void bosco_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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

	/* characters / sprites */
	for (i = 0;i < 64*4;i++)
	{
		colortable[i] = 15 - (color_prom[i + 32] & 0x0f);	/* chars */
		colortable[i+64*4] = 15 - (color_prom[i + 32] & 0x0f) + 0x10;	/* sprites */
		if (colortable[i+64*4] == 0x10) colortable[i+64*4] = 0;	/* preserve transparency */
	}

	/* radar dots lookup table */
	/* they use colors 0-3, I think */
	for (i = 0;i < 4;i++)
		COLOR(2,i) = i;

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

int bosco_vh_start(void)
{
	int generator;
	int x,y;
	int set = 0;

	if (generic_vh_start() != 0)
		return 1;

	if ((dirtybuffer2 = (unsigned char*)malloc(videoram_size)) == 0)
		return 1;
	memset(dirtybuffer2,1,videoram_size);

	if ((tmpbitmap1 = bitmap_alloc(32*8,32*8)) == 0)
	{
		free(dirtybuffer2);
		generic_vh_stop();
		return 1;
	}

	/* precalculate the star background */
	/* this comes from the Galaxian hardware, Bosconian is probably different */
	total_stars = 0;
	generator = 0;

	for (x = 255;x >= 0;x--)
	{
		for (y = 511;y >= 0;y--)
		{
			int bit1,bit2;


			generator <<= 1;
			bit1 = (~generator >> 17) & 1;
			bit2 = (generator >> 5) & 1;

			if (bit1 ^ bit2) generator |= 1;

			if (x >= Machine->visible_area.min_x &&
					x <= Machine->visible_area.max_x &&
					((~generator >> 16) & 1) &&
					(generator & 0xff) == 0xff)
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
	*bosco_staronoff = 1;

	displacement = 1;

	return 0;
}


/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void bosco_vh_stop(void)
{
	bitmap_free(tmpbitmap1);
	free(dirtybuffer2);
	generic_vh_stop();
}



WRITE_HANDLER( bosco_videoram2_w )
{
	if (bosco_videoram2[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		bosco_videoram2[offset] = data;
	}
}



WRITE_HANDLER( bosco_colorram2_w )
{
	if (bosco_colorram2[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		bosco_colorram2[offset] = data;
	}
}


WRITE_HANDLER( bosco_flipscreen_w )
{
	if (flipscreen != (~data & 1))
	{
		flipscreen = ~data & 1;
		memset(dirtybuffer,1,videoram_size);
		memset(dirtybuffer2,1,videoram_size);
	}
}

WRITE_HANDLER( bosco_scrollx_w )
{
	bosco_scrollx = data;
}

WRITE_HANDLER( bosco_scrolly_w )
{
	bosco_scrolly = data;
}

WRITE_HANDLER( bosco_starcontrol_w )
{
	bosco_starcontrol = data;
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void bosco_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,sx,sy;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer2[offs])
		{
			int flipx,flipy;


			dirtybuffer2[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			flipx = ~bosco_colorram2[offs] & 0x40;
			flipy = bosco_colorram2[offs] & 0x80;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(tmpbitmap1,Machine->gfx[0],
					bosco_videoram2[offs],
					bosco_colorram2[offs] & 0x3f,
					flipx,flipy,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}

	/* update radar */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int flipx,flipy;


			dirtybuffer[offs] = 0;

			sx = (offs % 32) ^ 4;
			sy = offs / 32 - 2;
			flipx = ~colorram[offs] & 0x40;
			flipy = colorram[offs] & 0x80;
			if (flipscreen)
			{
				sx = 7 - sx;
				sy = 27 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs],
					colorram[offs] & 0x3f,
					flipx,flipy,
					8*sx,8*sy,
					&radarvisibleareaflip,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scrollx,scrolly;


		if (flipscreen)
		{
			scrollx = (bosco_scrollx +32);//- 3*displacement) + 32;
			scrolly = (bosco_scrolly + 16) - 32;
		}
		else
		{
			scrollx = -(bosco_scrollx);
			scrolly = -(bosco_scrolly + 16);
		}

		copyscrollbitmap(bitmap,tmpbitmap1,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* radar */
	if (flipscreen)
		copybitmap(bitmap,tmpbitmap,0,0,0,0,&radarvisibleareaflip,TRANSPARENCY_NONE,0);
	else
		copybitmap(bitmap,tmpbitmap,0,0,28*8,0,&radarvisiblearea,TRANSPARENCY_NONE,0);


	/* draw the sprites */
	for (offs = 0;offs < spriteram_size;offs += 2)
	{
		sx = spriteram[offs + 1] - displacement;
if (flipscreen) sx += 32;
		sy = 225 - spriteram_2[offs] - displacement;

		drawgfx(bitmap,Machine->gfx[1],
				(spriteram[offs] & 0xfc) >> 2,
				spriteram_2[offs + 1] & 0x3f,
				spriteram[offs] & 1,spriteram[offs] & 2,
				sx,sy,
				flipscreen ? &spritevisibleareaflip : &spritevisiblearea,TRANSPARENCY_THROUGH,Machine->pens[0]);
	}


	/* draw the dots on the radar and the bullets */
	for (offs = 0; offs < bosco_radarram_size;offs++)
	{
		int x,y;


		x = bosco_radarx[offs] + ((~bosco_radarattr[offs] & 0x01) << 8) - 2;
		y = 235 - bosco_radary[offs];
		if (flipscreen)
		{
			x -= 1;
			y += 2;
		}

		drawgfx(bitmap,Machine->gfx[2],
				((bosco_radarattr[offs] & 0x0e) >> 1) ^ 0x07,
				0,
				flipscreen,flipscreen,
				x,y,
				&Machine->visible_area,TRANSPARENCY_PEN,3);
	}


	/* draw the stars */
	if ((*bosco_staronoff & 1) == 0)
	{
		int bpen;

		bpen = Machine->pens[0];
		for (offs = 0;offs < total_stars;offs++)
		{
			int x,y;
			int set;
			int starset[4][2] = {{0,3},{0,1},{2,3},{2,1}};

			x = (stars[offs].x + stars_scrollx) % 224;
			y = (stars[offs].y + stars_scrolly) % 224;

			set = (bosco_starblink[0] & 1) + ((bosco_starblink[1] & 1) << 1);

			if (((stars[offs].set == starset[set][0]) ||
				 (stars[offs].set == starset[set][1])))
			{
				if (read_pixel(bitmap, x, y) == bpen)
				{
					plot_pixel(bitmap, x, y, stars[offs].col);
				}
			}
		}
	}
}

void bosco_vh_interrupt(void)
{
	int speedsx[8] = { -1, -2, -3, 0, 3, 2, 1, 0 };
	int speedsy[8] = { 0, -1, -2, -3, 0, 3, 2, 1 };

	stars_scrollx += speedsx[bosco_starcontrol & 7];
	stars_scrolly += speedsy[(bosco_starcontrol & 56) >> 3];
}
