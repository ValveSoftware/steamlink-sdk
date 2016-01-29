/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


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

unsigned char *wiz_videoram2;
unsigned char *wiz_colorram2;
unsigned char *wiz_attributesram;
unsigned char *wiz_attributesram2;

static int flipx, flipy;

unsigned char *wiz_sprite_bank;
static unsigned char char_bank[2];
static unsigned char palbank[2];
static int palette_bank;


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Stinger has three 256x4 palette PROMs (one per gun).
  The palette PROMs are connected to the RGB output this way:

  bit 3 -- 100 ohm resistor  -- RED/GREEN/BLUE
        -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 1  kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void wiz_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2,bit3;


		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x42 * bit2 + 0x90 * bit3;
		bit0 = (color_prom[Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x42 * bit2 + 0x90 * bit3;
		bit0 = (color_prom[2*Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[2*Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[2*Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[2*Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x42 * bit2 + 0x90 * bit3;

		color_prom++;
	}
}

WRITE_HANDLER( wiz_attributes_w )
{
	if ((offset & 1) && wiz_attributesram[offset] != data)
	{
		int i;


		for (i = offset / 2;i < videoram_size;i += 32)
		{
			dirtybuffer[i] = 1;
		}
	}

	wiz_attributesram[offset] = data;
}

WRITE_HANDLER( wiz_palettebank_w )
{
	if (palbank[offset] != (data & 1))
	{
		palbank[offset] = data & 1;
		palette_bank = palbank[0] + 2 * palbank[1];

		memset(dirtybuffer,1,videoram_size);
	}
}

WRITE_HANDLER( wiz_char_bank_select_w )
{
	if (char_bank[offset] != (data & 1))
	{
		char_bank[offset] = data & 1;
		memset(dirtybuffer,1,videoram_size);
	}
}

WRITE_HANDLER( wiz_flipx_w )
{
    if (flipx != data)
    {
		flipx = data;

		memset(dirtybuffer, 1, videoram_size);
    }
}


WRITE_HANDLER( wiz_flipy_w )
{
    if (flipy != data)
    {
		flipy = data;

		memset(dirtybuffer, 1, videoram_size);
    }
}

static void draw_background(struct osd_bitmap *bitmap, int bank, int colortype)
{
	int i,offs;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */

	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,col;

			dirtybuffer[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;

			if (colortype)
			{
				col = (wiz_attributesram[2 * sx + 1] & 0x07);
			}
			else
			{
				col = (colorram[offs] & 0x07);
			}

			if (flipx) sx = 31 - sx;
			if (flipy) sy = 31 - sy;

			drawgfx(tmpbitmap,Machine->gfx[bank],
				videoram[offs],
				(wiz_attributesram[2 * (offs % 32) + 1] & 0x07) + 8 * palette_bank,
				flipx,flipy,
				8*sx,8*sy,
				0,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the temporary bitmap to the screen */
	{
		int scroll[32];


		if (flipx)
		{
			for (i = 0;i < 32;i++)
			{
				scroll[31-i] = -wiz_attributesram[2 * i];
				if (flipy) scroll[31-i] = -scroll[31-i];
			}
		}
		else
		{
			for (i = 0;i < 32;i++)
			{
				scroll[i] = -wiz_attributesram[2 * i];
				if (flipy) scroll[i] = -scroll[i];
			}
		}

		copyscrollbitmap(bitmap,tmpbitmap,0,0,32,scroll,&Machine->visible_area,TRANSPARENCY_NONE,0);

	}
}

static void draw_foreground(struct osd_bitmap *bitmap, int colortype)
{
	int offs;

	/* draw the frontmost playfield. They are characters, but draw them as sprites. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int scroll,sx,sy,col;


		sx = offs % 32;
		sy = offs / 32;

		if (colortype)
		{
			col = (wiz_attributesram2[2 * sx + 1] & 0x07);
		}
		else
		{
			col = (wiz_colorram2[offs] & 0x07);
		}

		scroll = (8*sy + 256 - wiz_attributesram2[2 * sx]) % 256;
		if (flipy)
		{
		   scroll = (248 - scroll) % 256;
		}
		if (flipx) sx = 31 - sx;


		drawgfx(bitmap,Machine->gfx[char_bank[1]],
			wiz_videoram2[offs],
			col + 8 * palette_bank,
			flipx,flipy,
			8*sx,scroll,
			&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

static void draw_sprites(struct osd_bitmap *bitmap, unsigned char* sprite_ram,
                         int bank, const struct rectangle* visible_area)
{
	int offs;

	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int sx,sy;


		sx = sprite_ram[offs + 3];
		sy = sprite_ram[offs];

		if (!sx || !sy) continue;

		if ( flipx) sx = 240 - sx;
		if (!flipy) sy = 240 - sy;

		drawgfx(bitmap,Machine->gfx[bank],
				sprite_ram[offs + 1],
				(sprite_ram[offs + 2] & 0x07) + 8 * palette_bank,
				flipx,flipy,
				sx,sy,
				visible_area,TRANSPARENCY_PEN,0);
	}
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void wiz_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int bank;
	const struct rectangle* visible_area;

	draw_background(bitmap, 2 + ((char_bank[0] << 1) | char_bank[1]), 0);
	draw_foreground(bitmap, 0);

	visible_area = flipx ? &spritevisibleareaflipx : &spritevisiblearea;

	/* I seriously doubt that the real hardware works this way */
	if ((spriteram[1] & 0x80) || !spriteram[3] || !spriteram[0])
	{
	    bank = 7 + *wiz_sprite_bank;
	}
	else
	{
		bank = 8;	// Dragon boss
	}

	draw_sprites(bitmap, spriteram_2, 6,    visible_area);
	draw_sprites(bitmap, spriteram  , bank, visible_area);
}


void stinger_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	draw_background(bitmap, 2 + char_bank[0], 1);
	draw_foreground(bitmap, 1);
	draw_sprites(bitmap, spriteram_2, 4, &Machine->visible_area);
	draw_sprites(bitmap, spriteram  , 5, &Machine->visible_area);
}
