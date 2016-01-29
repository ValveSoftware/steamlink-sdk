#include "driver.h"
#include "vidhrdw/generic.h"

#define COLORTABLE_START(gfxn,color)	Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + \
					color * Machine->gfx[gfxn]->color_granularity
#define GFX_COLOR_CODES(gfxn) 		Machine->gfx[gfxn]->total_colors
#define GFX_ELEM_COLORS(gfxn) 		Machine->gfx[gfxn]->color_granularity

unsigned char   *mnight_scrolly_ram;
unsigned char   *mnight_scrollx_ram;
unsigned char   *mnight_bgenable_ram;
unsigned char   *mnight_spoverdraw_ram;
unsigned char   *mnight_spriteram;
size_t mnight_spriteram_size;
unsigned char   *mnight_background_videoram;
size_t mnight_backgroundram_size;
unsigned char   *mnight_foreground_videoram;
size_t mnight_foregroundram_size;

static struct osd_bitmap *bitmap_bg;
static struct osd_bitmap *bitmap_sp;

static unsigned char     *bg_dirtybuffer;
static int       bg_enable = 1;
static int       sp_overdraw = 0;

int mnight_vh_start(void)
{
	int i;

	if ((bg_dirtybuffer = (unsigned char*)malloc(1024)) == 0)
	{
		return 1;
	}
	if ((bitmap_bg = bitmap_alloc (Machine->drv->screen_width*2,Machine->drv->screen_height*2)) == 0)
	{
		free(bg_dirtybuffer);
		return 1;
	}
	if ((bitmap_sp = bitmap_alloc (Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		free(bg_dirtybuffer);
		free(bitmap_bg);
		return 1;
	}
	memset(bg_dirtybuffer,1,1024);

	/* chars, background tiles, sprites */
	memset(palette_used_colors,PALETTE_COLOR_USED,Machine->drv->total_colors * sizeof(unsigned char));

	for (i = 0;i < GFX_COLOR_CODES(1);i++)
	{
		palette_used_colors[COLORTABLE_START(1,i)+15] = PALETTE_COLOR_TRANSPARENT;
		palette_used_colors[COLORTABLE_START(2,i)+15] = PALETTE_COLOR_TRANSPARENT;
	}
	return 0;
}

void mnight_vh_stop(void)
{
	bitmap_free(bitmap_bg);
	bitmap_free(bitmap_sp);
	free(bg_dirtybuffer);
}


WRITE_HANDLER( mnight_bgvideoram_w )
{
	if (mnight_background_videoram[offset] != data)
	{
		bg_dirtybuffer[offset >> 1] = 1;
		mnight_background_videoram[offset] = data;
	}
}

WRITE_HANDLER( mnight_fgvideoram_w )
{
	if (mnight_foreground_videoram[offset] != data)
		mnight_foreground_videoram[offset] = data;
}

WRITE_HANDLER( mnight_background_enable_w )
{
	if (bg_enable!=data)
	{
		mnight_bgenable_ram[offset] = data;
		bg_enable = data;
		if (bg_enable)
			memset(bg_dirtybuffer, 1, mnight_backgroundram_size / 2);
		else
			fillbitmap(bitmap_bg, palette_transparent_pen,0);
	}
}

WRITE_HANDLER( mnight_sprite_overdraw_w )
{
	if (sp_overdraw != (data&1))
	{
		mnight_spoverdraw_ram[offset] = data;
		fillbitmap(bitmap_sp,15,&Machine->visible_area);
		sp_overdraw = data & 1;
	}
}

void mnight_draw_foreground(struct osd_bitmap *bitmap)
{
	int offs;

	/* Draw the foreground text */

	for (offs = 0 ;offs < mnight_foregroundram_size / 2; offs++)
	{
		int sx,sy,tile,palette,flipx,flipy,lo,hi;

		if (mnight_foreground_videoram[offs*2] | mnight_foreground_videoram[offs*2+1])
		{
			sx = (offs % 32) << 3;
			sy = (offs >> 5) << 3;

			lo = mnight_foreground_videoram[offs*2];
			hi = mnight_foreground_videoram[offs*2+1];
			tile = ((hi & 0xc0) << 2) | lo;
			flipx = hi & 0x10;
			flipy = hi & 0x20;
			palette = hi & 0x0f;

			drawgfx(bitmap,Machine->gfx[3],
					tile,
					palette,
					flipx,flipy,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN, 15);
		}

	}
}


void mnight_draw_background(struct osd_bitmap *bitmap)
{
	int offs;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */

	for (offs = 0 ;offs < mnight_backgroundram_size / 2; offs++)
	{
		int sx,sy,tile,palette,flipy,lo,hi;

		if (bg_dirtybuffer[offs])
		{
			sx = (offs % 32) << 4;
			sy = (offs >> 5) << 4;

			bg_dirtybuffer[offs] = 0;

			lo = mnight_background_videoram[offs*2];
			hi = mnight_background_videoram[offs*2+1];
			tile = ((hi & 0x10) << 6) | ((hi & 0xc0) << 2) | lo;
			flipy = hi & 0x20;
			palette = hi & 0x0f;
			drawgfx(bitmap,Machine->gfx[0],
					tile,
					palette,
					0,flipy,
					sx,sy,
					0,TRANSPARENCY_NONE,0);
		}

	}
}

void mnight_draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;

	/* Draw the sprites */

	for (offs = 11 ;offs < mnight_spriteram_size; offs+=16)
	{
		int sx,sy,tile,palette,flipx,flipy,big;

		if (mnight_spriteram[offs+2] & 2)
		{
			sx = mnight_spriteram[offs+1];
			sy = mnight_spriteram[offs];
			if (mnight_spriteram[offs+2] & 1) sx-=256;
			tile = mnight_spriteram[offs+3]+((mnight_spriteram[offs+2] & 0xc0)<<2) + ((mnight_spriteram[offs+2] & 0x08)<<7);
			big  = mnight_spriteram[offs+2] & 4;
			if (big) tile /= 4;
			flipx = mnight_spriteram[offs+2] & 0x10;
			flipy = mnight_spriteram[offs+2] & 0x20;
			palette = mnight_spriteram[offs+4] & 0x0f;
			drawgfx(bitmap,Machine->gfx[(big)?2:1],
					tile,
					palette,
					flipx,flipy,
					sx,sy,
					&Machine->visible_area,
					TRANSPARENCY_PEN, 15);
		}
	}
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void mnight_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int scrollx,scrolly;

	/* TODO: handle palette properly, it overflows */
	if (palette_recalc ())
		memset(bg_dirtybuffer, 1, mnight_backgroundram_size / 2);

	if (bg_enable)
		mnight_draw_background(bitmap_bg);

	scrollx = -((mnight_scrollx_ram[0]+mnight_scrollx_ram[1]*256) & 0x1FF);
	scrolly = -((mnight_scrolly_ram[0]+mnight_scrolly_ram[1]*256) & 0x1FF);

	if (sp_overdraw)	/* overdraw sprite mode */
	{
		copyscrollbitmap(bitmap,bitmap_bg,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
		mnight_draw_sprites(bitmap_sp);
		mnight_draw_foreground(bitmap_sp);
		copybitmap(bitmap,bitmap_sp,0,0,0,0,&Machine->visible_area,TRANSPARENCY_PEN, 15);
	}
	else			/* normal sprite mode */
	{
		copyscrollbitmap(bitmap,bitmap_bg,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
		mnight_draw_sprites(bitmap);
		mnight_draw_foreground(bitmap);
	}

}
