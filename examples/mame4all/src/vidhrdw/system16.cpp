/***************************************************************************

Sega System 16 Video Hardware

Each scrolling layer (foreground, background) is an arrangement
of 4 pages selected from 16 available pages, laid out as follows:

	Page0  Page1
	Page2  Page3

Each page is an arrangement of 8x8 tiles, 64 tiles wide, and 32 tiles high.

***************************************************************************/
void dump_tilemap(void);

#include "driver.h"

//#define SYS16_DEBUG
//#define SPACEHARRIER_OFFSETS

// an attempt at fudging correct gamma for sys16 games, but I'm not sure that it's
// really worth using.
//#define GAMMA_ADJUST
#define TRANSPARENT_SHADOWS
#define MAXCOLOURS 8192

#ifdef TRANSPARENT_SHADOWS
#define ShadowColorsShift 8
UINT16 shade_table[MAXCOLOURS];
int sys16_sh_shadowpal;
#endif

int sys16_MaxShadowColors;
static int sys16_MaxShadowColors_Shift;

#define NUM_SPRITES 128

extern UINT8 *sys16_textram;
extern UINT8 *sys16_spriteram;
extern UINT8 *sys16_tileram; /* contains tilemaps for 16 pages */

static struct sprite_list *sprite_list;

/* video driver constants (potentially different for each game) */
int sys16_spritesystem;
int sys16_sprxoffset;
int sys16_bgxoffset;
int sys16_fgxoffset;
int *sys16_obj_bank;
int sys16_textmode;
int sys16_textlayer_lo_min;
int sys16_textlayer_lo_max;
int sys16_textlayer_hi_min;
int sys16_textlayer_hi_max;
int sys16_dactype;
int sys16_bg1_trans; // alien syn + sys18
int sys16_bg_priority_mode;
int sys16_fg_priority_mode;
int sys16_bg_priority_value;
int sys16_fg_priority_value;
int sys16_18_mode;
int sys16_spritelist_end;
int sys16_tilebank_switch;
int sys16_rowscroll_scroll;
int sys16_quartet_title_kludge;
extern void (* sys16_update_proc)( void );

/* video registers */
int sys16_tile_bank1;
int sys16_tile_bank0;
int sys16_refreshenable;
int sys16_clear_screen;
int sys16_bg_scrollx, sys16_bg_scrolly;
int sys16_bg_page[4];
int sys16_fg_scrollx, sys16_fg_scrolly;
int sys16_fg_page[4];

int sys16_bg2_scrollx, sys16_bg2_scrolly;
int sys16_bg2_page[4];
int sys16_fg2_scrollx, sys16_fg2_scrolly;
int sys16_fg2_page[4];

int sys18_bg2_active;
int sys18_fg2_active;
unsigned char *sys18_splittab_bg_x;
unsigned char *sys18_splittab_bg_y;
unsigned char *sys18_splittab_fg_x;
unsigned char *sys18_splittab_fg_y;

static int sys16_freezepalette;
static int sys16_palettedirty[MAXCOLOURS];

#ifdef SPACEHARRIER_OFFSETS
unsigned char *spaceharrier_patternoffsets;
#endif
unsigned char *gr_ver;
unsigned char *gr_hor;
unsigned char *gr_pal;
unsigned char *gr_flip;
int gr_palette;
int gr_palette_default;
unsigned char gr_colorflip[2][4];
unsigned char *gr_second_road;

static struct tilemap *background, *foreground, *text_layer;
static struct tilemap *background2, *foreground2;
static int old_bg_page[4],old_fg_page[4], old_tile_bank1, old_tile_bank0;
static int old_bg2_page[4],old_fg2_page[4];

static void draw_quartet_title_screen( struct osd_bitmap *bitmap,int playfield );

/***************************************************************************/

UINT32 sys16_bg_map( UINT32 col, UINT32 row, UINT32 num_cols, UINT32 num_rows ){
	int page = 0;
	if( row<32 ){ /* top */
		if( col<64 ) page = 0; else page = 1;
	}
	else { /* bottom */
		if( col<64 ) page = 2; else page = 3;
	}
	row = row%32;
	col = col%64;
	return page*64*32+row*64+col;
}

UINT32 sys16_text_map( UINT32 col, UINT32 row, UINT32 num_cols, UINT32 num_rows ){
	return row*64+col+(64-40);
}

/***************************************************************************/

WRITE_HANDLER( sys16_paletteram_w ){
	UINT16 oldword = READ_WORD (&paletteram[offset]);
	UINT16 newword = COMBINE_WORD (oldword, data);
	if( oldword!=newword ){
		/* we can do this, because we initialize palette RAM to all black in vh_start */
		/*	   byte 0    byte 1 */
		/*	GBGR BBBB GGGG RRRR */
		/*	5444 3210 3210 3210 */
		UINT8 r = (newword & 0x00f)<<1;
		UINT8 g = (newword & 0x0f0)>>2;
		UINT8 b = (newword & 0xf00)>>7;
		if( sys16_dactype == 0 ){
			/* dac_type == 0 (from GCS file) */
			if (newword&0x1000) r|=1;
			if (newword&0x2000) g|=2;
			if (newword&0x8000) g|=1;
			if (newword&0x4000) b|=1;
		}
		else if( sys16_dactype == 1 ){
			/* dac_type == 1 (from GCS file) Shinobi Only*/
			if (newword&0x1000) r|=1;
			if (newword&0x4000) g|=2;
			if (newword&0x8000) g|=1;
			if (newword&0x2000) b|=1;
		}

#ifndef TRANSPARENT_SHADOWS
		if( !sys16_freezepalette ){
			palette_change_color( offset/2,
				(r << 3) | (r >> 2), /* 5 bits red */
				(g << 2) | (g >> 4), /* 6 bits green */
				(b << 3) | (b >> 2) /* 5 bits blue */
			);
		}
		else{
			r=(r << 3) | (r >> 2); /* 5 bits red */
			g=(g << 2) | (g >> 4); /* 6 bits green */
			b=(b << 3) | (b >> 2); /* 5 bits blue */
			sys16_palettedirty[offset/2]=0xff000000+(r<<16)+(g<<8)+b;
		}
#else
		if (Machine->scrbitmap->depth == 8){ /* 8 bit shadows */
			if(!sys16_freezepalette){
				palette_change_color( offset/2,
					(r << 3) | (r >> 3), /* 5 bits red */
					(g << 2) | (g >> 4), /* 6 bits green */
					(b << 3) | (b >> 3) /* 5 bits blue */
				);
			}
			else {
				r=(r << 3) | (r >> 3); /* 5 bits red */
				g=(g << 2) | (g >> 4); /* 6 bits green */
				b=(b << 3) | (b >> 3); /* 5 bits blue */
				sys16_palettedirty[offset/2]=0xff000000+(r<<16)+(g<<8)+b;
			}
		}
		else {
			if(!sys16_freezepalette){
				r=(r << 3) | (r >> 2); /* 5 bits red */
				g=(g << 2) | (g >> 4); /* 6 bits green */
				b=(b << 3) | (b >> 2); /* 5 bits blue */

				palette_change_color( offset/2,r,g,b);

				/* shadow color */

				r= r * 160 / 256;
				g= g * 160 / 256;
				b= b * 160 / 256;

				palette_change_color( offset/2+Machine->drv->total_colors/2,r,g,b);
			}
			else {
				r=(r << 3) | (r >> 3); /* 5 bits red */
				g=(g << 2) | (g >> 4); /* 6 bits green */
				b=(b << 3) | (b >> 3); /* 5 bits blue */
				sys16_palettedirty[offset/2]=0xff000000+(r<<16)+(g<<8)+b;

				r= r * 160 / 256;
				g= g * 160 / 256;
				b= b * 160 / 256;
				sys16_palettedirty[offset/2+Machine->drv->total_colors/2]=0xff000000+(r<<16)+(g<<8)+b;
			}
		}
#endif
		WRITE_WORD (&paletteram[offset], newword);
	}
}

static void sys16_refresh_palette(void){
	UINT8 r,g,b;
	int i;
	for( i=0;i<Machine->drv->total_colors;i++ ){
		if( sys16_palettedirty[i] ){
			r=(sys16_palettedirty[i]&0x00ff0000) >> 16;
			g=(sys16_palettedirty[i]&0x0000ff00) >> 8;
			b=(sys16_palettedirty[i]&0x000000ff);
			palette_change_color(i,r,g,b);
			sys16_palettedirty[i]=0;
		}
	}
}

static void update_page( void ){
	int all_dirty = 0;
	int i,offset;
	if( old_tile_bank1 != sys16_tile_bank1 ){
		all_dirty = 1;
		old_tile_bank1 = sys16_tile_bank1;
	}
	if( old_tile_bank0 != sys16_tile_bank0 ){
		all_dirty = 1;
		old_tile_bank0 = sys16_tile_bank0;
		tilemap_mark_all_tiles_dirty( text_layer );
	}
	if( all_dirty ){
		tilemap_mark_all_tiles_dirty( background );
		tilemap_mark_all_tiles_dirty( foreground );
		if( sys16_18_mode ){
			tilemap_mark_all_tiles_dirty( background2 );
			tilemap_mark_all_tiles_dirty( foreground2 );
		}
	}
	else {
		for(i=0;i<4;i++){
			int page0 = 64*32*i;
			if( old_bg_page[i]!=sys16_bg_page[i] ){
				old_bg_page[i] = sys16_bg_page[i];
				for( offset = page0; offset<page0+64*32; offset++ ){
					tilemap_mark_tile_dirty( background, offset );
				}
			}
			if( old_fg_page[i]!=sys16_fg_page[i] ){
				old_fg_page[i] = sys16_fg_page[i];
				for( offset = page0; offset<page0+64*32; offset++ ){
					tilemap_mark_tile_dirty( foreground, offset );
				}
			}
			if( sys16_18_mode ){
				if( old_bg2_page[i]!=sys16_bg2_page[i] ){
					old_bg2_page[i] = sys16_bg2_page[i];
					for( offset = page0; offset<page0+64*32; offset++ ){
						tilemap_mark_tile_dirty( background2, offset );
					}
				}
				if( old_fg2_page[i]!=sys16_fg2_page[i] ){
					old_fg2_page[i] = sys16_fg2_page[i];
					for( offset = page0; offset<page0+64*32; offset++ ){
						tilemap_mark_tile_dirty( foreground2, offset );
					}
				}
			}
		}
	}
}

static void get_bg_tile_info( int offset ){
	const UINT16 *source = 64*32*sys16_bg_page[offset/(64*32)] + (UINT16 *)sys16_tileram;
	int data = source[offset%(64*32)];
	int tile_number = (data&0xfff) + 0x1000*((data&sys16_tilebank_switch)?sys16_tile_bank1:sys16_tile_bank0);

	if(sys16_textmode==0){
		SET_TILE_INFO( 0, tile_number, (data>>6)&0x7f );
	}
	else{
		SET_TILE_INFO( 0, tile_number, (data>>5)&0x7f );
	}

	switch(sys16_bg_priority_mode) {
	case 1: // Alien Syndrome
		tile_info.priority = (data&0x8000)?1:0;
		break;
	case 2: // Body Slam / wrestwar
		tile_info.priority = ((data&0xff00) >= sys16_bg_priority_value)?1:0;
		break;
	case 3: // sys18 games
		if( data&0x8000 ){
			tile_info.priority = 2;
		}
		else {
			tile_info.priority = ((data&0xff00) >= sys16_bg_priority_value)?1:0;
		}
		break;
	}
}

static void get_fg_tile_info( int offset ){
	const UINT16 *source = 64*32*sys16_fg_page[offset/(64*32)] + (UINT16 *)sys16_tileram;
	int data = source[offset%(64*32)];
	int tile_number = (data&0xfff) + 0x1000*((data&sys16_tilebank_switch)?sys16_tile_bank1:sys16_tile_bank0);

	if(sys16_textmode==0){
		SET_TILE_INFO( 0, tile_number, (data>>6)&0x7f );
	}
	else{
		SET_TILE_INFO( 0, tile_number, (data>>5)&0x7f );
	}
	switch(sys16_fg_priority_mode){
	case 1: // alien syndrome
		tile_info.priority = (data&0x8000)?1:0;
		break;

	case 3:
		tile_info.priority = ((data&0xff00) >= sys16_fg_priority_value)?1:0;
		break;

	default:
		if( sys16_fg_priority_mode>=0 ){
			tile_info.priority = (data&0x8000)?1:0;
		}
		break;
	}
}

