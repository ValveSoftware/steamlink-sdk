#include "driver.h"
#include "sndhrdw/m72.h"



unsigned char *m72_videoram1,*m72_videoram2,*majtitle_rowscrollram;
static unsigned char *m72_spriteram;
static int rastersplit;
static int splitline;
static struct tilemap *fg_tilemap,*bg_tilemap;
static int xadjust;
static int scrollx1[256],scrolly1[256],scrollx2[256],scrolly2[256];
extern unsigned char *spriteram,*spriteram_2;
extern size_t spriteram_size;

static int irq1,irq2;

void m72_init_machine(void)
{
	irq1 = 0x20;
	irq2 = 0x22;

	m72_init_sound();
}

void xmultipl_init_machine(void)
{
	irq1 = 0x08;
	irq2 = 0x0a;
	m72_init_sound();
}

void poundfor_init_machine(void)
{
	irq1 = 0x18;
	irq2 = 0x1a;
	m72_init_sound();
}

int m72_interrupt(void)
{
	int line = 15 - cpu_getiloops();

	if (line == 15)	/* vblank */
	{
		rastersplit = 0;
		interrupt_vector_w(0,irq1);
		return interrupt();
	}
	else
	{
		if (line != ((splitline - 128)>>4))
			return ignore_interrupt();

		rastersplit = splitline - 128 + 1;

		/* this is used to do a raster effect and show the score display at
		   the bottom of the screen or other things. The line where the
		   interrupt happens is programmable (and the interrupt can be triggered
		   multiple times, by changing the interrupt line register in the
		   interrupt handler).
		 */
		interrupt_vector_w(0,irq2);
		return interrupt();
	}
}



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void m72_get_bg_tile_info(int tile_index)
{
	unsigned char attr = m72_videoram2[4*tile_index+1];
	SET_TILE_INFO(2,m72_videoram2[4*tile_index] + ((attr & 0x3f) << 8),m72_videoram2[4*tile_index+2] & 0x0f)
	tile_info.flags = TILE_FLIPYX((attr & 0xc0) >> 6);
}

static void m72_get_fg_tile_info(int tile_index)
{
	unsigned char attr = m72_videoram1[4*tile_index+1];
	SET_TILE_INFO(1,m72_videoram1[4*tile_index] + ((attr & 0x3f) << 8),m72_videoram1[4*tile_index+2] & 0x0f)
/* bchopper: (videoram[4*tile_index+2] & 0x10) is used, priority? */
	tile_info.flags = TILE_FLIPYX((attr & 0xc0) >> 6);

	tile_info.priority = (m72_videoram1[4*tile_index+2] & 0x80) >> 7;
}

static void dbreed_get_bg_tile_info(int tile_index)
{
	unsigned char attr = m72_videoram2[4*tile_index+1];
	SET_TILE_INFO(2,m72_videoram2[4*tile_index] + ((attr & 0x3f) << 8),m72_videoram2[4*tile_index+2] & 0x0f)
	tile_info.flags = TILE_FLIPYX((attr & 0xc0) >> 6);

	/* this seems to apply only to Dragon Breed, it breaks R-Type and Gallop */
	tile_info.priority = (m72_videoram2[4*tile_index+2] & 0x80) >> 7;
}

static void rtype2_get_bg_tile_info(int tile_index)
{
	unsigned char attr = m72_videoram2[4*tile_index+2];
	SET_TILE_INFO(1,m72_videoram2[4*tile_index] + (m72_videoram2[4*tile_index+1] << 8),attr & 0x0f)
	tile_info.flags = TILE_FLIPYX((attr & 0x60) >> 5);
}

static void rtype2_get_fg_tile_info(int tile_index)
{
	unsigned char attr = m72_videoram1[4*tile_index+2];
	SET_TILE_INFO(1,m72_videoram1[4*tile_index] + (m72_videoram1[4*tile_index+1] << 8),attr & 0x0f)
	tile_info.flags = TILE_FLIPYX((attr & 0x60) >> 5);

	tile_info.priority = m72_videoram1[4*tile_index+3] & 0x01;

/* TODO: this is used on the continue screen by rtype2. Maybe it selects split tilemap */
/* like in M92 (top 8 pens appear over sprites), however if it is only used in that */
/* place there's no need to support it, it's just a black screen... */
	tile_info.priority |= (m72_videoram1[4*tile_index+2] & 0x80) >> 7;

/* (videoram[tile_index+2] & 0x10) is used by majtitle on the green, but it's not clear for what */
/* (videoram[tile_index+3] & 0xfe) are used as well */
}

