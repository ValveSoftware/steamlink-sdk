/***************************************************************************

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/konamiic.h"
#include "vidhrdw/generic.h"

static struct tilemap *k007121_tilemap[2];

unsigned char *k007121_ram;

int flkatck_irq_enabled;

static int k007121_flip_screen = 0;

/***************************************************************************

  Callbacks for the K007121

***************************************************************************/

static void get_tile_info_A(int tile_index)
{
	int attr = k007121_ram[tile_index];
	int code = k007121_ram[tile_index+0x400];
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

	if ((attr == 0x0d) && (!(K007121_ctrlram[0][0])) && (!(K007121_ctrlram[0][2])))
		bank = 0;	/*	this allows the game to print text
					in all banks selected by the k007121 */
	tile_info.flags = (attr & 0x20) ? TILE_FLIPY : 0;

	SET_TILE_INFO(0, code + 256*bank, (attr & 0x0f) + 16)
}

static void get_tile_info_B(int tile_index)
{
	int attr = k007121_ram[tile_index+0x800];
	int code = k007121_ram[tile_index+0xc00];

	SET_TILE_INFO(0, code, (attr & 0x0f) + 16)
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int flkatck_vh_start(void)
{
	k007121_tilemap[0] = tilemap_create(get_tile_info_A,tilemap_scan_rows,TILEMAP_OPAQUE,8,8,32,32);
	k007121_tilemap[1] = tilemap_create(get_tile_info_B,tilemap_scan_rows,TILEMAP_OPAQUE,8,8,32,32);

	if (!k007121_tilemap[0] || !k007121_tilemap[1])
		return 1;

	{
		struct rectangle clip = Machine->visible_area;
		clip.min_x += 40;
		tilemap_set_clip(k007121_tilemap[0],&clip);

		clip.max_x = 39;
		clip.min_x = 0;
		tilemap_set_clip(k007121_tilemap[1],&clip);

		return 0;
	}
}


/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( flkatck_k007121_w )
{
	if (offset < 0x1000){	/* tiles */
		if (k007121_ram[offset] != data)
		{
			k007121_ram[offset] = data;
			if (offset & 0x800)	/* score */
				tilemap_mark_tile_dirty(k007121_tilemap[1],offset & 0x3ff);
			else
				tilemap_mark_tile_dirty(k007121_tilemap[0],offset & 0x3ff);
		}
	}
	else	/* sprites */
		k007121_ram[offset] = data;
}

WRITE_HANDLER( flkatck_k007121_regs_w )
{
	switch (offset)
	{
		case 0x04:	/* ROM bank select */
			if (data != K007121_ctrlram[0][0x04])
				tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);
			break;

		case 0x07:	/* flip screen + IRQ control */
			k007121_flip_screen = data & 0x08;
			tilemap_set_flip(ALL_TILEMAPS, k007121_flip_screen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
			flkatck_irq_enabled = data & 0x02;
			break;
	}

	K007121_ctrl_0_w(offset,data);
}


/***************************************************************************

	Display Refresh

***************************************************************************/

/***************************************************************************

	Flack Attack sprites. Each sprite has 16 bytes!:


***************************************************************************/

void flkatck_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
#if 0
usrintf_showmessage("%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x  %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x",
	K007121_ctrlram[0][0x00],K007121_ctrlram[0][0x01],K007121_ctrlram[0][0x02],K007121_ctrlram[0][0x03],K007121_ctrlram[0][0x04],K007121_ctrlram[0][0x05],K007121_ctrlram[0][0x06],K007121_ctrlram[0][0x07],
	K007121_ctrlram[1][0x00],K007121_ctrlram[1][0x01],K007121_ctrlram[1][0x02],K007121_ctrlram[1][0x03],K007121_ctrlram[1][0x04],K007121_ctrlram[1][0x05],K007121_ctrlram[1][0x06],K007121_ctrlram[1][0x07]);
#endif
	tilemap_update( ALL_TILEMAPS );

	palette_init_used_colors();
	K007121_mark_sprites_colors(0,&k007121_ram[0x1000],0,0);
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	/* set scroll registers */
	tilemap_set_scrollx(k007121_tilemap[0],0,K007121_ctrlram[0][0x00] - 40);
	tilemap_set_scrolly(k007121_tilemap[0],0,K007121_ctrlram[0][0x02]);

	/* draw the graphics */
	tilemap_draw(bitmap,k007121_tilemap[0],0);
	K007121_sprites_draw(0,bitmap,&k007121_ram[0x1000],0,40,0,-1);
	tilemap_draw(bitmap,k007121_tilemap[1],0);
}
