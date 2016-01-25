#include "driver.h"
#include "vidhrdw/konamiic.h"
#include "vidhrdw/generic.h"

unsigned char *labyrunr_videoram1,*labyrunr_videoram2;
static struct tilemap *layer0, *layer1;


void labyrunr_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i,pal;

	for (pal = 0;pal < 8;pal++)
	{
		if (pal & 1)	/* chars, no lookup table */
		{
			for (i = 0;i < 256;i++)
				*(colortable++) = 16 * pal + (i & 0x0f);
		}
		else	/* sprites */
		{
			for (i = 0;i < 256;i++)
				if (color_prom[i] == 0)
					*(colortable++) = 0;
				else
					*(colortable++) = 16 * pal + color_prom[i];
		}
	}
}


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_tile_info0(int tile_index)
{
	int attr = labyrunr_videoram1[tile_index];
	int code = labyrunr_videoram1[tile_index + 0x400];
	int bit0 = (K007121_ctrlram[0][0x05] >> 0) & 0x03;
	int bit1 = (K007121_ctrlram[0][0x05] >> 2) & 0x03;
	int bit2 = (K007121_ctrlram[0][0x05] >> 4) & 0x03;
	int bit3 = (K007121_ctrlram[0][0x05] >> 6) & 0x03;
	int bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0+2)) & 0x02) |
			((attr >> (bit1+1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3-1)) & 0x10) |
			((K007121_ctrlram[0][0x03] & 0x01) << 5);
	int mask = (K007121_ctrlram[0][0x04] & 0xf0) >> 4;

	bank = (bank & ~(mask << 1)) | ((K007121_ctrlram[0][0x04] & mask) << 1);

	SET_TILE_INFO(0,code+bank*256,((K007121_ctrlram[0][6]&0x30)*2+16)+(attr&7))
}

static void get_tile_info1(int tile_index)
{
	int attr = labyrunr_videoram2[tile_index];
	int code = labyrunr_videoram2[tile_index + 0x400];
	int bit0 = (K007121_ctrlram[0][0x05] >> 0) & 0x03;
	int bit1 = (K007121_ctrlram[0][0x05] >> 2) & 0x03;
	int bit2 = (K007121_ctrlram[0][0x05] >> 4) & 0x03;
	int bit3 = (K007121_ctrlram[0][0x05] >> 6) & 0x03;
	int bank = ((attr & 0x80) >> 7) |
			((attr >> (bit0+2)) & 0x02) |
			((attr >> (bit1+1)) & 0x04) |
			((attr >> (bit2  )) & 0x08) |
			((attr >> (bit3-1)) & 0x10) |
			((K007121_ctrlram[0][0x03] & 0x01) << 5);
	int mask = (K007121_ctrlram[0][0x04] & 0xf0) >> 4;

	bank = (bank & ~(mask << 1)) | ((K007121_ctrlram[0][0x04] & mask) << 1);

	SET_TILE_INFO(0,code+bank*256,((K007121_ctrlram[0][6]&0x30)*2+16)+(attr&7))
}


/***************************************************************************

	Start the video hardware emulation.

***************************************************************************/

int labyrunr_vh_start(void)
{
	layer0 = tilemap_create(get_tile_info0,tilemap_scan_rows,TILEMAP_OPAQUE,8,8,32,32);
	layer1 = tilemap_create(get_tile_info1,tilemap_scan_rows,TILEMAP_OPAQUE,8,8,32,32);

	if (layer0 && layer1)
	{
		struct rectangle clip = Machine->visible_area;
		clip.min_x += 40;
		tilemap_set_clip(layer0,&clip);

		clip.max_x = 39;
		clip.min_x = 0;
		tilemap_set_clip(layer1,&clip);

		return 0;
	}
	return 1;
}



/***************************************************************************

  Memory Handlers

***************************************************************************/

WRITE_HANDLER( labyrunr_vram1_w )
{
	if (labyrunr_videoram1[offset] != data)
	{
		labyrunr_videoram1[offset] = data;
		tilemap_mark_tile_dirty(layer0,offset & 0x3ff);
	}
}

WRITE_HANDLER( labyrunr_vram2_w )
{
	if (labyrunr_videoram2[offset] != data)
	{
		labyrunr_videoram2[offset] = data;
		tilemap_mark_tile_dirty(layer1,offset & 0x3ff);
	}
}



/***************************************************************************

  Screen Refresh

***************************************************************************/

void labyrunr_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_set_scrollx(layer0,0,K007121_ctrlram[0][0x00] - 40);
	tilemap_set_scrolly(layer0,0,K007121_ctrlram[0][0x02]);

	tilemap_update(ALL_TILEMAPS);
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);
	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap,layer0,0);
	K007121_sprites_draw(0,bitmap,spriteram,(K007121_ctrlram[0][6]&0x30)*2,40,0,-1);
	tilemap_draw(bitmap,layer1,0 );
}
