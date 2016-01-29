#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *kingobox_videoram1;
unsigned char *kingobox_colorram1;
size_t kingobox_videoram1_size;
unsigned char *kingobox_scroll_y;

extern int kingofb_nmi_enable;

static int palette_bank;



/***************************************************************************

  Convert the color PROMs into a more useable format.

  King of Boxer has three 256x4 palette PROMs, connected to the RGB output
  this way:

  bit 3 -- 180 ohm resistor  -- RED/GREEN/BLUE
        -- 360 ohm resistor  -- RED/GREEN/BLUE
        -- 750 ohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 1.5kohm resistor  -- RED/GREEN/BLUE

  The foreground color code directly goes to the RGB output, this way:

  bit 5 --  51 ohm resistor  -- RED
  bit 4 --  51 ohm resistor  -- GREEN
  bit 3 --  51 ohm resistor  -- BLUE

***************************************************************************/
void kingobox_vh_convert_color_prom(unsigned char *palette,unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < 256;i++)
	{
		int bit0,bit1,bit2,bit3;


		/* red component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x10 * bit0 + 0x21 * bit1 + 0x45 * bit2 + 0x89 * bit3;
		/* green component */
		bit0 = (color_prom[256] >> 0) & 0x01;
		bit1 = (color_prom[256] >> 1) & 0x01;
		bit2 = (color_prom[256] >> 2) & 0x01;
		bit3 = (color_prom[256] >> 3) & 0x01;
		*(palette++) = 0x10 * bit0 + 0x21 * bit1 + 0x45 * bit2 + 0x89 * bit3;
		/* blue component */
		bit0 = (color_prom[2*256] >> 0) & 0x01;
		bit1 = (color_prom[2*256] >> 1) & 0x01;
		bit2 = (color_prom[2*256] >> 2) & 0x01;
		bit3 = (color_prom[2*256] >> 3) & 0x01;
		*(palette++) = 0x10 * bit0 + 0x21 * bit1 + 0x45 * bit2 + 0x89 * bit3;

		color_prom++;
	}


	/* the foreground chars directly map to primary colors */
	for (i = 0;i < 8;i++)
	{
		/* red component */
		*(palette++) = 0xff * ((i >> 2) & 0x01);
		/* green component */
		*(palette++) = 0xff * ((i >> 1) & 0x01);
		/* blue component */
		*(palette++) = 0xff * ((i >> 0) & 0x01);
	}

	for (i = 0;i < TOTAL_COLORS(0)/2;i++)
	{
		COLOR(0,2*i+0) = 0;	/* transparent */
		COLOR(0,2*i+1) = 256 + i;
	}
}


/* Ring King has one 256x8 PROM instead of two 256x4 */
void ringking_vh_convert_color_prom(unsigned char *palette,unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < 256;i++)
	{
		int bit0,bit1,bit2,bit3;


		/* red component */
		bit0 = (color_prom[0] >> 4) & 0x01;
		bit1 = (color_prom[0] >> 5) & 0x01;
		bit2 = (color_prom[0] >> 6) & 0x01;
		bit3 = (color_prom[0] >> 7) & 0x01;
		*(palette++) = 0x10 * bit0 + 0x21 * bit1 + 0x45 * bit2 + 0x89 * bit3;
		/* green component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x10 * bit0 + 0x21 * bit1 + 0x45 * bit2 + 0x89 * bit3;
		/* blue component */
		bit0 = (color_prom[256] >> 0) & 0x01;
		bit1 = (color_prom[256] >> 1) & 0x01;
		bit2 = (color_prom[256] >> 2) & 0x01;
		bit3 = (color_prom[256] >> 3) & 0x01;
		*(palette++) = 0x10 * bit0 + 0x21 * bit1 + 0x45 * bit2 + 0x89 * bit3;

		color_prom++;
	}


	/* the foreground chars directly map to primary colors */
	for (i = 0;i < 8;i++)
	{
		/* red component */
		*(palette++) = 0xff * ((i >> 2) & 0x01);
		/* green component */
		*(palette++) = 0xff * ((i >> 1) & 0x01);
		/* blue component */
		*(palette++) = 0xff * ((i >> 0) & 0x01);
	}

	for (i = 0;i < TOTAL_COLORS(0)/2;i++)
	{
		COLOR(0,2*i+0) = 0;	/* transparent */
		COLOR(0,2*i+1) = 256 + i;
	}
}



