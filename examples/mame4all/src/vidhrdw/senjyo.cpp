/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"

/* in machine/segacrpt.c */
void suprloco_decode(void);


extern unsigned char *spriteram;
extern size_t spriteram_size;

unsigned char *senjyo_fgscroll;
unsigned char *senjyo_bgstripes;
unsigned char *senjyo_scrollx1,*senjyo_scrolly1;
unsigned char *senjyo_scrollx2,*senjyo_scrolly2;
unsigned char *senjyo_scrollx3,*senjyo_scrolly3;
unsigned char *senjyo_fgvideoram,*senjyo_fgcolorram;
unsigned char *senjyo_bg1videoram,*senjyo_bg2videoram,*senjyo_bg3videoram;
unsigned char *senjyo_radarram;

static struct tilemap *fg_tilemap,*bg1_tilemap,*bg2_tilemap,*bg3_tilemap;

static int senjyo, scrollhack;

static struct osd_bitmap *bgbitmap;
static int bgbitmap_dirty;
static int flipscreen;


void init_starforc(void)
{
	senjyo = 0;
	scrollhack = 1;
}
void init_starfore(void)
{
	/* encrypted CPU */
	suprloco_decode();

	senjyo = 0;
	scrollhack = 0;
}
void init_senjyo(void)
{
	senjyo = 1;
	scrollhack = 0;
}


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_fg_tile_info(int tile_index)
{
	unsigned char attr = senjyo_fgcolorram[tile_index];
	SET_TILE_INFO(0,senjyo_fgvideoram[tile_index] + ((attr & 0x10) << 4),attr & 0x07)
	tile_info.flags = (attr & 0x80) ? TILE_FLIPY : 0;
	if (senjyo && (tile_index & 0x1f) >= 32-8)
		tile_info.flags |= TILE_IGNORE_TRANSPARENCY;
}

static void senjyo_bg1_tile_info(int tile_index)
{
	unsigned char code = senjyo_bg1videoram[tile_index];
	SET_TILE_INFO(1,code,(code & 0x70) >> 4)
}

static void starforc_bg1_tile_info(int tile_index)
{
	/* Star Force has more tiles in bg1, so to get a uniform color code spread */
	/* they wired bit 7 of the tile code in place of bit 4 to get the color code */
	static int colormap[8] = { 0,2,4,6,1,3,5,7 };
	unsigned char code = senjyo_bg1videoram[tile_index];
	SET_TILE_INFO(1,code,colormap[(code & 0xe0) >> 5])
}

static void get_bg2_tile_info(int tile_index)
{
	unsigned char code = senjyo_bg2videoram[tile_index];
	SET_TILE_INFO(2,code,(code & 0xe0) >> 5)
}

static void get_bg3_tile_info(int tile_index)
{
	unsigned char code = senjyo_bg3videoram[tile_index];
	SET_TILE_INFO(3,code,(code & 0xe0) >> 5)
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

void senjyo_vh_stop(void)
{
	bitmap_free(bgbitmap);
	bgbitmap = 0;
}

int senjyo_vh_start(void)
{
	bgbitmap = bitmap_alloc(256,256);
	if (!bgbitmap)
		return 1;

	fg_tilemap = tilemap_create(get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,32,32);
	if (senjyo)
	{
		bg1_tilemap = tilemap_create(senjyo_bg1_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,16,32);
		bg2_tilemap = tilemap_create(get_bg2_tile_info,   tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,16,48);	/* only 16x32 used by Star Force */
		bg3_tilemap = tilemap_create(get_bg3_tile_info,   tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,16,56);	/* only 16x32 used by Star Force */
	}
	else
	{
		bg1_tilemap = tilemap_create(starforc_bg1_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,16,32);
		bg2_tilemap = tilemap_create(get_bg2_tile_info,     tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,16,32);	/* only 16x32 used by Star Force */
		bg3_tilemap = tilemap_create(get_bg3_tile_info,     tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,16,32);	/* only 16x32 used by Star Force */
	}


	if (!fg_tilemap || !bg1_tilemap || !bg2_tilemap || !bg3_tilemap)
	{
		senjyo_vh_stop();

		return 1;
	}

	fg_tilemap->transparent_pen = 0;
	bg1_tilemap->transparent_pen = 0;
	bg2_tilemap->transparent_pen = 0;
	bg3_tilemap->transparent_pen = 0;
	tilemap_set_scroll_cols(fg_tilemap,32);

	bgbitmap_dirty = 1;

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( senjyo_fgvideoram_w )
{
	if (senjyo_fgvideoram[offset] != data)
	{
		senjyo_fgvideoram[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap,offset);
	}
}
WRITE_HANDLER( senjyo_fgcolorram_w )
{
	if (senjyo_fgcolorram[offset] != data)
	{
		senjyo_fgcolorram[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap,offset);
	}
}
WRITE_HANDLER( senjyo_bg1videoram_w )
{
	if (senjyo_bg1videoram[offset] != data)
	{
		senjyo_bg1videoram[offset] = data;
		tilemap_mark_tile_dirty(bg1_tilemap,offset);
	}
}
WRITE_HANDLER( senjyo_bg2videoram_w )
{
	if (senjyo_bg2videoram[offset] != data)
	{
		senjyo_bg2videoram[offset] = data;
		tilemap_mark_tile_dirty(bg2_tilemap,offset);
	}
}
WRITE_HANDLER( senjyo_bg3videoram_w )
{
	if (senjyo_bg3videoram[offset] != data)
	{
		senjyo_bg3videoram[offset] = data;
		tilemap_mark_tile_dirty(bg3_tilemap,offset);
	}
}

WRITE_HANDLER( senjyo_bgstripes_w )
{
	if (*senjyo_bgstripes != data)
	{
		*senjyo_bgstripes = data;
		bgbitmap_dirty = 1;
	}
}

WRITE_HANDLER( senjyo_flipscreen_w )
{
	flipscreen = data & 0x01;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPX | TILEMAP_FLIPY) : 0);
}


