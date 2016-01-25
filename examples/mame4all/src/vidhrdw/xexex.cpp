#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"


static int sprite_colorbase;


/***************************************************************************

  Callbacks for the K053247

***************************************************************************/

static void sprite_callback(int *code,int *color,int *priority_mask)
{
	*color = sprite_colorbase + (*color & 0x001f);
}



static struct tilemap *K053157_char01_tilemap, *K053157_char11_tilemap;
static int K053157_rambank, K053157_cur_rambank, K053157_rombank, K053157_cur_rombank, K053157_romnbbanks;
static unsigned char *K053157_rambase, *K053157_cur_rambase, *K053157_rombase;
static void (*K053157_cur_notifier)(int);

static void K053157_get_char01_tile_info(int tile_index)
{
	int attr = READ_WORD(K053157_rambase+4*tile_index+0x2000);
	int code = READ_WORD(K053157_rambase+4*tile_index+0x2000+2);

	SET_TILE_INFO (0, code, ((attr + 0x300) & 0x7f0)>>4);
	tile_info.flags = TILE_FLIPYX(attr & 3);

}

static void K053157_get_char11_tile_info(int tile_index)
{
	int attr = READ_WORD(K053157_rambase+4*tile_index+0xa000);
	int code = READ_WORD(K053157_rambase+4*tile_index+0xa000+2);

	SET_TILE_INFO (0, code, ((attr + 0x700) & 0x7f0)>>4);
	tile_info.flags = TILE_FLIPYX(attr & 3);
}

static void K053157_char01_m(int offset)
{
	tilemap_mark_tile_dirty(K053157_char01_tilemap,offset/4);
}

static void K053157_char11_m(int offset)
{
	tilemap_mark_tile_dirty(K053157_char11_tilemap,offset/4);
}

static void (*K053157_modify_notifiers[8])(int) = {
	0,					// 00
	K053157_char01_m,	// 01
	0,					// 08
	0,					// 09
	0,					// 10
	K053157_char11_m,	// 11
	0,					// 18
	0					// 19
};

int K053157_vh_start(int rambank, int rombank, int roms_memory_region)
{
	K053157_char01_tilemap = tilemap_create(K053157_get_char01_tile_info,tilemap_scan_rows,
											TILEMAP_OPAQUE,8,8,64,32);

	K053157_char11_tilemap = tilemap_create(K053157_get_char11_tile_info,tilemap_scan_rows,
											TILEMAP_TRANSPARENT,8,8,64,32);

	if(!K053157_char01_tilemap || !K053157_char11_tilemap)
		return 1;

	K053157_char11_tilemap->transparent_pen = 0;

	K053157_rambank = rambank;
	K053157_cur_rambank = 0;
	K053157_rambase = (unsigned char*)malloc(0x2000*8);
	K053157_cur_rambase = K053157_rambase;
	K053157_cur_notifier = K053157_modify_notifiers[0];
	cpu_setbank(K053157_rambank, K053157_cur_rambase);

	K053157_rombank = rombank;
	K053157_cur_rombank = 0;
	K053157_rombase = memory_region(roms_memory_region);
	K053157_romnbbanks = memory_region_length(roms_memory_region)/0x2000;
	cpu_setbank(K053157_rombank, K053157_rombase);

	return 0;
}

WRITE_HANDLER( K053157_ram_w )
{
	unsigned char *adr = K053157_cur_rambase + offset;
	int old = READ_WORD(adr);
	COMBINE_WORD_MEM(adr, data);

	if(K053157_cur_notifier && (READ_WORD(adr) != old))
		K053157_cur_notifier(offset);
}

READ_HANDLER( K053157_r )
{
	//logerror("K053157: unhandled read(%02x), pc=%08x\n", offset, cpu_get_pc());
	return 0;
}

WRITE_HANDLER( K053157_w )
{
	switch(offset) {
	case 0x32: {
		int nb;
		data &= 0xff;
		switch(data) {
		case 0:
			nb = 0;
			break;
		case 1:
			nb = 1;
			break;
		case 8:
			nb = 2;
			break;
		case 9:
			nb = 3;
			break;
		case 0x10:
			nb = 4;
			break;
		case 0x11:
			nb = 5;
			break;
		case 0x18:
			nb = 6;
			break;
		case 0x19:
			nb = 7;
			break;
		default:
			nb = 0;
			//logerror("Graphic bankswitching to unknown bank %02x (pc=%08x)\n", data, cpu_get_pc());
			break;
		}

		K053157_cur_rambank = data;
		K053157_cur_rambase = K053157_rambase + nb*0x2000;
		K053157_cur_notifier = K053157_modify_notifiers[nb];

		cpu_setbank(K053157_rambank, K053157_cur_rambase);
		break;
	}
	case 0x34: {
		K053157_cur_rombank = data % K053157_romnbbanks;
		cpu_setbank(K053157_rombank, K053157_rombase + 0x2000*K053157_cur_rombank);
		break;
	}
	default:
		//logerror("K053157: unhandled write(%02x, %04x), pc=%08x\n", offset, data & 0xffff, cpu_get_pc());
		break;
	}
}

void K053157_update(void)
{
	tilemap_update(K053157_char01_tilemap);
	tilemap_update(K053157_char11_tilemap);
}

void K053157_render(void)
{
	tilemap_render(K053157_char01_tilemap);
	tilemap_render(K053157_char11_tilemap);
}

void K053157_draw(struct osd_bitmap *bitmap)
{
	tilemap_draw(bitmap, K053157_char01_tilemap, 0);
	tilemap_draw(bitmap, K053157_char11_tilemap, 0);
}


extern unsigned char *xexex_palette_ram;

int xexex_vh_start(void)
{
	K053157_vh_start(2, 6, REGION_GFX1);
	if (K053247_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,sprite_callback))
	{
//		K053157_vh_stop();
		return 1;
	}
	return 0;
}

void xexex_vh_stop(void)
{
//	K053157_vh_stop();
	K053247_vh_stop();
}


WRITE_HANDLER( xexex_palette_w )
{
	int r, g, b;
	int data0, data1;

	COMBINE_WORD_MEM(xexex_palette_ram+offset, data);

	offset &= ~3;

	data0 = READ_WORD(xexex_palette_ram + offset);
	data1 = READ_WORD(xexex_palette_ram + offset + 2);

	r = data0 & 0xff;
	g = data1 >> 8;
	b = data1 & 0xff;

	palette_change_color(offset>>2, r, g, b);
}


void xexex_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	K053157_update();

	palette_init_used_colors();
	K053247_mark_sprites_colors();
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	K053157_render();
	K053157_draw(bitmap);
	K053247_sprites_draw(bitmap);
}
