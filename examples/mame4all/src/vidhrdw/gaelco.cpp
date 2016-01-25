/***************************************************************************

  Gaelco Type 1 Video Hardware

  Functions to emulate the video hardware of the machine

***************************************************************************/

#include "driver.h"
#include "tilemap.h"
#include "vidhrdw/generic.h"

unsigned char *gaelco_vregs;
unsigned char *gaelco_videoram;
unsigned char *gaelco_spriteram;

int sprite_count[5];
int *sprite_table[5];
static struct tilemap *pant[2];


/***************************************************************************

	Callbacks for the TileMap code

***************************************************************************/

/*
	Tile format
	-----------

	Screen 0 & 1: (32*32, 16x16 tiles)

	Byte | Bit(s)			 | Description
	-----+-FEDCBA98-76543210-+--------------------------
	  0  | -------- -------x | flip x
	  0  | -------- ------x- | flip y
	  0  | -------- xxxxxx-- | code (low 6 bits)
	  0  | xxxxxxxx -------- | code (high 8 bits)
	  2  | -------- --xxxxxx | color
	  2	 | -------- xx------ | priority
	  2  | xxxxxxxx -------- | not used
*/

static void get_tile_info_gaelco_screen0(int tile_index)
{
	int data = READ_WORD(&gaelco_videoram[tile_index << 2]);
	int data2 = READ_WORD(&gaelco_videoram[(tile_index << 2) + 2]);
	int code = ((data & 0xfffc) >> 2);

	tile_info.flags = TILE_FLIPYX(data & 0x03);
	tile_info.priority = (data2 >> 6) & 0x03;

	SET_TILE_INFO(1, 0x4000 + code, data2 & 0x3f);
}


static void get_tile_info_gaelco_screen1(int tile_index)
{
	int data = READ_WORD(&gaelco_videoram[0x1000 + (tile_index << 2)]);
	int data2 = READ_WORD(&gaelco_videoram[0x1000 + (tile_index << 2) + 2]);
	int code = ((data & 0xfffc) >> 2);

	tile_info.flags = TILE_FLIPYX(data & 0x03);
	tile_info.priority = (data2 >> 6) & 0x03;

	SET_TILE_INFO(1, 0x4000 + code, data2 & 0x3f);
}

/***************************************************************************

	Memory Handlers

***************************************************************************/

READ_HANDLER( gaelco_vram_r )
{
	return READ_WORD(&gaelco_videoram[offset]);
}

WRITE_HANDLER( gaelco_vram_w )
{
	COMBINE_WORD_MEM(&gaelco_videoram[offset],data);
	tilemap_mark_tile_dirty(pant[offset >> 12],(offset & 0x0fff) >> 2);
}

/***************************************************************************

	Start/Stop the video hardware emulation.

***************************************************************************/

void gaelco_vh_stop(void)
{
	int i;

	for (i = 0; i < 5; i++){
		if (sprite_table[i])
			free(sprite_table[i]);
		sprite_table[i] = NULL;
	}
}

int bigkarnk_vh_start(void)
{
	int i;

	pant[0] = tilemap_create(get_tile_info_gaelco_screen0,tilemap_scan_rows,TILEMAP_SPLIT,16,16,32,32);
	pant[1] = tilemap_create(get_tile_info_gaelco_screen1,tilemap_scan_rows,TILEMAP_SPLIT,16,16,32,32);

	if (!pant[0] || !pant[1])
		return 1;

	pant[0]->transparent_pen = 0;
	pant[1]->transparent_pen = 0;

	pant[0]->transmask[0] = 0xff01; /* pens 1-7 opaque, pens 0, 8-15 transparent */
	pant[1]->transmask[0] = 0xff01; /* pens 1-7 opaque, pens 0, 8-15 transparent */

	for (i = 0; i < 5; i++){
		sprite_table[i] = (int*)malloc(512*sizeof(int));
		if (!sprite_table[i]){
			gaelco_vh_stop();
			return 1;
		}
	}

	return 0;
}

