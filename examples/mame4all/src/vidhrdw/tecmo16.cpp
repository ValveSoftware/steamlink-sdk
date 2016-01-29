/******************************************************************************

  Ganbare Ginkun  (Japan)  (c)1995 TECMO
  Final StarForce (US)     (c)1992 TECMO

  Based on sprite drivers from vidhrdw/wc90.c by Ernesto Corvi (ernesto@imagina.com)

******************************************************************************/

#include "vidhrdw/generic.h"


unsigned char *tecmo16_videoram;
unsigned char *tecmo16_colorram;
unsigned char *tecmo16_videoram2;
unsigned char *tecmo16_colorram2;
unsigned char *tecmo16_charram;

static struct tilemap *fg_tilemap,*bg_tilemap,*tx_tilemap;

/******************************************************************************/

static void fg_get_tile_info(int tile_index)
{
	int offs = tile_index*2;
	int tile = READ_WORD(&tecmo16_videoram[offs]) & 0x1fff;
	int color = READ_WORD(&tecmo16_colorram[offs]) & 0x7f;

	SET_TILE_INFO(1,tile,color)
}

static void bg_get_tile_info(int tile_index)
{
	int offs = tile_index*2;
	int tile = READ_WORD(&tecmo16_videoram2[offs]) & 0x1fff;
	int color = (READ_WORD(&tecmo16_colorram2[offs]) & 0x7f)+0x10;

	SET_TILE_INFO(1,tile,color)
}

static void tx_get_tile_info(int tile_index)
{
	int offs = tile_index*2;
	int tile = READ_WORD(&tecmo16_charram[offs]) & 0x0fff;
	int color = (READ_WORD(&tecmo16_charram[offs]) >> 12) & 0x0f;

	SET_TILE_INFO(0,tile,color)
}

/******************************************************************************/