static void get_bg2_tile_info( int offset ){
	const UINT16 *source = 64*32*sys16_bg2_page[offset/(64*32)] + (UINT16 *)sys16_tileram;
	int data = source[offset%(64*32)];
	int tile_number = (data&0xfff) + 0x1000*((data&0x1000)?sys16_tile_bank1:sys16_tile_bank0);
	if(sys16_textmode==0){
		SET_TILE_INFO( 0, tile_number, (data>>6)&0x7f );
	}
	else{
		SET_TILE_INFO( 0, tile_number, (data>>5)&0x7f );
	}
	tile_info.priority = 0;
}

static void get_fg2_tile_info( int offset ){
	const UINT16 *source = 64*32*sys16_fg2_page[offset/(64*32)] + (UINT16 *)sys16_tileram;
	int data = source[offset%(64*32)];
	int tile_number = (data&0xfff) + 0x1000*((data&0x1000)?sys16_tile_bank1:sys16_tile_bank0);
	if(sys16_textmode==0){
		SET_TILE_INFO( 0, tile_number, (data>>6)&0x7f );
	}
	else{
		SET_TILE_INFO( 0, tile_number, (data>>5)&0x7f );
	}
	if((data&0xff00) >= sys16_fg_priority_value) tile_info.priority = 1;
	else tile_info.priority = 0;
}

