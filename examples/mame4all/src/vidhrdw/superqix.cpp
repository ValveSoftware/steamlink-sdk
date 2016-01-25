/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



static int gfxbank;
static unsigned char *superqix_bitmapram,*superqix_bitmapram2,*superqix_bitmapram_dirty,*superqix_bitmapram2_dirty;
static struct osd_bitmap *tmpbitmap2;
int sqix_minx,sqix_maxx,sqix_miny,sqix_maxy;
int sqix_last_bitmap;
int sqix_current_bitmap;



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int superqix_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	/* palette RAM is accessed thorough I/O ports, so we have to */
	/* allocate it ourselves */
	if ((paletteram = (unsigned char*)malloc(256 * sizeof(unsigned char))) == 0)
	{
		generic_vh_stop();
		return 1;
	}

	if ((superqix_bitmapram = (unsigned char*)malloc(0x7000 * sizeof(unsigned char))) == 0)
	{
		free(paletteram);
		generic_vh_stop();
		return 1;
	}

	if ((superqix_bitmapram2 = (unsigned char*)malloc(0x7000 * sizeof(unsigned char))) == 0)
	{
		free(superqix_bitmapram);
		free(paletteram);
		generic_vh_stop();
		return 1;
	}

	if ((superqix_bitmapram_dirty = (unsigned char*)malloc(0x7000 * sizeof(unsigned char))) == 0)
	{
		free(superqix_bitmapram2);
		free(superqix_bitmapram);
		free(paletteram);
		generic_vh_stop();
		return 1;
	}
	memset(superqix_bitmapram_dirty,1,0x7000);

	if ((superqix_bitmapram2_dirty = (unsigned char*)malloc(0x7000 * sizeof(unsigned char))) == 0)
	{
		free(superqix_bitmapram_dirty);
		free(superqix_bitmapram2);
		free(superqix_bitmapram);
		free(paletteram);
		generic_vh_stop();
		return 1;
	}
	memset(superqix_bitmapram2_dirty,1,0x7000);

	if ((tmpbitmap2 = bitmap_alloc(256, 256)) == 0)
	{
		free(superqix_bitmapram2_dirty);
		free(superqix_bitmapram_dirty);
		free(superqix_bitmapram2);
		free(superqix_bitmapram);
		free(paletteram);
		generic_vh_stop();
		return 1;
	}

	sqix_minx=0;sqix_maxx=127;sqix_miny=0;sqix_maxy=223;
	sqix_last_bitmap=0;

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void superqix_vh_stop(void)
{
	free(superqix_bitmapram2);
	free(superqix_bitmapram);
	free(superqix_bitmapram_dirty);
	free(superqix_bitmapram2_dirty);
	bitmap_free(tmpbitmap2);
	free(paletteram);
	generic_vh_stop();
}



READ_HANDLER( superqix_bitmapram_r )
{
	return superqix_bitmapram[offset];
}


WRITE_HANDLER( superqix_bitmapram_w )
{
	if(data != superqix_bitmapram[offset])
	{
		int x,y;
		superqix_bitmapram[offset] = data;
		superqix_bitmapram_dirty[offset] = 1;
		x=offset%128;
		y=offset/128;
		if(x<sqix_minx) sqix_minx=x;
		if(x>sqix_maxx) sqix_maxx=x;
		if(y<sqix_miny) sqix_miny=y;
		if(y>sqix_maxy) sqix_maxy=y;
	}
}

READ_HANDLER( superqix_bitmapram2_r )
{
	return superqix_bitmapram2[offset];
}

WRITE_HANDLER( superqix_bitmapram2_w )
{
	if(data != superqix_bitmapram2[offset])
	{
		int x,y;
		superqix_bitmapram2[offset] = data;
		superqix_bitmapram2_dirty[offset] = 1;
		x=offset%128;
		y=offset/128;
		if(x<sqix_minx) sqix_minx=x;
		if(x>sqix_maxx) sqix_maxx=x;
		if(y<sqix_miny) sqix_miny=y;
		if(y>sqix_maxy) sqix_maxy=y;
	}
}



