/***************************************************************************

 Pang Video Hardware

***************************************************************************/

#include "vidhrdw/generic.h"
#include "palette.h"


/* Globals */
size_t pang_videoram_size;
unsigned char *pang_videoram;
unsigned char *pang_colorram;

/* Private */
static unsigned char *pang_objram;           /* Sprite RAM */

static struct tilemap *bg_tilemap;
static int flipscreen;

/* Declarations */
void pang_vh_stop(void);



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_tile_info(int tile_index)
{
	unsigned char attr = pang_colorram[tile_index];
	int code = pang_videoram[2*tile_index] + (pang_videoram[2*tile_index+1] << 8);
	SET_TILE_INFO(0,code,attr & 0x7f)
	tile_info.flags = (attr & 0x80) ? TILE_FLIPX : 0;
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int pang_vh_start(void)
{
	pang_objram=NULL;
	paletteram=NULL;


	bg_tilemap = tilemap_create(get_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,8,8,64,32);

	if (!bg_tilemap)
		return 1;

	/*
		OBJ RAM
	*/
	pang_objram=(unsigned char*)malloc(pang_videoram_size);
	if (!pang_objram)
	{
		pang_vh_stop();
		return 1;
	}
	memset(pang_objram, 0, pang_videoram_size);

	/*
		Palette RAM
	*/
	paletteram = (unsigned char*)malloc(2*Machine->drv->total_colors);
	if (!paletteram)
	{
		pang_vh_stop();
		return 1;
	}
	memset(paletteram, 0, 2*Machine->drv->total_colors);

	palette_transparent_color = 0; /* background color (Block Block uses this on the title screen) */

	return 0;
}

void pang_vh_stop(void)
{
	free(pang_objram);
	pang_objram = 0;
	free(paletteram);
	paletteram = 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

/***************************************************************************
  OBJ / CHAR RAM HANDLERS (BANK 0 = CHAR, BANK 1=OBJ)
***************************************************************************/

static int video_bank;

WRITE_HANDLER( pang_video_bank_w )
{
	/* Bank handler (sets base pointers for video write) (doesn't apply to mgakuen) */
	video_bank = data;
}

WRITE_HANDLER( mgakuen_videoram_w )
{
	if (pang_videoram[offset] != data)
	{
		pang_videoram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset/2);
	}
}

READ_HANDLER( mgakuen_videoram_r )
{
	return pang_videoram[offset];
}

WRITE_HANDLER( mgakuen_objram_w )
{
	pang_objram[offset]=data;
}

READ_HANDLER( mgakuen_objram_r )
{
	return pang_objram[offset];
}

WRITE_HANDLER( pang_videoram_w )
{
	if (video_bank) mgakuen_objram_w(offset,data);
	else mgakuen_videoram_w(offset,data);
}

READ_HANDLER( pang_videoram_r )
{
	if (video_bank) return mgakuen_objram_r(offset);
	else return mgakuen_videoram_r(offset);
}

/***************************************************************************
  COLOUR RAM
****************************************************************************/

WRITE_HANDLER( pang_colorram_w )
{
	if (pang_colorram[offset] != data)
	{
		pang_colorram[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset);
	}
}

READ_HANDLER( pang_colorram_r )
{
	return pang_colorram[offset];
}

/***************************************************************************
  PALETTE HANDLERS (COLOURS: BANK 0 = 0x00-0x3f BANK 1=0x40-0xff)
****************************************************************************/

static int paletteram_bank;

WRITE_HANDLER( pang_gfxctrl_w )
{
//logerror("PC %04x: pang_gfxctrl_w %02x\n",cpu_get_pc(),data);
{
	char baf[40];
	sprintf(baf,"%02x",data);
//	usrintf_showmessage(baf);
}

	/* bit 0 is unknown (used, maybe back color enable?) */

	/* bit 1 is coin counter */
	coin_counter_w(0,data & 2);

	/* bit 2 is flip screen */
	if (flipscreen != (data & 0x04))
	{
		flipscreen = data & 0x04;
		tilemap_set_flip(ALL_TILEMAPS,flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
	}

	/* bit 3 is unknown (used, e.g. marukin pulses it on the title screen) */

	/* bit 4 selects OKI M6295 bank */
	OKIM6295_set_bank_base(0, ALL_VOICES, (data & 0x10) ? 0x40000 : 0x00000);

	/* bit 5 is palette RAM bank selector (doesn't apply to mgakuen) */
	paletteram_bank = data & 0x20;

	/* bits 6 and 7 are unknown, used in several places. At first I thought */
	/* they were bg and sprites enable, but this screws up spang (screen flickers */
	/* every time you pop a bubble). However, not using them as enable bits screws */
	/* up marukin - you can see partially built up screens during attract mode. */
}

WRITE_HANDLER( pang_paletteram_w )
{
	if (paletteram_bank) paletteram_xxxxRRRRGGGGBBBB_w(offset + 0x800,data);
	else paletteram_xxxxRRRRGGGGBBBB_w(offset,data);
}

READ_HANDLER( pang_paletteram_r )
{
	if (paletteram_bank) return paletteram_r(offset + 0x800);
	return paletteram_r(offset);
}

WRITE_HANDLER( mgakuen_paletteram_w )
{
	paletteram_xxxxRRRRGGGGBBBB_w(offset,data);
}

READ_HANDLER( mgakuen_paletteram_r )
{
	return paletteram_r(offset);
}



/***************************************************************************

  Display refresh

***************************************************************************/

static void mark_sprites_palette(void)
{
	int offs,color,code,attr,i;
	int colmask[16];
	int pal_base;


	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;

	for (color = 0;color < 16;color++) colmask[color] = 0;

	/* the last entry is not a sprite, we skip it otherwise spang shows a bubble */
	/* moving diagonally across the screen */
	for (offs = 0x1000-0x40;offs >= 0;offs -= 0x20)
	{
		attr = pang_objram[offs+1];
		code = pang_objram[offs] + ((attr & 0xe0) << 3);
		color = attr & 0x0f;

		colmask[color] |= Machine->gfx[1]->pen_usage[code];
	}

	for (color = 0;color < 16;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] |= PALETTE_COLOR_VISIBLE;
		}
	}
}

static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs,sx,sy;

	/* the last entry is not a sprite, we skip it otherwise spang shows a bubble */
	/* moving diagonally across the screen */
	for (offs = 0x1000-0x40;offs >= 0;offs -= 0x20)
	{
		int code = pang_objram[offs];
		int attr = pang_objram[offs+1];
		int color = attr & 0x0f;
		sx = pang_objram[offs+3] + ((attr & 0x10) << 4);
		sy = ((pang_objram[offs+2] + 8) & 0xff) - 8;
		code += (attr & 0xe0) << 3;
		if (flipscreen)
		{
			sx = 496 - sx;
			sy = 240 - sy;
		}
		drawgfx(bitmap,Machine->gfx[1],
				 code,
				 color,
				 flipscreen,flipscreen,
				 sx,sy,
				 &Machine->visible_area,TRANSPARENCY_PEN,15);
	}
}

void pang_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;

	tilemap_update(ALL_TILEMAPS);

	palette_init_used_colors();

	mark_sprites_palette();

	/* the following is required to make the colored background work */
	for (i = 15;i < Machine->drv->total_colors;i += 16)
		palette_used_colors[i] = PALETTE_COLOR_TRANSPARENT;

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap,bg_tilemap,0);

	draw_sprites(bitmap);
}
