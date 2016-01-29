#include "driver.h"
#include "vidhrdw/konamiic.h"


static int layer_colorbase[3],sprite_colorbase,bg_colorbase;
static int priorityflag;
static int layerpri[3];



/***************************************************************************

  Callbacks for the K052109

***************************************************************************/

/* Missing in Action */

static void mia_tile_callback(int layer,int bank,int *code,int *color)
{
	tile_info.flags = (*color & 0x04) ? TILE_FLIPX : 0;
	if (layer == 0)
	{
		*code |= ((*color & 0x01) << 8);
		*color = layer_colorbase[layer] + ((*color & 0x80) >> 5) + ((*color & 0x10) >> 1);
	}
	else
	{
		*code |= ((*color & 0x01) << 8) | ((*color & 0x18) << 6) | (bank << 11);
		*color = layer_colorbase[layer] + ((*color & 0xe0) >> 5);
	}
}

static void tmnt_tile_callback(int layer,int bank,int *code,int *color)
{
	*code |= ((*color & 0x03) << 8) | ((*color & 0x10) << 6) | ((*color & 0x0c) << 9)
			| (bank << 13);
	*color = layer_colorbase[layer] + ((*color & 0xe0) >> 5);
}

static int detatwin_rombank;

static void detatwin_tile_callback(int layer,int bank,int *code,int *color)
{
	/* (color & 0x02) is flip y handled internally by the 052109 */
	*code |= ((*color & 0x01) << 8) | ((*color & 0x10) << 5) | ((*color & 0x0c) << 8)
			| (bank << 12) | detatwin_rombank << 14;
	*color = layer_colorbase[layer] + ((*color & 0xe0) >> 5);
}



/***************************************************************************

  Callbacks for the K051960

***************************************************************************/

static void mia_sprite_callback(int *code,int *color,int *priority,int *shadow)
{
	*color = sprite_colorbase + (*color & 0x0f);
}

static void tmnt_sprite_callback(int *code,int *color,int *priority,int *shadow)
{
	*code |= (*color & 0x10) << 9;
	*color = sprite_colorbase + (*color & 0x0f);
}

static void punkshot_sprite_callback(int *code,int *color,int *priority_mask,int *shadow)
{
	int pri = 0x20 | ((*color & 0x60) >> 2);
	if (pri <= layerpri[2])								*priority_mask = 0;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority_mask = 0xf0;
	else if (pri > layerpri[1] && pri <= layerpri[0])	*priority_mask = 0xf0|0xcc;
	else 												*priority_mask = 0xf0|0xcc|0xaa;

	*code |= (*color & 0x10) << 9;
	*color = sprite_colorbase + (*color & 0x0f);
}

static void thndrx2_sprite_callback(int *code,int *color,int *priority_mask,int *shadow)
{
	int pri = 0x20 | ((*color & 0x60) >> 2);
	if (pri <= layerpri[2])								*priority_mask = 0;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority_mask = 0xf0;
	else if (pri > layerpri[1] && pri <= layerpri[0])	*priority_mask = 0xf0|0xcc;
	else 												*priority_mask = 0xf0|0xcc|0xaa;

	*color = sprite_colorbase + (*color & 0x0f);
}


/***************************************************************************

  Callbacks for the K053245

***************************************************************************/

static void lgtnfght_sprite_callback(int *code,int *color,int *priority_mask)
{
	int pri = 0x20 | ((*color & 0x60) >> 2);
	if (pri <= layerpri[2])								*priority_mask = 0;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority_mask = 0xf0;
	else if (pri > layerpri[1] && pri <= layerpri[0])	*priority_mask = 0xf0|0xcc;
	else 												*priority_mask = 0xf0|0xcc|0xaa;

	*color = sprite_colorbase + (*color & 0x1f);
}

