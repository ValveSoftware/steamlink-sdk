/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


static int charbank;
static int flipx,flipy;


void hexa_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


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
}



WRITE_HANDLER( hexa_d008_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	/* bit 0 = flipx (or y?) */
	if (flipx != (data & 0x01))
	{
		flipx = data & 0x01;
		memset(dirtybuffer,1,videoram_size);
	}

	/* bit 1 = flipy (or x?) */
	if (flipy != (data & 0x02))
	{
		flipy = data & 0x02;
		memset(dirtybuffer,1,videoram_size);
	}

	/* bit 2 - 3 unknown */

	/* bit 4 could be the ROM bank selector for 8000-bfff (not sure) */
	bankaddress = 0x10000 + ((data & 0x10) >> 4) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);

	/* bit 5 = char bank */
	if (charbank != ((data & 0x20) >> 5))
	{
		charbank = (data & 0x20) >> 5;
		memset(dirtybuffer,1,videoram_size);
	}

	/* bit 6 - 7 unknown */
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void hexa_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 2;offs >= 0;offs -= 2)
	{
		if (dirtybuffer[offs] || dirtybuffer[offs + 1])
		{
			int sx,sy;


			dirtybuffer[offs] = 0;
			dirtybuffer[offs + 1] = 0;

			sx = (offs/2) % 32;
			sy = (offs/2) / 32;
			if (flipx) sx = 31 - sx;
			if (flipy) sy = 31 - sy;

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs + 1] + ((videoram[offs] & 0x07) << 8) + (charbank << 11),
					(videoram[offs] & 0xf8) >> 3,
					flipx,flipy,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
}
