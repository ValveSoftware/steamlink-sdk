#include "driver.h"

unsigned char *bjtwin_cmdram;
unsigned char *bjtwin_workram;
unsigned char *bjtwin_spriteram;
unsigned char *bjtwin_txvideoram;
unsigned char *bjtwin_videocontrol;
size_t bjtwin_txvideoram_size;

static unsigned char * dirtybuffer;
static struct osd_bitmap *tmpbitmap;
static int flipscreen = 0;


int bjtwin_vh_start(void)
{
	dirtybuffer = (unsigned char*)malloc(bjtwin_txvideoram_size/2);
	tmpbitmap = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height);

	if (!dirtybuffer || !tmpbitmap)
	{
		if (tmpbitmap) bitmap_free(tmpbitmap);
		if (dirtybuffer) free(dirtybuffer);
		return 1;
	}

	bjtwin_spriteram = bjtwin_workram + 0x8000;

	return 0;
}

void bjtwin_vh_stop(void)
{
	bitmap_free(tmpbitmap);
	free(dirtybuffer);

	dirtybuffer = 0;
	tmpbitmap = 0;
}


READ_HANDLER( bjtwin_txvideoram_r )
{
	return READ_WORD(&bjtwin_txvideoram[offset]);
}

WRITE_HANDLER( bjtwin_txvideoram_w )
{
	int oldword = READ_WORD(&bjtwin_txvideoram[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&bjtwin_txvideoram[offset],newword);
		dirtybuffer[offset/2] = 1;
	}
}


WRITE_HANDLER( bjtwin_paletteram_w )
{
	int r,g,b;
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword,data);


	WRITE_WORD(&paletteram[offset],newword);

	r = ((newword >> 11) & 0x1e) | ((newword >> 3) & 0x01);
	g = ((newword >>  7) & 0x1e) | ((newword >> 2) & 0x01);
	b = ((newword >>  3) & 0x1e) | ((newword >> 1) & 0x01);

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	palette_change_color(offset / 2,r,g,b);
}


WRITE_HANDLER( bjtwin_flipscreen_w )
{
	if ((data & 1) != flipscreen)
	{
		flipscreen = data & 1;
		memset(dirtybuffer, 1, bjtwin_txvideoram_size/2);
	}
}

void bjtwin_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	static int oldbgstart = -1;
	int offs, bgstart;

	bgstart = 2048 * (READ_WORD(&bjtwin_videocontrol[0]) & 0x0f);

	palette_init_used_colors();

	for (offs = (bjtwin_txvideoram_size/2)-1; offs >= 0; offs--)
	{
		int color = (READ_WORD(&bjtwin_txvideoram[offs*2]) >> 12);
		memset(&palette_used_colors[16 * color],PALETTE_COLOR_USED,16);
	}

	for (offs = 0; offs < 256*16; offs += 16)
	{
		if (READ_WORD(&bjtwin_spriteram[offs]) != 0)
			memset(&palette_used_colors[256 + 16*READ_WORD(&bjtwin_spriteram[offs+14])],PALETTE_COLOR_USED,16);
	}

	if (palette_recalc() || (oldbgstart != bgstart))
	{
		oldbgstart = bgstart;
		memset(dirtybuffer, 1, bjtwin_txvideoram_size/2);
	}

	for (offs = (bjtwin_txvideoram_size/2)-1; offs >= 0; offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx = offs / 32;
			int sy = offs % 32;

			int tilecode = READ_WORD(&bjtwin_txvideoram[offs*2]);
			int bank = (tilecode & 0x800) ? 1 : 0;

			if (flipscreen)
			{
				sx = 47-sx;
				sy = 31-sy;
			}

			drawgfx(tmpbitmap,Machine->gfx[bank],
					(tilecode & 0x7ff) + ((bank) ? bgstart : 0),
					tilecode >> 12,
					flipscreen, flipscreen,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);

			dirtybuffer[offs] = 0;
		}
	}

	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	for (offs = 0; offs < 256*16; offs += 16)
	{
		if (READ_WORD(&bjtwin_spriteram[offs]) != 0)
		{
			int sx = (READ_WORD(&bjtwin_spriteram[offs+8]) & 0x1ff) + 64;
			int sy = (READ_WORD(&bjtwin_spriteram[offs+12]) & 0x1ff);
			int tilecode = READ_WORD(&bjtwin_spriteram[offs+6]);
			int xx = (READ_WORD(&bjtwin_spriteram[offs+2]) & 0x0f) + 1;
			int yy = (READ_WORD(&bjtwin_spriteram[offs+2]) >> 4) + 1;
			int width = xx;
			int delta = 16;
			int startx = sx;

			if (flipscreen)
			{
				sx = 367 - sx;
				sy = 239 - sy;
				delta = -16;
				startx = sx;
			}

			do
			{
				do
				{
					drawgfx(bitmap,Machine->gfx[2],
							tilecode & 0x1fff,
							READ_WORD(&bjtwin_spriteram[offs+14]),
							flipscreen, flipscreen,
							sx & 0x1ff,sy & 0x1ff,
							&Machine->visible_area,TRANSPARENCY_PEN,15);

					tilecode++;
					sx += delta;
				} while (--xx);

				sy += delta;
				sx = startx;
				xx = width;
			} while (--yy);
		}
	}
}
