/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

  This file is used by the Pengo and Pac Man drivers.
  They are almost identical, the only differences being the extra gfx bank
  in Pengo, and the need to compensate for an hardware sprite positioning
  "bug" in Pac Man.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



static int gfx_bank;
static int flipscreen;
static int xoffsethack;

static struct rectangle spritevisiblearea =
{
	2*8, 34*8-1,
	0*8, 28*8-1
};



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Pac Man has a 32x8 palette PROM and a 256x4 color lookup table PROM.

  Pengo has a 32x8 palette PROM and a 1024x4 color lookup table PROM.

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
void pacman_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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

	color_prom += 0x10;
	/* color_prom now points to the beginning of the lookup table */

	/* character lookup table */
	/* sprites use the same color lookup table as characters */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = *(color_prom++) & 0x0f;
}

void pengo_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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

	/* color_prom now points to the beginning of the lookup table */

	/* character lookup table */
	/* sprites use the same color lookup table as characters */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = *(color_prom++) & 0x0f;

	color_prom += 0x80;

	/* second bank character lookup table */
	/* sprites use the same color lookup table as characters */
	for (i = 0;i < TOTAL_COLORS(2);i++)
	{
		if (*color_prom) COLOR(2,i) = (*color_prom & 0x0f) + 0x10;	/* second palette bank */
		else COLOR(2,i) = 0;	/* preserve transparency */

		color_prom++;
	}
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int pengo_vh_start(void)
{
	gfx_bank = 0;
	xoffsethack = 0;

    return generic_vh_start();
}

int pacman_vh_start(void)
{
	gfx_bank = 0;
	/* In the Pac Man based games (NOT Pengo) the first two sprites must be offset */
	/* one pixel to the left to get a more correct placement */
	xoffsethack = 1;

	return generic_vh_start();
}



WRITE_HANDLER( pengo_gfxbank_w )
{
	/* the Pengo hardware can set independently the palette bank, color lookup */
	/* table, and chars/sprites. However the game always set them together (and */
	/* the only place where this is used is the intro screen) so I don't bother */
	/* emulating the whole thing. */
	if (gfx_bank != (data & 1))
	{
		gfx_bank = data & 1;
		memset(dirtybuffer,1,videoram_size);
	}
}

WRITE_HANDLER( pengo_flipscreen_w )
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
void pengo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;

	for (offs = videoram_size - 1; offs > 0; offs--)
	{
		if (dirtybuffer[offs])
		{
			int mx,my,sx,sy;

			dirtybuffer[offs] = 0;
            mx = offs % 32;
			my = offs / 32;

			if (my < 2)
			{
				if (mx < 2 || mx >= 30) continue; /* not visible */
				sx = my + 34;
				sy = mx - 2;
			}
			else if (my >= 30)
			{
				if (mx < 2 || mx >= 30) continue; /* not visible */
				sx = my - 30;
				sy = mx - 2;
			}
			else
			{
				sx = mx + 2;
				sy = my - 2;
			}

			if (flipscreen)
			{
				sx = 35 - sx;
				sy = 27 - sy;
			}

			drawgfx(tmpbitmap,Machine->gfx[gfx_bank*2],
					videoram[offs],
					colorram[offs] & 0x1f,
					flipscreen,flipscreen,
					sx*8,sy*8,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
        }
	}

	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

    /* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = spriteram_size - 2;offs > 2*2;offs -= 2)
	{
		int sx,sy;


		sx = 272 - spriteram_2[offs + 1];
		sy = spriteram_2[offs] - 31;

		drawgfx(bitmap,Machine->gfx[gfx_bank*2+1],
				spriteram[offs] >> 2,
				spriteram[offs + 1] & 0x1f,
				spriteram[offs] & 1,spriteram[offs] & 2,
				sx,sy,
				&spritevisiblearea,TRANSPARENCY_COLOR,0);

        /* also plot the sprite with wraparound (tunnel in Crush Roller) */
        drawgfx(bitmap,Machine->gfx[gfx_bank*2+1],
				spriteram[offs] >> 2,
				spriteram[offs + 1] & 0x1f,
				spriteram[offs] & 1,spriteram[offs] & 2,
				sx - 256,sy,
				&spritevisiblearea,TRANSPARENCY_COLOR,0);
	}
	/* In the Pac Man based games (NOT Pengo) the first two sprites must be offset */
	/* one pixel to the left to get a more correct placement */
	for (offs = 2*2;offs >= 0;offs -= 2)
	{
		int sx,sy;


		sx = 272 - spriteram_2[offs + 1];
		sy = spriteram_2[offs] - 31;

        drawgfx(bitmap,Machine->gfx[gfx_bank*2+1],
				spriteram[offs] >> 2,
				spriteram[offs + 1] & 0x1f,
				spriteram[offs] & 1,spriteram[offs] & 2,
				sx,sy + xoffsethack,
				&spritevisiblearea,TRANSPARENCY_COLOR,0);

        /* also plot the sprite with wraparound (tunnel in Crush Roller) */
        drawgfx(bitmap,Machine->gfx[gfx_bank*2+1],
				spriteram[offs] >> 2,
				spriteram[offs + 1] & 0x1f,
				spriteram[offs] & 2,spriteram[offs] & 1,
				sx - 256,sy + xoffsethack,
				&spritevisiblearea,TRANSPARENCY_COLOR,0);
    }
}