static void detatwin_sprite_callback(int *code,int *color,int *priority_mask)
{
#if 0
if (keyboard_pressed(KEYCODE_Q) && (*color & 0x20)) *color = rand();
if (keyboard_pressed(KEYCODE_W) && (*color & 0x40)) *color = rand();
if (keyboard_pressed(KEYCODE_E) && (*color & 0x80)) *color = rand();
#endif
	int pri = 0x20 | ((*color & 0x60) >> 2);
	if (pri <= layerpri[2])								*priority_mask = 0;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority_mask = 0xf0;
	else if (pri > layerpri[1] && pri <= layerpri[0])	*priority_mask = 0xf0|0xcc;
	else 												*priority_mask = 0xf0|0xcc|0xaa;

	*color = sprite_colorbase + (*color & 0x1f);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

int mia_vh_start(void)
{
	layer_colorbase[0] = 0;
	layer_colorbase[1] = 32;
	layer_colorbase[2] = 40;
	sprite_colorbase = 16;
	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,mia_tile_callback))
		return 1;
	if (K051960_vh_start(REGION_GFX2,REVERSE_PLANE_ORDER,mia_sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}
	return 0;
}

int tmnt_vh_start(void)
{
	layer_colorbase[0] = 0;
	layer_colorbase[1] = 32;
	layer_colorbase[2] = 40;
	sprite_colorbase = 16;
	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,tmnt_tile_callback))
		return 1;
	if (K051960_vh_start(REGION_GFX2,REVERSE_PLANE_ORDER,tmnt_sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}
	return 0;
}

int punkshot_vh_start(void)
{
	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,tmnt_tile_callback))
		return 1;
	if (K051960_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,punkshot_sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}
	return 0;
}

int lgtnfght_vh_start(void)	/* also tmnt2, ssriders */
{
	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,tmnt_tile_callback))
		return 1;
	if (K053245_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,lgtnfght_sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}
	return 0;
}

int detatwin_vh_start(void)
{
	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,detatwin_tile_callback))
		return 1;
	if (K053245_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,detatwin_sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}
	return 0;
}

int glfgreat_vh_start(void)
{
	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,tmnt_tile_callback))
		return 1;
	if (K053245_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,lgtnfght_sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}
	return 0;
}

int thndrx2_vh_start(void)
{
	if (K052109_vh_start(REGION_GFX1,NORMAL_PLANE_ORDER,tmnt_tile_callback))
		return 1;
	if (K051960_vh_start(REGION_GFX2,NORMAL_PLANE_ORDER,thndrx2_sprite_callback))
	{
		K052109_vh_stop();
		return 1;
	}
	return 0;
}

void punkshot_vh_stop(void)
{
	K052109_vh_stop();
	K051960_vh_stop();
}

void lgtnfght_vh_stop(void)
{
	K052109_vh_stop();
	K053245_vh_stop();
}

void detatwin_vh_stop(void)
{
	K052109_vh_stop();
	K053245_vh_stop();
}

void glfgreat_vh_stop(void)
{
	K052109_vh_stop();
	K053245_vh_stop();
}

void thndrx2_vh_stop(void)
{
	K052109_vh_stop();
	K051960_vh_stop();
}



/***************************************************************************

  Memory handlers

***************************************************************************/

WRITE_HANDLER( tmnt_paletteram_w )
{
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	WRITE_WORD(&paletteram[offset],newword);

	offset /= 4;
	{
		int palette = ((READ_WORD(&paletteram[offset * 4]) & 0x00ff) << 8)
				+ (READ_WORD(&paletteram[offset * 4 + 2]) & 0x00ff);
		int r = palette & 31;
		int g = (palette >> 5) & 31;
		int b = (palette >> 10) & 31;

		r = (r << 3) + (r >> 2);
		g = (g << 3) + (g >> 2);
		b = (b << 3) + (b >> 2);

		palette_change_color (offset,r,g,b);
	}
}



