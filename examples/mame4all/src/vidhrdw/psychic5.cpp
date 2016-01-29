/***************************************************************************
  Psychic 5

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


#define	BG_SCROLLX_LSB		0x308
#define	BG_SCROLLX_MSB		0x309
#define	BG_SCROLLY_LSB		0x30a
#define	BG_SCROLLY_MSB		0x30b
#define	BG_SCREEN_MODE		0x30c
#define	BG_PAL_INTENSITY_RG	0x1fe
#define	BG_PAL_INTENSITY_BU	0x1ff

static int 		 ps5_vram_page = 0x0;
static int		 bg_clip_mode;

static struct osd_bitmap *bitmap_bg;
static unsigned char 	 *bg_dirtybuffer;

/* Paged RAM 0 */
static unsigned char  	 *ps5_background_videoram;
static unsigned char  	 *ps5_dummy_bg_ram;

/* Paged RAM 1 */
static unsigned char 	 *ps5_io_ram;
static unsigned char 	 *ps5_palette_ram;
static unsigned char 	 *ps5_foreground_videoram;


int is_psychic5_title_mode(void)
{
	if (ps5_foreground_videoram[0x7C6]=='H') return 0;
	return 1;
}

void psychic5_init_machine(void)
{
	bg_clip_mode = -10;
}

WRITE_HANDLER( psychic5_vram_page_select_w )
{
	ps5_vram_page = data;
}

READ_HANDLER( psychic5_vram_page_select_r )
{
	return ps5_vram_page;
}

void psychic5_paletteram_w(int color_offs, int offset, int data)
{
	int bit0,bit1,bit2,bit3;
	int r,g,b,val;

	ps5_palette_ram[offset] = data;

	/* red component */
	val  = ps5_palette_ram[offset & ~1] >> 4;
	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;
	r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

	/* green component */
	val = ps5_palette_ram[offset & ~1];
	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;
	g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

	/* blue component */
	val = ps5_palette_ram[offset | 1] >> 4;
	bit0 = (val >> 0) & 0x01;
	bit1 = (val >> 1) & 0x01;
	bit2 = (val >> 2) & 0x01;
	bit3 = (val >> 3) & 0x01;
	b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

	palette_change_color((offset / 2)-color_offs,r,g,b);
}

void set_background_palette_intensity(void)
{
	int i,r,g,b,val,lo,hi,ir,ig,ib,ix;
	int bit0,bit1,bit2,bit3;

	/* red,green,blue intensites */
	ir = 15 - (ps5_palette_ram[BG_PAL_INTENSITY_RG] >> 4);
	ig = 15 - (ps5_palette_ram[BG_PAL_INTENSITY_RG] & 15);
	ib = 15 - (ps5_palette_ram[BG_PAL_INTENSITY_BU] >> 4);
	/* unknow but assumes value 2 during the ride on the witches' broom */
	ix = ps5_palette_ram[0x1ff] & 15;

	for (i=0; i<256; i++)
	{
		lo = ps5_palette_ram[0x400+i*2];
		hi = ps5_palette_ram[0x400+i*2+1];

		val  =  lo >> 4;
		bit0 = (val >> 0) & 0x01;
		bit1 = (val >> 1) & 0x01;
		bit2 = (val >> 2) & 0x01;
		bit3 = (val >> 3) & 0x01;

		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* green component */
		val = lo & 15;
		bit0 = (val >> 0) & 0x01;
		bit1 = (val >> 1) & 0x01;
		bit2 = (val >> 2) & 0x01;
		bit3 = (val >> 3) & 0x01;

		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* blue component */
		val = hi >> 4;
		bit0 = (val >> 0) & 0x01;
		bit1 = (val >> 1) & 0x01;
		bit2 = (val >> 2) & 0x01;
		bit3 = (val >> 3) & 0x01;

		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* grey background enable */

		if (ps5_io_ram[BG_SCREEN_MODE] & 2)
		{
			val = (unsigned char)(0.299*r + 0.587*g + 0.114*b);

 			if (ix==2)		/* purple background enable */
			 palette_change_color(256+i,val*0.6,0,val*0.8);
			else			/* grey bg */
			 palette_change_color(256+i,val,val,val);
		}
		else
		{

			/* background intensity enable  TO DO BETTER !!! */

			if (!is_psychic5_title_mode())
			{
				r = (r>>4) * ir;
	 		  	g = (g>>4) * ig;
 			  	b = (b>>4) * ib;
			}
			palette_change_color(256+i,r,g,b);
		}
	}
}