WRITE_HANDLER( sys16_tileram_w ){
	int oldword = READ_WORD(&sys16_tileram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	if( oldword != newword ){
		int page;
		WRITE_WORD(&sys16_tileram[offset],newword);
		offset = offset/2;
		page = offset/(64*32);
		offset = offset%(64*32);

		if( sys16_bg_page[0]==page ) tilemap_mark_tile_dirty( background, offset+64*32*0 );
		if( sys16_bg_page[1]==page ) tilemap_mark_tile_dirty( background, offset+64*32*1 );
		if( sys16_bg_page[2]==page ) tilemap_mark_tile_dirty( background, offset+64*32*2 );
		if( sys16_bg_page[3]==page ) tilemap_mark_tile_dirty( background, offset+64*32*3 );

		if( sys16_fg_page[0]==page ) tilemap_mark_tile_dirty( foreground, offset+64*32*0 );
		if( sys16_fg_page[1]==page ) tilemap_mark_tile_dirty( foreground, offset+64*32*1 );
		if( sys16_fg_page[2]==page ) tilemap_mark_tile_dirty( foreground, offset+64*32*2 );
		if( sys16_fg_page[3]==page ) tilemap_mark_tile_dirty( foreground, offset+64*32*3 );

		if( sys16_18_mode ){
			if( sys16_bg2_page[0]==page ) tilemap_mark_tile_dirty( background2, offset+64*32*0 );
			if( sys16_bg2_page[1]==page ) tilemap_mark_tile_dirty( background2, offset+64*32*1 );
			if( sys16_bg2_page[2]==page ) tilemap_mark_tile_dirty( background2, offset+64*32*2 );
			if( sys16_bg2_page[3]==page ) tilemap_mark_tile_dirty( background2, offset+64*32*3 );

			if( sys16_fg2_page[0]==page ) tilemap_mark_tile_dirty( foreground2, offset+64*32*0 );
			if( sys16_fg2_page[1]==page ) tilemap_mark_tile_dirty( foreground2, offset+64*32*1 );
			if( sys16_fg2_page[2]==page ) tilemap_mark_tile_dirty( foreground2, offset+64*32*2 );
			if( sys16_fg2_page[3]==page ) tilemap_mark_tile_dirty( foreground2, offset+64*32*3 );
		}
	}
}

READ_HANDLER( sys16_tileram_r ){
	return READ_WORD (&sys16_tileram[offset]);
}

/***************************************************************************/

static void get_text_tile_info( int offset ){
	const UINT16 *source = (UINT16 *)sys16_textram;
	int tile_number = source[offset];
	int pri = tile_number >> 8;
	if(sys16_textmode==0){
		SET_TILE_INFO( 0, (tile_number&0x1ff) + sys16_tile_bank0 * 0x1000, (tile_number>>9)%8 );
	}
	else{
		SET_TILE_INFO( 0, (tile_number&0xff)  + sys16_tile_bank0 * 0x1000, (tile_number>>8)%8 );
	}
	if(pri>=sys16_textlayer_lo_min && pri<=sys16_textlayer_lo_max)
		tile_info.priority = 1;
	if(pri>=sys16_textlayer_hi_min && pri<=sys16_textlayer_hi_max)
		tile_info.priority = 0;
}

WRITE_HANDLER( sys16_textram_w ){
	int oldword = READ_WORD(&sys16_textram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	if( oldword != newword ){
		WRITE_WORD(&sys16_textram[offset],newword);
		tilemap_mark_tile_dirty( text_layer, offset/2 );
	}
}

READ_HANDLER( sys16_textram_r ){
	return READ_WORD (&sys16_textram[offset]);
}

/***************************************************************************/

void sys16_vh_stop( void ){

#ifdef SPACEHARRIER_OFFSETS
	if(spaceharrier_patternoffsets) free(spaceharrier_patternoffsets);
	spaceharrier_patternoffsets=0;
#endif
}

int sys16_vh_start( void ){
	if( !sys16_bg1_trans )
		background = tilemap_create(
			get_bg_tile_info,
			sys16_bg_map,
			TILEMAP_OPAQUE,
			8,8,
			64*2,32*2 );
	else
		background = tilemap_create(
			get_bg_tile_info,
			sys16_bg_map,
			TILEMAP_TRANSPARENT,
			8,8,
			64*2,32*2 );

	foreground = tilemap_create(
		get_fg_tile_info,
		sys16_bg_map,
		TILEMAP_TRANSPARENT,
		8,8,
		64*2,32*2 );

	text_layer = tilemap_create(
		get_text_tile_info,
		sys16_text_map,
		TILEMAP_TRANSPARENT,
		8,8,
		40,28 );

	sprite_list = sprite_list_create( NUM_SPRITES, SPRITE_LIST_BACK_TO_FRONT | SPRITE_LIST_RAW_DATA );

#ifdef TRANSPARENT_SHADOWS
	sprite_set_shade_table(shade_table);
#endif

	if( background && foreground && text_layer && sprite_list ){
		/* initialize all entries to black - needed for Golden Axe*/
		int i;
		for( i=0; i<Machine->drv->total_colors; i++ ){
			palette_change_color( i, 0,0,0 );
		}
#ifdef TRANSPARENT_SHADOWS
		memset(&palette_used_colors[0], PALETTE_COLOR_UNUSED, Machine->drv->total_colors);
		if (Machine->scrbitmap->depth == 8) /* 8 bit shadows */
		{
			int j,color;
			for(j = 0, i = Machine->drv->total_colors/2;j<sys16_MaxShadowColors;i++,j++)
			{
				color=j * 160 / (sys16_MaxShadowColors-1);
				color=color | 0x04;
				palette_change_color(i, color, color, color);
			}
		}
		if(sys16_MaxShadowColors==32)
			sys16_MaxShadowColors_Shift = ShadowColorsShift;
		else if(sys16_MaxShadowColors==16)
			sys16_MaxShadowColors_Shift = ShadowColorsShift+1;

#endif
		for(i=0;i<MAXCOLOURS;i++){
			sys16_palettedirty[i]=0;
		}
		sys16_freezepalette=0;

		sprite_list->max_priority = 3;
		sprite_list->sprite_type = SPRITE_TYPE_ZOOM;

		if(sys16_bg1_trans) background->transparent_pen = 0;
		foreground->transparent_pen = 0;
		text_layer->transparent_pen = 0;

		sys16_tile_bank0 = 0;
		sys16_tile_bank1 = 1;

		sys16_fg_scrollx = 0;
		sys16_fg_scrolly = 0;

		sys16_bg_scrollx = 0;
		sys16_bg_scrolly = 0;

		sys16_refreshenable = 1;
		sys16_clear_screen = 0;

		/* common defaults */
		sys16_update_proc = 0;
		sys16_spritesystem = 1;
		sys16_sprxoffset = -0xb8;
		sys16_textmode = 0;
		sys16_bgxoffset = 0;
		sys16_dactype = 0;
		sys16_bg_priority_mode=0;
		sys16_fg_priority_mode=0;
		sys16_spritelist_end=0xffff;
		sys16_tilebank_switch=0x1000;

		// Defaults for sys16 games
		sys16_textlayer_lo_min=0;
		sys16_textlayer_lo_max=0x7f;
		sys16_textlayer_hi_min=0x80;
		sys16_textlayer_hi_max=0xff;

		sys16_18_mode=0;

#ifdef GAMMA_ADJUST
		{
			static float sys16_orig_gamma=0;
			static float sys16_set_gamma=0;
			float cur_gamma=osd_get_gamma();

			if(sys16_orig_gamma == 0)
			{
				sys16_orig_gamma = cur_gamma;
				sys16_set_gamma = cur_gamma - 0.35;
				if (sys16_set_gamma < 0.5) sys16_set_gamma = 0.5;
				if (sys16_set_gamma > 2.0) sys16_set_gamma = 2.0;
				osd_set_gamma(sys16_set_gamma);
			}
			else
			{
				if(sys16_orig_gamma == cur_gamma)
				{
					osd_set_gamma(sys16_set_gamma);
				}
			}
		}
#endif
		return 0;
	}
	return 1;
}

int sys16_ho_vh_start( void ){
	int ret;
	sys16_bg1_trans=1;
	ret = sys16_vh_start();
	if(ret) return 1;

	sys16_textlayer_lo_min=0;
	sys16_textlayer_lo_max=0;
	sys16_textlayer_hi_min=0;
	sys16_textlayer_hi_max=0xff;

	sys16_bg_priority_mode=-1;
	sys16_bg_priority_value=0x1800;
	sys16_fg_priority_value=0x2000;
	return 0;
}

int sys16_or_vh_start( void ){
	int ret;
	sys16_bg1_trans=1;
	ret = sys16_vh_start();
	if(ret) return 1;

	sys16_textlayer_lo_min=0;
	sys16_textlayer_lo_max=0;
	sys16_textlayer_hi_min=0;
	sys16_textlayer_hi_max=0xff;

	sys16_bg_priority_mode=-1;
	sys16_bg_priority_value=0x1800;
	sys16_fg_priority_value=0x2000;
	return 0;
}


int sys18_vh_start( void ){
	int ret;
	sys16_bg1_trans=1;

	background2 = tilemap_create(
		get_bg2_tile_info,
		sys16_bg_map,
		TILEMAP_OPAQUE,
		8,8,
		64*2,32*2 );

	foreground2 = tilemap_create(
		get_fg2_tile_info,
		sys16_bg_map,
		TILEMAP_TRANSPARENT,
		8,8,
		64*2,32*2 );

	if( background2 && foreground2)
	{
		ret = sys16_vh_start();
		if(ret) return 1;

		foreground2->transparent_pen = 0;

		if(sys18_splittab_fg_x)
		{
			tilemap_set_scroll_rows( foreground , 64 );
			tilemap_set_scroll_rows( foreground2 , 64 );
		}
		if(sys18_splittab_bg_x)
		{
			tilemap_set_scroll_rows( background , 64 );
			tilemap_set_scroll_rows( background2 , 64 );
		}

		sys16_textlayer_lo_min=0;
		sys16_textlayer_lo_max=0x1f;
		sys16_textlayer_hi_min=0x20;
		sys16_textlayer_hi_max=0xff;

		sys16_18_mode=1;
		sys16_bg_priority_mode=3;
		sys16_fg_priority_mode=3;
		sys16_bg_priority_value=0x1800;
		sys16_fg_priority_value=0x2000;
		return 0;
	}
	return 1;
}


/***************************************************************************/

static void get_sprite_info( void ){
	const unsigned short *base_pal = Machine->gfx[0]->colortable + 1024;
	const unsigned char *base_gfx = memory_region(REGION_GFX2);

	UINT16 *source = (UINT16 *)sys16_spriteram;
	struct sprite *sprite = sprite_list->sprite;
	const struct sprite *finish = sprite + NUM_SPRITES;

	int passshot_y=0;
	int passshot_width=0;

#ifdef SYS16_DEBUG
	int dump_spritedata=0;
	if(keyboard_pressed_memory(KEYCODE_W))
	{
		dump_spritedata=1;
		logerror("sprites\n");
	}
#endif
	switch( sys16_spritesystem  ){
		case 1: /* standard sprite hardware (Shinobi, Altered Beast, Golden Axe, ...) */
/*
	0	bottom--	top-----	(screen coordinates)
	1	???????X	XXXXXXXX	(screen coordinate)
	2	???????F	FWWWWWWW	(flipx, flipy, logical width)
	3	TTTTTTTT	TTTTTTTT	(pen data)
	4	????BBBB	PPCCCCCC	(attributes: bank, priority, color)
	5	??????ZZ	ZZZZZZZZ	zoomx
	6	??????ZZ	ZZZZZZZZ	zoomy (defaults to zoomx)
	7	?						"sprite offset"
*/
		while( sprite<finish ){
			UINT16 ypos = source[0];
			UINT16 width = source[2];
			int top = ypos&0xff;
			int bottom = ypos>>8;

			if( bottom == 0xff || width ==sys16_spritelist_end){ /* end of spritelist marker */
				do {
					sprite->flags = 0;
					sprite++;
				} while( sprite<finish );
				break;
			}
			sprite->flags = 0;

			if(bottom !=0 && bottom > top)
			{
				UINT16 attributes = source[4];
				UINT16 zoomx = source[5]&0x3ff;
				UINT16 zoomy = (source[6]&0x3ff);
				int gfx = source[3]*4;

#ifdef SYS16_DEBUG
				if(dump_spritedata) logerror("0:%4x  1:%4x  2:%4x  3:%4x  4:%4x  5:%4x  6:%4x  7:%4x\n",
						source[0],source[1],source[2],source[3],source[4],source[5],source[6],source[7]);
#endif

				if( zoomy==0 || source[6]==0xffff ) zoomy = zoomx; /* if zoomy is 0, use zoomx instead */

				sprite->x = source[1] + sys16_sprxoffset;
				sprite->y = top;
				sprite->priority = 3-((attributes>>6)&0x3);
				sprite->pal_data = base_pal + ((attributes&0x3f)<<4);

				sprite->total_height = bottom-top;
				sprite->tile_height = sprite->total_height*(0x400+zoomy)/0x400;

				sprite->line_offset = (width&0x7f)*4;

				sprite->flags = SPRITE_VISIBLE;
				if( width&0x100 ) sprite->flags |= SPRITE_FLIPX;
				if( width&0x080 ) sprite->flags |= SPRITE_FLIPY;

#ifdef TRANSPARENT_SHADOWS
				if ((attributes&0x3f)==0x3f)	// shadow sprite
					sprite->flags|= SPRITE_SHADOW;
#endif

				if( sprite->flags&SPRITE_FLIPY ){
					sprite->line_offset = 512-sprite->line_offset;
					if( sprite->flags&SPRITE_FLIPX ){
						gfx += 4 - sprite->line_offset*(sprite->tile_height+1);
					}
					else {
						gfx -= sprite->line_offset*sprite->tile_height;
					}
				}
				else {
					if( sprite->flags&SPRITE_FLIPX ){
						gfx += 4;
					}
					else {
						gfx += sprite->line_offset;
					}
				}

				sprite->tile_width = sprite->line_offset;
				sprite->total_width = sprite->tile_width*(0x800-zoomx)/0x800;
				sprite->pen_data = base_gfx + (gfx &0x3ffff) + (sys16_obj_bank[(attributes>>8)&0xf] << 17);

			}

			sprite++;
			source += 8;
		}
		break;

		case 8: /* Passing shot 4p */
			passshot_y=-0x23;
			passshot_width=1;
		case 0: /* Passing shot */
/*
	0	???????X	XXXXXXXX	(screen coordinate)
	1	bottom--	top-----	(screen coordinates)
	2	XTTTTTTT	YTTTTTTT	(pen data, flipx, flipy)
	3	????????	?WWWWWWW	(logical width)
	4	??????ZZ	ZZZZZZZZ	zoom
	5	PP???CCC	BBBB????	(attributes: bank, priority, color)
	6,7	(unused)
*/
		while( sprite<finish ){
			UINT16 attributes = source[5];
			UINT16 ypos = source[1];
			int bottom = (ypos>>8)+passshot_y;
			int top = (ypos&0xff)+passshot_y;
			sprite->flags = 0;

			if( bottom>top && ypos!=0xffff ){
				int bank = (attributes>>4)&0xf;
				UINT16 number = source[2];
				UINT16 width = source[3];

				int zoom = source[4]&0x3ff;
				int xpos = source[0] + sys16_sprxoffset;

#ifdef SYS16_DEBUG
				if(dump_spritedata) logerror("0:%4x  1:%4x  2:%4x  3:%4x  4:%4x  5:%4x  6:%4x  7:%4x\n",
						source[0],source[1],source[2],source[3],source[4],source[5],source[6],source[7]);
#endif
				sprite->priority = 3-((attributes>>14)&0x3);
				if(passshot_width) /* 4 player bootleg version */
				{
					width=-width;
					number-=width*(bottom-top-1)-1;
				}

				if( number & 0x8000 ) sprite->flags |= SPRITE_FLIPX;
				if( width  & 0x0080 ) sprite->flags |= SPRITE_FLIPY;
				sprite->flags |= SPRITE_VISIBLE;
				sprite->pal_data = base_pal + ((attributes>>4)&0x3f0);
				sprite->total_height = bottom - top;
				sprite->tile_height = sprite->total_height*(0x400+zoom)/0x400;

#ifdef TRANSPARENT_SHADOWS
				if (((attributes>>8)&0x3f)==0x3f)	// shadow sprite
					sprite->flags|= SPRITE_SHADOW;
#endif
				width &= 0x7f;

				if( sprite->flags&SPRITE_FLIPY ) width = 0x80-width;

				sprite->tile_width = sprite->line_offset = width*4;

				if( sprite->flags&SPRITE_FLIPX ){
					bank = (bank-1) & 0xf;
					if( sprite->flags&SPRITE_FLIPY ){
						xpos += 4;
					}
					else {
						number += 1-width;
					}
				}
				sprite->pen_data = base_gfx + number*4 + (sys16_obj_bank[bank] << 17);

				if( sprite->flags&SPRITE_FLIPY ){
					sprite->pen_data -= sprite->tile_height*sprite->tile_width;
					if( sprite->flags&SPRITE_FLIPX ) sprite->pen_data += 2;
				}

				sprite->x = xpos;
				sprite->y = top+2;

				if( sprite->flags&SPRITE_FLIPY ){
					if( sprite->flags&SPRITE_FLIPX ){
						sprite->tile_width-=4;
						sprite->pen_data+=4;
					}
					else {
						sprite->pen_data += sprite->line_offset;
					}
				}

				sprite->total_width = sprite->tile_width*(0x800-zoom)/0x800;
			}
			sprite++;
			source += 8;
		}
		break;

		case 4: // Aurail
/*
	0	bottom--	top-----	(screen coordinates)
	1	???????X	XXXXXXXX	(screen coordinate)
	2	???????F	FWWWWWWW	(flipx, flipy, logical width)
	3	TTTTTTTT	TTTTTTTT	(pen data)
	4	????BBBB	PPCCCCCC	(attributes: bank, priority, color)
	5	??????ZZ	ZZZZZZZZ	zoomx
	6	??????ZZ	ZZZZZZZZ	zoomy (defaults to zoomx)
	7	?						"sprite offset"
*/
		while( sprite<finish ){
			UINT16 ypos = source[0];
			UINT16 width = source[2];
			UINT16 attributes = source[4];
			int top = ypos&0xff;
			int bottom = ypos>>8;

			if( width == sys16_spritelist_end) {
				do {
					sprite->flags = 0;
					sprite++;
				} while( sprite<finish );
				break;
			}
			sprite->flags = 0;
#ifdef TRANSPARENT_SHADOWS
			if(bottom !=0 && bottom > top)
#else
			if(bottom !=0 && bottom > top && (attributes&0x3f) !=0x3f)
#endif
			{
				UINT16 zoomx = source[5]&0x3ff;
				UINT16 zoomy = (source[6]&0x3ff);
				int gfx = source[3]*4;

#ifdef SYS16_DEBUG
				if(dump_spritedata) logerror("0:%4x  1:%4x  2:%4x  3:%4x  4:%4x  5:%4x  6:%4x  7:%4x\n",
						source[0],source[1],source[2],source[3],source[4],source[5],source[6],source[7]);
#endif

				if( zoomy==0 ) zoomy = zoomx; /* if zoomy is 0, use zoomx instead */
				sprite->pal_data = base_pal + ((attributes&0x3f)<<4);

				sprite->x = source[1] + sys16_sprxoffset;;
				sprite->y = top;
				sprite->priority = 3-((attributes>>6)&0x3);

				sprite->total_height = bottom-top;
				sprite->tile_height = sprite->total_height*(0x400+zoomy)/0x400;

				sprite->line_offset = (width&0x7f)*4;

				sprite->flags = SPRITE_VISIBLE;
				if( width&0x100 ) sprite->flags |= SPRITE_FLIPX;
				if( width&0x080 ) sprite->flags |= SPRITE_FLIPY;
#ifdef TRANSPARENT_SHADOWS
				if ((attributes&0x3f)==0x3f)	// shadow sprite
					sprite->flags|= SPRITE_SHADOW;
#endif

				if( sprite->flags&SPRITE_FLIPY ){
					sprite->line_offset = 512-sprite->line_offset;
					if( sprite->flags&SPRITE_FLIPX ){
						gfx += 4 - sprite->line_offset*(sprite->tile_height+1);
					}
					else {
						gfx -= sprite->line_offset*sprite->tile_height;
					}
				}
				else {
					if( sprite->flags&SPRITE_FLIPX ){
						gfx += 4;
					}
					else {
						gfx += sprite->line_offset;
					}
				}

				sprite->tile_width = sprite->line_offset;
				sprite->total_width = sprite->tile_width*(0x800-zoomx)/0x800;
				sprite->pen_data = base_gfx + (gfx &0x3ffff) + (sys16_obj_bank[(attributes>>8)&0xf] << 17);

			}
			sprite++;
			source += 8;
		}
		break;
		case 3:	// Fantzone
			{
				int spr_no=0;
				while( sprite<finish ){
					UINT16 ypos = source[0];
					UINT16 pal=(source[4]>>8)&0x3f;
					int top = ypos&0xff;
					int bottom = ypos>>8;

					if( bottom == 0xff ){ /* end of spritelist marker */
						do {
							sprite->flags = 0;
							sprite++;
						} while( sprite<finish );
						break;
					}
					sprite->flags = 0;

#ifdef SYS16_DEBUG
					if(dump_spritedata) logerror("0:%4x  1:%4x  2:%4x  3:%4x  4:%4x  5:%4x  6:%4x  7:%4x\n",
							source[0],source[1],source[2],source[3],source[4],source[5],source[6],source[7]);
#endif
#ifdef TRANSPARENT_SHADOWS
					if(bottom !=0 && bottom > top)
#else
					if(bottom !=0 && bottom > top && pal !=0x3f)
#endif
					{
						UINT16 spr_pri=(source[4])&0xf;
						UINT16 bank=(source[4]>>4) &0xf;
						UINT16 tsource[4];
						UINT16 width;
						int gfx;

						if (spr_no==5 && (source[4]&0x00ff) == 0x0021 &&
							((source[3]&0xff00) == 0x5200 || (source[3]&0xff00) == 0x5300)) spr_pri=2; // tears fix for ending boss

						tsource[2]=source[2];
						tsource[3]=source[3];

						if((tsource[3] & 0x7f80) == 0x7f80)
						{
							bank=(bank-1)&0xf;
							tsource[3]^=0x8000;
						}

						tsource[2] &= 0x00ff;
						if (tsource[3]&0x8000)
						{ // reverse
							tsource[2] |= 0x0100;
							tsource[3] &= 0x7fff;
						}

						gfx = tsource[3]*4;
						width = tsource[2];
						top++;
						bottom++;

						sprite->x = source[1] + sys16_sprxoffset;
						if(sprite->x > 0x140) sprite->x-=0x200;
						sprite->y = top;
						sprite->priority = 3-spr_pri;
						sprite->pal_data = base_pal + (pal<<4);

						sprite->total_height = bottom-top;
						sprite->tile_height = sprite->total_height;

						sprite->line_offset = (width&0x7f)*4;

						sprite->flags = SPRITE_VISIBLE;
						if( width&0x100 ) sprite->flags |= SPRITE_FLIPX;
						if( width&0x080 ) sprite->flags |= SPRITE_FLIPY;
#ifdef TRANSPARENT_SHADOWS
						if (pal==0x3f)	// shadow sprite
							sprite->flags|= SPRITE_SHADOW;
#endif

						if( sprite->flags&SPRITE_FLIPY ){
							sprite->line_offset = 512-sprite->line_offset;
							if( sprite->flags&SPRITE_FLIPX ){
								gfx += 4 - sprite->line_offset*(sprite->tile_height+1);
							}
							else {
								gfx -= sprite->line_offset*sprite->tile_height;
							}
						}
						else {
							if( sprite->flags&SPRITE_FLIPX ){
								gfx += 4;
							}
							else {
								gfx += sprite->line_offset;
							}
						}

						sprite->tile_width = sprite->line_offset;
						if(width==0) sprite->tile_width=320;			// fixes laser
						sprite->total_width = sprite->tile_width;
						sprite->pen_data = base_gfx + (gfx &0x3ffff) + (sys16_obj_bank[bank] << 17);

					}

					source+=8;
					sprite++;
					spr_no++;
				}
			}
			break;
		case 2: // Quartet2 /Alexkidd + others
		while( sprite<finish ){
			UINT16 ypos = source[0];
			int top = ypos&0xff;
			int bottom = ypos>>8;

			if( bottom == 0xff ){ /* end of spritelist marker */
				do {
					sprite->flags = 0;
					sprite++;
				} while( sprite<finish );
				break;
			}
			sprite->flags = 0;

			if(bottom !=0 && bottom > top)
			{
				UINT16 spr_pri=(source[4])&0xf;
				UINT16 bank=(source[4]>>4) &0xf;
				UINT16 pal=(source[4]>>8)&0x3f;
				UINT16 tsource[4];
				UINT16 width;
				int gfx;

#ifdef SYS16_DEBUG
				if(dump_spritedata) logerror("0:%4x  1:%4x  2:%4x  3:%4x  4:%4x  5:%4x  6:%4x  7:%4x\n",
						source[0],source[1],source[2],source[3],source[4],source[5],source[6],source[7]);
#endif

				tsource[2]=source[2];
				tsource[3]=source[3];
#ifndef TRANSPARENT_SHADOWS
				if (pal==0x3f)	// shadow sprite
					pal=(bank<<1);
#endif

				if((tsource[3] & 0x7f80) == 0x7f80)
				{
					bank=(bank-1)&0xf;
					tsource[3]^=0x8000;
				}

				tsource[2] &= 0x00ff;
				if (tsource[3]&0x8000)
				{ // reverse
					tsource[2] |= 0x0100;
					tsource[3] &= 0x7fff;
				}

				gfx = tsource[3]*4;
				width = tsource[2];
				top++;
				bottom++;

				sprite->x = source[1] + sys16_sprxoffset;
				if(sprite->x > 0x140) sprite->x-=0x200;
				sprite->y = top;
				sprite->priority = 3 - spr_pri;
				sprite->pal_data = base_pal + (pal<<4);

				sprite->total_height = bottom-top;
				sprite->tile_height = sprite->total_height;

				sprite->line_offset = (width&0x7f)*4;

				sprite->flags = SPRITE_VISIBLE;
				if( width&0x100 ) sprite->flags |= SPRITE_FLIPX;
				if( width&0x080 ) sprite->flags |= SPRITE_FLIPY;
#ifdef TRANSPARENT_SHADOWS
				if (pal==0x3f)	// shadow sprite
					sprite->flags|= SPRITE_SHADOW;
#endif

				if( sprite->flags&SPRITE_FLIPY ){
					sprite->line_offset = 512-sprite->line_offset;
					if( sprite->flags&SPRITE_FLIPX ){
						gfx += 4 - sprite->line_offset*(sprite->tile_height+1);
					}
					else {
						gfx -= sprite->line_offset*sprite->tile_height;
					}
				}
				else {
					if( sprite->flags&SPRITE_FLIPX ){
						gfx += 4;
					}
					else {
						gfx += sprite->line_offset;
					}
				}

				sprite->tile_width = sprite->line_offset;
				sprite->total_width = sprite->tile_width;
				sprite->pen_data = base_gfx + (gfx &0x3ffff) + (sys16_obj_bank[bank] << 17);
			}

			source+=8;
			sprite++;
		}
		break;
		case 5: // Hang-On
		while( sprite<finish ){
			UINT16 ypos = source[0];
			int top = ypos&0xff;
			int bottom = ypos>>8;

			if( bottom == 0xff ){ /* end of spritelist marker */
				do {
					sprite->flags = 0;
					sprite++;
				} while( sprite<finish );
				break;
			}
			sprite->flags = 0;

			if(bottom !=0 && bottom > top)
			{
				UINT16 bank=(source[1]>>12);
				UINT16 pal=(source[4]>>8)&0x3f;
				UINT16 tsource[4];
				UINT16 width;
				int gfx;
				int zoomx,zoomy;

#ifdef SYS16_DEBUG
				if(dump_spritedata) logerror("0:%4x  1:%4x  2:%4x  3:%4x  4:%4x  5:%4x  6:%4x  7:%4x\n",
						source[0],source[1],source[2],source[3],source[4],source[5],source[6],source[7]);
#endif

				tsource[2]=source[2];
				tsource[3]=source[3];

				zoomx=((source[4]>>2) & 0x3f) *(1024/64);
		        zoomy = (1060*zoomx)/(2048-zoomx);

//				if (pal==0x3f)	// ????????????
//					pal=(bank<<1);

				if((tsource[3] & 0x7f80) == 0x7f80)
				{
					bank=(bank-1)&0xf;
					tsource[3]^=0x8000;
				}

				if (tsource[3]&0x8000)
				{ // reverse
					tsource[2] |= 0x0100;
					tsource[3] &= 0x7fff;
				}

				gfx = tsource[3]*4;
				width = tsource[2];

				sprite->x = ((source[1] & 0x3ff) + sys16_sprxoffset);
				if(sprite->x >= 0x200) sprite->x-=0x200;
				sprite->y = top;
				sprite->priority = 0;
				sprite->pal_data = base_pal + (pal<<4);

				sprite->total_height = bottom-top;
				sprite->tile_height = sprite->total_height*(0x400+zoomy)/0x400;

				sprite->line_offset = (width&0x7f)*4;

				sprite->flags = SPRITE_VISIBLE;
				if( width&0x100 ) sprite->flags |= SPRITE_FLIPX;
				if( width&0x080 ) sprite->flags |= SPRITE_FLIPY;


//				sprite->flags|= SPRITE_PARTIAL_SHADOW;
//				sprite->shadow_pen=10;

				if( sprite->flags&SPRITE_FLIPY ){
					sprite->line_offset = 512-sprite->line_offset;
					if( sprite->flags&SPRITE_FLIPX ){
						gfx += 4 - sprite->line_offset*(sprite->tile_height+1);
					}
					else {
						gfx -= sprite->line_offset*sprite->tile_height;
					}
				}
				else {
					if( sprite->flags&SPRITE_FLIPX ){
						gfx += 4;
					}
					else {
						gfx += sprite->line_offset;
					}
				}

				sprite->tile_width = sprite->line_offset;
				sprite->total_width = sprite->tile_width*(0x0800 - zoomx)/0x800;
				sprite->pen_data = base_gfx + (gfx &0x3ffff) + (sys16_obj_bank[bank] << 17);

			}

			source+=8;
			sprite++;
		}
		break;
		case 6: // Space Harrier
		while( sprite<finish ){
			UINT16 ypos = source[0];
			int top = ypos&0xff;
			int bottom = ypos>>8;

			if( bottom == 0xff ){ /* end of spritelist marker */
				do {
					sprite->flags = 0;
					sprite++;
				} while( sprite<finish );
				break;
			}
			sprite->flags = 0;

			if(bottom !=0 && bottom > top)
			{
				UINT16 bank=(source[1]>>12);
				UINT16 pal=(source[2]>>8)&0x3f;
				UINT16 tsource[4];
				UINT16 width;
				int gfx;
				int zoomx,zoomy;

#ifdef SYS16_DEBUG
				if(dump_spritedata) logerror("0:%4x  1:%4x  2:%4x  3:%4x  4:%4x  5:%4x  6:%4x  7:%4x\n",
						source[0],source[1],source[2],source[3],source[4],source[5],source[6],source[7]);
#endif

				tsource[2]=source[2]&0xff;
				tsource[3]=source[3];

				zoomx=(source[4] & 0x3f) *(1024/64);
		        zoomy = (1024*zoomx)/(2048-zoomx);

#ifndef TRANSPARENT_SHADOWS
				if (pal==0x3f)	// shadow sprite
					pal=(bank<<1);
#endif

				if((tsource[3] & 0x7f80) == 0x7f80)
				{
					bank=(bank-1)&0xf;
					tsource[3]^=0x8000;
				}

				if (tsource[3]&0x8000)
				{ // reverse
					tsource[2] |= 0x0100;
					tsource[3] &= 0x7fff;
				}

				gfx = tsource[3]*4;
				width = tsource[2];

				sprite->x = ((source[1] & 0x3ff) + sys16_sprxoffset);
				if(sprite->x >= 0x200) sprite->x-=0x200;
				sprite->y = top+1;
				sprite->priority = 0;
				sprite->pal_data = base_pal + (pal<<4);

				sprite->total_height = bottom-top;
//				sprite->tile_height = sprite->total_height*(0x400+zoomy)/0x400;
				sprite->tile_height = ((sprite->total_height)<<4|0xf)*(0x400+zoomy)/0x4000;

				sprite->line_offset = (width&0x7f)*4;

				sprite->flags = SPRITE_VISIBLE;
				if( width&0x100 ) sprite->flags |= SPRITE_FLIPX;
				if( width&0x080 ) sprite->flags |= SPRITE_FLIPY;

#ifdef TRANSPARENT_SHADOWS
				if (sys16_sh_shadowpal == 0)		// space harrier
				{
					if (pal==sys16_sh_shadowpal)	// shadow sprite
						sprite->flags|= SPRITE_SHADOW;
					// I think this looks better, but I'm sure it's wrong.
//					else if(READ_WORD(&paletteram[2048+pal*2+20]) == 0)
//					{
//						sprite->flags|= SPRITE_PARTIAL_SHADOW;
//						sprite->shadow_pen=10;
//					}
				}
				else								// enduro
				{
					sprite->flags|= SPRITE_PARTIAL_SHADOW;
					sprite->shadow_pen=10;
				}
#endif
				if( sprite->flags&SPRITE_FLIPY ){
					sprite->line_offset = 512-sprite->line_offset;
					if( sprite->flags&SPRITE_FLIPX ){
						gfx += 4 - sprite->line_offset*(sprite->tile_height+1) /*+4*/;
					}
					else {
						gfx -= sprite->line_offset*sprite->tile_height;
					}
				}
				else {
					if( sprite->flags&SPRITE_FLIPX ){
						gfx += 4 /*+ 4*/;		// +2 ???
					}
					else {
						gfx += sprite->line_offset;
					}
				}

				sprite->line_offset<<=1;
				sprite->tile_width = sprite->line_offset;
				sprite->total_width = sprite->tile_width*(0x0800 - zoomx)/0x800;

				sprite->pen_data = base_gfx + (((gfx &0x3ffff) + (sys16_obj_bank[bank] << 17)) << 1);


#ifdef SPACEHARRIER_OFFSETS
// doesn't seem to work properly and I'm not sure why it's needed.
				if( !(width & 0x180) && (width & 0x7f))
				{
					unsigned addr=tsource[3];
					unsigned offset_addr= (bank<<12) | (addr>>4);
					char rebuild = 0;

					if (spaceharrier_patternoffsets[offset_addr]!=0x7f)
					{
						offset = spaceharrier_patternoffsets[offset_addr];
					}
					else
					{ // build pattern offset
						unsigned width2=width&0x7f;
						UINT8 *p,*p0;
						unsigned height=sprite->tile_height;
						unsigned width_bytes;
						int len_pattern;
						int i,j;

						len_pattern = height*width2;
						if (len_pattern >= 16)
						{

							len_pattern >>= 4;

							// 32 bit sprite hardware ///////////////////////////////////////////
							width_bytes = width2<<3;
							p0 = (UINT8 *)sprite->pen_data;
							/////////////////////////////////////////////////////////////////////

							offset=0xe;
							for (i=0, p=p0; i<height; i++, p+=width_bytes)
							{
								for (j=0; j<width_bytes && j<offset; j++)
								{
									if ((p[j]!=0xff && p[j]!=0x00) /*|| (p[j+1]!=0xff && p[j+1]!=0x00 )*/) break;
								}
								if (j<offset) offset = j;
							}

							//              printf("rebuild %02x:%04x = %d\n", bank, addr, offset);

							p=&spaceharrier_patternoffsets[offset_addr];

							offset/=2;
							for (i=0; i<len_pattern; i++,p++) *p = offset;

						    logerror("addr=%04x offset=%4x bank=%02x offset=%d height=%d zoomy=%04x len_pattern=%d width=%d offset_addr=%04x\n", addr, offset, bank, offset, height, zoomy,  len_pattern, width2, offset_addr);
						}
					}
				}
				sprite->pen_data += offset*2;
#endif
			}

			source+=8;
			sprite++;
		}
		break;
		case 7: // Out Run
		while( sprite<finish ){

			if( source[0] == 0xffff ){ /* end of spritelist marker */
				do {
					sprite->flags = 0;
					sprite++;
				} while( sprite<finish );
				break;
			}
			sprite->flags = 0;

			if (!(source[0]&0x4000))
			{
				UINT16 bank=(source[0]>>8)&7;
				UINT16 pal=(source[5])&0x7f;
				UINT16 width;
				int gfx;
				int zoom;
				int x;

#ifdef SYS16_DEBUG
				if(dump_spritedata) logerror("0:%4x  1:%4x  2:%4x  3:%4x  4:%4x  5:%4x  6:%4x  7:%4x\n",
						source[0],source[1],source[2],source[3],source[4],source[5],source[6],source[7]);
#endif

				zoom=source[4]&0xfff;
//				if (zoom==0x32c) zoom=0x3cf;	//???

				if(zoom==0) zoom=1;

				if (source[1]&0x8000) bank=(bank+1)&0x7;

				gfx = (source[1]&0x7fff)*4;
				width = source[2]>>9;

				x = (source[2] & 0x1ff);

				// patch misplaced sprite on map
//				if(zoom == 0x3f0 && source[1]==0x142e && (source[0]&0xff)==0x19)
//					x-=2;

				sprite->y = source[0]&0xff;
				sprite->priority = 0;
				sprite->pal_data = base_pal + (pal<<4) + 1024;

				sprite->total_height = (source[5]>>8)+1;
				sprite->tile_height = ((sprite->total_height<<4)| 0xf)*(zoom)/0x2000;

				sprite->line_offset = (width&0x7f)*4;

				sprite->flags = SPRITE_VISIBLE;
#ifdef TRANSPARENT_SHADOWS
				if(pal==0)
					sprite->flags|= SPRITE_SHADOW;
				else if(source[3]&0x4000)
//				if(pal==0 || source[3]&0x4000)
				{
					sprite->flags|= SPRITE_PARTIAL_SHADOW;
					sprite->shadow_pen=10;
				}
#endif
				if(!(source[4]&0x2000))
				{
					if(!(source[4]&0x4000))
					{
						// Should be drawn right to left, but this should be ok.
						x-=(sprite->line_offset*2)*0x200/zoom;
						gfx+=4-sprite->line_offset;
					}
					else
					{
						int ofs=(sprite->line_offset*2)*0x200/zoom;
						x-=ofs;
						sprite->flags |= SPRITE_FLIPX;

						// x position compensation for rocks in round2R and round4RRR
/*						if((source[0]&0xff00)==0x0300)
						{
							if (source[1]==0xc027)
							{
								if ((source[2]>>8)==0x59) x += ofs/2;
								else if ((source[2]>>8)>0x59) x += ofs;
							}
							else if (source[1]==0xcf73)
							{
								if ((source[2]>>8)==0x2d) x += ofs/2;
								else if ((source[2]>>8)>0x2d) x += ofs;
							}
							else if (source[1]==0xd3046)
							{
								if ((source[2]>>8)==0x19) x += ofs/2;
								else if ((source[2]>>8)>0x19) x += ofs;
							}
							else if (source[1]==0xd44e)
							{
								if ((source[2]>>8)==0x0d) x += ofs/2;
								else if ((source[2]>>8)>0x0d) x += ofs;
							}
							else if (source[1]==0xd490)
							{
								if ((source[2]>>8)==0x09) x += ofs/2;
								else if ((source[2]>>8)>0x09) x += ofs;
							}
						}
*/
					}
				}
				else
				{
					if(!(source[4]&0x4000))
					{
						gfx-=sprite->line_offset-4;
						sprite->flags |= SPRITE_FLIPX;
					}
					else
					{
						if (source[4]&0x8000)
						{ // patch for car shadow position
							if (source[4]==0xe1a9 && source[1]==0x5817)
								x-=2;
						}
					}
				}

				sprite->x = x + sys16_sprxoffset;

				sprite->line_offset<<=1;
				sprite->tile_width = sprite->line_offset;
				sprite->total_width = sprite->tile_width*0x200/zoom;
				sprite->pen_data = base_gfx + (((gfx &0x3ffff) + (sys16_obj_bank[bank] << 17)) << 1);
			}
			source+=8;
			sprite++;
		}
		break;
	}
}

/***************************************************************************/

static void mark_sprite_colors( void ){
	unsigned short *source = (unsigned short *)sys16_spriteram;
	unsigned short *finish = source+NUM_SPRITES*8;
	int pal_start=1024,pal_size=64;

	char used[128];
	memset( used, 0, 128 );

	switch( sys16_spritesystem ){
		case 1: /* standard sprite hardware */
			do{
				if( source[0]>>8 == 0xff || source[2] == sys16_spritelist_end) break;
				used[source[4]&0x3f] = 1;
				source+=8;
			}while( source<finish );
			break;
		case 4: /* Aurail */
			do{
				if( (source[2]) == sys16_spritelist_end) break;
				used[source[4]&0x3f] = 1;
				source+=8;
			}while( source<finish );
			break;

		case 3:	/* Fantzone */
		case 5:	/* Hang-On */
		case 2: /* Quartet2 / alex kidd + others */
			do{
				if( (source[0]>>8) == 0xff ) break;;
				used[(source[4]>>8)&0x3f] = 1;
				source+=8;
			}while( source<finish );
			break;
		case 6:	/* Space Harrier */
			do{
				if( (source[0]>>8) == 0xff ) break;;
				used[(source[2]>>8)&0x3f] = 1;
				source+=8;
			}while( source<finish );
			break;
		case 7:	/* Out Run */
			do{
				if( (source[0]) == 0xffff ) break;;
				used[(source[5])&0x7f] = 1;
				source+=8;
			}while( source<finish );
			pal_start=2048;
			pal_size=128;
			break;
		case 0: /* passing shot */
		case 8: /* passing shot 4p */
			do{
				if( source[1]!=0xffff ) used[(source[5]>>8)&0x3f] = 1;
				source+=8;
			}while( source<finish );
			break;
	}

	{
		unsigned char *pal = &palette_used_colors[pal_start];
		int i;
		for (i = 0; i < pal_size; i++){
			if ( used[i] ){
				pal[0] = PALETTE_COLOR_UNUSED;
				memset( &pal[1],PALETTE_COLOR_USED,14 );
				pal[15] = PALETTE_COLOR_UNUSED;
			}
			else {
				memset( pal, PALETTE_COLOR_UNUSED, 16 );
			}
			pal += 16;
		}
	}
#ifdef TRANSPARENT_SHADOWS
	if (Machine->scrbitmap->depth == 8) /* 8 bit shadows */
	{
		memset(&palette_used_colors[Machine->drv->total_colors/2], PALETTE_COLOR_USED, sys16_MaxShadowColors);
	}
	else if(sys16_MaxShadowColors != 0) /* 16 bit shadows */
	{
		/* Mark the shadowed versions of the used pens */
		memcpy(&palette_used_colors[Machine->drv->total_colors/2], &palette_used_colors[0], Machine->drv->total_colors/2);
	}
#endif
}

#ifdef TRANSPARENT_SHADOWS
static void build_shadow_table(void)
{
	int i,size;
	int color_start=Machine->drv->total_colors/2;
	/* build the shading lookup table */
	if (Machine->scrbitmap->depth == 8) /* 8 bit shadows */
	{
		if(sys16_MaxShadowColors == 0) return;
		for (i = 0; i < 256; i++)
		{
			unsigned char r, g, b;
			int y;
			osd_get_pen(i, &r, &g, &b);
			y = (r * 10 + g * 18 + b * 4) >> sys16_MaxShadowColors_Shift;
			shade_table[i] = Machine->pens[color_start + y];
		}
		for(i=0;i<sys16_MaxShadowColors;i++)
		{
			shade_table[Machine->pens[color_start + i]]=Machine->pens[color_start + i];
		}
	}
	else
	{
		if(sys16_MaxShadowColors != 0)
		{
			size=Machine->drv->total_colors/2;
			for(i=0;i<size;i++)
			{
				shade_table[Machine->pens[i]]=Machine->pens[size + i];
				shade_table[Machine->pens[size+i]]=Machine->pens[size + i];
			}
		}
		else
		{
			size=Machine->drv->total_colors;
			for(i=0;i<size;i++)
			{
				shade_table[Machine->pens[i]]=Machine->pens[i];
			}
		}
	}
}
#else
#define build_shadow_table()
#endif

void sys16_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh){
	if( sys16_update_proc ) sys16_update_proc();
	update_page();

	// from sys16 emu (Not sure if this is the best place for this?)
	{
		static int freeze_counter=0;
		if (!sys16_refreshenable)
		{
			freeze_counter=4;
			sys16_freezepalette=1;
		}
		if (freeze_counter)
		{
			if( sys16_clear_screen)
				fillbitmap(bitmap,palette_transparent_color,&Machine->visible_area);
			freeze_counter--;
			return;
		}
		else if(sys16_freezepalette)
		{
			sys16_refresh_palette();
			sys16_freezepalette=0;
		}
	}

	if( sys16_refreshenable ){

		if(sys18_splittab_bg_x)
		{
			if((sys16_bg_scrollx&0xff00)  != sys16_rowscroll_scroll)
			{
				tilemap_set_scroll_rows( background , 1 );
				tilemap_set_scrollx( background, 0, -320-sys16_bg_scrollx+sys16_bgxoffset );
			}
			else
			{
				int offset, scroll,i;

				tilemap_set_scroll_rows( background , 64 );
				offset = 32+((sys16_bg_scrolly&0x1f8) >> 3);

				for(i=0;i<29;i++)
				{
					scroll = READ_WORD(&sys18_splittab_bg_x[i*2]);
					tilemap_set_scrollx( background , (i+offset)&0x3f, -320-(scroll&0x3ff)+sys16_bgxoffset );
				}
			}
		}
		else
		{
			tilemap_set_scrollx( background, 0, -320-sys16_bg_scrollx+sys16_bgxoffset );
		}

		if(sys18_splittab_bg_y)
		{
			if((sys16_bg_scrolly&0xff00)  != sys16_rowscroll_scroll)
			{
				tilemap_set_scroll_cols( background , 1 );
				tilemap_set_scrolly( background, 0, -256+sys16_bg_scrolly );
			}
			else
			{
				int offset, scroll,i;

				tilemap_set_scroll_cols( background , 128 );
				offset = 127-((sys16_bg_scrollx&0x3f8) >> 3)-40+2;

				for(i=0;i<41;i++)
				{
					scroll = READ_WORD(&sys18_splittab_bg_y[(i+24)&0xfffe]);
					tilemap_set_scrolly( background , (i+offset)&0x7f, -256+(scroll&0x3ff) );
				}
			}
		}
		else
		{
			tilemap_set_scrolly( background, 0, -256+sys16_bg_scrolly );
		}

		if(sys18_splittab_fg_x)
		{
			if((sys16_fg_scrollx&0xff00)  != sys16_rowscroll_scroll)
			{
				tilemap_set_scroll_rows( foreground , 1 );
				tilemap_set_scrollx( foreground, 0, -320-sys16_fg_scrollx+sys16_fgxoffset );
			}
			else
			{
				int offset, scroll,i;

				tilemap_set_scroll_rows( foreground , 64 );
				offset = 32+((sys16_fg_scrolly&0x1f8) >> 3);

				for(i=0;i<29;i++)
				{
					scroll = READ_WORD(&sys18_splittab_fg_x[i*2]);


					tilemap_set_scrollx( foreground , (i+offset)&0x3f, -320-(scroll&0x3ff)+sys16_fgxoffset );
				}
			}
		}
		else
		{
			tilemap_set_scrollx( foreground, 0, -320-sys16_fg_scrollx+sys16_fgxoffset );
		}

		if(sys18_splittab_fg_y)
		{
			if((sys16_fg_scrolly&0xff00)  != sys16_rowscroll_scroll)
			{
				tilemap_set_scroll_cols( foreground , 1 );
				tilemap_set_scrolly( foreground, 0, -256+sys16_fg_scrolly );
			}
			else
			{
				int offset, scroll,i;

				tilemap_set_scroll_cols( foreground , 128 );
				offset = 127-((sys16_fg_scrollx&0x3f8) >> 3)-40+2;

				for(i=0;i<41;i++)
				{
					scroll = READ_WORD(&sys18_splittab_fg_y[(i+24)&0xfffe]);
					tilemap_set_scrolly( foreground , (i+offset)&0x7f, -256+(scroll&0x3ff) );
				}
			}
		}
		else
		{
			tilemap_set_scrolly( foreground, 0, -256+sys16_fg_scrolly );
		}

		if(sys16_quartet_title_kludge)
		{
			int top,bottom,left,right;
			int top2,bottom2,left2,right2;
			struct rectangle clip;

			left = background->clip_left;
			right = background->clip_right;
			top = background->clip_top;
			bottom = background->clip_bottom;

			left2 = foreground->clip_left;
			right2 = foreground->clip_right;
			top2 = foreground->clip_top;
			bottom2 = foreground->clip_bottom;

			clip.min_x=0;
			clip.min_y=0;
			clip.max_x=1024;
			clip.max_y=512;

			tilemap_set_clip( background, &clip );
			tilemap_set_clip( foreground, &clip );

			tilemap_update(  ALL_TILEMAPS  );

			background->clip_left = left;
			background->clip_right = right;
			background->clip_top = top;
			background->clip_bottom = bottom;

			foreground->clip_left = left2;
			foreground->clip_right = right2;
			foreground->clip_top = top2;
			foreground->clip_bottom = bottom2;

		}
		else
			tilemap_update(  ALL_TILEMAPS  );


		get_sprite_info();

		palette_init_used_colors();
		mark_sprite_colors(); // custom; normally this would be handled by the sprite manager
		sprite_update();

		if( palette_recalc() ) tilemap_mark_all_pixels_dirty( ALL_TILEMAPS );
		build_shadow_table();
		tilemap_render(  ALL_TILEMAPS  );

		if(!sys16_quartet_title_kludge)
		{
			tilemap_draw( bitmap, background, TILEMAP_IGNORE_TRANSPARENCY );
			if(sys16_bg_priority_mode) tilemap_draw( bitmap, background, TILEMAP_IGNORE_TRANSPARENCY | 1 );
		}
		else
			draw_quartet_title_screen( bitmap, 0 );

		sprite_draw(sprite_list,3); // needed for Aurail
		if(sys16_bg_priority_mode==2) tilemap_draw( bitmap, background, 1 );		// body slam (& wrestwar??)
		sprite_draw(sprite_list,2);
		if(sys16_bg_priority_mode==1) tilemap_draw( bitmap, background, 1 );		// alien syndrome / aurail

		if(!sys16_quartet_title_kludge)
		{
			tilemap_draw( bitmap, foreground, 0 );
			sprite_draw(sprite_list,1);
			tilemap_draw( bitmap, foreground, 1 );
		}
		else
		{
			draw_quartet_title_screen( bitmap, 1 );
			sprite_draw(sprite_list,1);
		}

		if(sys16_textlayer_lo_max!=0) tilemap_draw( bitmap, text_layer, 1 ); // needed for Body Slam
		sprite_draw(sprite_list,0);
		tilemap_draw( bitmap, text_layer, 0 );
	}
#ifdef SYS16_DEBUG
	dump_tilemap();
#endif
}

void sys18_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh){
	int i;
	if( sys16_update_proc ) sys16_update_proc();
	update_page();

	// from sys16 emu (Not sure if this is the best place for this?)
	{
		static int freeze_counter=0;
		if (!sys16_refreshenable)
		{
			freeze_counter=4;
			sys16_freezepalette=1;
		}
		if (freeze_counter)
		{
//			if( sys16_clear_screen)
//				fillbitmap(bitmap,palette_transparent_color,&Machine->visible_area);
			freeze_counter--;
			return;
		}
		else if(sys16_freezepalette)
		{
			sys16_refresh_palette();
			sys16_freezepalette=0;
		}
	}

	if( sys16_refreshenable ){

		if(sys18_splittab_bg_x)
		{
			int offset,offset2, scroll,scroll2,orig_scroll;

			offset = 32+((sys16_bg_scrolly&0x1f8) >> 3);
			offset2 = 32+((sys16_bg2_scrolly&0x1f8) >> 3);

			for(i=0;i<29;i++)
			{
				orig_scroll = scroll2 = scroll = READ_WORD(&sys18_splittab_bg_x[i*2]);

				if((sys16_bg_scrollx &0xff00) != 0x8000)
					scroll = sys16_bg_scrollx;

				if((sys16_bg2_scrollx &0xff00) != 0x8000)
					scroll2 = sys16_bg2_scrollx;

				if(orig_scroll&0x8000)
				{
					tilemap_set_scrollx( background , (i+offset)&0x3f, TILE_LINE_DISABLED );
					tilemap_set_scrollx( background2, (i+offset2)&0x3f, -320-(scroll2&0x3ff)+sys16_bgxoffset );
				}
				else
				{
					tilemap_set_scrollx( background , (i+offset)&0x3f, -320-(scroll&0x3ff)+sys16_bgxoffset );
					tilemap_set_scrollx( background2 , (i+offset2)&0x3f, TILE_LINE_DISABLED );
				}
			}
		}
		else
		{
			tilemap_set_scrollx( background , 0, -320-(sys16_bg_scrollx&0x3ff)+sys16_bgxoffset );
			tilemap_set_scrollx( background2, 0, -320-(sys16_bg2_scrollx&0x3ff)+sys16_bgxoffset );
		}

		tilemap_set_scrolly( background , 0, -256+sys16_bg_scrolly );
		tilemap_set_scrolly( background2, 0, -256+sys16_bg2_scrolly );

		if(sys18_splittab_fg_x)
		{
			int offset,offset2, scroll,scroll2,orig_scroll;

			offset = 32+((sys16_fg_scrolly&0x1f8) >> 3);
			offset2 = 32+((sys16_fg2_scrolly&0x1f8) >> 3);

			for(i=0;i<29;i++)
			{
				orig_scroll = scroll2 = scroll = READ_WORD(&sys18_splittab_fg_x[i*2]);

				if((sys16_fg_scrollx &0xff00) != 0x8000)
					scroll = sys16_fg_scrollx;

				if((sys16_fg2_scrollx &0xff00) != 0x8000)
					scroll2 = sys16_fg2_scrollx;

				if(orig_scroll&0x8000)
				{
					tilemap_set_scrollx( foreground , (i+offset)&0x3f, TILE_LINE_DISABLED );
					tilemap_set_scrollx( foreground2, (i+offset2)&0x3f, -320-(scroll2&0x3ff)+sys16_fgxoffset );
				}
				else
				{
					tilemap_set_scrollx( foreground , (i+offset)&0x3f, -320-(scroll&0x3ff)+sys16_fgxoffset );
					tilemap_set_scrollx( foreground2 , (i+offset2)&0x3f, TILE_LINE_DISABLED );
				}
			}
		}
		else
		{
			tilemap_set_scrollx( foreground , 0, -320-(sys16_fg_scrollx&0x3ff)+sys16_fgxoffset );
			tilemap_set_scrollx( foreground2, 0, -320-(sys16_fg2_scrollx&0x3ff)+sys16_fgxoffset );
		}


		tilemap_set_scrolly( foreground , 0, -256+sys16_fg_scrolly );
		tilemap_set_scrolly( foreground2, 0, -256+sys16_fg2_scrolly );

		tilemap_set_enable( background2, sys18_bg2_active );
		tilemap_set_enable( foreground2, sys18_fg2_active );

		tilemap_update(  ALL_TILEMAPS  );
		get_sprite_info();

		palette_init_used_colors();
		mark_sprite_colors(); // custom; normally this would be handled by the sprite manager
		sprite_update();

		if( palette_recalc() ) tilemap_mark_all_pixels_dirty( ALL_TILEMAPS );
		build_shadow_table();
		tilemap_render(  ALL_TILEMAPS  );

		if(sys18_bg2_active)
			tilemap_draw( bitmap, background2, 0 );
		else
			fillbitmap(bitmap,palette_transparent_color,&Machine->visible_area);

		tilemap_draw( bitmap, background, TILEMAP_IGNORE_TRANSPARENCY );
		tilemap_draw( bitmap, background, TILEMAP_IGNORE_TRANSPARENCY | 1 );	//??
		tilemap_draw( bitmap, background, TILEMAP_IGNORE_TRANSPARENCY | 2 );	//??

		sprite_draw(sprite_list,3);
		tilemap_draw( bitmap, background, 1 );
		sprite_draw(sprite_list,2);
		tilemap_draw( bitmap, background, 2 );

		if(sys18_fg2_active) tilemap_draw( bitmap, foreground2, 0 );
		tilemap_draw( bitmap, foreground, 0 );
		sprite_draw(sprite_list,1);
		if(sys18_fg2_active) tilemap_draw( bitmap, foreground2, 1 );
		tilemap_draw( bitmap, foreground, 1 );

		tilemap_draw( bitmap, text_layer, 1 );
		sprite_draw(sprite_list,0);
		tilemap_draw( bitmap, text_layer, 0 );
	}
}