WRITE_HANDLER( tmnt_0a0000_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		static int last;

		/* bit 0/1 = coin counters */
		coin_counter_w(0,data & 0x01);
		coin_counter_w(1,data & 0x02);	/* 2 players version */

		/* bit 3 high then low triggers irq on sound CPU */
		if (last == 0x08 && (data & 0x08) == 0)
			cpu_cause_interrupt(1,0xff);

		last = data & 0x08;

		/* bit 5 = irq enable */
		interrupt_enable_w(0,data & 0x20);

		/* bit 7 = enable char ROM reading through the video RAM */
		K052109_set_RMRD_line((data & 0x80) ? ASSERT_LINE : CLEAR_LINE);

		/* other bits unused */
	}
}

WRITE_HANDLER( punkshot_0a0020_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		static int last;


		/* bit 0 = coin counter */
		coin_counter_w(0,data & 0x01);

		/* bit 2 = trigger irq on sound CPU */
		if (last == 0x04 && (data & 0x04) == 0)
			cpu_cause_interrupt(1,0xff);

		last = data & 0x04;

		/* bit 3 = enable char ROM reading through the video RAM */
		K052109_set_RMRD_line((data & 0x08) ? ASSERT_LINE : CLEAR_LINE);
	}
}

WRITE_HANDLER( lgtnfght_0a0018_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		static int last;


		/* bit 0,1 = coin counter */
		coin_counter_w(0,data & 0x01);
		coin_counter_w(1,data & 0x02);

		/* bit 2 = trigger irq on sound CPU */
		if (last == 0x00 && (data & 0x04) == 0x04)
			cpu_cause_interrupt(1,0xff);

		last = data & 0x04;

		/* bit 3 = enable char ROM reading through the video RAM */
		K052109_set_RMRD_line((data & 0x08) ? ASSERT_LINE : CLEAR_LINE);
	}
}

WRITE_HANDLER( detatwin_700300_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		/* bit 0,1 = coin counter */
		coin_counter_w(0,data & 0x01);
		coin_counter_w(1,data & 0x02);

		/* bit 3 = enable char ROM reading through the video RAM */
		K052109_set_RMRD_line((data & 0x08) ? ASSERT_LINE : CLEAR_LINE);

		/* bit 7 = select char ROM bank */
		if (detatwin_rombank != ((data & 0x80) >> 7))
		{
			detatwin_rombank = (data & 0x80) >> 7;
			tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);
		}

		/* other bits unknown */
	}
}

WRITE_HANDLER( glfgreat_122000_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		/* bit 0,1 = coin counter */
		coin_counter_w(0,data & 0x01);
		coin_counter_w(1,data & 0x02);

		/* bit 4 = enable char ROM reading through the video RAM */
		K052109_set_RMRD_line((data & 0x10) ? ASSERT_LINE : CLEAR_LINE);

		/* other bits unknown */
	}
}

WRITE_HANDLER( ssriders_1c0300_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		/* bit 0,1 = coin counter */
		coin_counter_w(0,data & 0x01);
		coin_counter_w(1,data & 0x02);

		/* bit 3 = enable char ROM reading through the video RAM */
		K052109_set_RMRD_line((data & 0x08) ? ASSERT_LINE : CLEAR_LINE);

		/* other bits unknown (bits 4-6 used in TMNT2) */
	}
}



WRITE_HANDLER( tmnt_priority_w )
{
	if ((data & 0x00ff0000) == 0)
	{
		/* bit 2/3 = priority; other bits unused */
		/* bit2 = PRI bit3 = PRI2
			  sprite/playfield priority is controlled by these two bits, by bit 3
			  of the background tile color code, and by the SHADOW sprite
			  attribute bit.
			  Priorities are encoded in a PROM (G19 for TMNT). However, in TMNT,
			  the PROM only takes into account the PRI and SHADOW bits.
			  PRI  Priority
			   0   bg fg spr text
			   1   bg spr fg text
			  The SHADOW bit, when set, torns a sprite into a shadow which makes
			  color below it darker (this is done by turning off three resistors
			  in parallel with the RGB output).

			  Note: the background color (color used when all of the four layers
			  are 0) is taken from the *foreground* palette, not the background
			  one as would be more intuitive.
		*/
		priorityflag = (data & 0x0c) >> 2;
	}
}



