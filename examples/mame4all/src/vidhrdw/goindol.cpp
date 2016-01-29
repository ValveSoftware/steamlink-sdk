/***************************************************************************
  Goindol

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"

unsigned char 	 	 *goindol_bg_videoram;
unsigned char 	 	 *goindol_fg_videoram;
unsigned char 		 *goindol_spriteram1;
unsigned char 		 *goindol_spriteram2;
unsigned char 		 *goindol_fg_scrollx;
unsigned char 		 *goindol_fg_scrolly;
static struct osd_bitmap *bitmap_bg;
static struct osd_bitmap *bitmap_fg;
static unsigned char 	 *fg_dirtybuffer;
static unsigned char 	 *bg_dirtybuffer;

size_t goindol_fg_videoram_size;
size_t goindol_bg_videoram_size;
size_t goindol_spriteram_size;
int 	 		 goindol_char_bank;


/***************************************************************************

  Convert the color PROMs into a more useable format.

***************************************************************************/
void goindol_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
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

	/* characters */

	for (i = 0;i < 256;i++)
			COLOR(0,i) = i;
}


WRITE_HANDLER( goindol_fg_videoram_w )
{
	if (goindol_fg_videoram[offset] != data)
	{
		fg_dirtybuffer[offset >> 1] = 1;
		goindol_fg_videoram[offset] = data;
	}
}

WRITE_HANDLER( goindol_bg_videoram_w )
{
	if (goindol_bg_videoram[offset] != data)
	{
		bg_dirtybuffer[offset >> 1] = 1;
		goindol_bg_videoram[offset] = data;
	}
}

int goindol_vh_start(void)
{
        if ((fg_dirtybuffer = (unsigned char*)malloc(32*32)) == 0)
	{
        	return 1;
	}
        if ((bg_dirtybuffer = (unsigned char*)malloc(32*32)) == 0)
	{
		free(bg_dirtybuffer);
        	return 1;
	}
	if ((bitmap_fg = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		free(fg_dirtybuffer);
		free(bg_dirtybuffer);
		return 1;
	}
	if ((bitmap_bg = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		bitmap_free(bitmap_fg);
		free(fg_dirtybuffer);
		free(bg_dirtybuffer);
		return 1;
	}
        memset(fg_dirtybuffer,1,32*32);
        memset(bg_dirtybuffer,1,32*32);
        return 0;
}

void goindol_vh_stop(void)
{
	free(fg_dirtybuffer);
	free(bg_dirtybuffer);
	bitmap_free(bitmap_fg);
	bitmap_free(bitmap_bg);
}

void goindol_draw_background(struct osd_bitmap *bitmap)
{
	int x,y,offs;
	int sx,sy,tile,palette,lo,hi;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */

	for (x = 0; x < 32; x++)
	{
		for (y = 0; y < 32; y++)
		{
			offs = y*64+(x*2);
			if (bg_dirtybuffer[offs >> 1])
			{
				sx = x << 3;
				sy = y << 3;

				bg_dirtybuffer[offs >> 1] = 0;

				hi = goindol_bg_videoram[offs];
				lo = goindol_bg_videoram[offs+1];
				tile = ((hi & 0x7) << 8) | lo;
				palette = hi >> 3;
				drawgfx(bitmap,Machine->gfx[1],
						  tile,
						  palette,
						  0,0,
						  sx,sy,
						  0,TRANSPARENCY_NONE,0);
			}
		}
	}
}

void goindol_draw_foreground(struct osd_bitmap *bitmap)
{
	int x,y,offs;
	int sx,sy,tile,palette,lo,hi;

	for (x = 0; x < 32; x++)
	{
		for (y = 0; y < 32; y++)
		{
			offs = y*64+(x*2);
			if (fg_dirtybuffer[offs >> 1])
			{
				sx = x << 3;
				sy = y << 3;

				fg_dirtybuffer[offs >> 1] = 0;

				hi = goindol_fg_videoram[offs];
				lo = goindol_fg_videoram[offs+1];
				tile = ((hi & 0x7) << 8) | lo;
				palette = hi >> 3;
				drawgfx(bitmap,Machine->gfx[0],
						  tile+(goindol_char_bank << 7),
						  palette,
						  0,0,
						  sx,sy,
						  0,TRANSPARENCY_NONE,0);
			}
		}
	}

}

void goindol_draw_sprites(struct osd_bitmap *bitmap, int gfxbank, unsigned char *sprite_ram)
{
	int offs,sx,sy,tile,palette;

	for (offs = 0 ;offs < goindol_spriteram_size; offs+=4)
	{
		sx = sprite_ram[offs];
		sy = 240-sprite_ram[offs+1];

		if ((sprite_ram[offs+1] >> 3) && (sx < 248))
		{
			tile	 = ((sprite_ram[offs+3])+((sprite_ram[offs+2] & 7) << 8));
			tile	+= tile;
			palette	 = sprite_ram[offs+2] >> 3;

			drawgfx(bitmap,Machine->gfx[gfxbank],
						tile,
						palette,
						0,0,
						sx,sy,
						&Machine->visible_area,
						TRANSPARENCY_PEN, 0);
			drawgfx(bitmap,Machine->gfx[gfxbank],
						tile+1,
						palette,
						0,0,
						sx,sy+8,
						&Machine->visible_area,
						TRANSPARENCY_PEN, 0);
		}
	}
}

void goindol_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int fg_scrollx,fg_scrolly;

	fg_scrollx = -*goindol_fg_scrollx;
	fg_scrolly = -*goindol_fg_scrolly;

	goindol_draw_background(bitmap_bg);
	goindol_draw_foreground(bitmap_fg);
	copybitmap(bitmap,bitmap_bg,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	copyscrollbitmap(bitmap,bitmap_fg,1,&fg_scrolly,1,&fg_scrollx,&Machine->visible_area,TRANSPARENCY_COLOR, 0);
	goindol_draw_sprites(bitmap,1,goindol_spriteram1);
	goindol_draw_sprites(bitmap,0,goindol_spriteram2);
}
