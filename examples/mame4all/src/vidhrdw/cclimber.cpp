/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



#define BIGSPRITE_WIDTH 128
#define BIGSPRITE_HEIGHT 128

unsigned char *cclimber_bsvideoram;
size_t cclimber_bsvideoram_size;
unsigned char *cclimber_bigspriteram;
unsigned char *cclimber_column_scroll;
static unsigned char *bsdirtybuffer;
static struct osd_bitmap *bsbitmap;
static int flipscreen[2];
static int palettebank;
static int sidepanel_enabled;
static int bgpen;


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Crazy Climber has three 32x8 palette PROMs.
  The palette PROMs are connected to the RGB output this way:

  bit 7 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

***************************************************************************/
void cclimber_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + (offs)])


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


	/* character and sprite lookup table */
	/* they use colors 0-63 */
	for (i = 0;i < TOTAL_COLORS(0);i++)
	{
		/* pen 0 always uses color 0 (background in River Patrol and Silver Land) */
		if (i % 4 == 0) COLOR(0,i) = 0;
		else COLOR(0,i) = i;
	}

	/* big sprite lookup table */
	/* it uses colors 64-95 */
	for (i = 0;i < TOTAL_COLORS(2);i++)
	{
		if (i % 4 == 0) COLOR(2,i) = 0;
		else COLOR(2,i) = i + 64;
	}

	bgpen = 0;
}


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Swimmer has two 256x4 char/sprite palette PROMs and one 32x8 big sprite
  palette PROM.
  The palette PROMs are connected to the RGB output this way:
  (the 500 and 250 ohm resistors are made of 1 kohm resistors in parallel)

  bit 3 -- 250 ohm resistor  -- BLUE
        -- 500 ohm resistor  -- BLUE
        -- 250 ohm resistor  -- GREEN
  bit 0 -- 500 ohm resistor  -- GREEN
  bit 3 -- 1  kohm resistor  -- GREEN
        -- 250 ohm resistor  -- RED
        -- 500 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

  bit 7 -- 250 ohm resistor  -- BLUE
        -- 500 ohm resistor  -- BLUE
        -- 250 ohm resistor  -- GREEN
        -- 500 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 250 ohm resistor  -- RED
        -- 500 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

  Additionally, the background color of the score panel is determined by
  these resistors:

                  /--- tri-state --  470 -- BLUE
  +5V -- 1kohm ------- tri-state --  390 -- GREEN
                  \--- tri-state -- 1000 -- RED

***************************************************************************/

#define BGPEN (256+32)
#define SIDEPEN (256+32+1)

void swimmer_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + (offs)])


	for (i = 0;i < 256;i++)
	{
		int bit0,bit1,bit2;


		/* red component */
		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		*(palette++) = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;
		/* green component */
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i+256] >> 0) & 0x01;
		bit2 = (color_prom[i+256] >> 1) & 0x01;
		*(palette++) = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i+256] >> 2) & 0x01;
		bit2 = (color_prom[i+256] >> 3) & 0x01;
		*(palette++) = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		/* side panel */
		if (i % 8)
		{
			COLOR(0,i) = i;
		    COLOR(0,i+256) = i;
		}
		else
		{
			/* background */
			COLOR(0,i) = BGPEN;
			COLOR(0,i+256) = SIDEPEN;
		}
	}

	color_prom += 2 * 256;

	/* big sprite */
	for (i = 0;i < 32;i++)
	{
		int bit0,bit1,bit2;


		/* red component */
		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		*(palette++) = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;
		/* green component */
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		*(palette++) = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;
		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		*(palette++) = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

		if (i % 8 == 0) COLOR(2,i) = BGPEN;  /* enforce transparency */
		else COLOR(2,i) = i+256;
	}

	/* background */
	*(palette++) = 0;
	*(palette++) = 0;
	*(palette++) = 0;
	/* side panel background color */
	*(palette++) = 0x24;
	*(palette++) = 0x5d;
	*(palette++) = 0x4e;

	palette_transparent_color = BGPEN; /* background color */
	bgpen = BGPEN;
}



