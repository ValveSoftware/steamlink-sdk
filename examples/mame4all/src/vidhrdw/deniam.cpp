#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *deniam_videoram,*deniam_textram;
static int display_enable;
static int bg_scrollx_offs,bg_scrolly_offs,fg_scrollx_offs,fg_scrolly_offs;
static int bg_scrollx_reg,bg_scrolly_reg,bg_page_reg;
static int fg_scrollx_reg,fg_scrolly_reg,fg_page_reg;
static int bg_page[4],fg_page[4];

static struct tilemap *bg_tilemap,*fg_tilemap,*tx_tilemap;



void init_logicpro(void)
{
	bg_scrollx_reg = 0x00a4;
	bg_scrolly_reg = 0x00a8;
	bg_page_reg    = 0x00ac;
	fg_scrollx_reg = 0x00a2;
	fg_scrolly_reg = 0x00a6;
	fg_page_reg    = 0x00aa;

	bg_scrollx_offs = 0x00d;
	bg_scrolly_offs = 0x000;
	fg_scrollx_offs = 0x009;
	fg_scrolly_offs = 0x000;
}
void init_karianx(void)
{
	bg_scrollx_reg = 0x00a4;
	bg_scrolly_reg = 0x00a8;
	bg_page_reg    = 0x00ac;
	fg_scrollx_reg = 0x00a2;
	fg_scrolly_reg = 0x00a6;
	fg_page_reg    = 0x00aa;

	bg_scrollx_offs = 0x10d;
	bg_scrolly_offs = 0x080;
	fg_scrollx_offs = 0x109;
	fg_scrolly_offs = 0x080;
}



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static UINT32 scan_pages(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return (col & 0x3f) + ((row & 0x1f) << 6) + ((col & 0x40) << 5) + ((row & 0x20) << 7);
}

static void get_bg_tile_info(int tile_index)
{
	int page = tile_index >> 11;
	UINT16 attr = READ_WORD(&deniam_videoram[bg_page[page] * 0x1000 + 2 * (tile_index & 0x7ff)]);
	SET_TILE_INFO(0,attr,(attr & 0x1fc0) >> 6)
}

static void get_fg_tile_info(int tile_index)
{
	int page = tile_index >> 11;
	UINT16 attr = READ_WORD(&deniam_videoram[fg_page[page] * 0x1000 + 2 * (tile_index & 0x7ff)]);
	SET_TILE_INFO(0,attr,(attr & 0x1fc0) >> 6)
}