WRITE_HANDLER( psychic5_bgvideoram_w )
{
	if (ps5_background_videoram[offset] != data)
	{
		bg_dirtybuffer[offset >> 1] = 1;
		ps5_background_videoram[offset] = data;
	}
}

READ_HANDLER( psychic5_paged_ram_r )
{
	int val;

	if (!ps5_vram_page)
	{

		if (offset < 0x1000)
			return ps5_background_videoram[offset];
		else
			return ps5_dummy_bg_ram[offset & 0xfff];

	} else
	{
		if (offset < 0x400)
		{
			val = 0;
			switch(offset)
			{
				case 0x00:
					val = input_port_0_r(0);
					break;
				case 0x01:
					val = input_port_1_r(0);
					break;
				case 0x02:
					val = input_port_2_r(0);
					break;
				case 0x03:
					val = input_port_3_r(0);
					break;
				case 0x04:
					val = input_port_4_r(0);
					break;
				default:
					val = ps5_io_ram[offset];
			}
			return (val);
		}
		else if (offset < 0x1000)
		{
			return ps5_palette_ram[offset-0x400];
		}
		else
		{
			return ps5_foreground_videoram[offset & 0xfff];
		}
	}
	return 0;
}

WRITE_HANDLER( psychic5_paged_ram_w )
{
	if (!ps5_vram_page)
	{
		if (offset < 0x1000)
			psychic5_bgvideoram_w(offset,data);
		else
			ps5_dummy_bg_ram[offset & 0xfff] = data;
	} else
	{
		if (offset < 0x400)
		{
			ps5_io_ram[offset] = data;
		}
		else if (offset < 0x600)
		{
			psychic5_paletteram_w(000, offset-0x400, data);
		}
		else if (offset > 0x5ff && offset< 0x800)
		{
			ps5_palette_ram[offset-0x400] = data;
		}
		else if (offset > 0x7ff && offset < 0xa00)
		{
			psychic5_paletteram_w(256, offset-0x400, data);
		}
		else if (offset > 0x9ff && offset < 0xc00)
		{
			psychic5_paletteram_w(256, offset-0x400, data);
		}
		else if (offset < 0x1000)
		{
			ps5_palette_ram[offset-0x400] = data;
		}
		else
		{
			ps5_foreground_videoram[offset & 0xfff] = data;
		}
	}
}


int psychic5_vh_start(void)
{
	if ((ps5_background_videoram = (unsigned char*)malloc(0x1000)) == 0)
	{
		return 1;
	}
	if ((ps5_dummy_bg_ram = (unsigned char*)malloc(0x1000)) == 0)
	{
		free(ps5_background_videoram);
		return 1;
	}
	if ((ps5_io_ram = (unsigned char*)malloc(0x400)) == 0)
	{
		free(ps5_background_videoram);
		free(ps5_dummy_bg_ram);
		return 1;
	}
	if ((ps5_palette_ram = (unsigned char*)malloc(0xc00)) == 0)
	{
		free(ps5_background_videoram);
		free(ps5_dummy_bg_ram);
		free(ps5_io_ram);
		return 1;
	}
	if ((ps5_foreground_videoram = (unsigned char*)malloc(0x1000)) == 0)
	{
		free(ps5_background_videoram);
		free(ps5_dummy_bg_ram);
		free(ps5_io_ram);
		free(ps5_palette_ram);
		return 1;
	}
        if ((bg_dirtybuffer = (unsigned char*)malloc(32*64)) == 0)
	{
		free(ps5_background_videoram);
		free(ps5_dummy_bg_ram);
		free(ps5_io_ram);
		free(ps5_palette_ram);
		free(ps5_foreground_videoram);
        	return 1;
	}
	if ((bitmap_bg = bitmap_alloc(Machine->drv->screen_width*4,Machine->drv->screen_height*2)) == 0)
	{
		free(ps5_background_videoram);
		free(ps5_dummy_bg_ram);
		free(ps5_io_ram);
		free(ps5_palette_ram);
		free(ps5_foreground_videoram);
		free(bg_dirtybuffer);
		return 1;
	}
        memset(bg_dirtybuffer,1,32*64);
	memset(ps5_background_videoram, 0,0x1000);
	memset(ps5_dummy_bg_ram,0,0x1000);
	memset(ps5_io_ram,0,0x400);
	memset(ps5_palette_ram,0,0xc00);
	memset(ps5_foreground_videoram,0,0x1000);
        return 0;
}