/***************************************************************************

  Display refresh

***************************************************************************/

/* useful function to sort the three tile layers by priority order */
static void sortlayers(int *layer,int *pri)
{
#define SWAP(a,b) \
	if (pri[a] < pri[b]) \
	{ \
		int t; \
		t = pri[a]; pri[a] = pri[b]; pri[b] = t; \
		t = layer[a]; layer[a] = layer[b]; layer[b] = t; \
	}

	SWAP(0,1)
	SWAP(0,2)
	SWAP(1,2)
}

void mia_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	K052109_tilemap_update();

	palette_init_used_colors();
	K051960_mark_sprites_colors();
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	K052109_tilemap_draw(bitmap,2,TILEMAP_IGNORE_TRANSPARENCY);
	if ((priorityflag & 1) == 1) K051960_sprites_draw(bitmap,0,0);
	K052109_tilemap_draw(bitmap,1,0);
	if ((priorityflag & 1) == 0) K051960_sprites_draw(bitmap,0,0);
	K052109_tilemap_draw(bitmap,0,0);
}

void tmnt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	K052109_tilemap_update();

	palette_init_used_colors();
	K051960_mark_sprites_colors();
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	K052109_tilemap_draw(bitmap,2,TILEMAP_IGNORE_TRANSPARENCY);
	if ((priorityflag & 1) == 1) K051960_sprites_draw(bitmap,0,0);
	K052109_tilemap_draw(bitmap,1,0);
	if ((priorityflag & 1) == 0) K051960_sprites_draw(bitmap,0,0);
	K052109_tilemap_draw(bitmap,0,0);
}


void punkshot_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layer[3];


	bg_colorbase       = K053251_get_palette_index(K053251_CI0);
	sprite_colorbase   = K053251_get_palette_index(K053251_CI1);
	layer_colorbase[0] = K053251_get_palette_index(K053251_CI2);
	layer_colorbase[1] = K053251_get_palette_index(K053251_CI4);
	layer_colorbase[2] = K053251_get_palette_index(K053251_CI3);

	K052109_tilemap_update();

	palette_init_used_colors();
	K051960_mark_sprites_colors();
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	layer[0] = 0;
	layerpri[0] = K053251_get_priority(K053251_CI2);
	layer[1] = 1;
	layerpri[1] = K053251_get_priority(K053251_CI4);
	layer[2] = 2;
	layerpri[2] = K053251_get_priority(K053251_CI3);

	sortlayers(layer,layerpri);

	fillbitmap(priority_bitmap,0,NULL);
	K052109_tilemap_draw(bitmap,layer[0],TILEMAP_IGNORE_TRANSPARENCY|(1<<16));
	K052109_tilemap_draw(bitmap,layer[1],2<<16);
	K052109_tilemap_draw(bitmap,layer[2],4<<16);

	K051960_sprites_draw(bitmap,-1,-1);
}


void lgtnfght_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layer[3];


	bg_colorbase       = K053251_get_palette_index(K053251_CI0);
	sprite_colorbase   = K053251_get_palette_index(K053251_CI1);
	layer_colorbase[0] = K053251_get_palette_index(K053251_CI2);
	layer_colorbase[1] = K053251_get_palette_index(K053251_CI4);
	layer_colorbase[2] = K053251_get_palette_index(K053251_CI3);

	K052109_tilemap_update();

	palette_init_used_colors();
	K053245_mark_sprites_colors();
	palette_used_colors[16 * bg_colorbase] |= PALETTE_COLOR_VISIBLE;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	layer[0] = 0;
	layerpri[0] = K053251_get_priority(K053251_CI2);
	layer[1] = 1;
	layerpri[1] = K053251_get_priority(K053251_CI4);
	layer[2] = 2;
	layerpri[2] = K053251_get_priority(K053251_CI3);

	sortlayers(layer,layerpri);

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[16 * bg_colorbase],&Machine->visible_area);
	K052109_tilemap_draw(bitmap,layer[0],1<<16);
	K052109_tilemap_draw(bitmap,layer[1],2<<16);
	K052109_tilemap_draw(bitmap,layer[2],4<<16);

	K053245_sprites_draw(bitmap);
}

