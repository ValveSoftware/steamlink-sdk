/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *vulgus_bgvideoram,*vulgus_bgcolorram;
size_t vulgus_bgvideoram_size;
unsigned char *vulgus_scrolllow,*vulgus_scrollhigh;
unsigned char *vulgus_palette_bank;
static unsigned char *dirtybuffer2;
static struct osd_bitmap *tmpbitmap2;



/***************************************************************************

  Convert the color PROMs into a more useable format.

***************************************************************************/
void vulgus_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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
	/* color_prom now points to the beginning of the lookup table */


	/* characters use colors 32-47 (?) */
	for (i = 0;i < TOTAL_COLORS(0);i++)
		COLOR(0,i) = *(color_prom++) + 32;

	/* sprites use colors 16-31 */
	for (i = 0;i < TOTAL_COLORS(2);i++)
	{
		COLOR(2,i) = *(color_prom++) + 16;
	}

	/* background tiles use colors 0-15, 64-79, 128-143, 192-207 in four banks */
	for (i = 0;i < TOTAL_COLORS(1)/4;i++)
	{
		COLOR(1,i) = *color_prom;
		COLOR(1,i+32*8) = *color_prom + 64;
		COLOR(1,i+2*32*8) = *color_prom + 128;
		COLOR(1,i+3*32*8) = *color_prom + 192;
		color_prom++;
	}
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int vulgus_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((dirtybuffer2 = (unsigned char*)malloc(vulgus_bgvideoram_size)) == 0)
	{
		generic_vh_stop();
		return 1;
	}
	memset(dirtybuffer2,1,vulgus_bgvideoram_size);

	/* the background area is twice as tall and twice as large as the screen */
	if ((tmpbitmap2 = bitmap_alloc(2*Machine->drv->screen_width,2*Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer2);
		generic_vh_stop();
		return 1;
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void vulgus_vh_stop(void)
{
	bitmap_free(tmpbitmap2);
	free(dirtybuffer2);
	generic_vh_stop();
}



WRITE_HANDLER( vulgus_bgvideoram_w )
{
	if (vulgus_bgvideoram[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		vulgus_bgvideoram[offset] = data;
	}
}



WRITE_HANDLER( vulgus_bgcolorram_w )
{
	if (vulgus_bgcolorram[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		vulgus_bgcolorram[offset] = data;
	}
}



WRITE_HANDLER( vulgus_palette_bank_w )
{
	if (*vulgus_palette_bank != data)
	{
		memset(dirtybuffer2,1,vulgus_bgvideoram_size);
		*vulgus_palette_bank = data;
	}
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void vulgus_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;
	int scrollx,scrolly;


	scrolly = -(vulgus_scrolllow[0] + 256 * vulgus_scrollhigh[0]);
	scrollx = -(vulgus_scrolllow[1] + 256 * vulgus_scrollhigh[1]);

	for (offs = vulgus_bgvideoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy;


		if (dirtybuffer2[offs])
		{
//			int minx,maxx,miny,maxy;


			sx = (offs % 32);
			sy = (offs / 32);

			/* between level Vulgus changes the palette bank every frame. Redrawing */
			/* the whole background every time would slow the game to a crawl, so here */
			/* we check and redraw only the visible tiles */
/*
			minx = (sx + scrollx) & 0x1ff;
			maxx = (sx + 15 + scrollx) & 0x1ff;
			if (minx > maxx) minx = maxx - 15;
			miny = (sy + scrolly) & 0x1ff;
			maxy = (sy + 15 + scrolly) & 0x1ff;
			if (miny > maxy) miny = maxy - 15;

			if (minx + 15 >= Machine->visible_area.min_x &&
					maxx - 15 <= Machine->visible_area.max_x &&
					miny + 15 >= Machine->visible_area.min_y &&
					maxy - 15 <= Machine->visible_area.max_y)
*/
			{
				dirtybuffer2[offs] = 0;

				drawgfx(tmpbitmap2,Machine->gfx[1],
						vulgus_bgvideoram[offs] + 2 * (vulgus_bgcolorram[offs] & 0x80),
						(vulgus_bgcolorram[offs] & 0x1f) + 32 * *vulgus_palette_bank,
						vulgus_bgcolorram[offs] & 0x20,vulgus_bgcolorram[offs] & 0x40,
						16*sy,16*sx,
						0,TRANSPARENCY_NONE,0);
			}
		}
	}


	/* copy the background graphics */
	copyscrollbitmap(bitmap,tmpbitmap2,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);


	/* Draw the sprites. */
	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int code,i,col,sx,sy;


		code = spriteram[offs];
		col = spriteram[offs + 1] & 0x0f;
		sx = spriteram[offs + 3];
		sy = spriteram[offs + 2];

		i = (spriteram[offs + 1] & 0xc0) >> 6;
		if (i == 2) i = 3;

		do
		{
			drawgfx(bitmap,Machine->gfx[2],
					code + i,
					col,
					0, 0,
					sx, sy + 16 * i,
					&Machine->visible_area,TRANSPARENCY_PEN,15);

			/* draw again with wraparound */
			drawgfx(bitmap,Machine->gfx[2],
					code + i,
					col,
					0, 0,
					sx, sy + 16 * i - 256,
					&Machine->visible_area,TRANSPARENCY_PEN,15);
			i--;
		} while (i >= 0);
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy;


		sx = 8 * (offs % 32);
		sy = 8 * (offs / 32);

		drawgfx(bitmap,Machine->gfx[0],
				videoram[offs] + 2 * (colorram[offs] & 0x80),
				colorram[offs] & 0x3f,
				0,0,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_COLOR,47);
	}
}