/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_bgbitmap(struct osd_bitmap *bitmap)
{
	int x,y,pen,strwid,count;


	if (*senjyo_bgstripes == 0xff)	/* off */
	{
		fillbitmap(bitmap,Machine->pens[0],0);
		return;
	}

	if (bgbitmap_dirty)
	{
		bgbitmap_dirty = 0;

		pen = 0;
		count = 0;
		strwid = *senjyo_bgstripes;
		if (strwid == 0) strwid = 0x100;
		if (flipscreen) strwid ^= 0xff;

		for (x = 0;x < 256;x++)
		{
			if (flipscreen)
			{
				for (y = 0;y < 256;y++)
				{
					plot_pixel(bgbitmap, 255 - x, y, Machine->pens[384 + pen]);
				}
			}
			else
			{
				for (y = 0;y < 256;y++)
				{
					plot_pixel(bgbitmap, x, y, Machine->pens[384 + pen]);
				}
			}

			count += 0x10;
			if (count >= strwid)
			{
				pen = (pen + 1) & 0x0f;
				count -= strwid;
			}
		}
	}

	copybitmap(bitmap,bgbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
}

static void draw_radar(struct osd_bitmap *bitmap)
{
	int offs,x;

	for (offs = 0;offs < 0x400;offs++)
	{
		if (senjyo_radarram[offs])
		{
			for (x = 0;x < 8;x++)
			{
				if (senjyo_radarram[offs] & (1 << x))
				{
					int sx, sy;

					sx = (8 * (offs % 8) + x) + 256-64;
					sy = ((offs & 0x1ff) / 8) + 96;

					if (flipscreen)
					{
						sx = 255 - sx;
						sy = 255 - sy;
					}

					plot_pixel(bitmap,
							   sx, sy,
							   Machine->pens[offs < 0x200 ? 400 : 401]);
				}
			}
		}
	}
}

static void draw_sprites(struct osd_bitmap *bitmap,int priority)
{
	const struct rectangle *clip = &Machine->visible_area;
	int offs;


	for (offs = spriteram_size - 4;offs >= 0;offs -= 4)
	{
		int big,sx,sy,flipx,flipy;

		if (((spriteram[offs+1] & 0x30) >> 4) == priority)
		{
			if (senjyo)	/* Senjyo */
				big = (spriteram[offs] & 0x80);
			else	/* Star Force */
				big = ((spriteram[offs] & 0xc0) == 0xc0);
			sx = spriteram[offs+3];
			if (big)
				sy = 224-spriteram[offs+2];
			else
				sy = 240-spriteram[offs+2];
			flipx = spriteram[offs+1] & 0x40;
			flipy = spriteram[offs+1] & 0x80;

			if (flipscreen)
			{
				flipx = !flipx;
				flipy = !flipy;

				if (big)
				{
					sx = 224 - sx;
					sy = 226 - sy;
				}
				else
				{
					sx = 240 - sx;
					sy = 242 - sy;
				}
			}


			drawgfx(bitmap,Machine->gfx[big ? 5 : 4],
					spriteram[offs],
					spriteram[offs + 1] & 0x07,
					flipx,flipy,
					sx,sy,
					clip,TRANSPARENCY_PEN,0);
		}
	}
}

void senjyo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;


	/* two colors for the radar dots (verified on the real board) */
	palette_change_color(400,0xff,0x00,0x00);	/* red for enemies */
	palette_change_color(401,0xff,0xff,0x00);	/* yellow for player */

	{
		int scrollx,scrolly;

		for (i = 0;i < 32;i++)
			tilemap_set_scrolly(fg_tilemap,i,senjyo_fgscroll[i]);

		scrollx = senjyo_scrollx1[0];
		scrolly = senjyo_scrolly1[0] + 256 * senjyo_scrolly1[1];
		if (flipscreen)
			scrollx = -scrollx;
		tilemap_set_scrollx(bg1_tilemap,0,scrollx);
		tilemap_set_scrolly(bg1_tilemap,0,scrolly);

		scrollx = senjyo_scrollx2[0];
		scrolly = senjyo_scrolly2[0] + 256 * senjyo_scrolly2[1];
		if (scrollhack)	/* Star Force, but NOT the encrypted version */
		{
			scrollx = senjyo_scrollx1[0];
			scrolly = senjyo_scrolly1[0] + 256 * senjyo_scrolly1[1];
		}
		if (flipscreen)
			scrollx = -scrollx;
		tilemap_set_scrollx(bg2_tilemap,0,scrollx);
		tilemap_set_scrolly(bg2_tilemap,0,scrolly);

		scrollx = senjyo_scrollx3[0];
		scrolly = senjyo_scrolly3[0] + 256 * senjyo_scrolly3[1];
		if (flipscreen)
			scrollx = -scrollx;
		tilemap_set_scrollx(bg3_tilemap,0,scrollx);
		tilemap_set_scrolly(bg3_tilemap,0,scrolly);
	}

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();
//	mark_sprite_colors();
	for (i = 320; i < 384; i++)
	{
		if (i % 8 != 0)
			palette_used_colors[i] = PALETTE_COLOR_USED;
	}
	for (i = 384; i < 400; i++)
	{
		palette_used_colors[i] = PALETTE_COLOR_USED;
	}
	palette_used_colors[400] = PALETTE_COLOR_USED;
	palette_used_colors[401] = PALETTE_COLOR_USED;

	if (palette_recalc())
	{
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
		bgbitmap_dirty = 1;
	}

	tilemap_render(ALL_TILEMAPS);

	draw_bgbitmap(bitmap);
	draw_sprites(bitmap,0);
	tilemap_draw(bitmap,bg3_tilemap,0);
	draw_sprites(bitmap,1);
	tilemap_draw(bitmap,bg2_tilemap,0);
	draw_sprites(bitmap,2);
	tilemap_draw(bitmap,bg1_tilemap,0);
	draw_sprites(bitmap,3);
	tilemap_draw(bitmap,fg_tilemap,0);
	draw_radar(bitmap);

#if 0
{
	char baf[80];

	sprintf(baf,"%02x %02x %02x %02x %02x %02x %02x %02x",
		senjyo_scrolly3[0x00],
		senjyo_scrolly3[0x01],
		senjyo_scrolly3[0x02],
		senjyo_scrolly3[0x03],
		senjyo_scrolly3[0x04],
		senjyo_scrolly3[0x05],
		senjyo_scrolly3[0x06],
		senjyo_scrolly3[0x07]);
	ui_text(baf,0,0);
	sprintf(baf,"%02x %02x %02x %02x %02x %02x %02x %02x",
		senjyo_scrolly3[0x08],
		senjyo_scrolly3[0x09],
		senjyo_scrolly3[0x0a],
		senjyo_scrolly3[0x0b],
		senjyo_scrolly3[0x0c],
		senjyo_scrolly3[0x0d],
		senjyo_scrolly3[0x0e],
		senjyo_scrolly3[0x0f]);
	ui_text(baf,0,10);
	sprintf(baf,"%02x %02x %02x %02x %02x %02x %02x %02x",
		senjyo_scrolly3[0x10],
		senjyo_scrolly3[0x11],
		senjyo_scrolly3[0x12],
		senjyo_scrolly3[0x13],
		senjyo_scrolly3[0x14],
		senjyo_scrolly3[0x15],
		senjyo_scrolly3[0x16],
		senjyo_scrolly3[0x17]);
	ui_text(baf,0,20);
	sprintf(baf,"%02x %02x %02x %02x %02x %02x %02x %02x",
		senjyo_scrolly3[0x18],
		senjyo_scrolly3[0x19],
		senjyo_scrolly3[0x1a],
		senjyo_scrolly3[0x1b],
		senjyo_scrolly3[0x1c],
		senjyo_scrolly3[0x1d],
		senjyo_scrolly3[0x1e],
		senjyo_scrolly3[0x1f]);
	ui_text(baf,0,30);
}
#endif
}
