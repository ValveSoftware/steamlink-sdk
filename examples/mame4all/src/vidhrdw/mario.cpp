/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



static int gfx_bank,palette_bank;

unsigned char *mario_scrolly;

/***************************************************************************

  Convert the color PROMs into a more useable format.

  Mario Bros. has a 512x8 palette PROM; interstingly, bytes 0-255 contain an
  inverted palette, as other Nintendo games like Donkey Kong, while bytes
  256-511 contain a non inverted palette. This was probably done to allow
  connection to both the special Nintendo and a standard monitor.
  The palette PROM is connected to the RGB output this way:

  bit 7 -- 220 ohm resistor -- inverter  -- RED
        -- 470 ohm resistor -- inverter  -- RED
        -- 1  kohm resistor -- inverter  -- RED
        -- 220 ohm resistor -- inverter  -- GREEN
        -- 470 ohm resistor -- inverter  -- GREEN
        -- 1  kohm resistor -- inverter  -- GREEN
        -- 220 ohm resistor -- inverter  -- BLUE
  bit 0 -- 470 ohm resistor -- inverter  -- BLUE

***************************************************************************/
void mario_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2;


		/* red component */
		bit0 = (*color_prom >> 5) & 1;
		bit1 = (*color_prom >> 6) & 1;
		bit2 = (*color_prom >> 7) & 1;
		*(palette++) = 255 - (0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2);
		/* green component */
		bit0 = (*color_prom >> 2) & 1;
		bit1 = (*color_prom >> 3) & 1;
		bit2 = (*color_prom >> 4) & 1;
		*(palette++) = 255 - (0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2);
		/* blue component */
		bit0 = (*color_prom >> 0) & 1;
		bit1 = (*color_prom >> 1) & 1;
		*(palette++) = 255 - (0x55 * bit0 + 0xaa * bit1);

		color_prom++;
	}

	/* characters use the same palette as sprites, however characters */
	/* use only colors 64-127 and 192-255. */
	for (i = 0;i < 8;i++)
	{
		COLOR(0,4*i) = 8*i + 64;
		COLOR(0,4*i+1) = 8*i+1 + 64;
		COLOR(0,4*i+2) = 8*i+2 + 64;
		COLOR(0,4*i+3) = 8*i+3 + 64;
	}
	for (i = 0;i < 8;i++)
	{
		COLOR(0,4*i+8*4) = 8*i + 192;
		COLOR(0,4*i+8*4+1) = 8*i+1 + 192;
		COLOR(0,4*i+8*4+2) = 8*i+2 + 192;
		COLOR(0,4*i+8*4+3) = 8*i+3 + 192;
	}

	/* sprites */
	for (i = 0;i < TOTAL_COLORS(1);i++)
		COLOR(1,i) = i;
}



WRITE_HANDLER( mario_gfxbank_w )
{
	if (gfx_bank != (data & 1))
	{
		memset(dirtybuffer,1,videoram_size);
		gfx_bank = data & 1;
	}
}



WRITE_HANDLER( mario_palettebank_w )
{
	if (palette_bank != (data & 1))
	{
		memset(dirtybuffer,1,videoram_size);
		palette_bank = data & 1;
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void mario_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
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

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + 256 * gfx_bank,
					(videoram[offs] >> 5) + 8 * palette_bank,
					0,0,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scrolly;

		/* I'm not positive the scroll direction is right */
		scrolly = -*mario_scrolly - 17;
		copyscrollbitmap(bitmap,tmpbitmap,0,0,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}

	/* Draw the sprites. */
	for (offs = 0;offs < spriteram_size;offs += 4)
	{
		if (spriteram[offs])
		{
			drawgfx(bitmap,Machine->gfx[1],
					spriteram[offs + 2],
					(spriteram[offs + 1] & 0x0f) + 16 * palette_bank,
					spriteram[offs + 1] & 0x80,spriteram[offs + 1] & 0x40,
					spriteram[offs + 3] - 8,240 - spriteram[offs] + 8,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}
}