WRITE_HANDLER( kingofb_f800_w )
{
	kingofb_nmi_enable = data & 0x20;

	if (palette_bank != ((data & 0x18) >> 3))
	{
		palette_bank = (data & 0x18) >> 3;
		memset(dirtybuffer,1,videoram_size);
	}
}



void kingobox_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;



	/* background */

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,code,bank;


			sx = offs / 16;
			sy = 15 - offs % 16;

			dirtybuffer[offs] = 0;

			code = videoram[offs] + ((colorram[offs] & 0x03) << 8);
			bank = (colorram[offs] & 0x04) >> 2;

			drawgfx(tmpbitmap,Machine->gfx[2 + bank],
					code,
					((colorram[offs] & 0x70) >> 4) + 8 * palette_bank,
					0,0,
					sx*16,sy*16,
					0,TRANSPARENCY_NONE,0);
		}
	}

	{
		int scrolly = kingobox_scroll_y[0];

		copyscrollbitmap(bitmap,tmpbitmap,0,0,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* sprites */
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int sx,sy,code,color,bank,flipy;


		sx = spriteram[offs+1];
		sy = spriteram[offs];

		code = spriteram[offs + 2] + ((spriteram[offs + 3] & 0x03) << 8);
		bank = (spriteram[offs + 3] & 0x04) >> 2;
		color = ((spriteram[offs + 3] & 0x70) >> 4) + 8 * palette_bank,
		flipy = spriteram[offs + 3] & 0x80;

		drawgfx(bitmap,Machine->gfx[2 + bank],
					code,
					color,
					0,flipy,
					sx,sy,
					0,TRANSPARENCY_PEN,0);
	}


	/* foreground */
	for (offs = kingobox_videoram1_size - 1;offs >= 0;offs--)
	{
		int sx,sy,code,bank;


		sx = offs / 32;
		sy = 31 - offs % 32;

		code = kingobox_videoram1[offs] + ((kingobox_colorram1[offs] & 0x01) << 8);
		bank = (kingobox_colorram1[offs] & 0x02) >> 1;

		drawgfx(bitmap,Machine->gfx[bank],
				code,
				(kingobox_colorram1[offs] & 0x38) >> 3,
				0,0,
				sx*8,sy*8,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

void ringking_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;



	/* background */

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy;


			sx = offs / 16;
			sy = 15 - offs % 16;

			dirtybuffer[offs] = 0;

			drawgfx(tmpbitmap,Machine->gfx[4],
					videoram[offs],
					((colorram[offs] & 0x70) >> 4 ) + 8 * palette_bank,
					0,0,
					sx*16,sy*16,
					0,TRANSPARENCY_NONE,0);
		}
	}

	{
		int scrolly = kingobox_scroll_y[0];

		copyscrollbitmap(bitmap,tmpbitmap,0,0,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* sprites */
	for (offs = 0; offs < spriteram_size;offs += 4)
	{
		int sx,sy,code,color,bank,flipy;

		sx = spriteram[offs+2];
		sy = spriteram[offs];

		code = spriteram[offs + 3] + ((spriteram[offs + 1] & 0x03) << 8);
		bank = (spriteram[offs + 1] & 0x04) >> 2;
		color = ((spriteram[offs + 1] & 0x70) >> 4) + 8 * palette_bank,
		flipy = ( spriteram[offs + 1] & 0x80 ) ? 0 : 1;

		drawgfx(bitmap,Machine->gfx[2 + bank],
					code,
					color,
					0,flipy,
					sx,sy,
					0,TRANSPARENCY_PEN,0);
	}


	/* foreground */
	for (offs = kingobox_videoram1_size - 1;offs >= 0;offs--)
	{
		int sx,sy,code,bank;


		sx = offs / 32;
		sy = 31 - offs % 32;

		code = kingobox_videoram1[offs] + ((kingobox_colorram1[offs] & 0x01) << 8);
		bank = (kingobox_colorram1[offs] & 0x02) >> 1;

		drawgfx(bitmap,Machine->gfx[bank],
				code,
				(kingobox_colorram1[offs] & 0x38) >> 3,
				0,0,
				sx*8,sy*8,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
