/*************************************************************************

  System1 / System 2.   By Jarek Parchanski & Mirko Buffoni.

  Many thanks to Roberto Ventura, for precious information about
  System 1 hardware.

*************************************************************************/

#include "system1.h"

extern int system1_bank;

unsigned char 	*system1_scroll_y;
unsigned char 	*system1_scroll_x;
unsigned char 	*system1_videoram;
unsigned char 	*system1_backgroundram;
unsigned char 	*system1_sprites_collisionram;
unsigned char 	*system1_background_collisionram;
unsigned char 	*system1_scrollx_ram;
size_t system1_videoram_size;
size_t system1_backgroundram_size;

static unsigned char	*bg_ram;
static unsigned char 	*bg_dirtybuffer;
static unsigned char 	*tx_dirtybuffer;
static unsigned char 	*SpritesCollisionTable;
static int	background_scrollx=0,background_scrolly=0;
static unsigned char bg_bank=0,bg_bank_latch=0;

static int		scrollx_row[32];
static struct osd_bitmap *bitmap1;
static struct osd_bitmap *bitmap2;

static int  system1_pixel_mode = 0,system1_background_memory,system1_video_mode=0;

static unsigned char palette_lookup[256*3];



/***************************************************************************

  Convert the color PROMs into a more useable format.

  There are two kind of color handling: in the System 1 games, values in the
  palette RAM are directly mapped to colors with the usual BBGGGRRR format;
  in the System 2 ones (Choplifter, WBML, etc.), the value in the palette RAM
  is a lookup offset for three palette PROMs in RRRRGGGGBBBB format.

  It's hard to tell for sure because they use resistor packs, but here's
  what I think the values are from measurment with a volt meter:

  Blue: .250K ohms
  Blue: .495K ohms
  Green:.250K ohms
  Green:.495K ohms
  Green:.995K ohms
  Red:  .495K ohms
  Red:  .250K ohms
  Red:  .995K ohms

  accurate to +/- .003K ohms.

***************************************************************************/
void system1_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;

	palette = palette_lookup;

	if (color_prom)
	{
		for (i = 0;i < 256;i++)
		{
			int bit0,bit1,bit2,bit3;

			bit0 = (color_prom[0*256] >> 0) & 0x01;
			bit1 = (color_prom[0*256] >> 1) & 0x01;
			bit2 = (color_prom[0*256] >> 2) & 0x01;
			bit3 = (color_prom[0*256] >> 3) & 0x01;
			*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
			bit0 = (color_prom[1*256] >> 0) & 0x01;
			bit1 = (color_prom[1*256] >> 1) & 0x01;
			bit2 = (color_prom[1*256] >> 2) & 0x01;
			bit3 = (color_prom[1*256] >> 3) & 0x01;
			*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
			bit0 = (color_prom[2*256] >> 0) & 0x01;
			bit1 = (color_prom[2*256] >> 1) & 0x01;
			bit2 = (color_prom[2*256] >> 2) & 0x01;
			bit3 = (color_prom[2*256] >> 3) & 0x01;
			*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
			color_prom++;
		}
	}
	else
	{
		for (i = 0;i < 256;i++)
		{
			int val;

			/* red component */
			val = (i >> 0) & 0x07;
			*(palette++) = (val << 5) | (val << 2) | (val >> 1);
			/* green component */
			val = (i >> 3) & 0x07;
			*(palette++) = (val << 5) | (val << 2) | (val >> 1);
			/* blue component */
			val = (i >> 5) & 0x06;
			if (val) val++;
			*(palette++) = (val << 5) | (val << 2) | (val >> 1);
		}
	}
}

WRITE_HANDLER( system1_paletteram_w )
{
	unsigned char *palette = palette_lookup + data * 3;
	int r,g,b;


	paletteram[offset] = data;

	r = *palette++;
	g = *palette++;
	b = *palette++;

	palette_change_color(offset,r,g,b);
}



