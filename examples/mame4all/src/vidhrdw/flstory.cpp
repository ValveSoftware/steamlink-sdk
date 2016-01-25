/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"


static int palette_bank;


void flstory_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


	/* no color PROMs here, only RAM, but the gfx data is inverted so we */
	/* cannot use the default lookup table */
	for (i = 0;i < Machine->drv->color_table_len;i++)
		colortable[i] = i ^ 0x0f;
}



int flstory_vh_start(void)
{
	paletteram = (unsigned char*)malloc(0x200);
	paletteram_2 = (unsigned char*)malloc(0x200);
	return generic_vh_start();
}

void flstory_vh_stop(void)
{
	free(paletteram);
	paletteram = 0;
	free(paletteram_2);
	paletteram_2 = 0;
	generic_vh_stop();
}



WRITE_HANDLER( flstory_palette_w )
{
	if (offset & 0x100)
		paletteram_xxxxBBBBGGGGRRRR_split2_w((offset & 0xff) + (palette_bank << 8),data);
	else
		paletteram_xxxxBBBBGGGGRRRR_split1_w((offset & 0xff) + (palette_bank << 8),data);
}

WRITE_HANDLER( flstory_gfxctrl_w )
{
	palette_bank = (data & 0x20) >> 5;
//logerror("%04x: gfxctrl = %02x\n",cpu_get_pc(),data);
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void flstory_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	if (palette_recalc())
		memset(dirtybuffer,1,videoram_size);

	for (offs = videoram_size - 2;offs >= 0;offs -= 2)
	{
		if (dirtybuffer[offs] || dirtybuffer[offs+1])
		{
			int sx,sy;


			dirtybuffer[offs] = 0;
			dirtybuffer[offs+1] = 0;

			sx = (offs/2)%32;
			sy = (offs/2)/32;

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + ((videoram[offs + 1] & 0xc0) << 2) + 0xc00,
					videoram[offs + 1] & 0x07,
					videoram[offs + 1] & 0x08,1,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	for (offs = 0;offs < spriteram_size;offs += 4)
	{
		int code,sx,sy,flipx,flipy;


		code = spriteram[offs+2] + ((spriteram[offs+1] & 0x30) << 4);
		sx = spriteram[offs+3];
		sy = 240 - spriteram[offs+0];
		flipx = spriteram[offs+1]&0x40;
		flipy = spriteram[offs+1]&0x80;

		drawgfx(bitmap,Machine->gfx[1],
				code,
				spriteram[offs+1] & 0x0f,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
		/* wrap around */
		if (sx > 240)
			drawgfx(bitmap,Machine->gfx[1],
					code,
					spriteram[offs+1] & 0x0f,
					flipx,flipy,
					sx-256,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
	}

	/* redraw chars with priority over sprites */
	for (offs = videoram_size - 2;offs >= 0;offs -= 2)
	{
		if (videoram[offs + 1] & 0x20)
		{
			int sx,sy;


			sx = (offs/2)%32;
			sy = (offs/2)/32;

			drawgfx(bitmap,Machine->gfx[0],
					videoram[offs] + ((videoram[offs + 1] & 0xc0) << 2) + 0xc00,
					videoram[offs + 1] & 0x07,
					videoram[offs + 1] & 0x08,1,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}
}