/***************************************************************************

  Swimmer can directly set the background color.
  The latch is connected to the RGB output this way:
  (the 500 and 250 ohm resistors are made of 1 kohm resistors in parallel)

  bit 7 -- 250 ohm resistor  -- RED
        -- 500 ohm resistor  -- RED
        -- 250 ohm resistor  -- GREEN
        -- 500 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 250 ohm resistor  -- BLUE
        -- 500 ohm resistor  -- BLUE
  bit 0 -- 1  kohm resistor  -- BLUE

***************************************************************************/
WRITE_HANDLER( swimmer_bgcolor_w )
{
	int bit0,bit1,bit2;
	int r,g,b;


	/* red component */
	bit0 = 0;
	bit1 = (data >> 6) & 0x01;
	bit2 = (data >> 7) & 0x01;
	r = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

	/* green component */
	bit0 = (data >> 3) & 0x01;
	bit1 = (data >> 4) & 0x01;
	bit2 = (data >> 5) & 0x01;
	g = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

	/* blue component */
	bit0 = (data >> 0) & 0x01;
	bit1 = (data >> 1) & 0x01;
	bit2 = (data >> 2) & 0x01;
	b = 0x20 * bit0 + 0x40 * bit1 + 0x80 * bit2;

	palette_change_color(BGPEN,r,g,b);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int cclimber_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((bsdirtybuffer = (unsigned char*)malloc(cclimber_bsvideoram_size)) == 0)
	{
		generic_vh_stop();
		return 1;
	}
	memset(bsdirtybuffer,1,cclimber_bsvideoram_size);

	if ((bsbitmap = bitmap_alloc(BIGSPRITE_WIDTH,BIGSPRITE_HEIGHT)) == 0)
	{
		free(bsdirtybuffer);
		generic_vh_stop();
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void cclimber_vh_stop(void)
{
	bitmap_free(bsbitmap);
	free(bsdirtybuffer);
	generic_vh_stop();
}



WRITE_HANDLER( cclimber_flipscreen_w )
{
	if (flipscreen[offset] != (data & 1))
	{
		flipscreen[offset] = data & 1;
		memset(dirtybuffer,1,videoram_size);
	}
}



WRITE_HANDLER( cclimber_colorram_w )
{
	if (colorram[offset] != data)
	{
		/* bit 5 of the address is not used for color memory. There is just */
		/* 512 bytes of memory; every two consecutive rows share the same memory */
		/* region. */
		offset &= 0xffdf;

		dirtybuffer[offset] = 1;
		dirtybuffer[offset + 0x20] = 1;

		colorram[offset] = data;
		colorram[offset + 0x20] = data;
	}
}



WRITE_HANDLER( cclimber_bigsprite_videoram_w )
{
	if (cclimber_bsvideoram[offset] != data)
	{
		bsdirtybuffer[offset] = 1;

		cclimber_bsvideoram[offset] = data;
	}
}



WRITE_HANDLER( swimmer_palettebank_w )
{
	if (palettebank != (data & 1))
	{
		palettebank = data & 1;
		memset(dirtybuffer,1,videoram_size);
	}
}



WRITE_HANDLER( swimmer_sidepanel_enable_w )
{
    if (data != sidepanel_enabled)
    {
		sidepanel_enabled = data;

		/* We only need to dirty the side panel, but this location is not */
		/* written to very often, so we just dirty the whole screen */
		memset(dirtybuffer,1,videoram_size);
    }
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
static void drawbigsprite(struct osd_bitmap *bitmap)
{
	int sx,sy,flipx,flipy;


	sx = 136 - cclimber_bigspriteram[3];
	sy = 128 - cclimber_bigspriteram[2];
	flipx = cclimber_bigspriteram[1] & 0x10;
	flipy = cclimber_bigspriteram[1] & 0x20;
	if (flipscreen[1])      /* only the Y direction has to be flipped */
	{
		sy = 128 - sy;
		flipy = !flipy;
	}

	/* we have to draw if four times for wraparound */
	sx &= 0xff;
	sy &= 0xff;
	copybitmap(bitmap,bsbitmap,
			flipx,flipy,
			sx,sy,
			&Machine->visible_area,TRANSPARENCY_COLOR,bgpen);
	copybitmap(bitmap,bsbitmap,
			flipx,flipy,
			sx-256,sy,
			&Machine->visible_area,TRANSPARENCY_COLOR,bgpen);
	copybitmap(bitmap,bsbitmap,
			flipx,flipy,
			sx-256,sy-256,
			&Machine->visible_area,TRANSPARENCY_COLOR,bgpen);
	copybitmap(bitmap,bsbitmap,
			flipx,flipy,
			sx,sy-256,
			&Machine->visible_area,TRANSPARENCY_COLOR,bgpen);
}


void cclimber_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,flipx,flipy;


			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			flipx = colorram[offs] & 0x40;
			flipy = colorram[offs] & 0x80;
			/* vertical flipping flips two adjacent characters */
			if (flipy) sy ^= 1;

			if (flipscreen[0])
			{
				sx = 31 - sx;
				flipx = !flipx;
			}
			if (flipscreen[1])
			{
				sy = 31 - sy;
				flipy = !flipy;
			}

			drawgfx(tmpbitmap,Machine->gfx[(colorram[offs] & 0x10) ? 1 : 0],
					videoram[offs] + 8 * (colorram[offs] & 0x20),
					colorram[offs] & 0x0f,
					flipx,flipy,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scroll[32];


		if (flipscreen[0])
		{
			for (offs = 0;offs < 32;offs++)
			{
				scroll[offs] = -cclimber_column_scroll[31 - offs];
				if (flipscreen[1]) scroll[offs] = -scroll[offs];
			}
		}
		else
		{
			for (offs = 0;offs < 32;offs++)
			{
				scroll[offs] = -cclimber_column_scroll[offs];
				if (flipscreen[1]) scroll[offs] = -scroll[offs];
			}
		}

		copyscrollbitmap(bitmap,tmpbitmap,0,0,32,scroll,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* update the "big sprite" */
	{
		int newcol;
		static int lastcol;


		newcol = cclimber_bigspriteram[1] & 0x07;

		for (offs = cclimber_bsvideoram_size - 1;offs >= 0;offs--)
		{
			int sx,sy;


			if (bsdirtybuffer[offs] || newcol != lastcol)
			{
				bsdirtybuffer[offs] = 0;

				sx = offs % 16;
				sy = offs / 16;

				drawgfx(bsbitmap,Machine->gfx[2],
						cclimber_bsvideoram[offs],newcol,
						0,0,
						8*sx,8*sy,
						0,TRANSPARENCY_NONE,0);
			}

		}

		lastcol = newcol;
	}


	if (cclimber_bigspriteram[0] & 1)
		/* draw the "big sprite" below sprites */
		drawbigsprite(bitmap);


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int sx,sy,flipx,flipy;


		sx = spriteram[offs + 3];
		sy = 240 - spriteram[offs + 2];
		flipx = spriteram[offs] & 0x40;
		flipy = spriteram[offs] & 0x80;
		if (flipscreen[0])
		{
			sx = 240 - sx;
			flipx = !flipx;
		}
		if (flipscreen[1])
		{
			sy = 240 - sy;
			flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[spriteram[offs + 1] & 0x10 ? 4 : 3],
				(spriteram[offs] & 0x3f) + 2 * (spriteram[offs + 1] & 0x20),
				spriteram[offs + 1] & 0x0f,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}


	if ((cclimber_bigspriteram[0] & 1) == 0)
		/* draw the "big sprite" over sprites */
		drawbigsprite(bitmap);
}



void swimmer_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	if (palette_recalc())
	{
		memset(dirtybuffer,1,videoram_size);
		memset(bsdirtybuffer,1,cclimber_bsvideoram_size);
	}

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,flipx,flipy,color;


			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;
			flipx = colorram[offs] & 0x40;
			flipy = colorram[offs] & 0x80;
			/* vertical flipping flips two adjacent characters */
			if (flipy) sy ^= 1;

			if (flipscreen[0])
			{
				sx = 31 - sx;
				flipx = !flipx;
			}
			if (flipscreen[1])
			{
				sy = 31 - sy;
				flipy = !flipy;
			}

			color = (colorram[offs] & 0x0f) + 0x10 * palettebank;
			if (sx >= 24 && sidepanel_enabled)
			{
			    color += 32;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + ((colorram[offs] & 0x10) << 4),
					color,
					flipx,flipy,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scroll[32];


		if (flipscreen[1])
		{
			for (offs = 0;offs < 32;offs++)
				scroll[offs] = cclimber_column_scroll[31 - offs];
		}
		else
		{
			for (offs = 0;offs < 32;offs++)
				scroll[offs] = -cclimber_column_scroll[offs];
		}

		copyscrollbitmap(bitmap,tmpbitmap,0,0,32,scroll,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* update the "big sprite" */
	{
		int newcol;
		static int lastcol;


		newcol = cclimber_bigspriteram[1] & 0x03;

		for (offs = cclimber_bsvideoram_size - 1;offs >= 0;offs--)
		{
			int sx,sy;


			if (bsdirtybuffer[offs] || newcol != lastcol)
			{
				bsdirtybuffer[offs] = 0;

				sx = offs % 16;
				sy = offs / 16;

				drawgfx(bsbitmap,Machine->gfx[2],
						cclimber_bsvideoram[offs] + ((cclimber_bigspriteram[1] & 0x08) << 5),
						newcol,
						0,0,
						8*sx,8*sy,
						0,TRANSPARENCY_NONE,0);
			}

		}

		lastcol = newcol;
	}


	if (cclimber_bigspriteram[0] & 1)
		/* draw the "big sprite" below sprites */
		drawbigsprite(bitmap);


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int sx,sy,flipx,flipy;


		sx = spriteram[offs + 3];
		sy = 240 - spriteram[offs + 2];
		flipx = spriteram[offs] & 0x40;
		flipy = spriteram[offs] & 0x80;
		if (flipscreen[0])
		{
			sx = 240 - sx;
			flipx = !flipx;
		}
		if (flipscreen[1])
		{
			sy = 240 - sy;
			flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[1],
				(spriteram[offs] & 0x3f) | (spriteram[offs + 1] & 0x10) << 2,
				(spriteram[offs + 1] & 0x0f) + 0x10 * palettebank,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}


	if ((cclimber_bigspriteram[0] & 1) == 0)
		/* draw the "big sprite" over sprites */
		drawbigsprite(bitmap);
}
