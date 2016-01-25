
#include "driver.h"
#include "vidhrdw/generic.h"

static struct tilemap *bg_layer,*fg_layer,*tx_layer;
unsigned char *dynduke_back_data,*dynduke_fore_data,*dynduke_scroll_ram,*dynduke_control_ram;

static int flipscreen,back_bankbase,fore_bankbase,back_palbase;
static int back_enable,fore_enable,sprite_enable;

/******************************************************************************/

WRITE_HANDLER( dynduke_paletteram_w )
{
	int r,g,b;

	paletteram[offset]=data;
	data=paletteram[offset&0xffe]|(paletteram[offset|1]<<8);

	r = (data >> 0) & 0x0f;
	g = (data >> 4) & 0x0f;
	b = (data >> 8) & 0x0f;

	r = (r << 4) | r;
	g = (g << 4) | g;
	b = (b << 4) | b;

	palette_change_color(offset/2,r,g,b);

	/* This is a kludge to handle 5bpp graphics but 4bpp palette data */
	if (offset<1024) {
		palette_change_color(((offset&0x1f)/2) | (offset&0xffe0) | 2048,r,g,b);
		palette_change_color(((offset&0x1f)/2) | (offset&0xffe0) | 2048 | 16,r,g,b);
	}
}

READ_HANDLER( dynduke_background_r )
{
	return dynduke_back_data[offset];
}

READ_HANDLER( dynduke_foreground_r )
{
	return dynduke_fore_data[offset];
}

WRITE_HANDLER( dynduke_background_w )
{
	dynduke_back_data[offset]=data;
	tilemap_mark_tile_dirty(bg_layer,offset/2);
}

WRITE_HANDLER( dynduke_foreground_w )
{
	dynduke_fore_data[offset]=data;
	tilemap_mark_tile_dirty(fg_layer,offset/2);
}

WRITE_HANDLER( dynduke_text_w )
{
	videoram[offset]=data;
	tilemap_mark_tile_dirty(tx_layer,offset/2);
}

static void get_bg_tile_info(int tile_index)
{
	int tile=dynduke_back_data[2*tile_index]+(dynduke_back_data[2*tile_index+1]<<8);
	int color=tile >> 12;

	tile=tile&0xfff;

	SET_TILE_INFO(1,tile+back_bankbase,color+back_palbase)
}

static void get_fg_tile_info(int tile_index)
{
	int tile=dynduke_fore_data[2*tile_index]+(dynduke_fore_data[2*tile_index+1]<<8);
	int color=tile >> 12;

	tile=tile&0xfff;

	SET_TILE_INFO(2,tile+fore_bankbase,color)
}

static void get_tx_tile_info(int tile_index)
{
	int tile=videoram[2*tile_index]+((videoram[2*tile_index+1]&0xc0)<<2);
	int color=videoram[2*tile_index+1]&0xf;

	SET_TILE_INFO(0,tile,color)
}

int dynduke_vh_start(void)
{
	bg_layer = tilemap_create(get_bg_tile_info,tilemap_scan_cols,TILEMAP_SPLIT,      16,16,32,32);
	fg_layer = tilemap_create(get_fg_tile_info,tilemap_scan_cols,TILEMAP_TRANSPARENT,16,16,32,32);
	tx_layer = tilemap_create(get_tx_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT, 8, 8,32,32);

	bg_layer->transmask[0] = 0x0000ffff; /* 4bpp */
	bg_layer->transmask[1] = 0xffff0000; /* The rest - 1bpp */

	fg_layer->transparent_pen = 15;
	tx_layer->transparent_pen = 15;

	return 0;
}

WRITE_HANDLER( dynduke_gfxbank_w )
{
	static int old_back,old_fore;

	if (data&0x01) back_bankbase=0x1000; else back_bankbase=0;
	if (data&0x10) fore_bankbase=0x1000; else fore_bankbase=0;

	if (back_bankbase!=old_back)
		tilemap_mark_all_tiles_dirty(bg_layer);
	if (fore_bankbase!=old_fore)
		tilemap_mark_all_tiles_dirty(fg_layer);

	old_back=back_bankbase;
	old_fore=fore_bankbase;
}