void glfgreat_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layer[3];


	bg_colorbase       = K053251_get_palette_index(K053251_CI0);
	sprite_colorbase   = K053251_get_palette_index(K053251_CI1);
	layer_colorbase[0] = K053251_get_palette_index(K053251_CI2);
	layer_colorbase[1] = K053251_get_palette_index(K053251_CI3) + 8;	/* weird... */
	layer_colorbase[2] = K053251_get_palette_index(K053251_CI4);

	K052109_tilemap_update();

	palette_init_used_colors();
	K053245_mark_sprites_colors();
	palette_used_colors[16 * bg_colorbase] |= PALETTE_COLOR_VISIBLE;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	layer[0] = 0;
	layerpri[0] = K053251_get_priority(K053251_CI2);
	layer[1] = 1;
	layerpri[1] = K053251_get_priority(K053251_CI3);
	layer[2] = 2;
	layerpri[2] = K053251_get_priority(K053251_CI4);

	sortlayers(layer,layerpri);

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[16 * bg_colorbase],&Machine->visible_area);
	K052109_tilemap_draw(bitmap,layer[0],1<<16);
	K052109_tilemap_draw(bitmap,layer[1],2<<16);
	K052109_tilemap_draw(bitmap,layer[2],4<<16);

	K053245_sprites_draw(bitmap);
}

void ssriders_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;

	for (i = 0;i < 128;i++)
		if ((K053245_word_r(16*i) & 0x8000) && !(K053245_word_r(16*i+2) & 0x8000))
			K053245_word_w(16*i,0xff000000|i);	/* workaround for protection */

	lgtnfght_vh_screenrefresh(bitmap,full_refresh);
}


void thndrx2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int layer[3];


	bg_colorbase       = K053251_get_palette_index(K053251_CI0);
	sprite_colorbase   = K053251_get_palette_index(K053251_CI1);
	layer_colorbase[0] = K053251_get_palette_index(K053251_CI2);
	layer_colorbase[1] = K053251_get_palette_index(K053251_CI4);
	layer_colorbase[2] = K053251_get_palette_index(K053251_CI3);

	K052109_tilemap_update();

	palette_init_used_colors();
	K051960_mark_sprites_colors();
	palette_used_colors[16 * bg_colorbase] |= PALETTE_COLOR_VISIBLE;
	if (palette_recalc())
		tilemap_mark_all_pixels_dirty(ALL_TILEMAPS);

	tilemap_render(ALL_TILEMAPS);

	layer[0] = 0;
	layerpri[0] = K053251_get_priority(K053251_CI2);
	layer[1] = 1;
	layerpri[1] = K053251_get_priority(K053251_CI4);
	layer[2] = 2;
	layerpri[2] = K053251_get_priority(K053251_CI3);

	sortlayers(layer,layerpri);

	fillbitmap(priority_bitmap,0,NULL);
	fillbitmap(bitmap,Machine->pens[16 * bg_colorbase],&Machine->visible_area);
	K052109_tilemap_draw(bitmap,layer[0],1<<16);
	K052109_tilemap_draw(bitmap,layer[1],2<<16);
	K052109_tilemap_draw(bitmap,layer[2],4<<16);

	K051960_sprites_draw(bitmap,-1,-1);
}
