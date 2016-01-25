/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


static struct osd_bitmap *tmpbitmap2,*charbitmap;
static unsigned char x,y,bmap;
static unsigned char *tmpvideoram,*tmpvideoram2;


void cloak_vh_stop(void);


/***************************************************************************

  CLOAK & DAGGER uses RAM to dynamically
  create the palette. The resolution is 9 bit (3 bits per gun). The palette
  contains 64 entries, but it is accessed through a memory windows 128 bytes
  long: writing to the first 64 bytes sets the msb of the red component to 0,
  while writing to the last 64 bytes sets it to 1.

  Colors 0-15  Character mapped graphics
  Colors 16-31 Bitmapped graphics (maybe 8 colors per bitmap?)
  Colors 32-47 Sprites
  Colors 48-63 not used

  I don't know the exact values of the resistors between the RAM and the
  RGB output, I assumed the usual ones.
  bit 8 -- inverter -- 220 ohm resistor  -- RED
        -- inverter -- 470 ohm resistor  -- RED
        -- inverter -- 1  kohm resistor  -- RED
        -- inverter -- 220 ohm resistor  -- GREEN
        -- inverter -- 470 ohm resistor  -- GREEN
        -- inverter -- 1  kohm resistor  -- GREEN
        -- inverter -- 220 ohm resistor  -- BLUE
        -- inverter -- 470 ohm resistor  -- BLUE
  bit 0 -- inverter -- 1  kohm resistor  -- BLUE

***************************************************************************/
WRITE_HANDLER( cloak_paletteram_w )
{
	int r,g,b;
	int bit0,bit1,bit2;


	/* a write to offset 64-127 means to set the msb of the red component */
	data |= (offset & 0x40) << 2;

	r = (~data & 0x1c0) >> 6;
	g = (~data & 0x038) >> 3;
	b = (~data & 0x007);

	bit0 = (r >> 0) & 0x01;
	bit1 = (r >> 1) & 0x01;
	bit2 = (r >> 2) & 0x01;
	r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
	bit0 = (g >> 0) & 0x01;
	bit1 = (g >> 1) & 0x01;
	bit2 = (g >> 2) & 0x01;
	g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
	bit0 = (b >> 0) & 0x01;
	bit1 = (b >> 1) & 0x01;
	bit2 = (b >> 2) & 0x01;
	b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	palette_change_color(offset & 0x3f,r,g,b);
}


WRITE_HANDLER( cloak_clearbmp_w )
{
	bmap = data & 1;
	if (data & 2)	/* clear */
	{
		if (bmap)
		{
			fillbitmap(tmpbitmap, Machine->pens[16], &Machine->visible_area);
			memset(tmpvideoram, 0, 256*256);
		}
		else
		{
			fillbitmap(tmpbitmap2, Machine->pens[16], &Machine->visible_area);
			memset(tmpvideoram2, 0, 256*256);
		}
	}
}


static void adjust_xy(int offset)
{
	switch(offset)
	{
	case 0x00:  x--; y++; break;
	case 0x01:       y--; break;
	case 0x02:  x--;      break;
	case 0x04:  x++; y++; break;
	case 0x05:  	 y++; break;
	case 0x06:  x++;      break;
	}
}


READ_HANDLER( graph_processor_r )
{
	int ret;

	if (bmap)
	{
		ret = tmpvideoram2[y*256+x];
	}
	else
	{
		ret = tmpvideoram[y*256+x];
	}

	adjust_xy(offset);

	return ret;
}


WRITE_HANDLER( graph_processor_w )
{
	int col;

	switch (offset)
	{
	case 0x03:  x=data; break;
	case 0x07:  y=data; break;
	default:
		col = data & 0x07;

		if (bmap)
		{
			plot_pixel(tmpbitmap, (x-6)&0xff, y, Machine->pens[16 + col]);
			tmpvideoram[y*256+x] = col;
		}
		else
		{
			plot_pixel(tmpbitmap2, (x-6)&0xff, y, Machine->pens[16 + col]);
			tmpvideoram2[y*256+x] = col;
		}

		adjust_xy(offset);
		break;
	}
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int cloak_vh_start(void)
{
	if ((tmpbitmap = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
		return 1;

	if ((charbitmap = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		cloak_vh_stop();
		return 1;
	}

	if ((tmpbitmap2 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		cloak_vh_stop();
		return 1;
	}

	if ((dirtybuffer = (unsigned char*)malloc(videoram_size)) == 0)
	{
		cloak_vh_stop();
		return 1;
	}
	memset(dirtybuffer,1,videoram_size);

	if ((tmpvideoram = (unsigned char*)malloc(256*256)) == 0)
	{
		cloak_vh_stop();
		return 1;
	}

	if ((tmpvideoram2 = (unsigned char*)malloc(256*256)) == 0)
	{
		cloak_vh_stop();
		return 1;
	}

	return 0;
}

/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void cloak_vh_stop(void)
{
	if (charbitmap)  bitmap_free(charbitmap);
	if (tmpbitmap2)  bitmap_free(tmpbitmap2);
	if (tmpbitmap)   bitmap_free(tmpbitmap);
	if (dirtybuffer) free(dirtybuffer);
	if (tmpvideoram) free(tmpvideoram);
	if (tmpvideoram2) free(tmpvideoram2);
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
static void refresh_bitmaps(void)
{
	int lx,ly;

	for (ly = 0; ly < 256; ly++)
	{
		for (lx = 0; lx < 256; lx++)
		{
			plot_pixel(tmpbitmap,  (lx-6)&0xff, ly, Machine->pens[16 + tmpvideoram[ly*256+lx]]);
			plot_pixel(tmpbitmap2, (lx-6)&0xff, ly, Machine->pens[16 + tmpvideoram2[ly*256+lx]]);
		}
	}
}


void cloak_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	palette_used_colors[16] = PALETTE_COLOR_TRANSPARENT;
	if (palette_recalc())
	{
		memset(dirtybuffer, 1, videoram_size);

		refresh_bitmaps();
	}

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

			drawgfx(charbitmap,Machine->gfx[0],
					videoram[offs],0,
					0,0,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the temporary bitmap to the screen */
    copybitmap(bitmap,charbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	copybitmap(bitmap, bmap ? tmpbitmap2 : tmpbitmap, 0,0,0,0,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);


	/* Draw the sprites */
	for (offs = spriteram_size/4-1; offs >= 0; offs--)
	{
		drawgfx(bitmap,Machine->gfx[1],
				spriteram[offs+64] & 0x7f,
				0,
				spriteram[offs+64] & 0x80,0,
				spriteram[offs+192],240-spriteram[offs],
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
