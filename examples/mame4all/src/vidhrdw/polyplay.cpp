/***************************************************************************

  Poly-Play
  (c) 1985 by VEB Polytechnik Karl-Marx-Stadt

  video hardware

  driver written by Martin Buchholz (buchholz@mail.uni-greifswald.de)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *polyplay_characterram;
static unsigned char dirtycharacter[256];

static int palette_bank;


void polyplay_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;

	static unsigned char polyplay_palette[] =
	{
		0x00,0x00,0x00,
		0xff,0xff,0xff,

		0x00,0x00,0x00,
		0xff,0x00,0x00,
		0x00,0xff,0x00,
		0xff,0xff,0x00,
		0x00,0x00,0xff,
		0xff,0x00,0xff,
		0x00,0xff,0xff,
		0xff,0xff,0xff,
	};


	for (i = 0;i < Machine->drv->total_colors;i++)
	{

		/* red component */
		*(palette++) = polyplay_palette[3*i];

		/* green component */
		*(palette++) = polyplay_palette[3*i+1];

		/* blue component */
		*(palette++) = polyplay_palette[3*i+2];

	}

	palette_bank = 0;

}


WRITE_HANDLER( polyplay_characterram_w )
{
	if (polyplay_characterram[offset] != data)
	{
		dirtycharacter[((offset / 8) & 0x7f) + 0x80] = 1;

		polyplay_characterram[offset] = data;
	}
}

READ_HANDLER( polyplay_characterram_r )
{
	return polyplay_characterram[offset];
}


void polyplay_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
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
		int charcode;


		charcode = videoram[offs];

		if (dirtybuffer[offs] || dirtycharacter[charcode])
		{
			int sx,sy;


			/* index=0 -> 1 bit chr; index=1 -> 3 bit chr */
			if (charcode < 0x80) {

				/* ROM chr, no need for decoding */

				dirtybuffer[offs] = 0;

				sx = offs % 64;
				sy = offs / 64;

				drawgfx(bitmap,Machine->gfx[0],
						charcode,
						0x0,
						0,0,
						8*sx,8*sy,
						&Machine->visible_area,TRANSPARENCY_NONE,0);

			}
			else {
				/* decode modified characters */
				if (dirtycharacter[charcode] == 1)
				{
					decodechar(Machine->gfx[1],charcode-0x80,polyplay_characterram,Machine->drv->gfxdecodeinfo[1].gfxlayout);
					dirtycharacter[charcode] = 2;
				}


				dirtybuffer[offs] = 0;

				sx = offs % 64;
				sy = offs / 64;

				drawgfx(bitmap,Machine->gfx[1],
						charcode,
						0x0,
						0,0,
						8*sx,8*sy,
						&Machine->visible_area,TRANSPARENCY_NONE,0);

			}
		}
	}


	for (offs = 0;offs < 256;offs++)
	{
		if (dirtycharacter[offs] == 2) dirtycharacter[offs] = 0;
	}
}
