#include "driver.h"
#include "vidhrdw/generic.h"

#define COLORTABLE_START(gfxn,color)	Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + \
					color * Machine->gfx[gfxn]->color_granularity
#define GFX_COLOR_CODES(gfxn) 		Machine->gfx[gfxn]->total_colors
#define GFX_ELEM_COLORS(gfxn) 		Machine->gfx[gfxn]->color_granularity

unsigned char 	*ninjakd2_scrolly_ram;
unsigned char 	*ninjakd2_scrollx_ram;
unsigned char 	*ninjakd2_bgenable_ram;
unsigned char 	*ninjakd2_spoverdraw_ram;
unsigned char 	*ninjakd2_spriteram;
size_t ninjakd2_spriteram_size;
unsigned char 	*ninjakd2_background_videoram;
size_t ninjakd2_backgroundram_size;
unsigned char 	*ninjakd2_foreground_videoram;
size_t ninjakd2_foregroundram_size;

static struct osd_bitmap *bitmap_bg;
static struct osd_bitmap *bitmap_sp;

static unsigned char 	 *bg_dirtybuffer;
static int 		 bg_enable = 1;
static int 		 sp_overdraw = 0;

int ninjakd2_vh_start(void)
{
	int i;

	if ((bg_dirtybuffer = (unsigned char*)malloc(1024)) == 0)
	{
		return 1;
	}
	if ((bitmap_bg = bitmap_alloc(Machine->drv->screen_width*2,Machine->drv->screen_height*2)) == 0)
	{
		free(bg_dirtybuffer);
		return 1;
	}
	if ((bitmap_sp = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
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

void ninjakd2_vh_stop(void)
{
	bitmap_free(bitmap_bg);
	bitmap_free(bitmap_sp);
	free(bg_dirtybuffer);
}


WRITE_HANDLER( ninjakd2_bgvideoram_w )
{
	if (ninjakd2_background_videoram[offset] != data)
	{
		bg_dirtybuffer[offset >> 1] = 1;
		ninjakd2_background_videoram[offset] = data;
	}
}

WRITE_HANDLER( ninjakd2_fgvideoram_w )
{
	if (ninjakd2_foreground_videoram[offset] != data)
		ninjakd2_foreground_videoram[offset] = data;
}

WRITE_HANDLER( ninjakd2_background_enable_w )
{
	if (bg_enable!=data)
	{
		ninjakd2_bgenable_ram[offset] = data;
		bg_enable = data;
		if (bg_enable)
		 memset(bg_dirtybuffer, 1, ninjakd2_backgroundram_size / 2);
		else
		 fillbitmap(bitmap_bg, palette_transparent_pen,0);
	}
}

WRITE_HANDLER( ninjakd2_sprite_overdraw_w )
{
	if (sp_overdraw!=data)
	{
		ninjakd2_spoverdraw_ram[offset] = data;
		fillbitmap(bitmap_sp,15,&Machine->visible_area);
		sp_overdraw = data;
	}
}

void ninjakd2_draw_foreground(struct osd_bitmap *bitmap)
{
	int offs;

	/* Draw the foreground text */

	for (offs = 0 ;offs < ninjakd2_foregroundram_size / 2; offs++)
	{
		int sx,sy,tile,palette,flipx,flipy,lo,hi;

		if (ninjakd2_foreground_videoram[offs*2] | ninjakd2_foreground_videoram[offs*2+1])
		{
			sx = (offs % 32) << 3;
			sy = (offs >> 5) << 3;

			lo = ninjakd2_foreground_videoram[offs*2];
			hi = ninjakd2_foreground_videoram[offs*2+1];
			tile = ((hi & 0xc0) << 2) | lo;
			flipx = hi & 0x20;
			flipy = hi & 0x10;
			palette = hi & 0x0f;

			drawgfx(bitmap,Machine->gfx[2],
						tile,
						palette,
						flipx,flipy,
						sx,sy,
						&Machine->visible_area,TRANSPARENCY_PEN, 15);
		}

	}
}


void ninjakd2_draw_background(struct osd_bitmap *bitmap)
{
	int offs;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */

	for (offs = 0 ;offs < ninjakd2_backgroundram_size / 2; offs++)
	{
		int sx,sy,tile,palette,flipx,flipy,lo,hi;

		if (bg_dirtybuffer[offs])
		{
			sx = (offs % 32) << 4;
			sy = (offs >> 5) << 4;

			bg_dirtybuffer[offs] = 0;

			lo = ninjakd2_background_videoram[offs*2];
			hi = ninjakd2_background_videoram[offs*2+1];
			tile = ((hi & 0xc0) << 2) | lo;
			flipx = hi & 0x20;
			flipy = hi & 0x10;
			palette = hi & 0x0f;
			drawgfx(bitmap,Machine->gfx[0],
					  tile,
					  palette,
					  flipx,flipy,
					  sx,sy,
					  0,TRANSPARENCY_NONE,0);
		}

	}
}

void ninjakd2_draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;

	/* Draw the sprites */

	for (offs = 11 ;offs < ninjakd2_spriteram_size; offs+=16)
	{
		int sx,sy,tile,palette,flipx,flipy;

		if (ninjakd2_spriteram[offs+2] & 2)
		{
			sx = ninjakd2_spriteram[offs+1];
			sy = ninjakd2_spriteram[offs];
			if (ninjakd2_spriteram[offs+2] & 1) sx-=256;
			tile = ninjakd2_spriteram[offs+3]+((ninjakd2_spriteram[offs+2] & 0xc0)<<2);
			flipx = ninjakd2_spriteram[offs+2] & 0x10;
			flipy = ninjakd2_spriteram[offs+2] & 0x20;
			palette = ninjakd2_spriteram[offs+4] & 0x0f;
			drawgfx(bitmap,Machine->gfx[1],
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
void ninjakd2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int scrollx,scrolly;

	if (palette_recalc ())
		memset(bg_dirtybuffer, 1, ninjakd2_backgroundram_size / 2);

	if (bg_enable)
		ninjakd2_draw_background(bitmap_bg);

	scrollx = -((ninjakd2_scrollx_ram[0]+ninjakd2_scrollx_ram[1]*256) & 0x1FF);
	scrolly = -((ninjakd2_scrolly_ram[0]+ninjakd2_scrolly_ram[1]*256) & 0x1FF);

	if (sp_overdraw)	/* overdraw sprite mode */
	{
		ninjakd2_draw_sprites(bitmap_sp);
		ninjakd2_draw_foreground(bitmap_sp);
		copyscrollbitmap(bitmap,bitmap_bg,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
		copybitmap(bitmap,bitmap_sp,0,0,0,0,&Machine->visible_area,TRANSPARENCY_PEN, 15);
	}
	else 			/* normal sprite mode */
	{
		copyscrollbitmap(bitmap,bitmap_bg,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
		ninjakd2_draw_sprites(bitmap);
		ninjakd2_draw_foreground(bitmap);
	}

}
