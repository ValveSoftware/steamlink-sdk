/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

  Video output ports:
    ????? (3)
        0x03 ?????

    Scroll page (0x0d)
        Read/write scroll page number register

    Screen layout (0x0e)
        Screen layout (8x4 or 4x8)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

#include "osdepend.h"

unsigned char *blktiger_backgroundram;
static unsigned char blktiger_video_control;
unsigned char *blktiger_screen_layout;
size_t blktiger_backgroundram_size;
WRITE_HANDLER( blktiger_scrollx_w );
WRITE_HANDLER( blktiger_scrolly_w );
WRITE_HANDLER( blktiger_scrollbank_w );

static int blktiger_scroll_bank;
static const int scroll_page_count=4;
static unsigned char blktiger_scrolly[2];
static unsigned char blktiger_scrollx[2];
static unsigned char *scroll_ram;
static unsigned char *dirtybuffer2;
static struct osd_bitmap *tmpbitmap2;
static struct osd_bitmap *tmpbitmap3;
static int screen_layout;
static int chon,objon,bgon;



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int blktiger_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((dirtybuffer2 = (unsigned char*)malloc(blktiger_backgroundram_size * scroll_page_count)) == 0)
	{
		generic_vh_stop();
		return 1;
	}
	memset(dirtybuffer2,1,blktiger_backgroundram_size * scroll_page_count);

	if ((scroll_ram = (unsigned char*)malloc(blktiger_backgroundram_size * scroll_page_count)) == 0)
	{
		generic_vh_stop();
		return 1;
	}
	memset(scroll_ram,0,blktiger_backgroundram_size * scroll_page_count);


	/* the background area is 8 x 4 */
	if ((tmpbitmap2 = bitmap_alloc(8 * Machine->drv->screen_width,
			scroll_page_count * Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer2);
		generic_vh_stop();
		return 1;
	}

	/* the alternative background area is 4 x 8 */
	if ((tmpbitmap3 = bitmap_alloc(4 * Machine->drv->screen_width,
			2 * scroll_page_count * Machine->drv->screen_height)) == 0)
	{
		free(dirtybuffer2);
		bitmap_free(tmpbitmap2);
		generic_vh_stop();
		return 1;
	}


	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void blktiger_vh_stop(void)
{
	bitmap_free(tmpbitmap2);
	free(dirtybuffer2);
	free(scroll_ram);
	generic_vh_stop();
}



WRITE_HANDLER( blktiger_background_w )
{
	offset += blktiger_scroll_bank;

	/* TODO: there's a bug lurking around. If I uncomment the following line, */
	/* the intro screen doesn't work anymore (complete black instead of city landscape) */
//	if (scroll_ram[offset] != data)
	{
		dirtybuffer2[offset] = 1;
		scroll_ram[offset] = data;
	}
}


WRITE_HANDLER( blktiger_scrollbank_w )
{
	blktiger_scroll_bank = (data & 0x03) * blktiger_backgroundram_size;
}


READ_HANDLER( blktiger_background_r )
{
	offset += blktiger_scroll_bank;
	return scroll_ram[offset];
}


WRITE_HANDLER( blktiger_scrolly_w )
{
	blktiger_scrolly[offset]=data;
}

WRITE_HANDLER( blktiger_scrollx_w )
{
	blktiger_scrollx[offset]=data;
}


WRITE_HANDLER( blktiger_video_control_w )
{
	/* bits 0 and 1 are coin counters */
	coin_counter_w(0,data & 1);
	coin_counter_w(1,data & 2);

	/* bit 5 resets the sound CPU - we ignore it */

	/* bit 6 flips screen */
	blktiger_video_control=data;

	/* bit 7 enables characters? Just a guess */
	chon = ~data & 0x80;
}

WRITE_HANDLER( blktiger_video_enable_w )
{
	/* not sure which is which, but I think that bit 1 and 2 enable background and sprites */
	/* bit 1 enables bg ? */
	bgon = ~data & 0x02;

	/* bit 2 enables sprites ? */
	objon = ~data & 0x04;
}

WRITE_HANDLER( blktiger_screen_layout_w )
{
	screen_layout = data;
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/

void blktiger_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs, sx, sy;


palette_init_used_colors();

{
	int color,code,i;
	int colmask[16];
	int pal_base;


	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;

	for (color = 0;color < 16;color++) colmask[color] = 0;

	for (offs = blktiger_backgroundram_size * scroll_page_count - 2;offs >= 0;offs -= 2)
	{
		int attr;

		attr = scroll_ram[offs + 1];
		color = (attr & 0x78) >> 3;
		code = scroll_ram[offs] + ((attr & 0x07) << 8);
		colmask[color] |= Machine->gfx[1]->pen_usage[code];
	}

	for (color = 0;color < 16;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
		if (colmask[color] & (1 << 15))
			palette_used_colors[pal_base + 16 * color + 15] = PALETTE_COLOR_TRANSPARENT;
	}
}
for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
{
	int colour,code,x;


	sy = spriteram[offs + 2];
	sx = spriteram[offs + 3] - ((spriteram[offs + 1] & 0x10) << 4);

	/* only count visible sprites */
	if (sx+15 >= Machine->visible_area.min_x &&
			sx <= Machine->visible_area.max_x &&
			sy+15 >= Machine->visible_area.min_y &&
			sy <= Machine->visible_area.max_y)
	{
		colour = spriteram[offs+1] & 0x07;
		code=scroll_ram[offs];
		code = spriteram[offs];
		code += ( ((int)(spriteram[offs+1]&0xe0)) << 3 );
		for (x = 0;x < 15;x++)
		{
			if (Machine->gfx[2]->pen_usage[code] & (1 << x))
				palette_used_colors[512 + 16 * colour + x] = PALETTE_COLOR_USED;
		}
	}
}
for (offs = videoram_size - 1;offs >= 0;offs--)
{
	int colour,code,x;

	colour = colorram[offs] & 0x1f;
	code = videoram[offs] + ((colorram[offs] & 0xe0) << 3);
	for (x = 0;x < 3;x++)
	{
		if (Machine->gfx[0]->pen_usage[code] & (1 << x))
			palette_used_colors[768 + 4 * colour + x] = PALETTE_COLOR_USED;
	}
}


if (palette_recalc())
	memset(dirtybuffer2,1,blktiger_backgroundram_size * scroll_page_count);



	if (bgon)
	{
		/*
		Draw the tiles.

		This method may look unnecessarily complex. Only tiles that are
		likely to be visible are drawn. The rest are kept dirty until they
		become visible.

		The reason for this is that on level 3, the palette changes a lot
		if the whole virtual screen is checked and redrawn then the
		game will slow down to a crawl.
		*/

		if (screen_layout)
		{
			/* 8x4 screen */
			int offsetbase;
			int scrollx,scrolly, y;
			scrollx = ((blktiger_scrollx[0]>>4) + 16 * blktiger_scrollx[1]);
			scrolly = ((blktiger_scrolly[0]>>4) + 16 * blktiger_scrolly[1]);

			for (sy=0; sy<18; sy++)
			{
				y=(scrolly+sy)&(16*4-1);
				offsetbase=((y&0xf0)<<8)+32*(y&0x0f);
				for (sx=0; sx<18; sx++)
				{
					int colour, attr, code, x;
					x=(scrollx+sx)&(16*8-1);
					offs=offsetbase + ((x&0xf0)<<5)+2*(x&0x0f);

					if (dirtybuffer2[offs] || dirtybuffer2[offs+1])
					{
						attr=scroll_ram[offs+1];
						colour=(attr&0x78)>>3;
						code=scroll_ram[offs];
						code+=256*(attr&0x07);

						dirtybuffer2[offs] = dirtybuffer2[offs+1] = 0;

						drawgfx(tmpbitmap2,Machine->gfx[1],
								code,
								colour,
								attr & 0x80,0,
								x*16, y*16,
								0,TRANSPARENCY_NONE,0);
					}
				}
			}

			/* copy the background graphics */
			{
				scrollx = -(blktiger_scrollx[0] + 256 * blktiger_scrollx[1]);
				scrolly = -(blktiger_scrolly[0] + 256 * blktiger_scrolly[1]);
				copyscrollbitmap(bitmap,tmpbitmap2,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
			}
		}
		else
		{
			/* 4x8 screen */
			int offsetbase;
			int scrollx,scrolly, y;
			scrollx = ((blktiger_scrollx[0]>>4) + 16 * blktiger_scrollx[1]);
			scrolly = ((blktiger_scrolly[0]>>4) + 16 * blktiger_scrolly[1]);

			for (sy=0; sy<18; sy++)
			{
				y=(scrolly+sy)&(16*8-1);
				offsetbase=((y&0xf0)<<7)+32*(y&0x0f);
				for (sx=0; sx<18; sx++)
				{
					int colour, attr, code, x;
					x=(scrollx+sx)&(16*4-1);
					offs=offsetbase + ((x&0xf0)<<5)+2*(x&0x0f);

					if (dirtybuffer2[offs] || dirtybuffer2[offs+1])
					{
						attr=scroll_ram[offs+1];
						colour=(attr&0x78)>>3;

						code=scroll_ram[offs];
						code+=256*(attr&0x07);

						dirtybuffer2[offs] = dirtybuffer2[offs+1] = 0;

						drawgfx(tmpbitmap3,Machine->gfx[1],
								code,
								colour,
								attr & 0x80,0,
								x*16, y*16,
								0,TRANSPARENCY_NONE,0);
					}
				}
			}

			/* copy the background graphics */
			{
			scrollx = -(blktiger_scrollx[0] + 256 * blktiger_scrollx[1]);
			scrolly = -(blktiger_scrolly[0] + 256 * blktiger_scrolly[1]);
			copyscrollbitmap(bitmap,tmpbitmap3,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
			}
		}
	}
	else fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);


	if (objon)
	{
		/* Draw the sprites. */
		for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
		{
			/*	SPRITES
				=====
				Attribute
				0x80 Code MSB
				0x40 Code MSB
				0x20 Code MSB
				0x10 X MSB
				0x08 X flip
				0x04 Colour
				0x02 Colour
				0x01 Colour
			*/

			int code,colour;

			code = spriteram[offs];
			code += ( ((int)(spriteram[offs+1]&0xe0)) << 3 );
			colour = spriteram[offs+1] & 0x07;

			sy = spriteram[offs + 2];
			sx = spriteram[offs + 3] - ((spriteram[offs + 1] & 0x10) << 4);

			drawgfx(bitmap,Machine->gfx[2],
					code,
					colour,
					spriteram[offs+1]&0x08,0,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,15);
		}
	}

	if (chon)
	{
		/* draw the frontmost playfield. They are characters, but draw them as sprites */
		for (offs = videoram_size - 1;offs >= 0;offs--)
		{
			sx = offs % 32;
			sy = offs / 32;

			drawgfx(bitmap,Machine->gfx[0],
					videoram[offs] + ((colorram[offs] & 0xe0) << 3),
					colorram[offs] & 0x1f,
					0,0,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_PEN,3);
		}
	}
}
