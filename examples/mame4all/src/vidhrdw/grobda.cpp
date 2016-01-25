/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int flipscreen;
extern unsigned char *grobda_spriteram;


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Grodba has one 32x8 palette PROM and two 256x4 color lookup table PROMs
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
void grobda_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;

	for (i = 0;i < 32;i++)
	{
		int bit0,bit1,bit2;

		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		palette[3*i] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		palette[3*i + 1] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		palette[3*i + 2] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
	}
	/* characters */
	for (i = 0; i < 256; i++)
		colortable[i] = (0x1f - (color_prom[i + 32] & 0x0f));
	/* sprites */
	for (i = 256; i < 512; i++)
		colortable[i] = (color_prom[i + 32] & 0x0f);
}



int grobda_vh_start( void )
{
	/* set up spriteram area */
	spriteram_size = 0x80;
	spriteram = &grobda_spriteram[0x780];
	spriteram_2 = &grobda_spriteram[0x780+0x800];
	spriteram_3 = &grobda_spriteram[0x780+0x800+0x800];

	return generic_vh_start();
}

void grobda_vh_stop( void )
{
	generic_vh_stop();
}

WRITE_HANDLER( grobda_flipscreen_w )
{
	if (flipscreen != data)
		memset(dirtybuffer,1,videoram_size);
	flipscreen = data;
}


/***************************************************************************

	Screen Refresh

***************************************************************************/

static void grobda_draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;

	for (offs = 0; offs < spriteram_size; offs += 2){
		int number = spriteram[offs];
		int color = spriteram[offs+1];
		int sx = (spriteram_2[offs+1]-40) + 0x100*(spriteram_3[offs+1] & 1);
		int sy = 28*8-spriteram_2[offs] - 16;
		int flipx = spriteram_3[offs] & 1;
		int flipy = spriteram_3[offs] & 2;
		int width,height;

		if (flipscreen){
			flipx = !flipx;
			flipy = !flipy;
		}

		if (spriteram_3[offs+1] & 2) continue;

		switch (spriteram_3[offs] & 0x0c){
			case 0x0c:	/* 2x both ways */
				width = height = 2; number &= (~3); break;
			case 0x08:	/* 2x vertical */
				width = 1; height = 2; number &= (~2); break;
			case 0x04:	/* 2x horizontal */
				width = 2; height = 1; number &= (~1); sy += 16; break;
			default:	/* normal sprite */
				width = height = 1; sy += 16; break;
		}

		{
			static int x_offset[2] = { 0x00, 0x01 };
			static int y_offset[2] = { 0x00, 0x02 };
			int x,y, ex, ey;

			for( y=0; y < height; y++ ){
				for( x=0; x < width; x++ ){
					ex = flipx ? (width-1-x) : x;
					ey = flipy ? (height-1-y) : y;

					drawgfx(bitmap,Machine->gfx[1],
						(number)+x_offset[ex]+y_offset[ey],
						color,
						flipx, flipy,
						sx+x*16,sy+y*16,
						&Machine->visible_area,
						TRANSPARENCY_PEN,0);
				}
			}
		}
	}
}

void grobda_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
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

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs],
					colorram[offs] & 0x3f,
					flipscreen,flipscreen,
					sx*8,sy*8,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
        }
	}

	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	grobda_draw_sprites(bitmap);
}