extern int gr_bitmap_width;

static void gr_colors(void)
{
	int i;
	UINT16 ver_data;
	int colorflip;
	UINT8 *data_ver=gr_ver;

	for(i=0;i<224;i++)
	{
		ver_data=READ_WORD(data_ver);
		palette_used_colors[(READ_WORD(&gr_pal[(ver_data<<1)&0x1fe])&0xff) + gr_palette] = PALETTE_COLOR_USED;

		if(!((ver_data & 0x500) == 0x100 || (ver_data & 0x300) == 0x200))
		{
			ver_data=ver_data & 0x00ff;
			colorflip = (READ_WORD(&gr_flip[ver_data<<1]) >> 3) & 1;

			palette_used_colors[ gr_colorflip[colorflip][0] + gr_palette_default ] = PALETTE_COLOR_USED;
			palette_used_colors[ gr_colorflip[colorflip][1] + gr_palette_default ] = PALETTE_COLOR_USED;
			palette_used_colors[ gr_colorflip[colorflip][2] + gr_palette_default ] = PALETTE_COLOR_USED;
			palette_used_colors[ gr_colorflip[colorflip][3] + gr_palette_default ] = PALETTE_COLOR_USED;
		}
		data_ver+=2;
	}
}

static void render_gr(struct osd_bitmap *bitmap,int priority)
{
	int i,j;
	UINT8 *data = memory_region(REGION_GFX3);
	UINT8 *source;
	UINT8 *line;
	UINT16 *line16;
	UINT32 *line32;
	UINT8 *data_ver=gr_ver;
	UINT32 ver_data,hor_pos;
	UINT16 colors[5];
//	UINT8 colors[5];
	UINT32 fastfill;
	int colorflip;
	int yflip=0,ypos;
	int dx=1,xoff=0;

	UINT16 *paldata1 = Machine->gfx[0]->colortable + gr_palette;
	UINT16 *paldata2 = Machine->gfx[0]->colortable + gr_palette_default;

	priority=priority << 10;

	if (Machine->scrbitmap->depth == 16) /* 16 bit */
	{
		if( Machine->orientation & ORIENTATION_SWAP_XY )
		{
			if( Machine->orientation & ORIENTATION_FLIP_Y ){
				dx=-1;
				xoff=319;
			}
			if( Machine->orientation & ORIENTATION_FLIP_X ){
				yflip=1;
			}

			for(i=0;i<224;i++)
			{
				if(yflip) ypos=223-i;
				else ypos=i;
				ver_data=READ_WORD(data_ver);
				if((ver_data & 0x400) == priority)
				{
					colors[0] = paldata1[ READ_WORD(&gr_pal[(ver_data<<1)&0x1fe])&0xff ];

					if((ver_data & 0x500) == 0x100 || (ver_data & 0x300) == 0x200)
					{
						// fill line
						for(j=0;j<320;j++)
						{
							line16=(UINT16 *)bitmap->line[j]+ypos;
							*line16=colors[0];
						}
					}
					else
					{
						// copy line
						ver_data=ver_data & 0x00ff;
						colorflip = (READ_WORD(&gr_flip[ver_data<<1]) >> 3) & 1;

						colors[1] = paldata2[ gr_colorflip[colorflip][0] ];
						colors[2] = paldata2[ gr_colorflip[colorflip][1] ];
						colors[3] = paldata2[ gr_colorflip[colorflip][2] ];
						colors[4] = paldata2[ gr_colorflip[colorflip][3] ];

						hor_pos = (READ_WORD(&gr_hor[ver_data<<1]) );
						ver_data = ver_data << gr_bitmap_width;

						if(hor_pos & 0xf000)
						{
							// reverse
							hor_pos=((0-((hor_pos&0x7ff)^7))+0x9f8)&0x3ff;
						}
						else
						{
							// normal
							hor_pos=(hor_pos+0x200) & 0x3ff;
						}

						source = data + hor_pos + ver_data + 18 + 8;

						for(j=0;j<320;j++)
						{
							line16=(UINT16 *)bitmap->line[xoff+j*dx]+ypos;
							*line16 = colors[*source++];
						}
					}
				}
				data_ver+=2;
			}
		}
		else
		{
			if( Machine->orientation & ORIENTATION_FLIP_X ){
				dx=-1;
				xoff=319;
			}
			if( Machine->orientation & ORIENTATION_FLIP_Y ){
				yflip=1;
			}

			for(i=0;i<224;i++)
			{
				if(yflip) ypos=223-i;
				else ypos=i;
				ver_data=READ_WORD(data_ver);
				if((ver_data & 0x400) == priority)
				{
					colors[0] = paldata1[ READ_WORD(&gr_pal[(ver_data<<1)&0x1fe])&0xff ];

					if((ver_data & 0x500) == 0x100 || (ver_data & 0x300) == 0x200)
					{
						line16=(UINT16 *)bitmap->line[ypos];
						for(j=0;j<320;j++)
						{
							*line16++=colors[0];
						}
					}
					else
					{
						// copy line
						line16 = (UINT16 *)bitmap->line[ypos]+xoff;
						ver_data=ver_data & 0x00ff;
						colorflip = (READ_WORD(&gr_flip[ver_data<<1]) >> 3) & 1;

						colors[1] = paldata2[ gr_colorflip[colorflip][0] ];
						colors[2] = paldata2[ gr_colorflip[colorflip][1] ];
						colors[3] = paldata2[ gr_colorflip[colorflip][2] ];
						colors[4] = paldata2[ gr_colorflip[colorflip][3] ];

						hor_pos = (READ_WORD(&gr_hor[ver_data<<1]) );
						ver_data = ver_data << gr_bitmap_width;

						if(hor_pos & 0xf000)
						{
							// reverse
							hor_pos=((0-((hor_pos&0x7ff)^7))+0x9f8)&0x3ff;
						}
						else
						{
							// normal
							hor_pos=(hor_pos+0x200) & 0x3ff;
						}

						source = data + hor_pos + ver_data + 18 + 8;

						for(j=0;j<320;j++)
						{
							*line16 = colors[*source++];
							line16+=dx;
						}
					}
				}
				data_ver+=2;
			}
		}
	}
	else /* 8 bit */
	{
		if( Machine->orientation & ORIENTATION_SWAP_XY )
		{
			if( Machine->orientation & ORIENTATION_FLIP_Y ){
				dx=-1;
				xoff=319;
			}
			if( Machine->orientation & ORIENTATION_FLIP_X ){
				yflip=1;
			}

			for(i=0;i<224;i++)
			{
				if(yflip) ypos=223-i;
				else ypos=i;
				ver_data=READ_WORD(data_ver);
				if((ver_data & 0x400) == priority)
				{
					colors[0] = paldata1[ READ_WORD(&gr_pal[(ver_data<<1)&0x1fe])&0xff ];

					if((ver_data & 0x500) == 0x100 || (ver_data & 0x300) == 0x200)
					{
						// fill line
						for(j=0;j<320;j++)
						{
							bitmap->line[j][ypos]=colors[0];
						}
					}
					else
					{
						// copy line
						ver_data=ver_data & 0x00ff;
						colorflip = (READ_WORD(&gr_flip[ver_data<<1]) >> 3) & 1;

						colors[1] = paldata2[ gr_colorflip[colorflip][0] ];
						colors[2] = paldata2[ gr_colorflip[colorflip][1] ];
						colors[3] = paldata2[ gr_colorflip[colorflip][2] ];
						colors[4] = paldata2[ gr_colorflip[colorflip][3] ];

						hor_pos = (READ_WORD(&gr_hor[ver_data<<1]) );
						ver_data = ver_data << gr_bitmap_width;

						if(hor_pos & 0xf000)
						{
							// reverse
							hor_pos=((0-((hor_pos&0x7ff)^7))+0x9f8)&0x3ff;
						}
						else
						{
							// normal
							hor_pos=(hor_pos+0x200) & 0x3ff;
						}

						source = data + hor_pos + ver_data + 18 + 8;

						for(j=0;j<320;j++)
						{
							bitmap->line[xoff+j*dx][ypos] = colors[*source++];
						}
					}
				}
				data_ver+=2;
			}
		}
		else
		{
			if( Machine->orientation & ORIENTATION_FLIP_X ){
				dx=-1;
				xoff=319;
			}
			if( Machine->orientation & ORIENTATION_FLIP_Y ){
				yflip=1;
			}

			for(i=0;i<224;i++)
			{
				if(yflip) ypos=223-i;
				else ypos=i;
				ver_data=READ_WORD(data_ver);
				if((ver_data & 0x400) == priority)
				{
					colors[0] = paldata1[ READ_WORD(&gr_pal[(ver_data<<1)&0x1fe])&0xff ];

					if((ver_data & 0x500) == 0x100 || (ver_data & 0x300) == 0x200)
					{
						// fill line
						line32 = (UINT32 *)bitmap->line[ypos];
						fastfill = colors[0] + (colors[0] << 8) + (colors[0] << 16) + (colors[0] << 24);
						for(j=0;j<320;j+=4)
						{
							*line32++ = fastfill;
						}
					}
					else
					{
						// copy line
						line = bitmap->line[ypos]+xoff;
						ver_data=ver_data & 0x00ff;
						colorflip = (READ_WORD(&gr_flip[ver_data<<1]) >> 3) & 1;

						colors[1] = paldata2[ gr_colorflip[colorflip][0] ];
						colors[2] = paldata2[ gr_colorflip[colorflip][1] ];
						colors[3] = paldata2[ gr_colorflip[colorflip][2] ];
						colors[4] = paldata2[ gr_colorflip[colorflip][3] ];

						hor_pos = (READ_WORD(&gr_hor[ver_data<<1]) );
						ver_data = ver_data << gr_bitmap_width;

						if(hor_pos & 0xf000)
						{
							// reverse
							hor_pos=((0-((hor_pos&0x7ff)^7))+0x9f8)&0x3ff;
						}
						else
						{
							// normal
							hor_pos=(hor_pos+0x200) & 0x3ff;
						}

						source = data + hor_pos + ver_data + 18 + 8;

						for(j=0;j<320;j++)
						{
							*line = colors[*source++];
							line+=dx;
						}
					}
				}
				data_ver+=2;
			}
		}
	}
}