int fstarfrc_vh_start(void)
{
	fg_tilemap = tilemap_create(fg_get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
	bg_tilemap = tilemap_create(bg_get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
	tx_tilemap = tilemap_create(tx_get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,64,32);

	if (!fg_tilemap || !bg_tilemap || !tx_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 0;
	bg_tilemap->transparent_pen = 0;
	tx_tilemap->transparent_pen = 0;

	tilemap_set_scrolly(tx_tilemap,0,-16);

	return 0;
}

int ginkun_vh_start(void)
{
	fg_tilemap = tilemap_create(fg_get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,64,32);
	bg_tilemap = tilemap_create(bg_get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,64,32);
	tx_tilemap = tilemap_create(tx_get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,64,32);

	if (!fg_tilemap || !bg_tilemap || !tx_tilemap)
		return 1;

	fg_tilemap->transparent_pen = 0;
	bg_tilemap->transparent_pen = 0;
	tx_tilemap->transparent_pen = 0;

	return 0;
}

/******************************************************************************/

READ_HANDLER( tecmo16_videoram_r )
{
   return READ_WORD(&tecmo16_videoram[offset]);
}

READ_HANDLER( tecmo16_colorram_r )
{
   return READ_WORD(&tecmo16_colorram[offset]);
}

READ_HANDLER( tecmo16_videoram2_r )
{
   return READ_WORD(&tecmo16_videoram2[offset]);
}

READ_HANDLER( tecmo16_colorram2_r )
{
   return READ_WORD(&tecmo16_colorram2[offset]);
}

READ_HANDLER( tecmo16_charram_r )
{
   return READ_WORD(&tecmo16_charram[offset]);
}

READ_HANDLER( tecmo16_spriteram_r )
{
   return READ_WORD(&spriteram[offset]);
}

WRITE_HANDLER( tecmo16_videoram_w )
{
	int oldword = READ_WORD(&tecmo16_videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&tecmo16_videoram[offset],newword);
		tilemap_mark_tile_dirty(fg_tilemap,offset/2);
	}
}

WRITE_HANDLER( tecmo16_colorram_w )
{
	int oldword = READ_WORD(&tecmo16_colorram[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&tecmo16_colorram[offset],newword);
		tilemap_mark_tile_dirty(fg_tilemap,offset/2);
	}
}

WRITE_HANDLER( tecmo16_videoram2_w )
{
	int oldword = READ_WORD(&tecmo16_videoram2[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&tecmo16_videoram2[offset],newword);
		tilemap_mark_tile_dirty(bg_tilemap,offset/2);
	}
}

WRITE_HANDLER( tecmo16_colorram2_w )
{
	int oldword = READ_WORD(&tecmo16_colorram2[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&tecmo16_colorram2[offset],newword);
		tilemap_mark_tile_dirty(bg_tilemap,offset/2);
	}
}


WRITE_HANDLER( tecmo16_charram_w )
{
	int oldword = READ_WORD(&tecmo16_charram[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&tecmo16_charram[offset],newword);
		tilemap_mark_tile_dirty(tx_tilemap,offset/2);
	}
}

WRITE_HANDLER( tecmo16_spriteram_w )
{
	COMBINE_WORD_MEM(&spriteram[offset],data);
}

/******************************************************************************/

WRITE_HANDLER( tecmo16_scroll_x_w )
{
	tilemap_set_scrollx(fg_tilemap,0,data);
}

WRITE_HANDLER( tecmo16_scroll_y_w )
{
	tilemap_set_scrolly(fg_tilemap,0,data);
}

WRITE_HANDLER( tecmo16_scroll2_x_w )
{
	tilemap_set_scrollx(bg_tilemap,0,data);
}

WRITE_HANDLER( tecmo16_scroll2_y_w )
{
	tilemap_set_scrolly(bg_tilemap,0,data);
}

WRITE_HANDLER( tecmo16_scroll_char_x_w )
{
	tilemap_set_scrollx(tx_tilemap,0,data);
}

WRITE_HANDLER( tecmo16_scroll_char_y_w )
{
	tilemap_set_scrolly(tx_tilemap,0,data-16);
}

/******************************************************************************/

static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;
	const UINT8 layout[8][8] =
	{
		{0,1,4,5,16,17,20,21},
		{2,3,6,7,18,19,22,23},
		{8,9,12,13,24,25,28,29},
		{10,11,14,15,26,27,30,31},
		{32,33,36,37,48,49,52,53},
		{34,35,38,39,50,51,54,55},
		{40,41,44,45,56,57,60,61},
		{42,43,46,47,58,59,62,63}
	};

	for (offs = spriteram_size - 0x10;offs >= 0;offs -= 0x10)
	{
		if (READ_WORD(&spriteram[offs]) & 0x04)	/* enable */
		{
			int code,color,sizex,sizey,flipx,flipy,xpos,ypos;
			int x,y,priority,priority_mask;

			code = READ_WORD(&spriteram[offs+0x02]);
			color = (READ_WORD(&spriteram[offs+0x04]) & 0xf0) >> 4;
			sizex = 1 << ((READ_WORD(&spriteram[offs+0x04]) & 0x03) >> 0);
			sizey = 1 << ((READ_WORD(&spriteram[offs+0x04]) & 0x0c) >> 2);
			if (sizex >= 2) code &= ~0x01;
			if (sizey >= 2) code &= ~0x02;
			if (sizex >= 4) code &= ~0x04;
			if (sizey >= 4) code &= ~0x08;
			if (sizex >= 8) code &= ~0x10;
			if (sizey >= 8) code &= ~0x20;
			flipx = READ_WORD(&spriteram[offs]) & 0x01;
			flipy = READ_WORD(&spriteram[offs]) & 0x02;
			xpos = READ_WORD(&spriteram[offs+0x08]);
			if (xpos >= 0x8000) xpos -= 0x10000;
			ypos = READ_WORD(&spriteram[offs+0x06]);
			if (ypos >= 0x8000) ypos -= 0x10000;
			priority = (READ_WORD(&spriteram[offs]) & 0xc0) >> 6;

			/* bg: 1; fg:2; text: 4 */
			switch (priority)
			{
				default:
				case 0x0: priority_mask = 0; break;
				case 0x1: priority_mask = 0xf0; break; /* obscured by text layer */
				case 0x2: priority_mask = 0xf0|0xcc; break;	/* obscured by foreground */
				case 0x3: priority_mask = 0xf0|0xcc|0xaa; break; /* obscured by bg and fg */
			}

			for (y = 0;y < sizey;y++)
			{
				for (x = 0;x < sizex;x++)
				{
					int sx = xpos + 8*(flipx?(sizex-1-x):x);
					int sy = ypos + 8*(flipy?(sizey-1-y):y);
					pdrawgfx(bitmap,Machine->gfx[2],
							code + layout[y][x],
							color,
							flipx,flipy,
							sx,sy,
							&Machine->visible_area,TRANSPARENCY_PEN,0,
							priority_mask);
				}
			}
		}
	}
}

static void mark_sprites_colors(void)
{
	int offs,i;
	UINT16 palette_map[16];


	memset(palette_map,0,sizeof(palette_map));

	for (offs = 0; offs < spriteram_size; offs += 0x10)
	{
		if (READ_WORD(&spriteram[offs]) & 0x04)	/* visible */
		{
			int color;

			color = (READ_WORD(&spriteram[offs+0x04]) & 0xf0) >> 4;
			palette_map[color] |= 0xffff;
		}
	}

	/* now build the final table */
	for (i = 0; i < 16;i++)
	{
		int usage = palette_map[i], j;
		if (usage)
		{
			for (j = 1; j < 16; j++)
				if (usage & (1 << j))
					palette_used_colors[i * 16 + j] |= PALETTE_COLOR_VISIBLE;
		}
	}
}

/******************************************************************************/

void tecmo16_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();
	mark_sprites_colors();
	palette_used_colors[0x300] = PALETTE_COLOR_USED;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[0x300],&Machine->visible_area);
	tilemap_draw(bitmap,bg_tilemap,1<<16);
	tilemap_draw(bitmap,fg_tilemap,2<<16);
	tilemap_draw(bitmap,tx_tilemap,4<<16);

	draw_sprites(bitmap);
}
