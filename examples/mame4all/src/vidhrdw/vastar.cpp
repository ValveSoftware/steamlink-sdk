/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



unsigned char *vastar_bg1colorram2, *vastar_sprite_priority;
unsigned char *vastar_fgvideoram,*vastar_fgcolorram1,*vastar_fgcolorram2;
unsigned char *vastar_bg2videoram,*vastar_bg2colorram1,*vastar_bg2colorram2;
unsigned char *vastar_bg1scroll,*vastar_bg2scroll;
static unsigned char *dirtybuffer2;
static struct osd_bitmap *tmpbitmap2;



void vastar_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2,bit3;

		/* red component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) =  0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* green component */
		bit0 = (color_prom[Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) =  0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* blue component */
		bit0 = (color_prom[2*Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[2*Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[2*Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[2*Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) =  0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int vastar_vh_start(void)
{
	if (generic_vh_start() != 0)
		return 1;

	if ((dirtybuffer2 = (unsigned char*)malloc(videoram_size)) == 0)
	{
		generic_vh_stop();
		return 1;
	}
	memset(dirtybuffer2,1,videoram_size);

	if ((tmpbitmap2 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
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
void vastar_vh_stop(void)
{
	bitmap_free(tmpbitmap2);
	free(dirtybuffer2);
	generic_vh_stop();
}



WRITE_HANDLER( vastar_bg1colorram2_w )
{
	if (vastar_bg1colorram2[offset] != data)
	{
		dirtybuffer[offset] = 1;

		vastar_bg1colorram2[offset] = data;
	}
}

WRITE_HANDLER( vastar_bg2videoram_w )
{
	if (vastar_bg2videoram[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		vastar_bg2videoram[offset] = data;
	}
}

WRITE_HANDLER( vastar_bg2colorram1_w )
{
	if (vastar_bg2colorram1[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		vastar_bg2colorram1[offset] = data;
	}
}

WRITE_HANDLER( vastar_bg2colorram2_w )
{
	if (vastar_bg2colorram2[offset] != data)
	{
		dirtybuffer2[offset] = 1;

		vastar_bg2colorram2[offset] = data;
	}
}


void vastar_draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;


	/* Draw the sprites. Note that it is important to draw them exactly in this */
	/* order, to have the correct priorities. */
	for (offs = 0; offs < spriteram_size; offs += 2)
	{
		int code;


		code = ((spriteram_3[offs] & 0xfc) >> 2) + ((spriteram_2[offs] & 0x01) << 6)
				+ ((offs & 0x20) << 2);

		if (spriteram_2[offs] & 0x08)	/* double width */
		{
			drawgfx(bitmap,Machine->gfx[2],
					code/2,
					spriteram[offs+1] & 0x3f,
					spriteram_3[offs] & 0x02,spriteram_3[offs] & 0x01,
					spriteram_3[offs + 1],224-spriteram[offs],
					&Machine->visible_area,TRANSPARENCY_PEN,0);
			/* redraw with wraparound */
			drawgfx(bitmap,Machine->gfx[2],
					code/2,
					spriteram[offs+1] & 0x3f,
					spriteram_3[offs] & 0x02,spriteram_3[offs] & 0x01,
					spriteram_3[offs + 1],256+224-spriteram[offs],
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
		else
			drawgfx(bitmap,Machine->gfx[1],
					code,
					spriteram[offs+1] & 0x3f,
					spriteram_3[offs] & 0x02,spriteram_3[offs] & 0x01,
					spriteram_3[offs + 1],240-spriteram[offs],
					&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void vastar_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


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

			drawgfx(tmpbitmap,Machine->gfx[3],
					videoram[offs] + 256 * (vastar_bg1colorram2[offs] & 0x01),
					colorram[offs],
					0,0,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}

		if (dirtybuffer2[offs])
		{
			int sx,sy;


			dirtybuffer2[offs] = 0;

			sx = offs % 32;
			sy = offs / 32;

			drawgfx(tmpbitmap2,Machine->gfx[4],
					vastar_bg2videoram[offs] + 256 * (vastar_bg2colorram2[offs] & 0x01),
					vastar_bg2colorram1[offs],
					0,0,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the temporary bitmaps to the screen */
	{
		int scroll[32];


		for (offs = 0;offs < 32;offs++)
			scroll[offs] = -vastar_bg2scroll[offs];
		copyscrollbitmap(bitmap,tmpbitmap2,0,0,32,scroll,&Machine->visible_area,TRANSPARENCY_NONE,0);

		if (*vastar_sprite_priority == 2)
		{
			vastar_draw_sprites(bitmap);	/* sprite must appear behind background */
			copyscrollbitmap(bitmap,tmpbitmap2,0,0,32,scroll,&Machine->visible_area,TRANSPARENCY_COLOR,92);
		}
		else if (*vastar_sprite_priority == 0)
			vastar_draw_sprites(bitmap);

		for (offs = 0;offs < 32;offs++)
			scroll[offs] = -vastar_bg1scroll[offs];
		copyscrollbitmap(bitmap,tmpbitmap,0,0,32,scroll,&Machine->visible_area,TRANSPARENCY_COLOR,0);
	}


	/* draw the frontmost playfield - they are characters, but draw them as sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy;


		sx = offs % 32;
		sy = offs / 32;

		drawgfx(bitmap,Machine->gfx[0],
				vastar_fgvideoram[offs] + 256 * (vastar_fgcolorram2[offs] & 0x01),
				vastar_fgcolorram1[offs],
				0,0,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}

	if (*vastar_sprite_priority == 3)
		vastar_draw_sprites(bitmap);
}
