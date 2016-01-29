/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


extern unsigned char *galaxian_attributesram;
static const unsigned char *fastfred_color_prom;


static struct rectangle spritevisiblearea =
{
      2*8, 32*8-1,
      2*8, 30*8-1
};

static struct rectangle spritevisibleareaflipx =
{
        0*8, 30*8-1,
        2*8, 30*8-1
};

static data_t character_bank[2];
static data_t color_bank[2];
static int canspritesflipx = 0;

void jumpcoas_init_machine(void)
{
	canspritesflipx = 1;
}

/***************************************************************************

  Convert the color PROMs into a more useable format.

  bit 0 -- 1  kohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 220 ohm resistor  -- RED/GREEN/BLUE
  bit 3 -- 100 ohm resistor  -- RED/GREEN/BLUE

***************************************************************************/

static void convert_color(int i, int* r, int* g, int* b)
{
	int bit0, bit1, bit2, bit3;
	const unsigned char *prom = fastfred_color_prom;
	int total = Machine->drv->total_colors;

	bit0 = (prom[i + 0*total] >> 0) & 0x01;
	bit1 = (prom[i + 0*total] >> 1) & 0x01;
	bit2 = (prom[i + 0*total] >> 2) & 0x01;
	bit3 = (prom[i + 0*total] >> 3) & 0x01;
	*r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
	bit0 = (prom[i + 1*total] >> 0) & 0x01;
	bit1 = (prom[i + 1*total] >> 1) & 0x01;
	bit2 = (prom[i + 1*total] >> 2) & 0x01;
	bit3 = (prom[i + 1*total] >> 3) & 0x01;
	*g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
	bit0 = (prom[i + 2*total] >> 0) & 0x01;
	bit1 = (prom[i + 2*total] >> 1) & 0x01;
	bit2 = (prom[i + 2*total] >> 2) & 0x01;
	bit3 = (prom[i + 2*total] >> 3) & 0x01;
	*b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
}

void fastfred_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
        int i;
        #define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
        #define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


		fastfred_color_prom = color_prom;	/* we'll need this later */

        for (i = 0;i < Machine->drv->total_colors;i++)
        {
                int r,g,b;

				convert_color(i, &r, &g, &b);

				*(palette++) = r;
				*(palette++) = g;
				*(palette++) = b;
        }


        /* characters and sprites use the same palette */
        for (i = 0;i < TOTAL_COLORS(0);i++)
        {
			int color;

			if (!(i & 0x07))
			{
				color = 0;
			}
			else
			{
				color = i;
			}

			COLOR(0,i) = COLOR(1,i) = color;
        }
}


WRITE_HANDLER( fastfred_character_bank_select_w )
{
	set_vh_global_attribute(&character_bank[offset], data & 0x01);

}


WRITE_HANDLER( fastfred_color_bank_select_w )
{
	set_vh_global_attribute(&color_bank[offset], data & 0x01);
}


WRITE_HANDLER( fastfred_background_color_w )
{
	int r,g,b;

	//logerror("Background color = %02X\n", data);

	convert_color(data, &r, &g, &b);

	palette_change_color(0,r,g,b);
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void fastfred_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs, charbank, colorbank;


	if (palette_recalc() || full_refresh)
		memset(dirtybuffer,1,videoram_size);


	charbank   = ((character_bank[1] << 9) | (character_bank[0] << 8));
	colorbank  = ((color_bank[1]     << 4) | (color_bank[0]     << 3));

	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int color;

		if (dirtybuffer[offs])
		{
			int sx,sy;

			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;

			color = colorbank | (galaxian_attributesram[2 * sx + 1] & 0x07);

			if (flip_screen_x) sx = 31 - sx;
			if (flip_screen_y) sy = 31 - sy;

			drawgfx(tmpbitmap,Machine->gfx[0],
					charbank | videoram[offs],
					color,
					flip_screen_x,flip_screen_y,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmap to the screen */
	{
		int i, scroll[32];

		if (flip_screen_x)
		{
			for (i = 0;i < 32;i++)
			{
				scroll[31-i] = -galaxian_attributesram[2 * i];
				if (flip_screen_y) scroll[31-i] = -scroll[31-i];
			}
		}
		else
		{
			for (i = 0;i < 32;i++)
			{
				scroll[i] = -galaxian_attributesram[2 * i];
				if (flip_screen_y) scroll[i] = -scroll[i];
			}
		}

		copyscrollbitmap(bitmap,tmpbitmap,0,0,32,scroll,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* draw the sprites */
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int code,sx,sy,flipx,flipy;

		sx = (spriteram[offs + 3] + 1) & 0xff;  /* ??? */
		sy = 240 - spriteram[offs];

		if (canspritesflipx)
		{
			// Jump Coaster
			code  =  spriteram[offs + 1] & 0x3f;
			flipx = ~spriteram[offs + 1] & 0x40;
			flipy =  spriteram[offs + 1] & 0x80;
		}
		else
		{
			// Fast Freddie
			code  =  spriteram[offs + 1] & 0x7f;
			flipx =  0;
			flipy = ~spriteram[offs + 1] & 0x80;
		}


		if (flip_screen_x)
		{
			sx = 241 - sx;  /* note: 241, not 240 */
			flipx = !flipx;
		}
		if (flip_screen_y)
		{
			sy = 240 - sy;
			flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[1],
				code,
				colorbank | (spriteram[offs + 2] & 0x07),
				flipx,flipy,
				sx,sy,
				flip_screen_x ? &spritevisibleareaflipx : &spritevisiblearea,TRANSPARENCY_PEN,0);
	}
}