static void majtitle_get_bg_tile_info(int tile_index)
{
	unsigned char attr = m72_videoram2[4*tile_index+2];
	SET_TILE_INFO(1,m72_videoram2[4*tile_index] + (m72_videoram2[4*tile_index+1] << 8),attr & 0x0f)
	tile_info.flags = TILE_FLIPYX((attr & 0x60) >> 5);
/* (videoram[4*tile_index+2] & 0x10) is used, but it's not clear for what (priority?) */
}

INLINE void hharry_get_tile_info(int gfxnum,unsigned char *videoram,int tile_index)
{
	unsigned char attr = videoram[4*tile_index+1];
	SET_TILE_INFO(gfxnum,videoram[4*tile_index] + ((attr & 0x3f) << 8),videoram[4*tile_index+2] & 0x0f)
	tile_info.flags = TILE_FLIPYX((attr & 0xc0) >> 6);
/* (videoram[4*tile_index+2] & 0x10) is used, but it's not clear for what (priority?) */
}

static void hharry_get_bg_tile_info(int tile_index)
{
	hharry_get_tile_info(1,m72_videoram2,tile_index);
}

static void hharry_get_fg_tile_info(int tile_index)
{
	hharry_get_tile_info(1,m72_videoram1,tile_index);
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int m72_vh_start(void)
{
	int i;


	bg_tilemap = tilemap_create(m72_get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,     8,8,64,64);
	fg_tilemap = tilemap_create(m72_get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);

	m72_spriteram = (unsigned char*)malloc(spriteram_size);

	if (!fg_tilemap || !bg_tilemap || !m72_spriteram)
		return 1;

	fg_tilemap->transparent_pen = 0;

	memset(m72_spriteram,0,spriteram_size);

	xadjust = 0;

	/* improves bad gfx in nspirit (but this is not a complete fix, maybe there's a */
	/* layer enable register */
	for (i = 0;i < Machine->drv->total_colors;i++)
		palette_change_color(i,0,0,0);

	return 0;
}

int dbreed_vh_start(void)
{
	bg_tilemap = tilemap_create(dbreed_get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,     8,8,64,64);
	fg_tilemap = tilemap_create(m72_get_fg_tile_info,   tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);

	m72_spriteram = (unsigned char*)malloc(spriteram_size);

	if (!fg_tilemap || !bg_tilemap || !m72_spriteram)
		return 1;

	fg_tilemap->transparent_pen = 0;

	memset(m72_spriteram,0,spriteram_size);

	xadjust = 0;

	return 0;
}

int rtype2_vh_start(void)
{
	bg_tilemap = tilemap_create(rtype2_get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,     8,8,64,64);
	fg_tilemap = tilemap_create(rtype2_get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);

	m72_spriteram = (unsigned char*)malloc(spriteram_size);

	if (!fg_tilemap || !bg_tilemap || !m72_spriteram)
		return 1;

	fg_tilemap->transparent_pen = 0;

	memset(m72_spriteram,0,spriteram_size);

	xadjust = -4;

	return 0;
}

/* Major Title has a larger background RAM, and rowscroll */
int majtitle_vh_start(void)
{
// tilemap can be 256x64, but seems to be used at 128x64 (scroll wraparound) */
//	bg_tilemap = tilemap_create(majtitle_get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,     8,8,256,64);
	bg_tilemap = tilemap_create(majtitle_get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,     8,8,128,64);
	fg_tilemap = tilemap_create(rtype2_get_fg_tile_info,  tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);

	m72_spriteram = (unsigned char*)malloc(spriteram_size);

	if (!fg_tilemap || !bg_tilemap || !m72_spriteram)
		return 1;

	fg_tilemap->transparent_pen = 0;

	memset(m72_spriteram,0,spriteram_size);

	xadjust = -4;

	return 0;
}

int hharry_vh_start(void)
{
	bg_tilemap = tilemap_create(hharry_get_bg_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,     8,8,64,64);
	fg_tilemap = tilemap_create(hharry_get_fg_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);

	m72_spriteram = (unsigned char*)malloc(spriteram_size);

	if (!fg_tilemap || !bg_tilemap || !m72_spriteram)
		return 1;

	fg_tilemap->transparent_pen = 0;

	memset(m72_spriteram,0,spriteram_size);

	xadjust = -4;

	return 0;
}

void m72_vh_stop(void)
{
	free(m72_spriteram);
	m72_spriteram = 0;
}



/***************************************************************************

  Memory handlers

***************************************************************************/

READ_HANDLER( m72_palette1_r )
{
	return paletteram[offset];
}

READ_HANDLER( m72_palette2_r )
{
	return paletteram_2[offset];
}

INLINE void changecolor(int color,int r,int g,int b)
{
	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	palette_change_color(color,r,g,b);
}

WRITE_HANDLER( m72_palette1_w )
{
	paletteram[offset] = data;
	if (offset & 1) return;
	offset &= 0x3ff;
	changecolor(offset / 2,
			paletteram[offset + 0x000],
			paletteram[offset + 0x400],
			paletteram[offset + 0x800]);
}

WRITE_HANDLER( m72_palette2_w )
{
	paletteram_2[offset] = data;
	if (offset & 1) return;
	offset &= 0x3ff;
	changecolor(offset / 2 + 512,
			paletteram_2[offset + 0x000],
			paletteram_2[offset + 0x400],
			paletteram_2[offset + 0x800]);
}

READ_HANDLER( m72_videoram1_r )
{
	return m72_videoram1[offset];
}

READ_HANDLER( m72_videoram2_r )
{
	return m72_videoram2[offset];
}

WRITE_HANDLER( m72_videoram1_w )
{
	if (m72_videoram1[offset] != data)
	{
		m72_videoram1[offset] = data;
		tilemap_mark_tile_dirty(fg_tilemap,offset/4);
	}
}

WRITE_HANDLER( m72_videoram2_w )
{
	if (m72_videoram2[offset] != data)
	{
		m72_videoram2[offset] = data;
		tilemap_mark_tile_dirty(bg_tilemap,offset/4);
	}
}

WRITE_HANDLER( majtitle_videoram2_w )
{
	if (m72_videoram2[offset] != data)
	{
		m72_videoram2[offset] = data;
//		tilemap_mark_tile_dirty(bg_tilemap,offset/4);
// tilemap can be 256x64, but seems to be used at 128x64 (scroll wraparound) */
if ((offset/4)%256 < 128)
		tilemap_mark_tile_dirty(bg_tilemap,offset/4);
	}
}

WRITE_HANDLER( m72_irq_line_w )
{
	offset *= 8;
	splitline = (splitline & (0xff00 >> offset)) | (data << offset);
}

WRITE_HANDLER( m72_scrollx1_w )
{
	int i;

	offset *= 8;
	scrollx1[rastersplit] = (scrollx1[rastersplit] & (0xff00 >> offset)) | (data << offset);

	for (i = rastersplit+1;i < 256;i++)
		scrollx1[i] = scrollx1[rastersplit];
}

WRITE_HANDLER( m72_scrollx2_w )
{
	int i;

	offset *= 8;
	scrollx2[rastersplit] = (scrollx2[rastersplit] & (0xff00 >> offset)) | (data << offset);

	for (i = rastersplit+1;i < 256;i++)
		scrollx2[i] = scrollx2[rastersplit];
}

WRITE_HANDLER( m72_scrolly1_w )
{
	int i;

	offset *= 8;
	scrolly1[rastersplit] = (scrolly1[rastersplit] & (0xff00 >> offset)) | (data << offset);

	for (i = rastersplit+1;i < 256;i++)
		scrolly1[i] = scrolly1[rastersplit];
}

WRITE_HANDLER( m72_scrolly2_w )
{
	int i;

	offset *= 8;
	scrolly2[rastersplit] = (scrolly2[rastersplit] & (0xff00 >> offset)) | (data << offset);

	for (i = rastersplit+1;i < 256;i++)
		scrolly2[i] = scrolly2[rastersplit];
}

WRITE_HANDLER( m72_spritectrl_w )
{
//logerror("%04x: write %02x to sprite ctrl+%d\n",cpu_get_pc(),data,offset);
	/* TODO: this is ok for R-Type, but might be wrong for others */
	if (offset == 1)
	{
		memcpy(m72_spriteram,spriteram,spriteram_size);
		if (data & 0x40) memset(spriteram,0,spriteram_size);
		/* bit 7 is used by bchopper, nspirit, imgfight, loht, gallop - meaning unknown */
		/* rtype2 uses bits 4,5,6 and 7 - of course it could be a different chip */
	}
}

WRITE_HANDLER( hharry_spritectrl_w )
{
//logerror("%04x: write %02x to sprite ctrl+%d\n",cpu_get_pc(),data,offset);
	if (offset == 0)
	{
		memcpy(m72_spriteram,spriteram,spriteram_size);
		memset(spriteram,0,spriteram_size);
	}
}

WRITE_HANDLER( hharryu_spritectrl_w )
{
//logerror("%04x: write %02x to sprite ctrl+%d\n",cpu_get_pc(),data,offset);
	if (offset == 1)
	{
		memcpy(m72_spriteram,spriteram,spriteram_size);
		if (data & 0x80) memset(spriteram,0,spriteram_size);
		/* hharryu uses bits 2,3,4,5,6 and 7 - of course it could be a different chip */
		/* majtitle uses bits 2,3,5,6 and 7 - of course it could be a different chip */
	}
}



/***************************************************************************

  Display refresh

***************************************************************************/

static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;

	for (offs = 0;offs < spriteram_size;offs += 8)
	{
		int code,color,sx,sy,flipx,flipy,w,h,x,y;


		code = m72_spriteram[offs+2] | (m72_spriteram[offs+3] << 8);
		color = m72_spriteram[offs+4] & 0x0f;
		sx = -256+(m72_spriteram[offs+6] | ((m72_spriteram[offs+7] & 0x03) << 8));
		sy = 512-(m72_spriteram[offs+0] | ((m72_spriteram[offs+1] & 0x01) << 8));
		flipx = m72_spriteram[offs+5] & 0x08;
		flipy = m72_spriteram[offs+5] & 0x04;

		w = 1 << ((m72_spriteram[offs+5] & 0xc0) >> 6);
		h = 1 << ((m72_spriteram[offs+5] & 0x30) >> 4);
		sy -= 16 * h;

		for (x = 0;x < w;x++)
		{
			for (y = 0;y < h;y++)
			{
				int c = code;

				if (flipx) c += 8*(w-1-x);
				else c += 8*x;
				if (flipy) c += h-1-y;
				else c += y;

				drawgfx(bitmap,Machine->gfx[0],
						c,
						color,
						flipx,flipy,
						sx + 16*x,sy + 16*y,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
		}
	}
}

static void majtitle_draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;

	for (offs = 0;offs < spriteram_size;offs += 8)
	{
		int code,color,sx,sy,flipx,flipy,w,h,x,y;


		code = spriteram_2[offs+2] | (spriteram_2[offs+3] << 8);
		color = spriteram_2[offs+4] & 0x0f;
		sx = -256+(spriteram_2[offs+6] | ((spriteram_2[offs+7] & 0x03) << 8));
		sy = 512-(spriteram_2[offs+0] | ((spriteram_2[offs+1] & 0x01) << 8));
		flipx = spriteram_2[offs+5] & 0x08;
		flipy = spriteram_2[offs+5] & 0x04;

		w = 1;// << ((spriteram_2[offs+5] & 0xc0) >> 6);
		h = 1 << ((spriteram_2[offs+5] & 0x30) >> 4);
		sy -= 16 * h;

		for (x = 0;x < w;x++)
		{
			for (y = 0;y < h;y++)
			{
				int c = code;

				if (flipx) c += 8*(w-1-x);
				else c += 8*x;
				if (flipy) c += h-1-y;
				else c += y;

				drawgfx(bitmap,Machine->gfx[2],
						c,
						color,
						flipx,flipy,
						sx + 16*x,sy + 16*y,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
		}
	}
}

static void mark_sprite_colors(unsigned char *ram)
{
	int offs,color,i;
	int colmask[32];
	int pal_base;


	pal_base = Machine->drv->gfxdecodeinfo[0].color_codes_start;

	for (color = 0;color < 32;color++) colmask[color] = 0;

	for (offs = 0;offs < spriteram_size;offs += 8)
	{
		color = ram[offs+4] & 0x0f;
		colmask[color] |= 0xffff;
	}

	for (color = 0;color < 32;color++)
	{
		for (i = 1;i < 16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] |= PALETTE_COLOR_VISIBLE;
		}
	}
}

static void draw_layer(struct osd_bitmap *bitmap,
		struct tilemap *tilemap,int *scrollx,int *scrolly,int priority)
{
	int start,i;
	/* use clip regions to split the screen */
	struct rectangle clip;

	clip.min_x = Machine->visible_area.min_x;
	clip.max_x = Machine->visible_area.max_x;
	start = Machine->visible_area.min_y - 128;
	do
	{
		i = start;
		while (scrollx[i+1] == scrollx[start] && scrolly[i+1] == scrolly[start]
				&& i < Machine->visible_area.max_y - 128)
			i++;

		clip.min_y = start + 128;
		clip.max_y = i + 128;
		tilemap_set_clip(tilemap,&clip);
		tilemap_set_scrollx(tilemap,0,scrollx[start] + xadjust);
		tilemap_set_scrolly(tilemap,0,scrolly[start]);
		tilemap_draw(bitmap,tilemap,priority);

		start = i+1;
	} while (start < Machine->visible_area.max_y - 128);
}

static void draw_bg(struct osd_bitmap *bitmap,int priority)
{
	draw_layer(bitmap,bg_tilemap,scrollx2,scrolly2,priority);
}

static void draw_fg(struct osd_bitmap *bitmap,int priority)
{
	draw_layer(bitmap,fg_tilemap,scrollx1,scrolly1,priority);
}


void m72_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_set_clip(fg_tilemap,0);
	tilemap_set_clip(bg_tilemap,0);

	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);

	palette_init_used_colors();
	mark_sprite_colors(m72_spriteram);
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	draw_bg(bitmap,0);
	draw_fg(bitmap,0);
	draw_sprites(bitmap);
	draw_fg(bitmap,1);
}