WRITE_HANDLER( superqix_0410_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	/* bits 0-1 select the tile bank */
	if (gfxbank != (data & 0x03))
	{
		gfxbank = data & 0x03;
		memset(dirtybuffer,1,videoram_size);
	}

	/* bit 2 controls bitmap 1/2 */
	sqix_current_bitmap=data&4;
	if(sqix_current_bitmap !=sqix_last_bitmap)
	{
		sqix_last_bitmap=sqix_current_bitmap;
		memset(superqix_bitmapram_dirty,1,0x7000);
		memset(superqix_bitmapram2_dirty,1,0x7000);
		sqix_minx=0;sqix_maxx=127;sqix_miny=0;sqix_maxy=223;
	}

	/* bit 3 enables NMI */
	interrupt_enable_w(offset,data & 0x08);

	/* bits 4-5 control ROM bank */
	bankaddress = 0x10000 + ((data & 0x30) >> 4) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void superqix_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs,i;
	unsigned char pens[16];


	/* recalc the palette if necessary */
	if (palette_recalc())
	{
		memset(dirtybuffer,1,videoram_size);
		memset(superqix_bitmapram_dirty,1,0x7000);
		memset(superqix_bitmapram2_dirty,1,0x7000);
		sqix_minx=0;sqix_maxx=127;sqix_miny=0;sqix_maxy=223;
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

			drawgfx(tmpbitmap,Machine->gfx[(colorram[offs] & 0x04) ? 0 : (1 + gfxbank)],
					videoram[offs] + 256 * (colorram[offs] & 0x03),
					(colorram[offs] & 0xf0) >> 4,
					0,0,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	for(i=1;i<16;i++)
		pens[i]=Machine->pens[i];
	pens[0]=palette_transparent_pen;

	if(sqix_current_bitmap==0)		/* Bitmap 1 */
	{
		int x,y;

		for (y = sqix_miny;y <= sqix_maxy;y++)
		{
			for (x = sqix_minx;x <= sqix_maxx;x++)
			{
				int sx,sy,d;

				if(superqix_bitmapram_dirty[y*128+x])
				{
					superqix_bitmapram_dirty[y*128+x]=0;
					d = superqix_bitmapram[y*128+x];

					sx = 2*x;
					sy = y+16;

					plot_pixel(tmpbitmap2, sx    , sy, pens[d >> 4]);
					plot_pixel(tmpbitmap2, sx + 1, sy, pens[d & 0x0f]);
				}
			}
		}
	}
	else		/* Bitmap 2 */
	{
		int x,y;

		for (y = sqix_miny;y <= sqix_maxy;y++)
		{
			for (x = sqix_minx;x <= sqix_maxx;x++)
			{
				int sx,sy,d;

				if(superqix_bitmapram2_dirty[y*128+x])
				{
					superqix_bitmapram2_dirty[y*128+x]=0;
					d = superqix_bitmapram2[y*128+x];

					sx = 2*x;
					sy = y+16;

					plot_pixel(tmpbitmap2, sx    , sy, pens[d >> 4]);
					plot_pixel(tmpbitmap2, sx + 1, sy, pens[d & 0x0f]);
				}
			}
		}
	}
	copybitmap(bitmap,tmpbitmap2,0,0,0,0,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);

	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = 0;offs < spriteram_size;offs += 4)
	{
		drawgfx(bitmap,Machine->gfx[5],
				spriteram[offs] + 256 * (spriteram[offs + 3] & 0x01),
				(spriteram[offs + 3] & 0xf0) >> 4,
				spriteram[offs + 3] & 0x04,spriteram[offs + 3] & 0x08,
				spriteram[offs + 1],spriteram[offs + 2],
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}


	/* redraw characters which have priority over the bitmap */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (colorram[offs] & 0x08)
		{
			int sx,sy;


			sx = offs % 32;
			sy = offs / 32;

			drawgfx(bitmap,Machine->gfx[(colorram[offs] & 0x04) ? 0 : 1],
					videoram[offs] + 256 * (colorram[offs] & 0x03),
					(colorram[offs] & 0xf0) >> 4,
					0,0,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}

	sqix_minx=1000;sqix_maxx=-1;sqix_miny=1000;sqix_maxy=-1;
}