static void get_tx_tile_info(int tile_index)
{
	UINT16 attr = READ_WORD(&deniam_textram[2*tile_index]);
	SET_TILE_INFO(0,attr & 0xf1ff,(attr & 0x0e00) >> 9)
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int deniam_vh_start(void)
{
	bg_tilemap = tilemap_create(get_bg_tile_info,scan_pages,       TILEMAP_OPAQUE,     8,8,128,64);
	fg_tilemap = tilemap_create(get_fg_tile_info,scan_pages,       TILEMAP_TRANSPARENT,8,8,128,64);
	tx_tilemap = tilemap_create(get_tx_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8, 64,32);

	if (!bg_tilemap || !fg_tilemap || !tx_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 0;
	tx_tilemap->transparent_pen = 0;

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

READ_HANDLER( deniam_videoram_r )
{
   return READ_WORD(&deniam_videoram[offset]);
}

WRITE_HANDLER( deniam_videoram_w )
{
	int oldword = READ_WORD(&deniam_videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		int page,i;

		WRITE_WORD(&deniam_videoram[offset],newword);

		page = offset >> 12;
		for (i = 0;i < 4;i++)
		{
			if (bg_page[i] == page)
				tilemap_mark_tile_dirty(bg_tilemap,i * 0x800 + (offset & 0xfff)/2);
			if (fg_page[i] == page)
				tilemap_mark_tile_dirty(fg_tilemap,i * 0x800 + (offset & 0xfff)/2);
		}
	}
}


READ_HANDLER( deniam_textram_r )
{
   return READ_WORD(&deniam_textram[offset]);
}

WRITE_HANDLER( deniam_textram_w )
{
	int oldword = READ_WORD(&deniam_textram[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&deniam_textram[offset],newword);
		tilemap_mark_tile_dirty(tx_tilemap,offset/2);
	}
}


WRITE_HANDLER( deniam_palette_w )
{
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	int r,g,b;


	WRITE_WORD(&paletteram[offset],newword);

	r = ((newword << 1) & 0x1e) | ((newword >> 12) & 0x01);
	g = ((newword >> 3) & 0x1e) | ((newword >> 13) & 0x01);
	b = ((newword >> 7) & 0x1e) | ((newword >> 14) & 0x01);

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	palette_change_color(offset/2,r,g,b);
}


static UINT16 coinctrl;

READ_HANDLER( deniam_coinctrl_r )
{
	return coinctrl;
}

WRITE_HANDLER( deniam_coinctrl_w )
{
	coinctrl = COMBINE_WORD(coinctrl,data);

	/* bit 0 is coin counter */
	coin_counter_w(0,coinctrl & 0x01);

	/* bit 6 is display enable (0 freezes screen) */
	display_enable = coinctrl & 0x20;

	/* other bits unknown (unused?) */
}



/***************************************************************************

  Display refresh

***************************************************************************/

/*
 * Sprite Format
 * ------------------
 *
 * Word | Bit(s)           | Use
 * -----+-fedcba9876543210-+----------------
 *   0  | --------xxxxxxxx | display y start
 *   0  | xxxxxxxx-------- | display y end
 *   2  | -------xxxxxxxxx | x position
 *   2  | ------x--------- | unknown (used in logicpr2, maybe just a bug?)
 *   2  | xxxxxx---------- | unused?
 *   4  | ---------xxxxxxx | width
 *   4  | --------x------- | is this flip y like in System 16?
 *   4  | -------x-------- | flip x
 *   4  | xxxxxxx--------- | unused?
 *   6  | xxxxxxxxxxxxxxxx | ROM address low bits
 *   8  | ----------xxxxxx | color
 *   8  | --------xx------ | priority
 *   8  | ---xxxxx-------- | ROM address high bits
 *   8  | xxx------------- | unused? (extra address bits for larger ROMs?)
 *   a  | ---------------- | zoomx like in System 16?
 *   c  | ---------------- | zoomy like in System 16?
 *   e  | ---------------- |
 */
static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;

	for (offs = spriteram_size-16;offs >= 0;offs -= 16)
	{
		int sx,starty,endy,x,y,start,color,width,flipx,primask;
		UINT8 *rom = memory_region(REGION_GFX2);


		sx = (READ_WORD(&spriteram[offs+2]) & 0x01ff) + 16*8 - 1;
		starty = READ_WORD(&spriteram[offs+0]) & 0xff;
		endy = READ_WORD(&spriteram[offs+0]) >> 8;

		width = READ_WORD(&spriteram[offs+4]) & 0x007f;
		flipx = READ_WORD(&spriteram[offs+4]) & 0x0100;

		color = 0x40 + (READ_WORD(&spriteram[offs+8]) & 0x3f);

		primask = 8;
		switch (READ_WORD(&spriteram[offs+8]) & 0xc0)
		{
			case 0x00: primask |= 4|2|1; break;	/* below everything */
			case 0x40: primask |= 4|2;   break;	/* below fg and tx */
			case 0x80: primask |= 4;     break;	/* below tx */
			case 0xc0:                   break;	/* above everything */
		}


if (READ_WORD(&spriteram[offs+4]) & 0x0080) color = rand() & 0x7f;
if (READ_WORD(&spriteram[offs+10])) color = rand() & 0x7f;

		start = READ_WORD(&spriteram[offs+6]) +
				((READ_WORD(&spriteram[offs+8]) & 0x1f00) << 8);
		rom += 2*start;

		for (y = starty+1;y <= endy;y++)
		{
			int drawing = 0;
			int i=0;

			rom += 2*width;	/* note that the first line is skipped */
			x = 0;
			while (i < 512)	/* safety check */
			{
				if (flipx)
				{
					if ((rom[i] & 0x0f) == 0x0f)
					{
						if (!drawing) drawing = 1;
						else break;
					}
					else
					{
						if (rom[i] & 0x0f)
						{
							if (sx+x >= 0 && sx+x < 512 && y >= 0 && y < 256)
							{
								if ((priority_bitmap->line[y][sx+x] & primask) == 0)
									plot_pixel(bitmap,sx+x,y,Machine->pens[color*16+(rom[i]&0x0f)]);
								priority_bitmap->line[y][sx+x] = 8;
							}
						}
						x++;
					}

					if ((rom[i] & 0xf0) == 0xf0)
					{
						if (!drawing) drawing = 1;
						else break;
					}
					else
					{
						if (rom[i] & 0xf0)
						{
							if (sx+x >= 0 && sx+x < 512 && y >= 0 && y < 256)
							{
								if ((priority_bitmap->line[y][sx+x] & primask) == 0)
									plot_pixel(bitmap,sx+x,y,Machine->pens[color*16+(rom[i]>>4)]);
								priority_bitmap->line[y][sx+x] = 8;
							}
						}
						x++;
					}

					i--;
				}
				else
				{
					if ((rom[i] & 0xf0) == 0xf0)
					{
						if (!drawing) drawing = 1;
						else break;
					}
					else
					{
						if (rom[i] & 0xf0)
						{
							if (sx+x >= 0 && sx+x < 512 && y >= 0 && y < 256)
							{
								if ((priority_bitmap->line[y][sx+x] & primask) == 0)
									plot_pixel(bitmap,sx+x,y,Machine->pens[color*16+(rom[i]>>4)]);
								priority_bitmap->line[y][sx+x] = 8;
							}
						}
						x++;
					}

					if ((rom[i] & 0x0f) == 0x0f)
					{
						if (!drawing) drawing = 1;
						else break;
					}
					else
					{
						if (rom[i] & 0x0f)
						{
							if (sx+x >= 0 && sx+x < 512 && y >= 0 && y < 256)
							{
								if ((priority_bitmap->line[y][sx+x] & primask) == 0)
									plot_pixel(bitmap,sx+x,y,Machine->pens[color*16+(rom[i]&0x0f)]);
								priority_bitmap->line[y][sx+x] = 8;
							}
						}
						x++;
					}

					i++;
				}
			}
		}
	}
}

static void mark_sprite_colors(void)
{
	int i;
	unsigned short palette_map[0x80];
	int pal_base;

	memset(palette_map,0,sizeof(palette_map));

	for (i = 0;i < spriteram_size;i += 16)
	{
		int color;

		color = 0x40 + (READ_WORD(&spriteram[i+8]) & 0x3f);
		palette_map[color] |= 0xffff;
	}

	/* now build the final table */
	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;
	for (i = 0;i < sizeof(palette_map)/sizeof(palette_map[0]);i++)
	{
		int usage = palette_map[i],j;
		if (usage)
		{
			for (j = 1; j < 16; j++)
				if (usage & (1 << j))
					palette_used_colors[pal_base + i * 16 + j] |= PALETTE_COLOR_VISIBLE;
		}
	}
}


static void set_bg_page(int page,int value)
{
	int tile_index;

	if (bg_page[page] != value)
	{
		bg_page[page] = value;
		for (tile_index = page * 0x800;tile_index < (page+1)*0x800;tile_index++)
			tilemap_mark_tile_dirty(bg_tilemap,tile_index);
	}
}

static void set_fg_page(int page,int value)
{
	int tile_index;

	if (fg_page[page] != value)
	{
		fg_page[page] = value;
		for (tile_index = page * 0x800;tile_index < (page+1)*0x800;tile_index++)
			tilemap_mark_tile_dirty(fg_tilemap,tile_index);
	}
}

void deniam_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int bg_scrollx,bg_scrolly,fg_scrollx,fg_scrolly;
	int page;


	if (!display_enable) return;	/* don't update (freeze display) */

	bg_scrollx = READ_WORD(&deniam_textram[bg_scrollx_reg]) - bg_scrollx_offs;
	bg_scrolly = (READ_WORD(&deniam_textram[bg_scrolly_reg]) & 0xff) - bg_scrolly_offs;
	page = READ_WORD(&deniam_textram[bg_page_reg]);
	set_bg_page(3,(page >>12) & 0x0f);
	set_bg_page(2,(page >> 8) & 0x0f);
	set_bg_page(1,(page >> 4) & 0x0f);
	set_bg_page(0,(page >> 0) & 0x0f);

	fg_scrollx = READ_WORD(&deniam_textram[fg_scrollx_reg]) - fg_scrollx_offs;
	fg_scrolly = (READ_WORD(&deniam_textram[fg_scrolly_reg]) & 0xff) - fg_scrolly_offs;
	page = READ_WORD(&deniam_textram[fg_page_reg]);
	set_fg_page(3,(page >>12) & 0x0f);
	set_fg_page(2,(page >> 8) & 0x0f);
	set_fg_page(1,(page >> 4) & 0x0f);
	set_fg_page(0,(page >> 0) & 0x0f);

#if 0
usrintf_showmessage("bg %x%x%x%x %04x %04x  fg %x%x%x%x %04x %04x",
		bg_page[0],bg_page[1],bg_page[2],bg_page[3],
		READ_WORD(&deniam_textram[bg_scrollx_reg]),
		READ_WORD(&deniam_textram[bg_scrolly_reg]),
		fg_page[0],fg_page[1],fg_page[2],fg_page[3],
		READ_WORD(&deniam_textram[fg_scrollx_reg]),
		READ_WORD(&deniam_textram[fg_scrolly_reg]));
#endif

	tilemap_set_scrollx(bg_tilemap,0,bg_scrollx & 0x1ff);
	tilemap_set_scrolly(bg_tilemap,0,bg_scrolly & 0x0ff);
	tilemap_set_scrollx(fg_tilemap,0,fg_scrollx & 0x1ff);
	tilemap_set_scrolly(fg_tilemap,0,fg_scrolly & 0x0ff);

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();
	mark_sprite_colors();

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(priority_bitmap,0,NULL);

	tilemap_draw(bitmap,bg_tilemap,1<<16);
	tilemap_draw(bitmap,fg_tilemap,2<<16);
	tilemap_draw(bitmap,tx_tilemap,4<<16);

	draw_sprites(bitmap);
}
