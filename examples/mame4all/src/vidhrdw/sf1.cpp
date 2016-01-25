#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *sf1_objectram;

int sf1_active = 0;

static struct tilemap *bgb_tilemap, *bgm_tilemap, *char_tilemap;


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_bgb_tile_info(int tile_index)
{
	unsigned char *base = memory_region(REGION_GFX5) + 2*tile_index;
	int attr = base[0x10000];
	int color = base[0];
	int code = (base[0x10000+1]<<8) | base[1];
	SET_TILE_INFO (0, code, color);
	tile_info.flags = TILE_FLIPYX(attr & 3);
}

static void get_bgm_tile_info(int tile_index)
{
	unsigned char *base = memory_region(REGION_GFX5) + 0x20000 + 2*tile_index;
	int attr = base[0x10000];
	int color = base[0];
	int code = (base[0x10000+1]<<8) | base[1];
	SET_TILE_INFO (1, code, color);
	tile_info.flags = TILE_FLIPYX(attr & 3);
}

static void get_char_tile_info(int tile_index)
{
	int code = READ_WORD(&videoram[2*tile_index]);
	SET_TILE_INFO (3, code & 0x3ff, code>>12);
	tile_info.flags = TILE_FLIPYX((code & 0xc00)>>10);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int sf1_vh_start(void)
{
	int i;

	bgb_tilemap =  tilemap_create(get_bgb_tile_info, tilemap_scan_cols,TILEMAP_OPAQUE,     16,16,2048,16);
	bgm_tilemap =  tilemap_create(get_bgm_tile_info, tilemap_scan_cols,TILEMAP_TRANSPARENT,16,16,2048,16);
	char_tilemap = tilemap_create(get_char_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,  64,32);

	if (!bgb_tilemap || !bgm_tilemap || !char_tilemap)
		return 1;

	bgm_tilemap->transparent_pen = 15;
	char_tilemap->transparent_pen = 3;

	for(i = 832; i<1024; i++)
		palette_used_colors[i] = PALETTE_COLOR_UNUSED;

	return 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( sf1_videoram_w )
{
	int old = READ_WORD(&videoram[offset]);
	int _new = COMBINE_WORD(old, data);
	if (old != _new)
	{
		WRITE_WORD(&videoram[offset], _new);
		tilemap_mark_tile_dirty(char_tilemap,offset/2);
	}
}

WRITE_HANDLER( sf1_deltaxb_w )
{
	tilemap_set_scrollx(bgb_tilemap, 0, data);
}

WRITE_HANDLER( sf1_deltaxm_w )
{
	tilemap_set_scrollx(bgm_tilemap, 0, data);
}

void sf1_active_w(int data)
{
	sf1_active = data;
	tilemap_set_enable(bgb_tilemap, data & 0x20);
	tilemap_set_enable(bgm_tilemap, data & 0x40);
	tilemap_set_enable(char_tilemap, data & 0x08);
}



/***************************************************************************

  Display refresh

***************************************************************************/

INLINE int sf1_invert(int nb)
{
	static int delta[4] = {0x00, 0x18, 0x18, 0x00};
	return nb^delta[(nb>>3)&3];
}


static void mark_sprites_palette(void)
{
	unsigned char *umap = &palette_used_colors[Machine->drv->gfxdecodeinfo[2].color_codes_start];
	unsigned int cmap = 0;
	unsigned char *pt = sf1_objectram + 0x2000-0x40;
	int i, j;

	while(pt>=sf1_objectram)
	{
		int at = READ_WORD(pt+2);
		int y = READ_WORD(pt+4);
		int x = READ_WORD(pt+6);

		if(x>32 && x<415 && y>0 && y<256)
			cmap |= (1<<(at & 0x0f));

		pt -= 0x40;
	}

	for(i=0;i<16;i++)
	{
		if(cmap & (1<<i))
		{
			for(j=0;j<15;j++)
				*umap++ = PALETTE_COLOR_USED;
			*umap++ = PALETTE_COLOR_TRANSPARENT;
		}
		else
		{
			for(j=0;j<16;j++)
				*umap++ = PALETTE_COLOR_UNUSED;
		}
	}
}

static void draw_sprites(struct osd_bitmap *bitmap)
{
	unsigned char *pt = sf1_objectram + 0x2000-0x40;

	while(pt>=sf1_objectram) {
		int c = READ_WORD(pt);
		int at = READ_WORD(pt+2);
		int y = READ_WORD(pt+4);
		int x = READ_WORD(pt+6);

		if(x>32 && x<415 && y>0 && y<256) {
			if(!(at&0x400)) {
				drawgfx(bitmap,
						Machine->gfx[2],
						sf1_invert(c),
						at & 0xf,
						at & 0x100, at & 0x200,
						x, y,
						&Machine->visible_area, TRANSPARENCY_PEN, 15);
			} else {
				int c1, c2, c3, c4;
				switch(at & 0x300) {
				case 0x000:
				default:
					c1 = c;
					c2 = c+1;
					c3 = c+16;
					c4 = c+17;
					break;
				case 0x100:
					c1 = c+1;
					c2 = c;
					c3 = c+17;
					c4 = c+16;
					break;
				case 0x200:
					c1 = c+16;
					c2 = c+17;
					c3 = c;
					c4 = c+1;
					break;
				case 0x300:
					c1 = c+17;
					c2 = c+16;
					c3 = c+1;
					c4 = c;
					break;
				}
				drawgfx(bitmap,
						Machine->gfx[2],
						sf1_invert(c1),
						at & 0xf,
						at & 0x100, at & 0x200,
						x, y,
						&Machine->visible_area, TRANSPARENCY_PEN, 15);
				drawgfx(bitmap,
						Machine->gfx[2],
						sf1_invert(c2),
						at & 0xf,
						at & 0x100, at & 0x200,
						x+16, y,
						&Machine->visible_area, TRANSPARENCY_PEN, 15);
				drawgfx(bitmap,
						Machine->gfx[2],
						sf1_invert(c3),
						at & 0xf,
						at & 0x100, at & 0x200,
						x, y+16,
						&Machine->visible_area, TRANSPARENCY_PEN, 15);
				drawgfx(bitmap,
						Machine->gfx[2],
						sf1_invert(c4),
						at & 0xf,
						at & 0x100, at & 0x200,
						x+16, y+16,
						&Machine->visible_area, TRANSPARENCY_PEN, 15);
			}
		}
		pt -= 0x40;
	}
}


void sf1_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();

	if(sf1_active & 0x80)
		mark_sprites_palette();

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap, bgb_tilemap, 0);

	if(!(sf1_active & 0x20))
		fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);

	tilemap_draw(bitmap, bgm_tilemap, 0);

	if(sf1_active & 0x80)
		draw_sprites(bitmap);

	tilemap_draw(bitmap, char_tilemap, 0);
}