int system1_vh_start(void)
{
	if ((SpritesCollisionTable = (unsigned char*)malloc(256*256)) == 0)
		return 1;
	memset(SpritesCollisionTable,255,256*256);

	if ((bg_dirtybuffer = (unsigned char*)malloc(1024)) == 0)
	{
		free(SpritesCollisionTable);
		return 1;
	}
	memset(bg_dirtybuffer,1,1024);
	if ((tx_dirtybuffer = (unsigned char*)malloc(1024)) == 0)
	{
		free(bg_dirtybuffer);
		free(SpritesCollisionTable);
		return 1;
	}
	memset(tx_dirtybuffer,1,1024);
	if ((bg_ram = (unsigned char*)malloc(0x4000)) == 0)			/* Allocate 16k for background banked ram */
	{
		free(bg_dirtybuffer);
		free(tx_dirtybuffer);
		free(SpritesCollisionTable);
		return 1;
	}
	memset(bg_ram,0,0x4000);
	if ((bitmap1 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		free(bg_ram);
		free(bg_dirtybuffer);
		free(tx_dirtybuffer);
		free(SpritesCollisionTable);
		return 1;
	}
	if ((bitmap2 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		bitmap_free(bitmap1);
		free(bg_ram);
		free(bg_dirtybuffer);
		free(tx_dirtybuffer);
		free(SpritesCollisionTable);
		return 1;
	}

	return 0;
}

void system1_vh_stop(void)
{
	bitmap_free(bitmap2);
	bitmap_free(bitmap1);
	free(bg_ram);
	free(bg_dirtybuffer);
	free(tx_dirtybuffer);
	free(SpritesCollisionTable);
}

WRITE_HANDLER( system1_videomode_w )
{
//if (data & 0xef) logerror("videomode = %02x\n",data);

	/* bit 0 is coin counter */

	/* bit 3 is ??? */

	/* bit 4 is screen blank */
	system1_video_mode = data;
}

READ_HANDLER( system1_videomode_r )
{
	return system1_video_mode;
}

void system1_define_sprite_pixelmode(int Mode)
{
	system1_pixel_mode = Mode;
}

void system1_define_background_memory(int Mode)
{
	system1_background_memory = Mode;
}

static int GetSpriteBottomY(int spr_number)
{
	return  spriteram[0x10 * spr_number + SPR_Y_BOTTOM];
}


static void Pixel(struct osd_bitmap *bitmap,int x,int y,int spr_number,int color)
{
	int xr,yr,spr_y1,spr_y2;
	int SprOnScreen;


	if (x < Machine->visible_area.min_x ||
		x > Machine->visible_area.max_x ||
		y < Machine->visible_area.min_y ||
		y > Machine->visible_area.max_y)
		return;

	if (SpritesCollisionTable[256*y+x] == 255)
	{
		SpritesCollisionTable[256*y+x] = spr_number;
		plot_pixel(bitmap, x, y, color);
	}
	else
	{
		SprOnScreen=SpritesCollisionTable[256*y+x];
		system1_sprites_collisionram[SprOnScreen + 32 * spr_number] = 0xff;
		if (system1_pixel_mode==system1_SPRITE_PIXEL_MODE1)
		{
			spr_y1 = GetSpriteBottomY(spr_number);
			spr_y2 = GetSpriteBottomY(SprOnScreen);
			if (spr_y1 >= spr_y2)
			{
				plot_pixel(bitmap, x, y, color);
				SpritesCollisionTable[256*y+x]=spr_number;
			}
		}
		else
		{
			plot_pixel(bitmap, x, y, color);
			SpritesCollisionTable[256*y+x]=spr_number;
		}
	}

	xr = ((x - background_scrollx) & 0xff) / 8;
	yr = ((y - background_scrolly) & 0xff) / 8;

	/* TODO: bits 5 and 6 of backgroundram are also used (e.g. Pitfall2, Mr. Viking) */
	/* what's the difference? Bit 7 is used in Choplifter/WBML for extra char bank */
	/* selection, but it is also set in Pitfall2 */

	if (system1_background_memory == system1_BACKGROUND_MEMORY_SINGLE)
	{
		if (system1_backgroundram[2 * (32 * yr + xr) + 1] & 0x10)
			system1_background_collisionram[0x20 + spr_number] = 0xff;
	}
	else
	{
		/* TODO: I should handle the paged background memory here. */
		/* maybe collision detection is not used by the paged games */
		/* (wbml and tokisens), though tokisens doesn't play very well */
		/* (you can't seem to fit in gaps where you should fit) */
	}

	/* TODO: collision should probably be checked with the foreground as well */
	/* (TeddyBoy Blues, head of the tiger in girl bonus round) */
}

WRITE_HANDLER( system1_background_collisionram_w )
{
	/* to do the RAM check, Mister Viking writes 0xff and immediately */
	/* reads it back, expecting bit 0 to be NOT set. */
	system1_background_collisionram[offset] = 0x7e;
}

WRITE_HANDLER( system1_sprites_collisionram_w )
{
	/* to do the RAM check, Mister Viking write 0xff and immediately */
	/* reads it back, expecting bit 0 to be NOT set. */
	/* Up'n Down expects to find 0x7e at f800 before doing the whole */
	/* collision test */
	system1_sprites_collisionram[offset] = 0x7e;
}



extern struct GameDriver driver_wbml;

static void RenderSprite(struct osd_bitmap *bitmap,int spr_number)
{
	int SprX,SprY,Col,Row,Height,src;
	int bank;
	unsigned char *SprReg;
	unsigned short *SprPalette;
	short skip;	/* bytes to skip before drawing each row (can be negative) */


	SprReg		= spriteram + 0x10 * spr_number;

	src = SprReg[SPR_GFXOFS_LO] + (SprReg[SPR_GFXOFS_HI] << 8);
	bank = 0x8000 * (((SprReg[SPR_X_HI] & 0x80) >> 7) + ((SprReg[SPR_X_HI] & 0x40) >> 5));
	bank &= (memory_region_length(REGION_GFX2)-1);	/* limit to the range of available ROMs */
	skip = SprReg[SPR_SKIP_LO] + (SprReg[SPR_SKIP_HI] << 8);

	Height		= SprReg[SPR_Y_BOTTOM] - SprReg[SPR_Y_TOP];
	SprPalette	= Machine->remapped_colortable + 0x10 * spr_number;
	SprX = SprReg[SPR_X_LO] + ((SprReg[SPR_X_HI] & 0x01) << 8);
	SprX /= 2;	/* the hardware has sub-pixel placement, it seems */
	if (Machine->gamedrv == &driver_wbml || Machine->gamedrv->clone_of == &driver_wbml)
		SprX += 7;
	SprY = SprReg[SPR_Y_TOP] + 1;

	for (Row = 0;Row < Height;Row++)
	{
		src += skip;

		Col = 0;
		while (Col < 256)	/* this is only a safety check, */
							/* drawing is stopped by color == 15 */
		{
			int color1,color2;

			if (src & 0x8000)	/* flip x */
			{
				int offs,data;

				offs = ((src - Col / 2) & 0x7fff) + bank;

				/* memory region #2 contains the packed sprite data */
				data = memory_region(REGION_GFX2)[offs];
				color1 = data & 0x0f;
				color2 = data >> 4;
			}
			else
			{
				int offs,data;

				offs = ((src + Col / 2) & 0x7fff) + bank;

				/* memory region #2 contains the packed sprite data */
				data = memory_region(REGION_GFX2)[offs];
				color1 = data >> 4;
				color2 = data & 0x0f;
			}

			if (color1 == 15) break;
			if (color1)
				Pixel(bitmap,SprX+Col,SprY+Row,spr_number,SprPalette[color1]);

			Col++;

			if (color2 == 15) break;
			if (color2)
				Pixel(bitmap,SprX+Col,SprY+Row,spr_number,SprPalette[color2]);

			Col++;
		}
	}
}


static void DrawSprites(struct osd_bitmap *bitmap)
{
	int spr_number,SprBottom,SprTop;
	unsigned char *SprReg;


	memset(SpritesCollisionTable,255,256*256);

	for (spr_number = 0;spr_number < 32;spr_number++)
	{
		SprReg 		= spriteram + 0x10 * spr_number;
		SprTop		= SprReg[SPR_Y_TOP];
		SprBottom	= SprReg[SPR_Y_BOTTOM];
		if (SprBottom && (SprBottom-SprTop > 0))
			RenderSprite(bitmap,spr_number);
	}
}



void system1_compute_palette (void)
{
	unsigned char bg_usage[64], tx_usage[64], sp_usage[32];
	int i;

	memset (bg_usage, 0, sizeof (bg_usage));
	memset (tx_usage, 0, sizeof (tx_usage));
	memset (sp_usage, 0, sizeof (sp_usage));

	for (i = 0; i<system1_backgroundram_size; i+=2)
	{
		int code = (system1_backgroundram[i] + (system1_backgroundram[i+1] << 8)) & 0x7FF;
		int palette = code >> 5;
		bg_usage[palette & 0x3f] = 1;
	}

	for (i = 0; i<system1_videoram_size; i+=2)
	{
		int code = (system1_videoram[i] + (system1_videoram[i+1] << 8)) & 0x7FF;

		if (code)
		{
			int palette = code>>5;
			tx_usage[palette & 0x3f] = 1;
		}
	}

	for (i=0; i<32; i++)
	{
		unsigned char *reg;
		int top, bottom;

		reg 	= spriteram + 0x10 * i;
		top		= reg[SPR_Y_TOP];
		bottom	= reg[SPR_Y_BOTTOM];
		if (bottom && (bottom - top > 0))
			sp_usage[i] = 1;
	}

	for (i = 0; i < 64; i++)
	{
		if (bg_usage[i])
			memset (palette_used_colors + 1024 + i * 8, PALETTE_COLOR_USED, 8);
		else
			memset (palette_used_colors + 1024 + i * 8, PALETTE_COLOR_UNUSED, 8);

		palette_used_colors[512 + i * 8] = PALETTE_COLOR_TRANSPARENT;
		if (tx_usage[i])
			memset (palette_used_colors + 512 + i * 8 + 1, PALETTE_COLOR_USED, 7);
		else
			memset (palette_used_colors + 512 + i * 8 + 1, PALETTE_COLOR_UNUSED, 7);
	}

	for (i = 0; i < 32; i++)
	{
		palette_used_colors[0 + i * 16] = PALETTE_COLOR_TRANSPARENT;
		if (sp_usage[i])
			memset (palette_used_colors + 0 + i * 16 + 1, PALETTE_COLOR_USED, 15);
		else
			memset (palette_used_colors + 0 + i * 16 + 1, PALETTE_COLOR_UNUSED, 15);
	}

	if (palette_recalc ())
	{
		memset(bg_dirtybuffer,1,1024);
		memset(tx_dirtybuffer,1,1024);
	}
}


WRITE_HANDLER( system1_videoram_w )
{
	system1_videoram[offset] = data;
	tx_dirtybuffer[offset>>1] = 1;
}

WRITE_HANDLER( system1_backgroundram_w )
{
	system1_backgroundram[offset] = data;
	bg_dirtybuffer[offset>>1] = 1;
}


static int system1_draw_fg(struct osd_bitmap *bitmap,int priority)
{
	int sx,sy,offs;
	int drawn = 0;


	priority <<= 3;

	for (offs = 0;offs < system1_videoram_size;offs += 2)
	{
		if ((system1_videoram[offs+1] & 0x08) == priority)
		{
			int code,color;


			code = (system1_videoram[offs] | (system1_videoram[offs+1] << 8));
			code = ((code >> 4) & 0x800) | (code & 0x7ff);	/* Heavy Metal only */
			color = ((code >> 5) & 0x3f);
			sx = (offs/2) % 32;
			sy = (offs/2) / 32;

			if (Machine->gfx[0]->pen_usage[code] & ~1)
			{
				drawn = 1;

				drawgfx(bitmap,Machine->gfx[0],
						code,
						color,
						0,0,
						8*sx,8*sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
		}
	}

	return drawn;
}

static void system1_draw_bg(struct osd_bitmap *bitmap,int priority)
{
	int sx,sy,offs;


	background_scrollx = ((system1_scroll_x[0] >> 1) + ((system1_scroll_x[1] & 1) << 7) + 14) & 0xff;
	background_scrolly = (-*system1_scroll_y) & 0xff;

	if (priority == -1)
	{
		/* optimized far background */

		/* for every character in the background video RAM, check if it has
		 * been modified since last time and update it accordingly.
		 */

		for (offs = 0;offs < system1_backgroundram_size;offs += 2)
		{
			if (bg_dirtybuffer[offs / 2])
			{
				int code,color;


				bg_dirtybuffer[offs / 2] = 0;

				code = (system1_backgroundram[offs] | (system1_backgroundram[offs+1] << 8));
				code = ((code >> 4) & 0x800) | (code & 0x7ff);	/* Heavy Metal only */
				color = ((code >> 5) & 0x3f) + 0x40;
				sx = (offs/2) % 32;
				sy = (offs/2) / 32;

				drawgfx(bitmap1,Machine->gfx[0],
						code,
						color,
						0,0,
						8*sx,8*sy,
						0,TRANSPARENCY_NONE,0);
			}
		}

		/* copy the temporary bitmap to the screen */
		copyscrollbitmap(bitmap,bitmap1,1,&background_scrollx,1,&background_scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
	else
	{
		priority <<= 3;

		for (offs = 0;offs < system1_backgroundram_size;offs += 2)
		{
			if ((system1_backgroundram[offs+1] & 0x08) == priority)
			{
				int code,color;


				code = (system1_backgroundram[offs] | (system1_backgroundram[offs+1] << 8));
				code = ((code >> 4) & 0x800) | (code & 0x7ff);	/* Heavy Metal only */
				color = ((code >> 5) & 0x3f) + 0x40;
				sx = (offs/2) % 32;
				sy = (offs/2) / 32;

				sx = 8*sx + background_scrollx;
				sy = 8*sy + background_scrolly;

				drawgfx(bitmap,Machine->gfx[0],
						code,
						color,
						0,0,
						sx,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
				if (sx > 248)
					drawgfx(bitmap,Machine->gfx[0],
							code,
							color,
							0,0,
							sx-256,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
				if (sy > 248)
				{
					drawgfx(bitmap,Machine->gfx[0],
							code,
							color,
							0,0,
							sx,sy-256,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
					if (sx > 248)
						drawgfx(bitmap,Machine->gfx[0],
								code,
								color,
								0,0,
								sx-256,sy-256,
								&Machine->visible_area,TRANSPARENCY_PEN,0);
				}
			}
		}
	}
}

void system1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int drawn;


	system1_compute_palette();

	system1_draw_bg(bitmap,-1);
	drawn = system1_draw_fg(bitmap,0);
	/* redraw low priority bg tiles if necessary */
	if (drawn) system1_draw_bg(bitmap,0);
	DrawSprites(bitmap);
	system1_draw_bg(bitmap,1);
	system1_draw_fg(bitmap,1);

	/* even if screen is off, sprites must still be drawn to update the collision table */
	if (system1_video_mode & 0x10)  /* screen off */
		fillbitmap(bitmap,palette_transparent_color,&Machine->visible_area);
}









WRITE_HANDLER( choplifter_scroll_x_w )
{
	system1_scrollx_ram[offset] = data;

	scrollx_row[offset/2] = (system1_scrollx_ram[offset & ~1] >> 1) + ((system1_scrollx_ram[offset | 1] & 1) << 7);
}

void chplft_draw_bg(struct osd_bitmap *bitmap, int priority)
{
	int sx,sy,offs;
	int choplifter_scroll_x_on = (system1_scrollx_ram[0] == 0xE5 && system1_scrollx_ram[1] == 0xFF) ? 0:1;


	if (priority == -1)
	{
		/* optimized far background */

		/* for every character in the background video RAM, check if it has
		 * been modified since last time and update it accordingly.
		 */

		for (offs = 0;offs < system1_backgroundram_size;offs += 2)
		{
			if (bg_dirtybuffer[offs / 2])
			{
				int code,color;


				bg_dirtybuffer[offs / 2] = 0;

				code = (system1_backgroundram[offs] | (system1_backgroundram[offs+1] << 8));
				code = ((code >> 4) & 0x800) | (code & 0x7ff);	/* Heavy Metal only */
				color = ((code >> 5) & 0x3f) + 0x40;
				sx = (offs/2) % 32;
				sy = (offs/2) / 32;

				drawgfx(bitmap1,Machine->gfx[0],
						code,
						color,
						0,0,
						8*sx,8*sy,
						0,TRANSPARENCY_NONE,0);
			}
		}

		/* copy the temporary bitmap to the screen */
		if (choplifter_scroll_x_on)
			copyscrollbitmap(bitmap,bitmap1,32,scrollx_row,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
		else
			copybitmap(bitmap,bitmap1,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}

	else
	{
		priority <<= 3;

		for (offs = 0;offs < system1_backgroundram_size;offs += 2)
		{
			if ((system1_backgroundram[offs+1] & 0x08) == priority)
			{
				int code,color;


				code = (system1_backgroundram[offs] | (system1_backgroundram[offs+1] << 8));
				code = ((code >> 4) & 0x800) | (code & 0x7ff);	/* Heavy Metal only */
				color = ((code >> 5) & 0x3f) + 0x40;
				sx = 8*((offs/2) % 32);
				sy = (offs/2) / 32;

				if (choplifter_scroll_x_on)
					sx = (sx + scrollx_row[sy]) & 0xff;
				sy = 8*sy;

				drawgfx(bitmap,Machine->gfx[0],
						code,
						color,
						0,0,
						sx,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
		}
	}
}

void choplifter_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int drawn;


	system1_compute_palette();

	chplft_draw_bg(bitmap,-1);
	drawn = system1_draw_fg(bitmap,0);
	/* redraw low priority bg tiles if necessary */
	if (drawn) chplft_draw_bg(bitmap,0);
	DrawSprites(bitmap);
	chplft_draw_bg(bitmap,1);
	system1_draw_fg(bitmap,1);

	/* even if screen is off, sprites must still be drawn to update the collision table */
	if (system1_video_mode & 0x10)  /* screen off */
		fillbitmap(bitmap,palette_transparent_color,&Machine->visible_area);
}



READ_HANDLER( wbml_bg_bankselect_r )
{
	return bg_bank_latch;
}

WRITE_HANDLER( wbml_bg_bankselect_w )
{
	bg_bank_latch = data;
	bg_bank = (data >> 1) & 0x03;	/* Select 4 banks of 4k, bit 2,1 */
}

READ_HANDLER( wbml_paged_videoram_r )
{
	return bg_ram[0x1000*bg_bank + offset];
}

WRITE_HANDLER( wbml_paged_videoram_w )
{
	bg_ram[0x1000*bg_bank + offset] = data;
}

void wbml_backgroundrefresh(struct osd_bitmap *bitmap, int trasp)
{
	int page;


	int xscroll = (bg_ram[0x7c0] >> 1) + ((bg_ram[0x7c1] & 1) << 7) - 256 + 5;
	int yscroll = -bg_ram[0x7ba];

	for (page=0; page < 4; page++)
	{
		const unsigned char *source = bg_ram + (bg_ram[0x0740 + page*2] & 0x07)*0x800;
		int startx = (page&1)*256+xscroll;
		int starty = (page>>1)*256+yscroll;
		int row,col;


		for( row=0; row<32*8; row+=8 )
		{
			for( col=0; col<32*8; col+=8 )
			{
				int code,priority;
				int x = (startx+col) & 0x1ff;
				int y = (starty+row) & 0x1ff;
				if (x > 256) x -= 512;
				if (y > 224) y -= 512;


				if (x > -8 && y > -8)
				{
					code = source[0] + (source[1] << 8);
					priority = code & 0x800;
					code = ((code >> 4) & 0x800) | (code & 0x7ff);

					if (!trasp)
						drawgfx(bitmap,Machine->gfx[0],
								code,
								((code >> 5) & 0x3f) + 64,
								0,0,
								x,y,
								&Machine->visible_area, TRANSPARENCY_NONE, 0);
					else if (priority)
						drawgfx(bitmap,Machine->gfx[0],
								code,
								((code >> 5) & 0x3f) + 64,
								0,0,
								x,y,
								&Machine->visible_area, TRANSPARENCY_PEN, 0);
				}

				source+=2;
			}
		}
	} /* next page */
}

void wbml_textrefresh(struct osd_bitmap *bitmap)
{
	int offs;


	for (offs = 0;offs < 0x700;offs += 2)
	{
		int sx,sy,code;


		sx = (offs/2) % 32;
		sy = (offs/2) / 32;
		code = bg_ram[offs] | (bg_ram[offs+1] << 8);
		code = ((code >> 4) & 0x800) | (code & 0x7ff);

		drawgfx(bitmap,Machine->gfx[0],
				code,
				(code >> 5) & 0x3f,
				0,0,
				8*sx,8*sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}


void wbml_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	palette_recalc();
	/* no need to check the return code since we redraw everything each frame */

	wbml_backgroundrefresh(bitmap,0);
	DrawSprites(bitmap);
	wbml_backgroundrefresh(bitmap,1);
	wbml_textrefresh(bitmap);

	/* even if screen is off, sprites must still be drawn to update the collision table */
	if (system1_video_mode & 0x10)  /* screen off */
		fillbitmap(bitmap,palette_transparent_color,&Machine->visible_area);
}