// Refresh for hang-on, etc.
void sys16_ho_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh){
	if( sys16_update_proc ) sys16_update_proc();
	update_page();

	// from sys16 emu (Not sure if this is the best place for this?)
	{
		static int freeze_counter=0;
		if (!sys16_refreshenable)
		{
			freeze_counter=4;
			sys16_freezepalette=1;
		}
		if (freeze_counter)
		{
			freeze_counter--;
			return;
		}
		else if(sys16_freezepalette)
		{
			sys16_refresh_palette();
			sys16_freezepalette=0;
		}
	}

	if( sys16_refreshenable ){

		tilemap_set_scrollx( background, 0, -320-sys16_bg_scrollx+sys16_bgxoffset );
		tilemap_set_scrollx( foreground, 0, -320-sys16_fg_scrollx+sys16_fgxoffset );

		tilemap_set_scrolly( background, 0, -256+sys16_bg_scrolly );
		tilemap_set_scrolly( foreground, 0, -256+sys16_fg_scrolly );

		tilemap_update(  ALL_TILEMAPS  );
		get_sprite_info();

		palette_init_used_colors();
		mark_sprite_colors(); // custom; normally this would be handled by the sprite manager
		sprite_update();
		gr_colors();

		if( palette_recalc() ) tilemap_mark_all_pixels_dirty( ALL_TILEMAPS );
		build_shadow_table();
		tilemap_render(  ALL_TILEMAPS  );

		render_gr(bitmap,0);

		tilemap_draw( bitmap, background, 0 );
		tilemap_draw( bitmap, foreground, 0 );

		render_gr(bitmap,1);

		sprite_draw(sprite_list,0);
		tilemap_draw( bitmap, text_layer, 0 );

	}

#ifdef SYS16_DEBUG
	dump_tilemap();
#endif
}