void psychic5_vh_stop(void)
{
	free(ps5_background_videoram);
	free(ps5_dummy_bg_ram);
	free(ps5_io_ram);
	free(ps5_palette_ram);
	free(ps5_foreground_videoram);
	free(bg_dirtybuffer);
	bitmap_free(bitmap_bg);
}

void psychic5_draw_background(struct osd_bitmap *bitmap)
{
	int x,y,offs;
	int sx,sy,tile,palette,flipx,flipy,lo,hi;

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */

	for (x = 31; x >=0; x--)
	{
		for (y = 0 ; y < 64; y++)
		{
			offs = y*64+(x*2);
			if (bg_dirtybuffer[offs >> 1])
			{
				sx = y << 4;
				sy = x << 4;

				bg_dirtybuffer[offs >> 1] = 0;

				lo = ps5_background_videoram[offs];
				hi = ps5_background_videoram[offs+1];
				tile = ((hi & 0xc0) << 2) | lo;
				flipx = hi & 0x10;
				flipy = hi & 0x20;
				palette = hi & 0x0f;
				drawgfx(bitmap,Machine->gfx[1],
						  tile,
						  palette,
						  flipx,flipy,
						  sx,sy,
						  0,TRANSPARENCY_NONE,0);
			}
		}
	}
}

