/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *gyruss_spritebank,*gyruss_6809_drawplanet,*gyruss_6809_drawship;
static int flipscreen;


typedef struct {
  unsigned char y;
  unsigned char shape;
  unsigned char attr;
  unsigned char x;
} Sprites;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Gyruss has one 32x8 palette PROM and two 256x4 lookup table PROMs
  (one for characters, one for sprites).
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
void gyruss_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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

	/* color_prom now points to the beginning of the sprite lookup table */

	/* sprites */
	for (i = 0;i < TOTAL_COLORS(1);i++)
		COLOR(1,i) = *(color_prom++) & 0x0f;

	/* characters */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = (*(color_prom++) & 0x0f) + 0x10;
}



/* convert sprite coordinates from polar to cartesian */
static int SprTrans(Sprites *u)
{
#define YTABLE_START (0xe000)
#define SINTABLE_START (0xe400)
#define COSTABLE_START (0xe600)
	int ro;
	int theta2;
	unsigned char *table;


	ro = memory_region(REGION_CPU4)[YTABLE_START + u->y];
	theta2 = 2 * u->x;

	/* cosine table */
	table = &memory_region(REGION_CPU4)[COSTABLE_START];

	u->y = (table[theta2+1] * ro) >> 8;
	if (u->y >= 0x80)
	{
		u->x = 0;
		return 0;
	}
	if (table[theta2] != 0)	/* negative */
	{
		if (u->y >= 0x78)	/* avoid wraparound from top to bottom of screen */
		{
			u->x = 0;
			return 0;
		}
		u->y = -u->y;
	}

	/* sine table */
	table = &memory_region(REGION_CPU4)[SINTABLE_START];

	u->x = (table[theta2+1] * ro) >> 8;
	if (u->x >= 0x80)
	{
		u->x = 0;
		return 0;
	}
	if (table[theta2] != 0)	/* negative */
		u->x = -u->x;


	/* convert from logical coordinates to screen coordinates */
	if (u->attr & 0x10)
		u->y += 0x78;
	else
		u->y += 0x7C;

	u->x += 0x78;


    return 1;	/* queue this sprite */
}



/* this macro queues 'nq' sprites at address 'u', into queue at 'q' */
#define SPR(n) ((Sprites*)&sr[4*(n)])



	/* Gyruss uses a semaphore system to queue sprites, and when critic
	   region is released, the 6809 processor writes queued sprites onto
	   screen visible area.
	   When a701 = 0 and a702 = 1 gyruss hardware queue sprites.
	   When a701 = 1 and a702 = 0 gyruss hardware draw sprites.

           both Z80 e 6809 are interrupted at the same time by the
           VBLANK interrupt.  If there is some work to do (example
           A7FF is not 0 or FF), 6809 waits for Z80 to store a 1 in
           A701 and draw currently queued sprites
	*/
WRITE_HANDLER( gyruss_queuereg_w )
{
	if (data == 1)
	{
        int n;
		unsigned char *sr;


        /* Gyruss hardware stores alternatively sprites at position
           0xa000 and 0xa200.  0xA700 tells which one is used.
        */

		if (*gyruss_spritebank == 0)
			sr = spriteram;
		else sr = spriteram_2;


		/* #0-#3 - ship */

		/* #4-#23 */
        if (*gyruss_6809_drawplanet)	/* planet is on screen */
		{
			SprTrans(SPR(4));	/* #4 - polar coordinates - ship */

			SPR(5)->x = 0;	/* #5 - unused */

			/* #6-#23 - planet */
        }
		else
		{
			for (n = 4;n < 24;n += 2)	/* 10 double height sprites in polar coordinates - enemies flying */
			{
				SprTrans(SPR(n));

				SPR(n+1)->x = 0;
			}
		}


		/* #24-#59 */
		for (n = 24;n < 60;n++)	/* 36 sprites in polar coordinates - enemies at center of screen */
			SprTrans(SPR(n));


		/* #60-#63 - unused */


		/* #64-#77 */
        if (*gyruss_6809_drawship == 0)
		{
			for (n = 64;n < 78;n++)	/* 14 sprites in polar coordinates - bullets */
				SprTrans(SPR(n));
		}
		/* else 14 sprites - player ship being formed */


		/* #78-#93 - stars */
	    for (n = 78;n < 86;n++)
		{
			if (SprTrans(SPR(n)))
			{
				/* make a mirror copy */
				SPR(n+8)->x = SPR(n)->y - 4;
				SPR(n+8)->y = SPR(n)->x + 4;
			}
			else
				SPR(n+8)->x = 0;
		}
	}
}