int maniacsq_vh_start(void)
{
	int i;

	pant[0] = tilemap_create(get_tile_info_gaelco_screen0,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
	pant[1] = tilemap_create(get_tile_info_gaelco_screen1,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);

	if (!pant[0] || !pant[1])
		return 1;

	pant[0]->transparent_pen = 0;
	pant[1]->transparent_pen = 0;

	for (i = 0; i < 5; i++){
		sprite_table[i] = (int*)malloc(512*sizeof(int));
		if (!sprite_table[i]){
			gaelco_vh_stop();
			return 1;
		}
	}

	return 0;
}


/***************************************************************************

	Sprites

***************************************************************************/

static void gaelco_sort_sprites(void)
{
	int i;

	sprite_count[0] = 0;
	sprite_count[1] = 0;
	sprite_count[2] = 0;
	sprite_count[3] = 0;
	sprite_count[4] = 0;

	for (i = 6; i < 0x1000 - 6; i += 8){
		int color = (READ_WORD(&gaelco_spriteram[i+4]) & 0x7e00) >> 9;
		int priority = (READ_WORD(&gaelco_spriteram[i]) & 0x3000) >> 12;

		/* palettes 0x38-0x3f are used for high priority sprites in Big Karnak */
		if (color >= 0x38){
			sprite_table[4][sprite_count[4]] = i;
			sprite_count[4]++;
		}

		/* save sprite number in the proper array for later */
		sprite_table[priority][sprite_count[priority]] = i;
		sprite_count[priority]++;
	}
}

/*
	Sprite Format
	-------------

	Byte | Bit(s)			 | Description
	-----+-FEDCBA98-76543210-+--------------------------
	  0  | -------- xxxxxxxx | y position
	  0  | -----xxx -------- | not used
	  0  | ----x--- -------- | sprite size
	  0  | --xx---- -------- | sprite priority
	  0  | -x------ -------- | flipx
	  0  | x------- -------- | flipy
	  2  | xxxxxxxx xxxxxxxx | not used
	  4  | -------x xxxxxxxx | x position
	  4  | -xxxxxx- -------- | sprite color
	  6	 | -------- ------xx | sprite code (8x8 cuadrant)
	  6  | xxxxxxxx xxxxxx-- | sprite code
*/

static void gaelco_draw_sprites(struct osd_bitmap *bitmap, int pri)
{
	int j, x, y, ex, ey;
	const struct GfxElement *gfx = Machine->gfx[0];

	static int x_offset[2] = {0x0,0x2};
	static int y_offset[2] = {0x0,0x1};

	for (j = 0; j < sprite_count[pri]; j++){
		int i = sprite_table[pri][j];
		int sx = READ_WORD(&gaelco_spriteram[i+4]) & 0x01ff;
		int sy = (240 - (READ_WORD(&gaelco_spriteram[i]) & 0x00ff)) & 0x00ff;
		int number = READ_WORD(&gaelco_spriteram[i+6]);
		int color = (READ_WORD(&gaelco_spriteram[i+4]) & 0x7e00) >> 9;
		int attr = (READ_WORD(&gaelco_spriteram[i]) & 0xfe00) >> 9;

		int xflip = attr & 0x20;
		int yflip = attr & 0x40;
		int spr_size;

		if (attr & 0x04){
			spr_size = 1;
		}
		else{
			spr_size = 2;
			number &= (~3);
		}

		for (y = 0; y < spr_size; y++){
			for (x = 0; x < spr_size; x++){

				ex = xflip ? (spr_size-1-x) : x;
				ey = yflip ? (spr_size-1-y) : y;

				drawgfx(bitmap,gfx,number + x_offset[ex] + y_offset[ey],
						color,xflip,yflip,
						sx-0x0f+x*8,sy+y*8,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
		}
	}
}

/***************************************************************************

	Display Refresh

***************************************************************************/

void maniacsq_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	/* set scroll registers */
	tilemap_set_scrolly(pant[0], 0, READ_WORD(&gaelco_vregs[0]));
	tilemap_set_scrollx(pant[0], 0, READ_WORD(&gaelco_vregs[2])+4);
	tilemap_set_scrolly(pant[1], 0, READ_WORD(&gaelco_vregs[4]));
	tilemap_set_scrollx(pant[1], 0, READ_WORD(&gaelco_vregs[6]));

	tilemap_update(ALL_TILEMAPS);
	gaelco_sort_sprites();

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);


	fillbitmap( bitmap, Machine->pens[0], &Machine->visible_area );

	tilemap_draw(bitmap,pant[1],3);
	tilemap_draw(bitmap,pant[0],3);
	gaelco_draw_sprites(bitmap,3);

	tilemap_draw(bitmap,pant[1],2);
	tilemap_draw(bitmap,pant[0],2);
	gaelco_draw_sprites(bitmap,2);

	tilemap_draw(bitmap,pant[1],1);
	tilemap_draw(bitmap,pant[0],1);
	gaelco_draw_sprites(bitmap,1);

	tilemap_draw(bitmap,pant[1],0);
	tilemap_draw(bitmap,pant[0],0);
	gaelco_draw_sprites(bitmap,0);
}

void bigkarnk_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	/* set scroll registers */
	tilemap_set_scrolly(pant[0], 0, READ_WORD(&gaelco_vregs[0]));
	tilemap_set_scrollx(pant[0], 0, READ_WORD(&gaelco_vregs[2])+4);
	tilemap_set_scrolly(pant[1], 0, READ_WORD(&gaelco_vregs[4]));
	tilemap_set_scrollx(pant[1], 0, READ_WORD(&gaelco_vregs[6]));

	tilemap_update(ALL_TILEMAPS);
	gaelco_sort_sprites();

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);


	fillbitmap( bitmap, Machine->pens[0], &Machine->visible_area );

	tilemap_draw(bitmap,pant[1],TILEMAP_BACK | 3);
	tilemap_draw(bitmap,pant[0],TILEMAP_BACK | 3);
	gaelco_draw_sprites(bitmap,3);
	tilemap_draw(bitmap,pant[1],TILEMAP_FRONT | 3);
	tilemap_draw(bitmap,pant[0],TILEMAP_FRONT | 3);

	tilemap_draw(bitmap,pant[1],TILEMAP_BACK | 2);
	tilemap_draw(bitmap,pant[0],TILEMAP_BACK | 2);
	gaelco_draw_sprites(bitmap,2);
	tilemap_draw(bitmap,pant[1],TILEMAP_FRONT | 2);
	tilemap_draw(bitmap,pant[0],TILEMAP_FRONT | 2);

	tilemap_draw(bitmap,pant[1],TILEMAP_BACK | 1);
	tilemap_draw(bitmap,pant[0],TILEMAP_BACK | 1);
	gaelco_draw_sprites(bitmap,1);
	tilemap_draw(bitmap,pant[1],TILEMAP_FRONT | 1);
	tilemap_draw(bitmap,pant[0],TILEMAP_FRONT | 1);

	tilemap_draw(bitmap,pant[1],TILEMAP_BACK | 0);
	tilemap_draw(bitmap,pant[0],TILEMAP_BACK | 0);
	gaelco_draw_sprites(bitmap,0);
	tilemap_draw(bitmap,pant[1],TILEMAP_FRONT | 0);
	tilemap_draw(bitmap,pant[0],TILEMAP_FRONT | 0);

	gaelco_draw_sprites(bitmap,4);
}