static void grv2_colors(void)
{
	int i;
	UINT16 ver_data;
	int colorflip,colorflip_info;
	UINT8 *data_ver=gr_ver;

	for(i=0;i<224;i++)
	{
		ver_data=READ_WORD(data_ver);

		if(!(ver_data & 0x800))
		{
			ver_data=ver_data & 0x01ff;
			colorflip_info = READ_WORD(&gr_flip[ver_data<<1]);

			palette_used_colors[ (((colorflip_info >> 8) & 0x1f) + 0x20) + gr_palette_default] = PALETTE_COLOR_USED;

			colorflip = (colorflip_info >> 3) & 1;

			palette_used_colors[ gr_colorflip[colorflip][0] + gr_palette_default ] = PALETTE_COLOR_USED;
			palette_used_colors[ gr_colorflip[colorflip][1] + gr_palette_default ] = PALETTE_COLOR_USED;
			palette_used_colors[ gr_colorflip[colorflip][2] + gr_palette_default ] = PALETTE_COLOR_USED;
		}
		else
		{
			palette_used_colors[(ver_data&0x3f) + gr_palette] = PALETTE_COLOR_USED;
		}
		data_ver+=2;
	}
}

static void render_grv2(struct osd_bitmap *bitmap,int priority)
{
	int i,j;
	UINT8 *data = memory_region(REGION_GFX3);
	UINT8 *source,*source2,*temp;
	UINT8 *line;
	UINT16 *line16;
	UINT32 *line32;
	UINT8 *data_ver=gr_ver;
	UINT32 ver_data,hor_pos,hor_pos2;
	UINT16 colors[5];
	UINT32 fastfill;
	int colorflip,colorflip_info;
	int yflip=0,ypos;
	int dx=1,xoff=0;

	int second_road = READ_WORD(gr_second_road);

	UINT16 *paldata1 = Machine->gfx[0]->colortable + gr_palette;
	UINT16 *paldata2 = Machine->gfx[0]->colortable + gr_palette_default;

	priority=priority << 11;

	if (Machine->scrbitmap->depth == 16) /* 16 bit */
	{
		if( Machine->orientation & ORIENTATION_SWAP_XY )
		{
			if( Machine->orientation & ORIENTATION_FLIP_Y ){
				dx=-1;
				xoff=319;
			}
			if( Machine->orientation & ORIENTATION_FLIP_X ){
				yflip=1;
			}

			for(i=0;i<224;i++)
			{
				if(yflip) ypos=223-i;
				else ypos=i;
				ver_data=READ_WORD(data_ver);
				if((ver_data & 0x800) == priority)
				{

					if(ver_data & 0x800)
					{
						colors[0] = paldata1[ ver_data&0x3f ];
						// fill line
						for(j=0;j<320;j++)
						{
							line16=(UINT16 *)bitmap->line[j]+ypos;
							*line16=colors[0];
						}
					}
					else
					{
						// copy line
						ver_data=ver_data & 0x01ff;		//???
						colorflip_info = READ_WORD(&gr_flip[ver_data<<1]);

						colors[0] = paldata2[ ((colorflip_info >> 8) & 0x1f) + 0x20 ];		//??

						colorflip = (colorflip_info >> 3) & 1;

						colors[1] = paldata2[ gr_colorflip[colorflip][0] ];
						colors[2] = paldata2[ gr_colorflip[colorflip][1] ];
						colors[3] = paldata2[ gr_colorflip[colorflip][2] ];

						hor_pos = (READ_WORD(&gr_hor[ver_data<<1]) );
						hor_pos2= (READ_WORD(&gr_hor[(ver_data<<1)+0x400]) );

						ver_data=ver_data>>1;
						if( ver_data != 0 )
						{
							ver_data = (ver_data-1) << gr_bitmap_width;
						}

						source  = data + ((hor_pos +0x200) & 0x7ff) + 768 + ver_data + 8;
						source2 = data + ((hor_pos2+0x200) & 0x7ff) + 768 + ver_data + 8;

						switch(second_road)
						{
							case 0:	source2=source;	break;
							case 2:	temp=source;source=source2;source2=temp; break;
							case 3:	source=source2;	break;
						}

						source2++;

						for(j=0;j<320;j++)
						{
							line16=(UINT16 *)bitmap->line[xoff+j*dx]+ypos;
							if(*source2 <= *source)
								*line16 = colors[*source];
							else
								*line16 = colors[*source2];
							source++;
							source2++;
						}
					}
				}
				data_ver+=2;
			}
		}
		else
		{
			if( Machine->orientation & ORIENTATION_FLIP_X ){
				dx=-1;
				xoff=319;
			}
			if( Machine->orientation & ORIENTATION_FLIP_Y ){
				yflip=1;
			}

			for(i=0;i<224;i++)
			{
				if(yflip) ypos=223-i;
				else ypos=i;
				ver_data=READ_WORD(data_ver);
				if((ver_data & 0x800) == priority)
				{

					if(ver_data & 0x800)
					{
						colors[0] = paldata1[ ver_data&0x3f ];
						// fill line
						line16 = (UINT16 *)bitmap->line[ypos];
						for(j=0;j<320;j++)
						{
							*line16++ = colors[0];
						}
					}
					else
					{
						// copy line
						line16 = (UINT16 *)bitmap->line[ypos]+xoff;
						ver_data=ver_data & 0x01ff;		//???
						colorflip_info = READ_WORD(&gr_flip[ver_data<<1]);

						colors[0] = paldata2[ ((colorflip_info >> 8) & 0x1f) + 0x20 ];		//??

						colorflip = (colorflip_info >> 3) & 1;

						colors[1] = paldata2[ gr_colorflip[colorflip][0] ];
						colors[2] = paldata2[ gr_colorflip[colorflip][1] ];
						colors[3] = paldata2[ gr_colorflip[colorflip][2] ];

						hor_pos = (READ_WORD(&gr_hor[ver_data<<1]) );
						hor_pos2= (READ_WORD(&gr_hor[(ver_data<<1)+0x400]) );

						ver_data=ver_data>>1;
						if( ver_data != 0 )
						{
							ver_data = (ver_data-1) << gr_bitmap_width;
						}

						source  = data + ((hor_pos +0x200) & 0x7ff) + 768 + ver_data + 8;
						source2 = data + ((hor_pos2+0x200) & 0x7ff) + 768 + ver_data + 8;

						switch(second_road)
						{
							case 0:	source2=source;	break;
							case 2:	temp=source;source=source2;source2=temp; break;
							case 3:	source=source2;	break;
						}

						source2++;

						for(j=0;j<320;j++)
						{
							if(*source2 <= *source)
								*line16 = colors[*source];
							else
								*line16 = colors[*source2];
							source++;
							source2++;
							line16+=dx;
						}
					}
				}
				data_ver+=2;
			}
		}
	}
	else
	{
		if( Machine->orientation & ORIENTATION_SWAP_XY )
		{
			if( Machine->orientation & ORIENTATION_FLIP_Y ){
				dx=-1;
				xoff=319;
			}
			if( Machine->orientation & ORIENTATION_FLIP_X ){
				yflip=1;
			}

			for(i=0;i<224;i++)
			{
				if(yflip) ypos=223-i;
				else ypos=i;
				ver_data=READ_WORD(data_ver);
				if((ver_data & 0x800) == priority)
				{

					if(ver_data & 0x800)
					{
						colors[0] = paldata1[ ver_data&0x3f ];
						// fill line
						for(j=0;j<320;j++)
						{
							bitmap->line[j][ypos]=colors[0];
						}
					}
					else
					{
						// copy line
						ver_data=ver_data & 0x01ff;		//???
						colorflip_info = READ_WORD(&gr_flip[ver_data<<1]);

						colors[0] = paldata2[ ((colorflip_info >> 8) & 0x1f) + 0x20 ];		//??

						colorflip = (colorflip_info >> 3) & 1;

						colors[1] = paldata2[ gr_colorflip[colorflip][0] ];
						colors[2] = paldata2[ gr_colorflip[colorflip][1] ];
						colors[3] = paldata2[ gr_colorflip[colorflip][2] ];

						hor_pos = (READ_WORD(&gr_hor[ver_data<<1]) );
						hor_pos2= (READ_WORD(&gr_hor[(ver_data<<1)+0x400]) );

						ver_data=ver_data>>1;
						if( ver_data != 0 )
						{
							ver_data = (ver_data-1) << gr_bitmap_width;
						}

						source  = data + ((hor_pos +0x200) & 0x7ff) + 768 + ver_data + 8;
						source2 = data + ((hor_pos2+0x200) & 0x7ff) + 768 + ver_data + 8;

						switch(second_road)
						{
							case 0:	source2=source;	break;
							case 2:	temp=source;source=source2;source2=temp; break;
							case 3:	source=source2;	break;
						}

						source2++;

						for(j=0;j<320;j++)
						{
							if(*source2 <= *source)
								bitmap->line[xoff+j*dx][ypos] = colors[*source];
							else
								bitmap->line[xoff+j*dx][ypos] = colors[*source2];
							source++;
							source2++;
						}
					}
				}
				data_ver+=2;
			}
		}
		else
		{
			if( Machine->orientation & ORIENTATION_FLIP_X ){
				dx=-1;
				xoff=319;
			}
			if( Machine->orientation & ORIENTATION_FLIP_Y ){
				yflip=1;
			}

			for(i=0;i<224;i++)
			{
				if(yflip) ypos=223-i;
				else ypos=i;
				ver_data=READ_WORD(data_ver);
				if((ver_data & 0x800) == priority)
				{

					if(ver_data & 0x800)
					{
						colors[0] = paldata1[ ver_data&0x3f ];
						// fill line
						line32 = (UINT32 *)bitmap->line[ypos];
						fastfill = colors[0] + (colors[0] << 8) + (colors[0] << 16) + (colors[0] << 24);
						for(j=0;j<320;j+=4)
						{
							*line32++ = fastfill;
						}
					}
					else
					{
						// copy line
						line = bitmap->line[ypos]+xoff;
						ver_data=ver_data & 0x01ff;		//???
						colorflip_info = READ_WORD(&gr_flip[ver_data<<1]);

						colors[0] = paldata2[ ((colorflip_info >> 8) & 0x1f) + 0x20 ];		//??

						colorflip = (colorflip_info >> 3) & 1;

						colors[1] = paldata2[ gr_colorflip[colorflip][0] ];
						colors[2] = paldata2[ gr_colorflip[colorflip][1] ];
						colors[3] = paldata2[ gr_colorflip[colorflip][2] ];

						hor_pos = (READ_WORD(&gr_hor[ver_data<<1]) );
						hor_pos2= (READ_WORD(&gr_hor[(ver_data<<1)+0x400]) );

						ver_data=ver_data>>1;
						if( ver_data != 0 )
						{
							ver_data = (ver_data-1) << gr_bitmap_width;
						}

						source  = data + ((hor_pos +0x200) & 0x7ff) + 768 + ver_data + 8;
						source2 = data + ((hor_pos2+0x200) & 0x7ff) + 768 + ver_data + 8;

						switch(second_road)
						{
							case 0:	source2=source;	break;
							case 2:	temp=source;source=source2;source2=temp; break;
							case 3:	source=source2;	break;
						}

						source2++;

						for(j=0;j<320;j++)
						{
							if(*source2 <= *source)
								*line = colors[*source];
							else
								*line = colors[*source2];
							source++;
							source2++;
							line+=dx;
						}
					}
				}
				data_ver+=2;
			}
		}
	}
}