void dbreed_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	tilemap_set_clip(fg_tilemap,0);
	tilemap_set_clip(bg_tilemap,0);

	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);

	palette_init_used_colors();
	mark_sprite_colors(m72_spriteram);
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	draw_bg(bitmap,0);
	draw_fg(bitmap,0);
	draw_sprites(bitmap);
	draw_bg(bitmap,1);
	draw_fg(bitmap,1);
}

void majtitle_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;


	tilemap_set_clip(fg_tilemap,0);

	/* TODO: find how rowscroll is disabled */
	if (1)
	{
		tilemap_set_scroll_rows(bg_tilemap,512);
		for (i = 0;i < 512;i++)
			tilemap_set_scrollx(bg_tilemap,(i+scrolly2[0])&0x1ff,
					256 + majtitle_rowscrollram[2*i] + (majtitle_rowscrollram[2*i+1] << 8) + xadjust);
	}
	else
	{
		tilemap_set_scroll_rows(bg_tilemap,1);
		tilemap_set_scrollx(bg_tilemap,0,256 + scrollx2[0] + xadjust);
	}
	tilemap_set_scrolly(bg_tilemap,0,scrolly2[0]);
	tilemap_update(bg_tilemap);
	tilemap_update(fg_tilemap);

	palette_init_used_colors();
	mark_sprite_colors(m72_spriteram);
	mark_sprite_colors(spriteram_2);
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	tilemap_draw(bitmap,bg_tilemap,0);

	draw_fg(bitmap,0);
	majtitle_draw_sprites(bitmap);
	draw_sprites(bitmap);
	draw_fg(bitmap,1);
}


void m72_eof_callback(void)
{
	int i;

	for (i = 0;i < 255;i++)
	{
		scrollx1[i] = scrollx1[255];
		scrolly1[i] = scrolly1[255];
		scrollx2[i] = scrollx2[255];
		scrolly2[i] = scrolly2[255];
	}
}