WRITE_HANDLER( dynduke_control_w )
{
	static int old_bpal;

	dynduke_control_ram[offset]=data;

	if (offset!=6) return;

	if (data&0x1) back_enable=0; else back_enable=1;
	if (data&0x2) back_palbase=16; else back_palbase=0;
	if (data&0x4) fore_enable=0; else fore_enable=1;
	if (data&0x8) sprite_enable=0; else sprite_enable=1;

	if (back_palbase!=old_bpal)
		tilemap_mark_all_tiles_dirty(bg_layer);

	old_bpal=back_palbase;
	flipscreen=data&0x40;
	tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
}

static void draw_sprites(struct osd_bitmap *bitmap,int pri)
{
	int offs,fx,fy,x,y,color,sprite;

	if (!sprite_enable) return;

	for (offs = 0x1000-8;offs >= 0;offs -= 8)
	{
		/* Don't draw empty sprite table entries */
		if (buffered_spriteram[offs+7]!=0xf) continue;
		if (buffered_spriteram[offs+0]==0xf0f) continue;
		if (((buffered_spriteram[offs+5]>>5)&3)!=pri) continue;

		fx= buffered_spriteram[offs+1]&0x20;
		fy= buffered_spriteram[offs+1]&0x40;
		y = buffered_spriteram[offs+0];
		x = buffered_spriteram[offs+4];

		if (buffered_spriteram[offs+5]&1) x=0-(0x100-x);

		color = buffered_spriteram[offs+1]&0x1f;
		sprite = buffered_spriteram[offs+2]+(buffered_spriteram[offs+3]<<8);
		sprite &= 0x3fff;

		if (flipscreen) {
			x=240-x;
			y=240-y;
			if (fx) fx=0; else fx=1;
			if (fy) fy=0; else fy=1;
		}

		drawgfx(bitmap,Machine->gfx[3],
				sprite,
				color,fx,fy,x,y,
				&Machine->visible_area,TRANSPARENCY_PEN,15);
	}
}

void dynduke_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int color,offs,sprite;
	int colmask[32],i,pal_base;

	/* Setup the tilemaps */
	tilemap_set_scrolly( bg_layer,0, ((dynduke_scroll_ram[0x02]&0x30)<<4)+((dynduke_scroll_ram[0x04]&0x7f)<<1)+((dynduke_scroll_ram[0x04]&0x80)>>7) );
	tilemap_set_scrollx( bg_layer,0, ((dynduke_scroll_ram[0x12]&0x30)<<4)+((dynduke_scroll_ram[0x14]&0x7f)<<1)+((dynduke_scroll_ram[0x14]&0x80)>>7) );
	tilemap_set_scrolly( fg_layer,0, ((dynduke_scroll_ram[0x22]&0x30)<<4)+((dynduke_scroll_ram[0x24]&0x7f)<<1)+((dynduke_scroll_ram[0x24]&0x80)>>7) );
	tilemap_set_scrollx( fg_layer,0, ((dynduke_scroll_ram[0x32]&0x30)<<4)+((dynduke_scroll_ram[0x34]&0x7f)<<1)+((dynduke_scroll_ram[0x34]&0x80)>>7) );
	tilemap_set_enable( bg_layer,back_enable);
	tilemap_set_enable( fg_layer,fore_enable);
	tilemap_update(ALL_TILEMAPS);

	/* Build the dynamic palette */
	palette_init_used_colors();

	/* Sprites */
	pal_base = Machine->drv->gfxdecodeinfo[3].color_codes_start;
	for (color = 0;color < 32;color++) colmask[color] = 0;
	for (offs = 0;offs <0x1000;offs += 8)
	{
		color = spriteram[offs+1]&0x1f;
		sprite = buffered_spriteram[offs+2]+(buffered_spriteram[offs+3]<<8);
		sprite &= 0x3fff;
		colmask[color] |= Machine->gfx[3]->pen_usage[sprite];
	}
	for (color = 0;color < 32;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	if (back_enable)
		tilemap_draw(bitmap,bg_layer,TILEMAP_BACK);
	else
		fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);

	draw_sprites(bitmap,0); /* Untested: does anything use it? Could be behind background */
	draw_sprites(bitmap,1);
	tilemap_draw(bitmap,bg_layer,TILEMAP_FRONT);
	draw_sprites(bitmap,2);
	tilemap_draw(bitmap,fg_layer,0);
	draw_sprites(bitmap,3);
	tilemap_draw(bitmap,tx_layer,0);
}