// Refresh for Outrun
void sys16_or_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh){
	if( sys16_update_proc ) sys16_update_proc();
	update_page();

	// from sys16 emu (Not sure if this is the best place for this?)
	{
		static int freeze_counter=0;
		if (!sys16_refreshenable)
		{
			freeze_counter=4;
			sys16_freezepalette=1;
		}
		if (freeze_counter)
		{
			freeze_counter--;
			return;
		}
		else if(sys16_freezepalette)
		{
			sys16_refresh_palette();
			sys16_freezepalette=0;
		}
	}

	if( sys16_refreshenable ){

		tilemap_set_scrollx( background, 0, -320-sys16_bg_scrollx+sys16_bgxoffset );
		tilemap_set_scrollx( foreground, 0, -320-sys16_fg_scrollx+sys16_fgxoffset );

		tilemap_set_scrolly( background, 0, -256+sys16_bg_scrolly );
		tilemap_set_scrolly( foreground, 0, -256+sys16_fg_scrolly );

		tilemap_update(  ALL_TILEMAPS  );
		get_sprite_info();

		palette_init_used_colors();
		mark_sprite_colors(); // custom; normally this would be handled by the sprite manager
		sprite_update();
		grv2_colors();

		if( palette_recalc() ) tilemap_mark_all_pixels_dirty( ALL_TILEMAPS );
		build_shadow_table();
		tilemap_render(  ALL_TILEMAPS  );

		render_grv2(bitmap,1);

		tilemap_draw( bitmap, background, 0 );
		tilemap_draw( bitmap, foreground, 0 );

		render_grv2(bitmap,0);

		sprite_draw(sprite_list,0);

		tilemap_draw( bitmap, text_layer, 0 );

	}

#ifdef SYS16_DEBUG
	dump_tilemap();
#endif
}