WRITE_HANDLER( gyruss_flipscreen_w )
{
	if (flipscreen != (data & 1))
	{
		flipscreen = data & 1;
		memset(dirtybuffer,1,videoram_size);
	}
}



/* Return the current video scan line */
READ_HANDLER( gyruss_scanline_r )
{
	return cpu_scalebyfcount(256);
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void gyruss_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
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
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + 8 * (colorram[offs] & 0x20),
					colorram[offs] & 0x0f,
					flipx,flipy,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);


	/*
	   offs+0 :  Ypos
	   offs+1 :  Sprite number
	   offs+2 :  Attribute in the form HF-VF-BK-DH-p3-p2-p1-p0
				 where  HF is horizontal flip
						VF is vertical flip
						BK is for bank select
						DH is for double height (if set sprite is 16*16, else is 16*8)
						px is palette weight
	   offs+3 :  Xpos
	*/

	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	{
		unsigned char *sr;


		if (*gyruss_spritebank == 0)
			sr = spriteram;
		else sr = spriteram_2;


		for (offs = spriteram_size - 8;offs >= 0;offs -= 8)
		{
			if (sr[2 + offs] & 0x10)	/* double height */
			{
				if (sr[offs + 0] != 0)
					drawgfx(bitmap,Machine->gfx[3],
							sr[offs + 1]/2 + 4*(sr[offs + 2] & 0x20),
							sr[offs + 2] & 0x0f,
							!(sr[offs + 2] & 0x40),sr[offs + 2] & 0x80,
							sr[offs + 0],240-sr[offs + 3]+1,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
			else	/* single height */
			{
				if (sr[offs + 0] != 0)
					drawgfx(bitmap,Machine->gfx[1 + (sr[offs + 1] & 1)],
							sr[offs + 1]/2 + 4*(sr[offs + 2] & 0x20),
							sr[offs + 2] & 0x0f,
							!(sr[offs + 2] & 0x40),sr[offs + 2] & 0x80,
							sr[offs + 0],240-sr[offs + 3]+1,
							&Machine->visible_area,TRANSPARENCY_PEN,0);

				if (sr[offs + 4] != 0)
					drawgfx(bitmap,Machine->gfx[1 + (sr[offs + 5] & 1)],
							sr[offs + 5]/2 + 4*(sr[offs + 6] & 0x20),
							sr[offs + 6] & 0x0f,
							!(sr[offs + 6] & 0x40),sr[offs + 6] & 0x80,
							sr[offs + 4],240-sr[offs + 7]+1,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
		}
	}


	/* redraw the characters which have priority over sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy,flipx,flipy;


		sx = offs % 32;
		sy = offs / 32;
		flipx = colorram[offs] & 0x40;
		flipy = colorram[offs] & 0x80;
		if (flipscreen)
		{
			sx = 31 - sx;
			sy = 31 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (colorram[offs] & 0x10)
			drawgfx(bitmap,Machine->gfx[0],
					videoram[offs] + 8 * (colorram[offs] & 0x20),
					colorram[offs] & 0x0f,
					flipx,flipy,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
}

void gyruss_6809_vh_screenrefresh(struct osd_bitmap *bitmap)
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
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + 8 * (colorram[offs] & 0x20),
					colorram[offs] & 0x0f,
					flipx,flipy,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	{
		for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
		{
			drawgfx(bitmap,Machine->gfx[1 + (spriteram[offs + 1] & 1)],
					spriteram[offs + 1]/2 + 4*(spriteram[offs + 2] & 0x20),
					spriteram[offs + 2] & 0x0f,
					!(spriteram[offs + 2] & 0x40),spriteram[offs + 2] & 0x80,
					spriteram[offs],240-spriteram[offs + 3]+1,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}


	/* redraw the characters which have priority over sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy,flipx,flipy;


		sx = offs % 32;
		sy = offs / 32;
		flipx = colorram[offs] & 0x40;
		flipy = colorram[offs] & 0x80;
		if (flipscreen)
		{
			sx = 31 - sx;
			sy = 31 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if ((colorram[offs] & 0x10) != 0)
			drawgfx(bitmap,Machine->gfx[0],
					videoram[offs] + 8 * (colorram[offs] & 0x20),
					colorram[offs] & 0x0f,
					flipx,flipy,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
}
