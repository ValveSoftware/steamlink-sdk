#include "driver.h"
#include "vidhrdw/generic.h"
#include "konamiic.h"


unsigned char *tail2nos_bgvideoram;


static struct tilemap *bg_tilemap;

static int charbank,charpalette,video_enable;
static unsigned char *zoomdata;
static int dirtygfx;
static unsigned char *dirtychar;

#define TOTAL_CHARS 0x400


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void get_tile_info(int tile_index)
{
	UINT16 code = READ_WORD(&tail2nos_bgvideoram[2*tile_index]);
	SET_TILE_INFO(0,(code & 0x1fff) + (charbank << 13),((code & 0xe000) >> 13) + charpalette * 16)
}


/***************************************************************************

  Callbacks for the K051316

***************************************************************************/

static void zoom_callback(int *code,int *color)
{
	*code |= ((*color & 0x03) << 8);
	*color = 32 + ((*color & 0x38) >> 3);
}

/***************************************************************************

	Start the video hardware emulation.

***************************************************************************/

int tail2nos_vh_start(void)
{
	bg_tilemap = tilemap_create(get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);

	if (!bg_tilemap)
		return 1;

	if (K051316_vh_start_0(REGION_GFX3,4,zoom_callback))
		return 1;

	if (!(dirtychar = (unsigned char*)malloc(TOTAL_CHARS)))
	{
		K051316_vh_stop_0();
		return 1;
	}
	memset(dirtychar,1,TOTAL_CHARS);

	bg_tilemap->transparent_pen = 15;

	K051316_wraparound_enable(0,1);
	K051316_set_offset(0,-89,-14);
	zoomdata = memory_region(REGION_GFX3);

	return 0;
}

void tail2nos_vh_stop(void)
{
	K051316_vh_stop_0();
	free(dirtychar);
	dirtychar = 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

READ_HANDLER( tail2nos_bgvideoram_r )
{
	return READ_WORD(&tail2nos_bgvideoram[offset]);
}

WRITE_HANDLER( tail2nos_bgvideoram_w )
{
	int oldword = READ_WORD(&tail2nos_bgvideoram[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		WRITE_WORD(&tail2nos_bgvideoram[offset],newword);
		tilemap_mark_tile_dirty(bg_tilemap,offset/2);
	}
}

READ_HANDLER( tail2nos_zoomdata_r )
{
	return READ_WORD(&zoomdata[offset]);
}

WRITE_HANDLER( tail2nos_zoomdata_w )
{
	int oldword = READ_WORD(&zoomdata[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword)
	{
		dirtygfx = 1;
		dirtychar[offset / 128] = 1;
		WRITE_WORD(&zoomdata[offset],newword);
	}
}

WRITE_HANDLER( tail2nos_gfxbank_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		int bank;

		/* bits 0 and 2 select char bank */
		if (data & 0x04) bank = 2;
		else if (data & 0x01) bank = 1;
		else bank = 0;

		if (charbank != bank)
		{
			charbank = bank;
			tilemap_mark_all_tiles_dirty(bg_tilemap);
		}

		/* bit 5 seems to select palette bank (used on startup) */
		if (data & 0x20) bank = 7;
		else bank = 3;

		if (charpalette != bank)
		{
			charpalette = bank;
			tilemap_mark_all_tiles_dirty(bg_tilemap);
		}

		/* bit 4 seems to be video enable */
		video_enable = data & 0x10;
	}
}


/***************************************************************************

	Display Refresh

***************************************************************************/

static void drawsprites(struct osd_bitmap *bitmap)
{
	int offs;


	for (offs = 0;offs < spriteram_size;offs += 8)
	{
		int sx,sy,flipx,flipy,code,color;

		sx = READ_WORD(&spriteram[offs + 2]);
		if (sx >= 0x8000) sx -= 0x10000;
		sy = 0x10000 - READ_WORD(&spriteram[offs + 0]);
		if (sy >= 0x8000) sy -= 0x10000;
		code = READ_WORD(&spriteram[offs + 4]) & 0x07ff;
		color = (READ_WORD(&spriteram[offs + 4]) & 0xe000) >> 13;
		flipx = READ_WORD(&spriteram[offs + 4]) & 0x1000;
		flipy = READ_WORD(&spriteram[offs + 4]) & 0x0800;

		drawgfx(bitmap,Machine->gfx[1],
				code,
				40 + color,
				flipx,flipy,
				sx+3,sy+1,	/* placement relative to zoom layer verified on the real thing */
				&Machine->visible_area,TRANSPARENCY_PEN,15);
	}
}

void tail2nos_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	static struct GfxLayout tilelayout =
	{
		16,16,
		TOTAL_CHARS,
		4,
		{ 0, 1, 2, 3 },
#ifdef LSB_FIRST
		{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4,
				10*4, 11*4, 8*4, 9*4, 14*4, 15*4, 12*4, 13*4 },
#else
		{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4,
				8*4, 9*4, 10*4, 11*4, 12*4, 13*4, 14*4, 15*4 },
#endif
		{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
				8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
		128*8
	};


	if (dirtygfx)
	{
		int i;

		dirtygfx = 0;

		for (i = 0;i < TOTAL_CHARS;i++)
		{
			if (dirtychar[i])
			{
				dirtychar[i] = 0;
				decodechar(Machine->gfx[2],i,zoomdata,&tilelayout);
			}
		}

		tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);
	}


	K051316_tilemap_update_0();
	tilemap_update(bg_tilemap);

	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	if (video_enable)
	{
		K051316_zoom_draw_0(bitmap,0);
		drawsprites(bitmap);
		tilemap_draw(bitmap,bg_tilemap,0);
	}
	else
		fillbitmap(bitmap,Machine->pens[0],&Machine->visible_area);
}