// hideous kludge to display quartet title screen correctly
static void draw_quartet_title_screen( struct osd_bitmap *bitmap,int playfield )
{
	unsigned char *xscroll,*yscroll;
	int r,c,scroll;
	struct tilemap *tilemap;
	struct rectangle clip;

	int top,bottom,left,right;


	if(playfield==0) // background
	{
		xscroll=&sys16_textram[0x0fc0];
		yscroll=&sys16_textram[0x0f58];
		tilemap=background;
	}
	else
	{
		xscroll=&sys16_textram[0x0f80];
		yscroll=&sys16_textram[0x0f30];
		tilemap=foreground;
	}

	left = tilemap->clip_left;
	right = tilemap->clip_right;
	top = tilemap->clip_top;
	bottom = tilemap->clip_bottom;

	for(r=0;r<14;r++)
	{
		clip.min_y=r*16;
		clip.max_y=r*16+15;
		for(c=0;c<10;c++)
		{
			clip.min_x=c*32;
			clip.max_x=c*32+31;
			tilemap_set_clip( tilemap, &clip );
			scroll = READ_WORD(&xscroll[r*4])&0x3ff;
			tilemap_set_scrollx( tilemap, 0, (-320-scroll+sys16_bgxoffset)&0x3ff );
			scroll = READ_WORD(&yscroll[c*4])&0x1ff;
			tilemap_set_scrolly( tilemap, 0, (-256+scroll)&0x1ff );
			tilemap_draw( bitmap, tilemap, 0 );
		}
	}
/*
	for(r=0;r<28;r++)
	{
		clip.min_y=r*8;
		clip.max_y=r*8+7;
		for(c=0;c<20;c++)
		{
			clip.min_x=c*16;
			clip.max_x=c*16+15;
			tilemap_set_clip( tilemap, &clip );
			scroll = READ_WORD(&xscroll[r*2])&0x3ff;
			tilemap_set_scrollx( tilemap, 0, (-320-scroll+sys16_bgxoffset)&0x3ff );
			scroll = READ_WORD(&yscroll[c*2])&0x1ff;
			tilemap_set_scrolly( tilemap, 0, (-256+scroll)&0x1ff );
			tilemap_draw( bitmap, tilemap, 0 );
		}
	}
*/
	tilemap->clip_left = left;
	tilemap->clip_right = right;
	tilemap->clip_top = top;
	tilemap->clip_bottom = bottom;
}


#ifdef SYS16_DEBUG
void dump_tilemap(void)
{
	const UINT16 *source;
	int row,col,r,c;
	int data;

	if(!keyboard_pressed_memory(KEYCODE_Y)) return;

	logerror("Back %2x %2x %2x %2x\n",sys16_bg_page[0],sys16_bg_page[1],sys16_bg_page[2],sys16_bg_page[3]);
	for(r=0;r<64;r++)
	{
		for(c=0;c<128;c++)
		{
			source = (const UINT16 *)sys16_tileram;
			row=r;col=c;
			if( row<32 ){
				if( col<64 ){
					source += 64*32*sys16_bg_page[0];
				}
				else {
					source += 64*32*sys16_bg_page[1];
				}
			}
			else {
				if( col<64 ){
					source += 64*32*sys16_bg_page[2];
				}
				else {
					source += 64*32*sys16_bg_page[3];
				}
			}
			row = row%32;
			col = col%64;

			data = source[row*64+col];
			logerror("%4x ",data);
		}
		logerror("\n");
	}

	logerror("Front %2x %2x %2x %2x\n",sys16_fg_page[0],sys16_fg_page[1],sys16_fg_page[2],sys16_fg_page[3]);
	for(r=0;r<64;r++)
	{
		for(c=0;c<128;c++)
		{
			source = (const UINT16 *)sys16_tileram;
			row=r;col=c;
			if( row<32 ){
				if( col<64 ){
					source += 64*32*sys16_fg_page[0];
				}
				else {
					source += 64*32*sys16_fg_page[1];
				}
			}
			else {
				if( col<64 ){
					source += 64*32*sys16_fg_page[2];
				}
				else {
					source += 64*32*sys16_fg_page[3];
				}
			}
			row = row%32;
			col = col%64;

			data = source[row*64+col];
			logerror("%4x ",data);
		}
		logerror("\n");
	}

	logerror("Text\n");
	for(r=0;r<28;r++)
	{
		for(c=0;c<64;c++)
		{
			source = (const UINT16 *)sys16_textram;
			data = source[r*64+c];
			logerror("%4x ",data);
		}
		logerror("\n");
	}
}
#endif

