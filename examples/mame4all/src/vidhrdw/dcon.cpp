/***************************************************************************

	D-Con video hardware.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static struct tilemap *background_layer,*foreground_layer,*midground_layer,*text_layer;
unsigned char *dcon_back_data,*dcon_fore_data,*dcon_mid_data,*dcon_scroll_ram;
static int dcon_enable;

/******************************************************************************/

WRITE_HANDLER( dcon_control_w )
{
	dcon_enable=data;
	if ((dcon_enable&4)==4)
		tilemap_set_enable(foreground_layer,0);
	else
		tilemap_set_enable(foreground_layer,1);

	if ((dcon_enable&2)==2)
		tilemap_set_enable(midground_layer,0);
	else
		tilemap_set_enable(midground_layer,1);

	if ((dcon_enable&1)==1)
		tilemap_set_enable(background_layer,0);
	else
		tilemap_set_enable(background_layer,1);
}

WRITE_HANDLER( dcon_background_w )
{
	COMBINE_WORD_MEM(&dcon_back_data[offset],data);
	tilemap_mark_tile_dirty( background_layer,offset/2);
}

WRITE_HANDLER( dcon_foreground_w )
{
	COMBINE_WORD_MEM(&dcon_fore_data[offset],data);
	tilemap_mark_tile_dirty( foreground_layer,offset/2);
}

WRITE_HANDLER( dcon_midground_w )
{
	COMBINE_WORD_MEM(&dcon_mid_data[offset],data);
	tilemap_mark_tile_dirty( midground_layer,offset/2);
}

WRITE_HANDLER( dcon_text_w )
{
	COMBINE_WORD_MEM(&videoram[offset],data);
	tilemap_mark_tile_dirty( text_layer,offset/2);
}

static void get_back_tile_info(int tile_index)
{
	int tile=READ_WORD(&dcon_back_data[2*tile_index]);
	int color=(tile>>12)&0xf;

	tile&=0xfff;

	SET_TILE_INFO(1,tile,color)
}

static void get_fore_tile_info(int tile_index)
{
	int tile=READ_WORD(&dcon_fore_data[2*tile_index]);
	int color=(tile>>12)&0xf;

	tile&=0xfff;

	SET_TILE_INFO(2,tile,color)
}

static void get_mid_tile_info(int tile_index)
{
	int tile=READ_WORD(&dcon_mid_data[2*tile_index]);
	int color=(tile>>12)&0xf;

	tile&=0xfff;

	SET_TILE_INFO(3,tile,color)
}

static void get_text_tile_info(int tile_index)
{
	int tile=READ_WORD(&videoram[2*tile_index]);
	int color=(tile>>12)&0xf;

	tile&=0xfff;

	SET_TILE_INFO(0,tile,color)
}

int dcon_vh_start(void)
{
	background_layer = tilemap_create(get_back_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,     16,16,32,32);
	foreground_layer = tilemap_create(get_fore_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
	midground_layer =  tilemap_create(get_mid_tile_info, tilemap_scan_rows,TILEMAP_TRANSPARENT,16,16,32,32);
	text_layer =       tilemap_create(get_text_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,  8,8,64,32);

	if (!background_layer || !foreground_layer || !midground_layer || !text_layer)
		return 1;

	midground_layer->transparent_pen = 15;
	foreground_layer->transparent_pen = 15;
	text_layer->transparent_pen = 15;

	return 0;
}

static void draw_sprites(struct osd_bitmap *bitmap,int pri)
{
	int offs,fx,fy,x,y,color,sprite;
	int dx,dy,ax,ay;

	for (offs = 0x800-8;offs >= 0;offs -= 8)
	{
		if ((READ_WORD(&spriteram[offs+0])&0x8000)!=0x8000) continue;
		sprite = READ_WORD(&spriteram[offs+2]);
		if ((sprite>>14)!=pri) continue;
		sprite &= 0x3fff;

		y = READ_WORD(&spriteram[offs+6]);
		x = READ_WORD(&spriteram[offs+4]);

		if (x&0x8000) x=0-(0x200-(x&0x1ff));
		else x&=0x1ff;
		if (y&0x8000) y=0-(0x200-(y&0x1ff));
		else y&=0x1ff;

		color = READ_WORD(&spriteram[offs+0])&0x3f;
		fx = 0; /* To do */
		fy = 0; /* To do */
		dy=((READ_WORD(&spriteram[offs+0])&0x0380)>>7)+1;
		dx=((READ_WORD(&spriteram[offs+0])&0x1c00)>>10)+1;

		for (ax=0; ax<dx; ax++)
			for (ay=0; ay<dy; ay++) {
				drawgfx(bitmap,Machine->gfx[4],
				sprite++,
				color,fx,fy,x+ax*16,y+ay*16,
				&Machine->visible_area,TRANSPARENCY_PEN,15);
			}
	}
}

static void mark_sprite_colours(void)
{
	int colmask[64],i,pal_base,color,offs,sprite,multi;

	pal_base = Machine->drv->gfxdecodeinfo[4].color_codes_start;
	for (color = 0;color < 64;color++) colmask[color] = 0;
	for (offs = 8;offs <0x800;offs += 8)
	{
		color = READ_WORD(&spriteram[offs+0])&0x3f;
		sprite = READ_WORD(&spriteram[offs+2]);
		sprite &= 0x3fff;
		multi=(((READ_WORD(&spriteram[offs+0])&0x0380)>>7)+1)*(((READ_WORD(&spriteram[offs+0])&0x1c00)>>10)+1);

		for (i=0; i<multi; i++)
			colmask[color] |= Machine->gfx[4]->pen_usage[(sprite+i)&0x3fff];
	}
	for (color = 0;color < 64;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}
}

void dcon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	/* Setup the tilemaps */
	tilemap_set_scrollx( background_layer,0, READ_WORD(&dcon_scroll_ram[0]) );
	tilemap_set_scrolly( background_layer,0, READ_WORD(&dcon_scroll_ram[2]) );
	tilemap_set_scrollx( midground_layer, 0, READ_WORD(&dcon_scroll_ram[4]) );
	tilemap_set_scrolly( midground_layer, 0, READ_WORD(&dcon_scroll_ram[6]) );
	tilemap_set_scrollx( foreground_layer,0, READ_WORD(&dcon_scroll_ram[8]) );
	tilemap_set_scrolly( foreground_layer,0, READ_WORD(&dcon_scroll_ram[0xa]) );

	tilemap_update(ALL_TILEMAPS);

	/* Build the dynamic palette */
	palette_init_used_colors();
	mark_sprite_colours();

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	if ((dcon_enable&1)!=1)
		tilemap_draw(bitmap,background_layer,0);
	else
		fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);

	draw_sprites(bitmap,2);
	tilemap_draw(bitmap,midground_layer,0);
	draw_sprites(bitmap,1);
	tilemap_draw(bitmap,foreground_layer,0);
	draw_sprites(bitmap,0);
	draw_sprites(bitmap,3);
	tilemap_draw(bitmap,text_layer,0);
}
