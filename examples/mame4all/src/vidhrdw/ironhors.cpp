/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *ironhors_scroll;
static int palettebank,charbank,spriterambank;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Iron Horse has three 256x4 palette PROMs (one per gun) and two 256x4
  lookup table PROMs (one for characters, one for sprites).
  I don't know for sure how the palette PROMs are connected to the RGB
  output, but it's probably the usual:

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void ironhors_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2,bit3;


		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = (color_prom[2*Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[2*Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[2*Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[2*Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}

	color_prom += 2*Machine->drv->total_colors;
	/* color_prom now points to the beginning of the character lookup table */


	/* there are eight 32 colors palette banks; sprites use colors 0-15 and */
	/* characters 16-31 of each bank. */
	for (i = 0;i < TOTAL_COLORS(0)/8;i++)
	{
		int j;


		for (j = 0;j < 8;j++)
			COLOR(0,i + j * TOTAL_COLORS(0)/8) = (*color_prom & 0x0f) + 32 * j + 16;

		color_prom++;
	}

	for (i = 0;i < TOTAL_COLORS(1)/8;i++)
	{
		int j;


		for (j = 0;j < 8;j++)
			COLOR(1,i + j * TOTAL_COLORS(1)/8) = (*color_prom & 0x0f) + 32 * j;

		color_prom++;
	}
}



WRITE_HANDLER( ironhors_charbank_w )
{
	if (charbank != (data & 1))
	{
		charbank = data & 1;
		memset(dirtybuffer,1,videoram_size);
	}

	spriterambank = data & 8;

	/* other bits unknown */
}



WRITE_HANDLER( ironhors_palettebank_w )
{
	if (palettebank != (data & 7))
	{
		palettebank = data & 7;
		memset(dirtybuffer,1,videoram_size);
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void ironhors_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,i;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy;


			dirtybuffer[offs] = 0;

			sx = 8 * (offs % 32);
			sy = 8 * (offs / 32);

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + 16*(colorram[offs] & 0x20) + 4*(colorram[offs] & 0x40) + 1024 * charbank,
					(colorram[offs] & 0x0f) + 16 * palettebank,
					colorram[offs] & 0x10,colorram[offs] & 0x20,
					sx,sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int scroll[32];


		for (i = 0;i < 32;i++)
			scroll[i] = -(ironhors_scroll[i]);

		copyscrollbitmap(bitmap,tmpbitmap,32,scroll,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* Draw the sprites. */
	{
		unsigned char *sr;


		if (spriterambank != 0)
			sr = spriteram;
		else sr = spriteram_2;

		for (offs = 0;offs < spriteram_size;offs += 5)
		{
			if (sr[offs+2])
			{
				int sx,sy,flipx,flipy,code,color;


				sx = sr[offs+3];
				sy = sr[offs+2];
				flipx = sr[offs+4] & 0x20;
				flipy = sr[offs+4] & 0x40;
				code = (sr[offs] << 2) + ((sr[offs+1] & 0x01) << 10) + ((sr[offs+1] & 0x0c) >> 2);
				color = ((sr[offs+1] & 0xf0)>>4) + 16 * palettebank;

				switch (sr[offs+4] & 0x0c)
				{
					case 0x00:	/* 16x16 */
						drawgfx(bitmap,Machine->gfx[1],
								code/4,
								color,
								flipx,flipy,
								sx,sy,
								&Machine->visible_area,TRANSPARENCY_PEN,0);
						break;

					case 0x04:	/* 16x8 */
						{
							drawgfx(bitmap,Machine->gfx[2],
									code & ~1,
									color,
									flipx,flipy,
									flipx?sx+8:sx,sy,
									&Machine->visible_area,TRANSPARENCY_PEN,0);
							drawgfx(bitmap,Machine->gfx[2],
									code | 1,
									color,
									flipx,flipy,
									flipx?sx:sx+8,sy,
									&Machine->visible_area,TRANSPARENCY_PEN,0);
						}
						break;

					case 0x08:	/* 8x16 */
						{
							drawgfx(bitmap,Machine->gfx[2],
									code & ~2,
									color,
									flipx,flipy,
									sx,flipy?sy+8:sy,
									&Machine->visible_area,TRANSPARENCY_PEN,0);
							drawgfx(bitmap,Machine->gfx[2],
									code | 2,
									color,
									flipx,flipy,
									sx,flipy?sy:sy+8,
									&Machine->visible_area,TRANSPARENCY_PEN,0);
						}
						break;

					case 0x0c:	/* 8x8 */
						{
							drawgfx(bitmap,Machine->gfx[2],
									code,
									color,
									flipx,flipy,
									sx,sy,
									&Machine->visible_area,TRANSPARENCY_PEN,0);
						}
						break;
				}
			}
		}
	}
}