void psychic5_draw_foreground(struct osd_bitmap *bitmap)
{
	int x,y,offs;
	int sx,sy,tile,palette,flipx,flipy,lo,hi;

	/* Draw the foreground text */

	for (x = 31; x >=0; x--)
	{
		for (y = 0; y < 32; y++)
		{
			offs = y*64+(x*2);
			if (ps5_foreground_videoram[offs+1]!=0xFF)
			{
				sx = y << 3;
				sy = x << 3;

				lo = ps5_foreground_videoram[offs];
				hi = ps5_foreground_videoram[offs+1];
				tile = ((hi & 0xc0) << 2) | lo;
				flipx = hi & 0x10;
				flipy = hi & 0x20;
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
}

void psychic5_draw_sprites(struct osd_bitmap *bitmap)
{
	int offs,sx,sy,tile,palette,flipx,flipy;
	int size32,tileofs0,tileofs1,tileofs2,tileofs3,temp1,temp2;

	/* Draw the sprites */

	for (offs = 11 ;offs < spriteram_size; offs+=16)
	{
		if (!(spriteram[offs+4]==0 && spriteram[offs]==0xf0))
		{
			sx = spriteram[offs+1];
			sy = spriteram[offs];
			if (spriteram[offs+2] & 0x01) sx-=256;
			if (spriteram[offs+2] & 0x04) sy-=256;
			tile	 = spriteram[offs+3]+((spriteram[offs+2] & 0xc0)<<2);
			size32	 = spriteram[offs+2] & 0x08;
			flipx	 = spriteram[offs+2] & 0x20;
			flipy	 = spriteram[offs+2] & 0x10;
			palette	 = spriteram[offs+4] & 0x0f;
			if (flipx)
			{
				tileofs0 = 1;
				tileofs1 = 0;
				tileofs2 = 3;
				tileofs3 = 2;
			} else
			{
				tileofs0 = 0;
				tileofs1 = 1;
				tileofs2 = 2;
				tileofs3 = 3;
			}
			if (flipy)
			{
				temp1	 = tileofs0;
				temp2	 = tileofs1;
				tileofs0 = tileofs2;
				tileofs1 = tileofs3;
				tileofs2 = temp1;
				tileofs3 = temp2;
			}
			if (size32)
			{
				drawgfx(bitmap,Machine->gfx[0],
							tile+tileofs0,
							palette,
							flipy,flipx,
							sx,sy,
							&Machine->visible_area,
							TRANSPARENCY_PEN, 15);
				drawgfx(bitmap,Machine->gfx[0],
							tile+tileofs1,
							palette,
							flipy,flipx,
							sx,sy+16,
							&Machine->visible_area,
							TRANSPARENCY_PEN, 15);
				drawgfx(bitmap,Machine->gfx[0],
							tile+tileofs2,
							palette,
							flipy,flipx,
							sx+16,sy,
							&Machine->visible_area,
							TRANSPARENCY_PEN, 15);
				drawgfx(bitmap,Machine->gfx[0],
							tile+tileofs3,
							palette,
							flipy,flipx,
							sx+16,sy+16,
							&Machine->visible_area,
							TRANSPARENCY_PEN, 15);
			}
			else
			{
				drawgfx(bitmap,Machine->gfx[0],
							tile,
							palette,
							flipy,flipx,
							sx,sy,
							&Machine->visible_area,
							TRANSPARENCY_PEN, 15);
			}
		}
	}
}




void psychic5_draw_sprites2(struct osd_bitmap *bitmap)
{
	int offs,sx,sy,tile,palette,flipx,flipy;
	int size32,tileofs0,tileofs1,tileofs2,tileofs3;

	for (offs = 11 ;offs < spriteram_size; offs+=16)
	{
		if (!(spriteram[offs+4]==0 && spriteram[offs]==0xf0))
		{
			sx = spriteram[offs+1];
			sy = spriteram[offs];
			if (spriteram[offs+2] & 0x01) sx-=256;
			if (spriteram[offs+2] & 0x04) sy-=256;
			tile	 = spriteram[offs+3]+((spriteram[offs+2] & 0xc0)<<2);
			size32	 = spriteram[offs+2] & 0x08;
			flipx	 = spriteram[offs+2] & 0x20;
			flipy	 = spriteram[offs+2] & 0x10;
			palette	 = spriteram[offs+4] & 0x0;
			tileofs0 = 0;
			tileofs1 = 1;
			tileofs2 = 2;
			tileofs3 = 3;
			if (size32)
			{
				drawgfx(bitmap,Machine->gfx[0],
							tile+tileofs0,
							palette,
							flipy,flipx,
							sx,sy,
							&Machine->visible_area,
							TRANSPARENCY_NONE, 0);
				drawgfx(bitmap,Machine->gfx[0],
							tile+tileofs1,
							palette,
							flipy,flipx,
							sx,sy+16,
							&Machine->visible_area,
							TRANSPARENCY_NONE, 0);
				drawgfx(bitmap,Machine->gfx[0],
							tile+tileofs2,
							palette,
							flipy,flipx,
							sx+16,sy,
							&Machine->visible_area,
							TRANSPARENCY_NONE, 0);
				drawgfx(bitmap,Machine->gfx[0],
							tile+tileofs3,
							palette,
							flipy,flipx,
							sx+16,sy+16,
							&Machine->visible_area,
							TRANSPARENCY_NONE, 0);
			}
		}
	}
}





void set_visible_colors(int bg_scrollx,int bg_scrolly)
{
	int x,y,color,offs,lo,hi,tile,size32;
	unsigned short colors_used[16];

	palette_init_used_colors();

	/* visible background palette */
	memset (colors_used, 0, sizeof (colors_used));
	for (x = 23; x >=0; x--)
	{
		for (y = 0 ; y < 24; y++)
		{
			offs = (((bg_scrollx+y-3) & 63)*64)+(((bg_scrolly+x-3) & 31)*2);
			lo = ps5_background_videoram[offs];
			hi = ps5_background_videoram[offs+1];
			tile = ((hi & 0xc0) << 2) | lo;
			color = hi & 0x0f;
			colors_used[color] |= Machine->gfx[1]->pen_usage[tile];
			if (y<4 || y>19 || x<4 || x>19)
				bg_dirtybuffer[offs >> 1] = 1;
		}
	}
	for (x = 0; x < 16; x++)
	{
		unsigned short temp = colors_used[x];
		if (temp)
			for (y = 0; y < 16; y++)
				if (temp & (1 << y)) palette_used_colors[256 + 16 * x + y] = PALETTE_COLOR_USED;
	}

	/* visible foreground palette */
	memset (colors_used, 0, sizeof (colors_used));
	for (offs = 0; offs < 0x800; offs+=2)
	{
		if (ps5_foreground_videoram[offs+1]!=0xFF)
		{
			lo = ps5_foreground_videoram[offs];
			hi = ps5_foreground_videoram[offs+1];
			tile = ((hi & 0xc0) << 2) | lo;
			color = hi & 0x0f;
			colors_used[color] |= Machine->gfx[2]->pen_usage[tile];
		}
	}
	for (x = 0; x < 16; x++)
	{
		unsigned short temp = colors_used[x];
		if (temp)
		{
			for (y = 0; y < 15; y++)
				if (temp & (1 << y)) palette_used_colors[512 + 16 * x + y] = PALETTE_COLOR_USED;
			palette_used_colors[512 + 16 * x + 15] = PALETTE_COLOR_TRANSPARENT;
		}
	}

	/* visible sprites palette */
	memset (colors_used, 0, sizeof (colors_used));
	for (offs = 11; offs < spriteram_size; offs += 16)
	{
		if (!(spriteram[offs+4]==0 && spriteram[offs]==0xf0))
		{
			tile	 = spriteram[offs+3]+((spriteram[offs+2] & 0xc0)<<2);
			color	 = spriteram[offs+4] & 0x0f;
			size32	 = spriteram[offs+2] & 0x08;
			if (size32)
				colors_used[color] |=
					Machine->gfx[0]->pen_usage[tile+0] |
					Machine->gfx[0]->pen_usage[tile+1] |
					Machine->gfx[0]->pen_usage[tile+2] |
					Machine->gfx[0]->pen_usage[tile+3];
			else
				colors_used[color] |= Machine->gfx[0]->pen_usage[tile];
		}
	}
	for (x = 0; x < 16; x++)
	{
		unsigned short temp = colors_used[x];
		if (temp)
		{
			for (y = 0; y < 15; y++)
				if (temp & (1 << y)) palette_used_colors[0 + 16 * x + y] = PALETTE_COLOR_USED;
			palette_used_colors[0 + 16 * x + 15] = PALETTE_COLOR_TRANSPARENT;
		}
	}
}

void psychic5_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int bg_scrollx,bg_scrolly;

	bg_scrollx = ((ps5_io_ram[BG_SCROLLX_LSB]+((ps5_io_ram[BG_SCROLLX_MSB] & 3)*256)) & 0x3FF) >> 4;
	bg_scrolly = ((ps5_io_ram[BG_SCROLLY_LSB]+((ps5_io_ram[BG_SCROLLY_MSB] & 1)*256)) & 0x1FF) >> 4;

	set_visible_colors(bg_scrollx,bg_scrolly);
	set_background_palette_intensity();

	if (palette_recalc())
	{
		/* set bg dirty buffer only on visible area + border */
		int offs,x,y;
		for (x = 23; x >=0; x--)
		{
			for (y = 0 ; y < 24; y++)
			{
				offs = (((bg_scrollx+y-3) & 63)*64)+(((bg_scrolly+x-3) & 31)*2);
				bg_dirtybuffer[offs >> 1] = 1;
			}
		}
	}

	bg_scrollx = -((ps5_io_ram[BG_SCROLLX_LSB]+((ps5_io_ram[BG_SCROLLX_MSB] & 3)*256)) & 0x3FF);
	bg_scrolly = -((ps5_io_ram[BG_SCROLLY_LSB]+((ps5_io_ram[BG_SCROLLY_MSB] & 1)*256)) & 0x1FF);

	if (ps5_io_ram[BG_SCREEN_MODE] & 1)  /* background enable */
	{
		psychic5_draw_background(bitmap_bg);

		if (is_psychic5_title_mode())
		{
			struct rectangle clip;
			int sx1,sy1,sy2,tile;

			sx1  = spriteram[12];		/* sprite 0 */
			sy1  = spriteram[11];
			tile = spriteram[14];
			sy2 = spriteram[11+128];	/* sprite 8 */

			clip.min_x = Machine->visible_area.min_x;
			clip.min_y = Machine->visible_area.min_y;
			clip.max_x = Machine->visible_area.max_x;
			clip.max_y = Machine->visible_area.max_y;

			if (bg_clip_mode >=0 && bg_clip_mode < 3 && sy1==240) bg_clip_mode = 0;
			if (bg_clip_mode > 2 && bg_clip_mode < 5 && sy2==240) bg_clip_mode = -10;
			if (bg_clip_mode > 4 && bg_clip_mode < 7 && sx1==240) bg_clip_mode = 0;
			if (bg_clip_mode > 6 && bg_clip_mode < 9 && (sx1==240 || sx1==0)) bg_clip_mode = -10;

			if (sy1!=240 && sy1!=0 && bg_clip_mode<=0)
			{
				if (sy1>128)
				 bg_clip_mode=1;
				else
				 bg_clip_mode=2;
			}
			if (sy2!=240 && sy2!=0 && bg_clip_mode<=0)
			{
				if (sy2>128)
				 bg_clip_mode=3;
				else
				 bg_clip_mode=4;
			}
			if (sx1!=240 && sx1!=0 && bg_clip_mode<=0 && tile==0x3c)
			{
				if (sx1>128)
				 bg_clip_mode=5;
				else
				 bg_clip_mode=6;
			}
			if (sx1!=240 && sx1!=0 && bg_clip_mode<=0 && tile==0x1c)
			{
				if (sx1>128)
				 bg_clip_mode=7;
				else
				 bg_clip_mode=8;
			}
			if (bg_clip_mode)
			{
				if (bg_clip_mode==1)
				 clip.min_y = sy1;
				else if (bg_clip_mode==2)
 				 clip.max_y = sy1;
				else if (bg_clip_mode==3)
				 clip.max_y = sy2;
				else if (bg_clip_mode==4)
				 clip.min_y = sy2;
				else if (bg_clip_mode==5)
				 clip.min_x = sx1;
				else if (bg_clip_mode==6)
 				 clip.max_x = sx1;
				else if (bg_clip_mode==7)
				 clip.max_x = sx1;
				else if (bg_clip_mode==8)
				 clip.min_x = sx1;
				else if (bg_clip_mode==-10)
				{
					clip.min_x = 0;
					clip.min_y = 0;
					clip.max_x = 0;
					clip.max_y = 0;
				}
				fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);
				copyscrollbitmap(bitmap,bitmap_bg,1,&bg_scrollx,1,&bg_scrolly,&clip,TRANSPARENCY_NONE,0);
			} else
				copyscrollbitmap(bitmap,bitmap_bg,1,&bg_scrollx,1,&bg_scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);

		} else
			copyscrollbitmap(bitmap,bitmap_bg,1,&bg_scrollx,1,&bg_scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);
	} else
	{
		fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);
		memset(bg_dirtybuffer,1,64*32);
	}

	if (!is_psychic5_title_mode())
	{
		psychic5_draw_sprites(bitmap);
		bg_clip_mode=-10;
	}
	psychic5_draw_foreground(bitmap);
}
