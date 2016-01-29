/* tilemap.c

In Progress
-	visibility walk (temporarily broken)
-	nowrap

To Do:
-	virtualization for huge tilemaps
-	precompute spans per row (to speed up the low level code)
-	support for unusual tile sizes (8x12, 8x10)
-	screenwise scrolling
-	internal profiling

	Usage Notes:

	When the videoram for a tile changes, call tilemap_mark_tile_dirty
	with the appropriate tile_index.

	In the video driver, follow these steps:

	1)	set each tilemap's scroll registers

	2)	call tilemap_update for each tilemap.

	3)	call palette_init_used_colors.
		mark the colors used by sprites.
		call palette recalc.  If the palette manager has compressed the palette,
			call tilemap_mark_all_pixels_dirty for each tilemap.

	4)	call tilemap_render for each tilemap.

	5)	call tilemap_draw to draw the tilemaps to the screen, from back to front
*/

#include "driver.h"
#include "tilemap.h"

#define SWAP(X,Y) {UINT32 temp=X; X=Y; Y=temp; }

/***********************************************************************************/
/* some common mappings */

UINT32 tilemap_scan_rows( UINT32 col, UINT32 row, UINT32 num_cols, UINT32 num_rows ){
	/* logical (col,row) -> memory offset */
	return row*num_cols + col;
}
UINT32 tilemap_scan_cols( UINT32 col, UINT32 row, UINT32 num_cols, UINT32 num_rows ){
	/* logical (col,row) -> memory offset */
	return col*num_rows + row;
}

/*********************************************************************************/

static struct osd_bitmap *create_tmpbitmap( int width, int height, int depth ){
	return osd_alloc_bitmap( width,height,depth );
}

static struct osd_bitmap *create_bitmask( int width, int height ){
	width = (width+7)>>3; /* 8 bits per byte */
	return osd_alloc_bitmap( width,height, 8 );
}

/***********************************************************************************/

static int mappings_create( struct tilemap *tilemap ){
	int max_memory_offset = 0;
	UINT32 col,row;
	UINT32 num_logical_rows = tilemap->num_logical_rows;
	UINT32 num_logical_cols = tilemap->num_logical_cols;
	/* count offsets (might be larger than num_tiles) */
	for( row=0; row<num_logical_rows; row++ ){
		for( col=0; col<num_logical_cols; col++ ){
			UINT32 memory_offset = tilemap->get_memory_offset( col, row, num_logical_cols, num_logical_rows );
			if( memory_offset>max_memory_offset ) max_memory_offset = memory_offset;
		}
	}
	max_memory_offset++;
	tilemap->max_memory_offset = max_memory_offset;
	/* logical to cached (tilemap_mark_dirty) */
	tilemap->memory_offset_to_cached_index = (int*)malloc( sizeof(int)*max_memory_offset );
	if( tilemap->memory_offset_to_cached_index ){
		/* cached to logical (get_tile_info) */
		tilemap->cached_index_to_memory_offset = (UINT32*)malloc( sizeof(UINT32)*tilemap->num_tiles );
		if( tilemap->cached_index_to_memory_offset ) return 0; /* no error */
		free( tilemap->memory_offset_to_cached_index );
	}
	return -1; /* error */
}

static void mappings_dispose( struct tilemap *tilemap ){
	free( tilemap->cached_index_to_memory_offset );
	free( tilemap->memory_offset_to_cached_index );
}

static void mappings_update( struct tilemap *tilemap ){
	int logical_flip;
	UINT32 logical_index, cached_index;
	UINT32 num_cached_rows = tilemap->num_cached_rows;
	UINT32 num_cached_cols = tilemap->num_cached_cols;
	UINT32 num_logical_rows = tilemap->num_logical_rows;
	UINT32 num_logical_cols = tilemap->num_logical_cols;
	for( logical_index=0; logical_index<tilemap->max_memory_offset; logical_index++ ){
		tilemap->memory_offset_to_cached_index[logical_index] = -1;
	}

	/*logerror("log size(%dx%d); cach size(%dx%d)\n",
			num_logical_cols,num_logical_rows,
			num_cached_cols,num_cached_rows);*/

	for( logical_index=0; logical_index<tilemap->num_tiles; logical_index++ ){
		UINT32 logical_col = logical_index%num_logical_cols;
		UINT32 logical_row = logical_index/num_logical_cols;
		int memory_offset = tilemap->get_memory_offset( logical_col, logical_row, num_logical_cols, num_logical_rows );
		UINT32 cached_col = logical_col;
		UINT32 cached_row = logical_row;
		if( tilemap->orientation & ORIENTATION_SWAP_XY ) SWAP(cached_col,cached_row)
		if( tilemap->orientation & ORIENTATION_FLIP_X ) cached_col = (num_cached_cols-1)-cached_col;
		if( tilemap->orientation & ORIENTATION_FLIP_Y ) cached_row = (num_cached_rows-1)-cached_row;
		cached_index = cached_row*num_cached_cols+cached_col;
		tilemap->memory_offset_to_cached_index[memory_offset] = cached_index;
		tilemap->cached_index_to_memory_offset[cached_index] = memory_offset;
	}
	for( logical_flip = 0; logical_flip<4; logical_flip++ ){
		int cached_flip = logical_flip;
		if( tilemap->attributes&TILEMAP_FLIPX ) cached_flip ^= TILE_FLIPX;
		if( tilemap->attributes&TILEMAP_FLIPY ) cached_flip ^= TILE_FLIPY;
#ifndef PREROTATE_GFX
		if( Machine->orientation & ORIENTATION_SWAP_XY ){
			if( Machine->orientation & ORIENTATION_FLIP_X ) cached_flip ^= TILE_FLIPY;
			if( Machine->orientation & ORIENTATION_FLIP_Y ) cached_flip ^= TILE_FLIPX;
		}
		else {
			if( Machine->orientation & ORIENTATION_FLIP_X ) cached_flip ^= TILE_FLIPX;
			if( Machine->orientation & ORIENTATION_FLIP_Y ) cached_flip ^= TILE_FLIPY;
		}
#endif
		if( tilemap->orientation & ORIENTATION_SWAP_XY ){
			cached_flip = ((cached_flip&1)<<1) | ((cached_flip&2)>>1);
		}
		tilemap->logical_flip_to_cached_flip[logical_flip] = cached_flip;
	}
}

/***********************************************************************************/

struct osd_bitmap *priority_bitmap; /* priority buffer (corresponds to screen bitmap) */
int priority_bitmap_line_offset;

static UINT8 flip_bit_table[0x100]; /* horizontal flip for 8 pixels */
static struct tilemap *first_tilemap; /* resource tracking */
static int screen_width, screen_height;
struct tile_info tile_info;

enum {
	TILE_TRANSPARENT,
	TILE_MASKED,
	TILE_OPAQUE
};

/* the following parameters are constant across tilemap_draw calls */
static struct {
	int clip_left, clip_top, clip_right, clip_bottom;
	int source_width, source_height;
	int dest_line_offset,source_line_offset,mask_line_offset;
	int dest_row_offset,source_row_offset,mask_row_offset;
	struct osd_bitmap *screen, *pixmap, *bitmask;
	UINT8 **mask_data_row;
	UINT8 **priority_data_row;
	int tile_priority;
	int tilemap_priority_code;
} blit;

#define MASKROWBYTES(W) (((W)+7)>>3)

static void memsetbitmask8( UINT8 *dest, int value, const UINT8 *bitmask, int count ){
/* TBA: combine with memcpybitmask */
	do{
		UINT32 data = *bitmask++;
		if( data&0x80 ) dest[0] |= value;
		if( data&0x40 ) dest[1] |= value;
		if( data&0x20 ) dest[2] |= value;
		if( data&0x10 ) dest[3] |= value;
		if( data&0x08 ) dest[4] |= value;
		if( data&0x04 ) dest[5] |= value;
		if( data&0x02 ) dest[6] |= value;
		if( data&0x01 ) dest[7] |= value;
		dest+=8;
		count--;
	}while (count);
}

/*
static void memcpybitmask8( UINT8 *dest, const UINT8 *source, const UINT8 *bitmask, int count ){
	do{
		UINT32 data = *bitmask++;
		if( data&0x80 ) dest[0] = source[0];
		if( data&0x40 ) dest[1] = source[1];
		if( data&0x20 ) dest[2] = source[2];
		if( data&0x10 ) dest[3] = source[3];
		if( data&0x08 ) dest[4] = source[4];
		if( data&0x04 ) dest[5] = source[5];
		if( data&0x02 ) dest[6] = source[6];
		if( data&0x01 ) dest[7] = source[7];
		source+=8;
		dest+=8;
		count--;
	}while (count);
}
*/

static void memcpybitmask8( UINT8 *dest, const UINT32 *source, const UINT8 *bitmask, int count ){
	do{
		UINT32 data = *bitmask++;
		UINT32 src = *source++;
		if( data&0x80 ) dest[0] = src;
		if( data&0x40 ) dest[1] = src>>8;
		if( data&0x20 ) dest[2] = src>>16;
		if( data&0x10 ) dest[3] = src>>24;;
		src=*source++;
		if( data&0x08 ) dest[4] = src;
		if( data&0x04 ) dest[5] = src>>8;
		if( data&0x02 ) dest[6] = src>>16;
		if( data&0x01 ) dest[7] = src>>24;
		dest+=8;
		count--;
	}while (count);
}

/***********************************************************************************/

/*
static void memcpybitmask16( UINT16 *dest, const UINT16 *source, const UINT8 *bitmask, int count ){
	do{
		UINT32 data = *bitmask++;
		if( data&0x80 ) dest[0] = source[0];
		if( data&0x40 ) dest[1] = source[1];
		if( data&0x20 ) dest[2] = source[2];
		if( data&0x10 ) dest[3] = source[3];
		if( data&0x08 ) dest[4] = source[4];
		if( data&0x04 ) dest[5] = source[5];
		if( data&0x02 ) dest[6] = source[6];
		if( data&0x01 ) dest[7] = source[7];
		source+=8;
		dest+=8;
		count--;
	}while (count);
}
*/

static void memcpybitmask16( UINT16 *dest, const UINT32 *source, const UINT8 *bitmask, int count ){
	do{
		UINT32 data = *bitmask++;
		UINT32 src = *source++;
		if( data&0x80 ) dest[0] = src;
		if( data&0x40 ) dest[1] = src>>16;
		src=*source++;
		if( data&0x20 ) dest[2] = src;
		if( data&0x10 ) dest[3] = src>>16;
		src=*source++;
		if( data&0x08 ) dest[4] = src;
		if( data&0x04 ) dest[5] = src>>16;
		src=*source++;
		if( data&0x02 ) dest[6] = src;
		if( data&0x01 ) dest[7] = src>>16;
		dest+=8;
		count--;
	}while (count);
}

/***********************************************************************************/

#define TILE_WIDTH	8
#define TILE_HEIGHT	8
#define DATA_TYPE UINT8
#define memcpybitmask memcpybitmask8
#define DECLARE(function,args,body) static void function##8x8x8BPP args body
#include "tilemap_draw.h"

#define TILE_WIDTH	16
#define TILE_HEIGHT	16
#define DATA_TYPE UINT8
#define memcpybitmask memcpybitmask8
#define DECLARE(function,args,body) static void function##16x16x8BPP args body
#include "tilemap_draw.h"

#define TILE_WIDTH	32
#define TILE_HEIGHT	32
#define DATA_TYPE UINT8
#define memcpybitmask memcpybitmask8
#define DECLARE(function,args,body) static void function##32x32x8BPP args body
#include "tilemap_draw.h"

#define TILE_WIDTH	8
#define TILE_HEIGHT	8
#define DATA_TYPE UINT16
#define memcpybitmask memcpybitmask16
#define DECLARE(function,args,body) static void function##8x8x16BPP args body
#include "tilemap_draw.h"

#define TILE_WIDTH	16
#define TILE_HEIGHT	16
#define DATA_TYPE UINT16
#define memcpybitmask memcpybitmask16
#define DECLARE(function,args,body) static void function##16x16x16BPP args body
#include "tilemap_draw.h"

#define TILE_WIDTH	32
#define TILE_HEIGHT	32
#define DATA_TYPE UINT16
#define memcpybitmask memcpybitmask16
#define DECLARE(function,args,body) static void function##32x32x16BPP args body
#include "tilemap_draw.h"

/*********************************************************************************/

static void mask_dispose( struct tilemap_mask *mask ){
	if( mask ){
		free( mask->data_row );
		free( mask->data );
		osd_free_bitmap( mask->bitmask );
		free( mask );
	}
}

static struct tilemap_mask *mask_create( struct tilemap *tilemap ){
	struct tilemap_mask *mask = (struct tilemap_mask *)malloc( sizeof(struct tilemap_mask) );
	if( mask ){
		mask->data = (UINT8*)malloc( tilemap->num_tiles );
		mask->data_row = (UINT8**)malloc( tilemap->num_cached_rows * sizeof(UINT8 *) );
		mask->bitmask = create_bitmask( tilemap->cached_width, tilemap->cached_height );
		if( mask->data && mask->data_row && mask->bitmask ){
			int row;
			for( row=0; row<tilemap->num_cached_rows; row++ ){
				mask->data_row[row] = mask->data + row*tilemap->num_cached_cols;
			}
			mask->line_offset = mask->bitmask->line[1] - mask->bitmask->line[0];
			return mask;
		}
	}
	mask_dispose( mask );
	return NULL;
}

/***********************************************************************************/

static void install_draw_handlers( struct tilemap *tilemap ){
	int tile_width = tilemap->cached_tile_width;
	int tile_height = tilemap->cached_tile_height;
	tilemap->draw = tilemap->draw_opaque = NULL;
	if( Machine->scrbitmap->depth==16 ){
		if( tile_width==8 && tile_height==8 ){
			tilemap->draw = draw8x8x16BPP;
			tilemap->draw_opaque = draw_opaque8x8x16BPP;
		}
		else if( tile_width==16 && tile_height==16 ){
			tilemap->draw = draw16x16x16BPP;
			tilemap->draw_opaque = draw_opaque16x16x16BPP;
		}
		else if( tile_width==32 && tile_height==32 ){
			tilemap->draw = draw32x32x16BPP;
			tilemap->draw_opaque = draw_opaque32x32x16BPP;
		}
	}
	else {
		if( tile_width==8 && tile_height==8 ){
			tilemap->draw = draw8x8x8BPP;
			tilemap->draw_opaque = draw_opaque8x8x8BPP;
		}
		else if( tile_width==16 && tile_height==16 ){
			tilemap->draw = draw16x16x8BPP;
			tilemap->draw_opaque = draw_opaque16x16x8BPP;
		}
		else if( tile_width==32 && tile_height==32 ){
			tilemap->draw = draw32x32x8BPP;
			tilemap->draw_opaque = draw_opaque32x32x8BPP;
		}
	}
}

/***********************************************************************************/

int tilemap_init( void ){
	UINT32 value, data, bit;
	for( value=0; value<0x100; value++ ){
		data = 0;
		for( bit=0; bit<8; bit++ ) if( (value>>bit)&1 ) data |= 0x80>>bit;
		flip_bit_table[value] = data;
	}
	screen_width = Machine->scrbitmap->width;
	screen_height = Machine->scrbitmap->height;
	first_tilemap = 0;
	priority_bitmap = create_tmpbitmap( screen_width, screen_height, 8 );
	if( priority_bitmap ){
		priority_bitmap_line_offset = priority_bitmap->line[1] - priority_bitmap->line[0];
		return 0;
	}
	return -1;
}

void tilemap_close( void ){
	while( first_tilemap ){
		struct tilemap *next = first_tilemap->next;
		tilemap_dispose( first_tilemap );
		first_tilemap = next;
	}
	osd_free_bitmap( priority_bitmap );
}

/***********************************************************************************/

struct tilemap *tilemap_create(
	void (*tile_get_info)( int memory_offset ),
	UINT32 (*get_memory_offset)( UINT32 col, UINT32 row, UINT32 num_cols, UINT32 num_rows ),
	int type,
	int tile_width, int tile_height, /* in pixels */
	int num_cols, int num_rows /* in tiles */
){
	struct tilemap *tilemap = (struct tilemap *)calloc( 1,sizeof( struct tilemap ) );
	if( tilemap ){
		int num_tiles = num_cols*num_rows;
		tilemap->num_logical_cols = num_cols;
		tilemap->num_logical_rows = num_rows;
		if( Machine->orientation & ORIENTATION_SWAP_XY ){
		/*logerror("swap!!\n" );*/
			SWAP( tile_width, tile_height )
			SWAP( num_cols,num_rows )
		}
		tilemap->num_cached_cols = num_cols;
		tilemap->num_cached_rows = num_rows;
		tilemap->num_tiles = num_tiles;
		tilemap->cached_tile_width = tile_width;
		tilemap->cached_tile_height = tile_height;
		tilemap->cached_width = tile_width*num_cols;
		tilemap->cached_height = tile_height*num_rows;
		tilemap->tile_get_info = tile_get_info;
		tilemap->get_memory_offset = get_memory_offset;
		tilemap->orientation = Machine->orientation;
		tilemap->enable = 1;
		tilemap->type = type;
		tilemap->scroll_rows = 1;
		tilemap->scroll_cols = 1;
		tilemap->transparent_pen = -1;
		tilemap->cached_tile_info = (struct cached_tile_info*)calloc( num_tiles, sizeof(struct cached_tile_info) );
		tilemap->priority = (UINT8*)calloc( num_tiles,1 );
		tilemap->visible = (UINT8*)calloc( num_tiles,1 );
		tilemap->dirty_vram = (UINT8*)malloc( num_tiles );
		tilemap->dirty_pixels = (UINT8*)malloc( num_tiles );
		tilemap->rowscroll = (int*)calloc(tilemap->cached_height,sizeof(int));
		tilemap->colscroll = (int*)calloc(tilemap->cached_width,sizeof(int));
		tilemap->priority_row = (UINT8**)malloc( sizeof(UINT8 *)*num_rows );
		tilemap->pixmap = create_tmpbitmap( tilemap->cached_width, tilemap->cached_height, Machine->scrbitmap->depth );
		tilemap->foreground = mask_create( tilemap );
		tilemap->background = (type & TILEMAP_SPLIT)?mask_create( tilemap ):NULL;
		if( tilemap->cached_tile_info &&
			tilemap->priority && tilemap->visible &&
			tilemap->dirty_vram && tilemap->dirty_pixels &&
			tilemap->rowscroll && tilemap->colscroll &&
			tilemap->priority_row &&
			tilemap->pixmap && tilemap->foreground &&
			((type&TILEMAP_SPLIT)==0 || tilemap->background) &&
			(mappings_create( tilemap )==0)
		){
			UINT32 row;
			for( row=0; row<num_rows; row++ ){
				tilemap->priority_row[row] = tilemap->priority+num_cols*row;
			}
			install_draw_handlers( tilemap );
			mappings_update( tilemap );
			tilemap_set_clip( tilemap, &Machine->visible_area );
			memset( tilemap->dirty_vram, 1, num_tiles );
			memset( tilemap->dirty_pixels, 1, num_tiles );
			tilemap->pixmap_line_offset = tilemap->pixmap->line[1] - tilemap->pixmap->line[0];
			tilemap->next = first_tilemap;
			first_tilemap = tilemap;
			return tilemap;
		}
		tilemap_dispose( tilemap );
	}
	return 0;
}

void tilemap_dispose( struct tilemap *tilemap ){
	if( tilemap==first_tilemap ){
		first_tilemap = tilemap->next;
	}
	else {
		struct tilemap *prev = first_tilemap;
		while( prev->next != tilemap ) prev = prev->next;
		prev->next =tilemap->next;
	}

	free( tilemap->cached_tile_info );
	free( tilemap->priority );
	free( tilemap->visible );
	free( tilemap->dirty_vram );
	free( tilemap->dirty_pixels );
	free( tilemap->rowscroll );
	free( tilemap->colscroll );
	free( tilemap->priority_row );
	osd_free_bitmap( tilemap->pixmap );
	mask_dispose( tilemap->foreground );
	mask_dispose( tilemap->background );
	mappings_dispose( tilemap );
	free( tilemap );
}

/***********************************************************************************/

static void unregister_pens( struct cached_tile_info *cached_tile_info, int num_pens ){
	const UINT16 *pal_data = cached_tile_info->pal_data;
	if( pal_data ){
		UINT32 pen_usage = cached_tile_info->pen_usage;
		if( pen_usage ){
			palette_decrease_usage_count(
				pal_data-Machine->remapped_colortable,
				pen_usage,
				PALETTE_COLOR_VISIBLE|PALETTE_COLOR_CACHED );
		}
		else {
			palette_decrease_usage_countx(
				pal_data-Machine->remapped_colortable,
				num_pens,
				cached_tile_info->pen_data,
				PALETTE_COLOR_VISIBLE|PALETTE_COLOR_CACHED );
		}
		cached_tile_info->pal_data = NULL;
	}
}

static void register_pens( struct cached_tile_info *cached_tile_info, int num_pens ){
	UINT32 pen_usage = cached_tile_info->pen_usage;
	if( pen_usage ){
		palette_increase_usage_count(
			cached_tile_info->pal_data-Machine->remapped_colortable,
			pen_usage,
			PALETTE_COLOR_VISIBLE|PALETTE_COLOR_CACHED );
	}
	else {
		palette_increase_usage_countx(
			cached_tile_info->pal_data-Machine->remapped_colortable,
			num_pens,
			cached_tile_info->pen_data,
			PALETTE_COLOR_VISIBLE|PALETTE_COLOR_CACHED );
	}
}

/***********************************************************************************/

void tilemap_set_enable( struct tilemap *tilemap, int enable ){
	tilemap->enable = enable;
}

void tilemap_set_transparent_pen( struct tilemap *tilemap, int pen ){
	tilemap->transparent_pen = pen;
}

void tilemap_set_flip( struct tilemap *tilemap, int attributes ){
	if( tilemap==ALL_TILEMAPS ){
		tilemap = first_tilemap;
		while( tilemap ){
			tilemap_set_flip( tilemap, attributes );
			tilemap = tilemap->next;
		}
	}
	else if( tilemap->attributes!=attributes ){
		tilemap->attributes = attributes;
		tilemap->orientation = Machine->orientation;
		if( attributes&TILEMAP_FLIPY ){
			tilemap->orientation ^= ORIENTATION_FLIP_Y;
			tilemap->scrolly_delta = tilemap->dy_if_flipped;
		}
		else {
			tilemap->scrolly_delta = tilemap->dy;
		}
		if( attributes&TILEMAP_FLIPX ){
			tilemap->orientation ^= ORIENTATION_FLIP_X;
			tilemap->scrollx_delta = tilemap->dx_if_flipped;
		}
		else {
			tilemap->scrollx_delta = tilemap->dx;
		}

		mappings_update( tilemap );
		tilemap_mark_all_tiles_dirty( tilemap );
	}
}

void tilemap_set_clip( struct tilemap *tilemap, const struct rectangle *clip ){
	int left,top,right,bottom;
	if( clip ){
		left = clip->min_x;
		top = clip->min_y;
		right = clip->max_x+1;
		bottom = clip->max_y+1;
		if( tilemap->orientation & ORIENTATION_SWAP_XY ){
			SWAP(left,top)
			SWAP(right,bottom)
		}
		if( tilemap->orientation & ORIENTATION_FLIP_X ){
			SWAP(left,right)
			left = screen_width-left;
			right = screen_width-right;
		}
		if( tilemap->orientation & ORIENTATION_FLIP_Y ){
			SWAP(top,bottom)
			top = screen_height-top;
			bottom = screen_height-bottom;
		}
	}
	else {
		left = 0;
		top = 0;
		right = tilemap->cached_width;
		bottom = tilemap->cached_height;
	}
	tilemap->clip_left = left;
	tilemap->clip_right = right;
	tilemap->clip_top = top;
	tilemap->clip_bottom = bottom;
//	logerror("clip: %d,%d,%d,%d\n", left,top,right,bottom );
}

/***********************************************************************************/

void tilemap_set_scroll_cols( struct tilemap *tilemap, int n ){
	if( tilemap->orientation & ORIENTATION_SWAP_XY ){
		if (tilemap->scroll_rows != n){
			tilemap->scroll_rows = n;
		}
	}
	else {
		if (tilemap->scroll_cols != n){
			tilemap->scroll_cols = n;
		}
	}
}

void tilemap_set_scroll_rows( struct tilemap *tilemap, int n ){
	if( tilemap->orientation & ORIENTATION_SWAP_XY ){
		if (tilemap->scroll_cols != n){
			tilemap->scroll_cols = n;
		}
	}
	else {
		if (tilemap->scroll_rows != n){
			tilemap->scroll_rows = n;
		}
	}
}

/***********************************************************************************/

void tilemap_mark_tile_dirty( struct tilemap *tilemap, int memory_offset ){
	if( memory_offset<tilemap->max_memory_offset ){
		int cached_index = tilemap->memory_offset_to_cached_index[memory_offset];
		if( cached_index>=0 ){
			tilemap->dirty_vram[cached_index] = 1;
		}
	}
}

void tilemap_mark_all_tiles_dirty( struct tilemap *tilemap ){
	if( tilemap==ALL_TILEMAPS ){
		tilemap = first_tilemap;
		while( tilemap ){
			tilemap_mark_all_tiles_dirty( tilemap );
			tilemap = tilemap->next;
		}
	}
	else {
		memset( tilemap->dirty_vram, 1, tilemap->num_tiles );
	}
}

void tilemap_mark_all_pixels_dirty( struct tilemap *tilemap ){
	if( tilemap==ALL_TILEMAPS ){
		tilemap = first_tilemap;
		while( tilemap ){
			tilemap_mark_all_pixels_dirty( tilemap );
			tilemap = tilemap->next;
		}
	}
	else {
		/* invalidate all offscreen tiles */
		UINT32 cached_tile_index;
		UINT32 num_pens = tilemap->cached_tile_width*tilemap->cached_tile_height;
		for( cached_tile_index=0; cached_tile_index<tilemap->num_tiles; cached_tile_index++ ){
			if( !tilemap->visible[cached_tile_index] ){
				unregister_pens( &tilemap->cached_tile_info[cached_tile_index], num_pens );
				tilemap->dirty_vram[cached_tile_index] = 1;
			}
		}
		memset( tilemap->dirty_pixels, 1, tilemap->num_tiles );
	}
}

/***********************************************************************************/

static void draw_tile(
		struct tilemap *tilemap,
		UINT32 cached_index,
		UINT32 col, UINT32 row
){
	struct osd_bitmap *pixmap = tilemap->pixmap;
	UINT32 tile_width = tilemap->cached_tile_width;
	UINT32 tile_height = tilemap->cached_tile_height;
	struct cached_tile_info *cached_tile_info = &tilemap->cached_tile_info[cached_index];
	const UINT8 *pendata = cached_tile_info->pen_data;
	const UINT16 *paldata = cached_tile_info->pal_data;

	UINT32 flags = cached_tile_info->flags;
	int x, sx = tile_width*col;
	int sy,y1,y2,dy;

	if( Machine->scrbitmap->depth==16 ){
		if( flags&TILE_FLIPY ){
			y1 = tile_height*row+tile_height-1;
			y2 = y1-tile_height;
	 		dy = -1;
	 	}
	 	else {
			y1 = tile_height*row;
			y2 = y1+tile_height;
	 		dy = 1;
	 	}

		if( flags&TILE_FLIPX ){
			tile_width--;
			for( sy=y1; sy!=y2; sy+=dy ){
				UINT16 *dest  = sx + (UINT16 *)pixmap->line[sy];
				for( x=tile_width; x>=0; x-- ) dest[x] = paldata[*pendata++];
			}
		}
		else {
			for( sy=y1; sy!=y2; sy+=dy ){
				UINT16 *dest  = sx + (UINT16 *)pixmap->line[sy];
				for( x=0; x<tile_width; x++ ) dest[x] = paldata[*pendata++];
			}
		}
	}
	else {
		if( flags&TILE_FLIPY ){
			y1 = tile_height*row+tile_height-1;
			y2 = y1-tile_height;
	 		dy = -1;
	 	}
	 	else {
			y1 = tile_height*row;
			y2 = y1+tile_height;
	 		dy = 1;
	 	}

		if( flags&TILE_FLIPX ){
			tile_width--;
			for( sy=y1; sy!=y2; sy+=dy ){
				UINT8 *dest  = sx + (UINT8 *)pixmap->line[sy];
				for( x=tile_width; x>=0; x-- ) dest[x] = paldata[*pendata++];
			}
		}
		else {
			for( sy=y1; sy!=y2; sy+=dy ){
				UINT8 *dest  = sx + (UINT8 *)pixmap->line[sy];
				for( x=0; x<tile_width; x++ ) dest[x] = paldata[*pendata++];
			}
		}
	}
}

void tilemap_render( struct tilemap *tilemap ){
profiler_mark(PROFILER_TILEMAP_RENDER);
	if( tilemap==ALL_TILEMAPS ){
		tilemap = first_tilemap;
		while( tilemap ){
			tilemap_render( tilemap );
			tilemap = tilemap->next;
		}
	}
	else if( tilemap->enable ){
		UINT8 *dirty_pixels = tilemap->dirty_pixels;
		const UINT8 *visible = tilemap->visible;
		UINT32 cached_index = 0;
		UINT32 row,col;

		/* walk over cached rows/cols (better to walk screen coords) */
		for( row=0; row<tilemap->num_cached_rows; row++ ){
			for( col=0; col<tilemap->num_cached_cols; col++ ){
				if( visible[cached_index] && dirty_pixels[cached_index] ){
					draw_tile( tilemap, cached_index, col, row );
					dirty_pixels[cached_index] = 0;
				}
				cached_index++;
			} /* next col */
		} /* next row */
	}
profiler_mark(PROFILER_END);
}

/***********************************************************************************/

static int draw_bitmask(
		struct osd_bitmap *mask,
		UINT32 col, UINT32 row,
		UINT32 tile_width, UINT32 tile_height,
		const UINT8 *maskdata,
		UINT32 flags )
{
	int is_opaque = 1, is_transparent = 1;
	int x,sx = tile_width*col;
	int sy,y1,y2,dy;

	if( maskdata==TILEMAP_BITMASK_TRANSPARENT ) return TILE_TRANSPARENT;
	if( maskdata==TILEMAP_BITMASK_OPAQUE) return TILE_OPAQUE;

	if( flags&TILE_FLIPY ){
		y1 = tile_height*row+tile_height-1;
		y2 = y1-tile_height;
 		dy = -1;
 	}
 	else {
		y1 = tile_height*row;
		y2 = y1+tile_height;
 		dy = 1;
 	}

	if( flags&TILE_FLIPX ){
		tile_width--;
		for( sy=y1; sy!=y2; sy+=dy ){
			UINT8 *mask_dest  = mask->line[sy]+(sx>>3);
			for( x=tile_width>>3; x>=0; x-- ){
				UINT8 data = flip_bit_table[*maskdata++];
				if( data!=0x00 ) is_transparent = 0;
				if( data!=0xff ) is_opaque = 0;
				mask_dest[x] = data;
			}
		}
	}
	else {
		for( sy=y1; sy!=y2; sy+=dy ){
			UINT8 *mask_dest  = mask->line[sy]+(sx>>3);
			for( x=0; x<(tile_width>>3); x++ ){
				UINT8 data = *maskdata++;
				if( data!=0x00 ) is_transparent = 0;
				if( data!=0xff ) is_opaque = 0;
				mask_dest[x] = data;
			}
		}
	}

	if( is_transparent ) return TILE_TRANSPARENT;
	if( is_opaque ) return TILE_OPAQUE;
	return TILE_MASKED;
}

static int draw_color_mask(
	struct osd_bitmap *mask,
	UINT32 col, UINT32 row,
	UINT32 tile_width, UINT32 tile_height,
	const UINT8 *pendata,
	const UINT16 *clut,
	int transparent_color,
	UINT32 flags )
{
	int is_opaque = 1, is_transparent = 1;

	int x,bit,sx = tile_width*col;
	int sy,y1,y2,dy;

	if( flags&TILE_FLIPY ){
		y1 = tile_height*row+tile_height-1;
		y2 = y1-tile_height;
 		dy = -1;
 	}
 	else {
		y1 = tile_height*row;
		y2 = y1+tile_height;
 		dy = 1;
 	}

	if( flags&TILE_FLIPX ){
		tile_width--;
		for( sy=y1; sy!=y2; sy+=dy ){
			UINT8 *mask_dest  = mask->line[sy]+(sx>>3);
			for( x=(tile_width>>3); x>=0; x-- ){
				UINT32 data = 0;
				for( bit=0; bit<8; bit++ ){
					UINT32 pen = *pendata++;
					data = data>>1;
					if( clut[pen]!=transparent_color ) data |=0x80;
				}
				if( data!=0x00 ) is_transparent = 0;
				if( data!=0xff ) is_opaque = 0;
				mask_dest[x] = data;
			}
		}
	}
	else {
		for( sy=y1; sy!=y2; sy+=dy ){
			UINT8 *mask_dest  = mask->line[sy]+(sx>>3);
			for( x=0; x<(tile_width>>3); x++ ){
				UINT32 data = 0;
				for( bit=0; bit<8; bit++ ){
					UINT32 pen = *pendata++;
					data = data<<1;
					if( clut[pen]!=transparent_color ) data |=0x01;
				}
				if( data!=0x00 ) is_transparent = 0;
				if( data!=0xff ) is_opaque = 0;
				mask_dest[x] = data;
			}
		}
	}
	if( is_transparent ) return TILE_TRANSPARENT;
	if( is_opaque ) return TILE_OPAQUE;
	return TILE_MASKED;
}

static int draw_pen_mask(
	struct osd_bitmap *mask,
	UINT32 col, UINT32 row,
	UINT32 tile_width, UINT32 tile_height,
	const UINT8 *pendata,
	int transparent_pen,
	UINT32 flags )
{
	int is_opaque = 1, is_transparent = 1;

	int x,bit,sx = tile_width*col;
	int sy,y1,y2,dy;

	if( flags&TILE_FLIPY ){
		y1 = tile_height*row+tile_height-1;
		y2 = y1-tile_height;
 		dy = -1;
 	}
 	else {
		y1 = tile_height*row;
		y2 = y1+tile_height;
 		dy = 1;
 	}

	if( flags&TILE_FLIPX ){
		tile_width--;
		for( sy=y1; sy!=y2; sy+=dy ){
			UINT8 *mask_dest  = mask->line[sy]+(sx>>3);
			for( x=(tile_width>>3); x>=0; x-- ){
				UINT32 data = 0;
				for( bit=0; bit<8; bit++ ){
					UINT32 pen = *pendata++;
					data = data>>1;
					if( pen!=transparent_pen ) data |=0x80;
				}
				if( data!=0x00 ) is_transparent = 0;
				if( data!=0xff ) is_opaque = 0;
				mask_dest[x] = data;
			}
		}
	}
	else {
		for( sy=y1; sy!=y2; sy+=dy ){
			UINT8 *mask_dest  = mask->line[sy]+(sx>>3);
			for( x=0; x<(tile_width>>3); x++ ){
				UINT32 data = 0;
				for( bit=0; bit<8; bit++ ){
					UINT32 pen = *pendata++;
					data = data<<1;
					if( pen!=transparent_pen ) data |=0x01;
				}
				if( data!=0x00 ) is_transparent = 0;
				if( data!=0xff ) is_opaque = 0;
				mask_dest[x] = data;
			}
		}
	}
	if( is_transparent ) return TILE_TRANSPARENT;
	if( is_opaque ) return TILE_OPAQUE;
	return TILE_MASKED;
}

static void draw_mask(
	struct osd_bitmap *mask,
	UINT32 col, UINT32 row,
	UINT32 tile_width, UINT32 tile_height,
	const UINT8 *pendata,
	UINT32 transmask,
	UINT32 flags )
{
	int x,bit,sx = tile_width*col;
	int sy,y1,y2,dy;

	if( flags&TILE_FLIPY ){
		y1 = tile_height*row+tile_height-1;
		y2 = y1-tile_height;
 		dy = -1;
 	}
 	else {
		y1 = tile_height*row;
		y2 = y1+tile_height;
 		dy = 1;
 	}

	if( flags&TILE_FLIPX ){
		tile_width--;
		for( sy=y1; sy!=y2; sy+=dy ){
			UINT8 *mask_dest  = mask->line[sy]+(sx>>3);
			for( x=(tile_width>>3); x>=0; x-- ){
				UINT32 data = 0;
				for( bit=0; bit<8; bit++ ){
					UINT32 pen = *pendata++;
					data = data>>1;
					if( !((1<<pen)&transmask) ) data |= 0x80;
				}
				mask_dest[x] = data;
			}
		}
	}
	else {
		for( sy=y1; sy!=y2; sy+=dy ){
			UINT8 *mask_dest  = mask->line[sy]+(sx>>3);
			for( x=0; x<(tile_width>>3); x++ ){
				UINT32 data = 0;
				for( bit=0; bit<8; bit++ ){
					UINT32 pen = *pendata++;
					data = (data<<1);
					if( !((1<<pen)&transmask) ) data |= 0x01;
				}
				mask_dest[x] = data;
			}
		}
	}
}

static void render_mask( struct tilemap *tilemap, UINT32 cached_index ){
	const struct cached_tile_info *cached_tile_info = &tilemap->cached_tile_info[cached_index];
	UINT32 col = cached_index%tilemap->num_cached_cols;
	UINT32 row = cached_index/tilemap->num_cached_cols;
	UINT32 type = tilemap->type;

	UINT32 transparent_pen = tilemap->transparent_pen;
	UINT32 *transmask = tilemap->transmask;
	UINT32 tile_width = tilemap->cached_tile_width;
	UINT32 tile_height = tilemap->cached_tile_height;

	UINT32 pen_usage = cached_tile_info->pen_usage;
	const UINT8 *pen_data = cached_tile_info->pen_data;
	UINT32 flags = cached_tile_info->flags;

	if( type & TILEMAP_BITMASK ){
		tilemap->foreground->data_row[row][col] =
			draw_bitmask( tilemap->foreground->bitmask,col, row,
				tile_width, tile_height,tile_info.mask_data, flags );
	}
	else if( type & TILEMAP_SPLIT ){
		UINT32 pen_mask = (transparent_pen<0)?0:(1<<transparent_pen);
		if( flags&TILE_IGNORE_TRANSPARENCY ){
			tilemap->foreground->data_row[row][col] = TILE_OPAQUE;
			tilemap->background->data_row[row][col] = TILE_OPAQUE;
		}
		else if( pen_mask == pen_usage ){ /* totally transparent */
			tilemap->foreground->data_row[row][col] = TILE_TRANSPARENT;
			tilemap->background->data_row[row][col] = TILE_TRANSPARENT;
		}
		else {
			UINT32 fg_transmask = transmask[(flags>>2)&3];
			UINT32 bg_transmask = (~fg_transmask)|pen_mask;
			if( (pen_usage & fg_transmask)==0 ){ /* foreground totally opaque */
				tilemap->foreground->data_row[row][col] = TILE_OPAQUE;
				tilemap->background->data_row[row][col] = TILE_TRANSPARENT;
			}
			else if( (pen_usage & bg_transmask)==0 ){ /* background totally opaque */
				tilemap->foreground->data_row[row][col] = TILE_TRANSPARENT;
				tilemap->background->data_row[row][col] = TILE_OPAQUE;
			}
			else if( (pen_usage & ~bg_transmask)==0 ){ /* background transparent */
				draw_mask( tilemap->foreground->bitmask,
					col, row, tile_width, tile_height,
					pen_data, fg_transmask, flags );
				tilemap->foreground->data_row[row][col] = TILE_MASKED;
				tilemap->background->data_row[row][col] = TILE_TRANSPARENT;
			}
			else if( (pen_usage & ~fg_transmask)==0 ){ /* foreground transparent */
				draw_mask( tilemap->background->bitmask,
					col, row, tile_width, tile_height,
					pen_data, bg_transmask, flags );
				tilemap->foreground->data_row[row][col] = TILE_TRANSPARENT;
				tilemap->background->data_row[row][col] = TILE_MASKED;
			}
			else { /* split tile - opacity in both foreground and background */
				draw_mask( tilemap->foreground->bitmask,
					col, row, tile_width, tile_height,
					pen_data, fg_transmask, flags );
				draw_mask( tilemap->background->bitmask,
					col, row, tile_width, tile_height,
					pen_data, bg_transmask, flags );
				tilemap->foreground->data_row[row][col] = TILE_MASKED;
				tilemap->background->data_row[row][col] = TILE_MASKED;
			}
		}
	}
	else if( type==TILEMAP_TRANSPARENT ){
		if( pen_usage ){
			UINT32 fg_transmask = 1 << transparent_pen;
		 	if( flags&TILE_IGNORE_TRANSPARENCY ) fg_transmask = 0;
			if( pen_usage == fg_transmask ){
				tilemap->foreground->data_row[row][col] = TILE_TRANSPARENT;
			}
			else if( pen_usage & fg_transmask ){
				draw_mask( tilemap->foreground->bitmask,
					col, row, tile_width, tile_height,
					pen_data, fg_transmask, flags );
				tilemap->foreground->data_row[row][col] = TILE_MASKED;
			}
			else {
				tilemap->foreground->data_row[row][col] = TILE_OPAQUE;
			}
		}
		else {
			tilemap->foreground->data_row[row][col] =
				draw_pen_mask(
					tilemap->foreground->bitmask,
					col, row, tile_width, tile_height,
					pen_data,
					transparent_pen,
					flags
				);
		}
	}
	else if( type==TILEMAP_TRANSPARENT_COLOR ){
		tilemap->foreground->data_row[row][col] =
			draw_color_mask(
				tilemap->foreground->bitmask,
				col, row, tile_width, tile_height,
				pen_data,
				Machine->game_colortable +
					(cached_tile_info->pal_data - Machine->remapped_colortable),
				transparent_pen,
				flags
			);
	}
	else {
		tilemap->foreground->data_row[row][col] = TILE_OPAQUE;
	}
}

static void update_tile_info( struct tilemap *tilemap ){
	int *logical_flip_to_cached_flip = tilemap->logical_flip_to_cached_flip;
	UINT32 num_pens = tilemap->cached_tile_width*tilemap->cached_tile_height;
	UINT32 num_tiles = tilemap->num_tiles;
	UINT32 cached_index;
	UINT8 *visible = tilemap->visible;
	UINT8 *dirty_vram = tilemap->dirty_vram;
	UINT8 *dirty_pixels = tilemap->dirty_pixels;
	tile_info.flags = 0;
	tile_info.priority = 0;
	for( cached_index=0; cached_index<num_tiles; cached_index++ ){
		if( visible[cached_index] && dirty_vram[cached_index] ){
			struct cached_tile_info *cached_tile_info = &tilemap->cached_tile_info[cached_index];
			UINT32 memory_offset = tilemap->cached_index_to_memory_offset[cached_index];
			unregister_pens( cached_tile_info, num_pens );
			tilemap->tile_get_info( memory_offset );
			{
				UINT32 flags = tile_info.flags;
				cached_tile_info->flags = (flags&0xfc)|logical_flip_to_cached_flip[flags&0x3];
			}
			cached_tile_info->pen_usage = tile_info.pen_usage;
			cached_tile_info->pen_data = tile_info.pen_data;
			cached_tile_info->pal_data = tile_info.pal_data;
			tilemap->priority[cached_index] = tile_info.priority;
			register_pens( cached_tile_info, num_pens );
			dirty_pixels[cached_index] = 1;
			dirty_vram[cached_index] = 0;
			render_mask( tilemap, cached_index );
		}
	}
}

static void update_visible( struct tilemap *tilemap ){
	// temporary hack
	memset( tilemap->visible, 1, tilemap->num_tiles );

#if 0
	int yscroll = scrolly[0];
	int row0, y0;

	int xscroll = scrollx[0];
	int col0, x0;

	if( yscroll>=0 ){
		row0 = yscroll/tile_height;
		y0 = -(yscroll%tile_height);
	}
	else {
		yscroll = tile_height-1-yscroll;
		row0 = num_rows - yscroll/tile_height;
		y0 = (yscroll+1)%tile_height;
		if( y0 ) y0 = y0-tile_height;
	}

	if( xscroll>=0 ){
		col0 = xscroll/tile_width;
		x0 = -(xscroll%tile_width);
	}
	else {
		xscroll = tile_width-1-xscroll;
		col0 = num_cols - xscroll/tile_width;
		x0 = (xscroll+1)%tile_width;
		if( x0 ) x0 = x0-tile_width;
	}

	{
		int ypos = y0;
		int row = row0;
		while( ypos<screen_height ){
			int xpos = x0;
			int col = col0;
			while( xpos<screen_width ){
				process_visible_tile( col, row );
				col++;
				if( col>=num_cols ) col = 0;
				xpos += tile_width;
			}
			row++;
			if( row>=num_rows ) row = 0;
			ypos += tile_height;
		}
	}
#endif
}

void tilemap_update( struct tilemap *tilemap ){
profiler_mark(PROFILER_TILEMAP_UPDATE);
	if( tilemap==ALL_TILEMAPS ){
		tilemap = first_tilemap;
		while( tilemap ){
			tilemap_update( tilemap );
			tilemap = tilemap->next;
		}
	}
	else if( tilemap->enable ){
		update_visible( tilemap );
		update_tile_info( tilemap );
	}
profiler_mark(PROFILER_END);
}

/***********************************************************************************/

void tilemap_set_scrolldx( struct tilemap *tilemap, int dx, int dx_if_flipped ){
	tilemap->dx = dx;
	tilemap->dx_if_flipped = dx_if_flipped;
	tilemap->scrollx_delta = ( tilemap->attributes & TILEMAP_FLIPX )?dx_if_flipped:dx;
}

void tilemap_set_scrolldy( struct tilemap *tilemap, int dy, int dy_if_flipped ){
	tilemap->dy = dy;
	tilemap->dy_if_flipped = dy_if_flipped;
	tilemap->scrolly_delta = ( tilemap->attributes & TILEMAP_FLIPY )?dy_if_flipped:dy;
}

void tilemap_set_scrollx( struct tilemap *tilemap, int which, int value ){
	value = tilemap->scrollx_delta-value;

	if( tilemap->orientation & ORIENTATION_SWAP_XY ){
		if( tilemap->orientation & ORIENTATION_FLIP_X ) which = tilemap->scroll_cols-1 - which;
		if( tilemap->orientation & ORIENTATION_FLIP_Y ) value = screen_height-tilemap->cached_height-value;
		if( tilemap->colscroll[which]!=value ){
			tilemap->colscroll[which] = value;
		}
	}
	else {
		if( tilemap->orientation & ORIENTATION_FLIP_Y ) which = tilemap->scroll_rows-1 - which;
		if( tilemap->orientation & ORIENTATION_FLIP_X ) value = screen_width-tilemap->cached_width-value;
		if( tilemap->rowscroll[which]!=value ){
			tilemap->rowscroll[which] = value;
		}
	}
}
void tilemap_set_scrolly( struct tilemap *tilemap, int which, int value ){
	value = tilemap->scrolly_delta - value;

	if( tilemap->orientation & ORIENTATION_SWAP_XY ){
		if( tilemap->orientation & ORIENTATION_FLIP_Y ) which = tilemap->scroll_rows-1 - which;
		if( tilemap->orientation & ORIENTATION_FLIP_X ) value = screen_width-tilemap->cached_width-value;
		if( tilemap->rowscroll[which]!=value ){
			tilemap->rowscroll[which] = value;
		}
	}
	else {
		if( tilemap->orientation & ORIENTATION_FLIP_X ) which = tilemap->scroll_cols-1 - which;
		if( tilemap->orientation & ORIENTATION_FLIP_Y ) value = screen_height-tilemap->cached_height-value;
		if( tilemap->colscroll[which]!=value ){
			tilemap->colscroll[which] = value;
		}
	}
}
/***********************************************************************************/

void tilemap_draw( struct osd_bitmap *dest, struct tilemap *tilemap, UINT32 priority ){
	int xpos,ypos;

profiler_mark(PROFILER_TILEMAP_DRAW);
	if( tilemap->enable ){
		void (*draw)( int, int );

		int rows = tilemap->scroll_rows;
		const int *rowscroll = tilemap->rowscroll;
		int cols = tilemap->scroll_cols;
		const int *colscroll = tilemap->colscroll;

		int left = tilemap->clip_left;
		int right = tilemap->clip_right;
		int top = tilemap->clip_top;
		int bottom = tilemap->clip_bottom;

		int tile_height = tilemap->cached_tile_height;

		blit.screen = dest;
		blit.dest_line_offset = dest->line[1] - dest->line[0];

		blit.pixmap = tilemap->pixmap;
		blit.source_line_offset = tilemap->pixmap_line_offset;

		if( tilemap->type==TILEMAP_OPAQUE || (priority&TILEMAP_IGNORE_TRANSPARENCY) ){
			draw = tilemap->draw_opaque;
		}
		else {
			draw = tilemap->draw;
			if( priority&TILEMAP_BACK ){
				blit.bitmask = tilemap->background->bitmask;
				blit.mask_line_offset = tilemap->background->line_offset;
				blit.mask_data_row = tilemap->background->data_row;
			}
			else {
				blit.bitmask = tilemap->foreground->bitmask;
				blit.mask_line_offset = tilemap->foreground->line_offset;
				blit.mask_data_row = tilemap->foreground->data_row;
			}

			blit.mask_row_offset = tile_height*blit.mask_line_offset;
		}

		if( dest->depth==16 ){
			blit.dest_line_offset >>= 1;
			blit.source_line_offset >>= 1;
		}

		blit.source_row_offset = tile_height*blit.source_line_offset;
		blit.dest_row_offset = tile_height*blit.dest_line_offset;

		blit.priority_data_row = tilemap->priority_row;
		blit.source_width = tilemap->cached_width;
		blit.source_height = tilemap->cached_height;
		blit.tile_priority = priority&0xf;
		blit.tilemap_priority_code = priority>>16;

		if( rows == 1 && cols == 1 ){ /* XY scrolling playfield */
			int scrollx = rowscroll[0];
			int scrolly = colscroll[0];

			if( scrollx < 0 ){
				scrollx = blit.source_width - (-scrollx) % blit.source_width;
			}
			else {
				scrollx = scrollx % blit.source_width;
			}

			if( scrolly < 0 ){
				scrolly = blit.source_height - (-scrolly) % blit.source_height;
			}
			else {
				scrolly = scrolly % blit.source_height;
			}

	 		blit.clip_left = left;
	 		blit.clip_top = top;
	 		blit.clip_right = right;
	 		blit.clip_bottom = bottom;

			for(
				ypos = scrolly - blit.source_height;
				ypos < blit.clip_bottom;
				ypos += blit.source_height
			){
				for(
					xpos = scrollx - blit.source_width;
					xpos < blit.clip_right;
					xpos += blit.source_width
				){
					draw( xpos,ypos );
				}
			}
		}
		else if( rows == 1 ){ /* scrolling columns + horizontal scroll */
			int col = 0;
			int colwidth = blit.source_width / cols;
			int scrollx = rowscroll[0];

			if( scrollx < 0 ){
				scrollx = blit.source_width - (-scrollx) % blit.source_width;
			}
			else {
				scrollx = scrollx % blit.source_width;
			}

			blit.clip_top = top;
			blit.clip_bottom = bottom;

			while( col < cols ){
				int cons = 1;
				int scrolly = colscroll[col];

	 			/* count consecutive columns scrolled by the same amount */
				if( scrolly != TILE_LINE_DISABLED ){
					while( col + cons < cols &&	colscroll[col + cons] == scrolly ) cons++;

					if( scrolly < 0 ){
						scrolly = blit.source_height - (-scrolly) % blit.source_height;
					}
					else {
						scrolly %= blit.source_height;
					}

					blit.clip_left = col * colwidth + scrollx;
					if (blit.clip_left < left) blit.clip_left = left;
					blit.clip_right = (col + cons) * colwidth + scrollx;
					if (blit.clip_right > right) blit.clip_right = right;

					for(
						ypos = scrolly - blit.source_height;
						ypos < blit.clip_bottom;
						ypos += blit.source_height
					){
						draw( scrollx,ypos );
					}

					blit.clip_left = col * colwidth + scrollx - blit.source_width;
					if (blit.clip_left < left) blit.clip_left = left;
					blit.clip_right = (col + cons) * colwidth + scrollx - blit.source_width;
					if (blit.clip_right > right) blit.clip_right = right;

					for(
						ypos = scrolly - blit.source_height;
						ypos < blit.clip_bottom;
						ypos += blit.source_height
					){
						draw( scrollx - blit.source_width,ypos );
					}
				}
				col += cons;
			}
		}
		else if( cols == 1 ){ /* scrolling rows + vertical scroll */
			int row = 0;
			int rowheight = blit.source_height / rows;
			int scrolly = colscroll[0];
			if( scrolly < 0 ){
				scrolly = blit.source_height - (-scrolly) % blit.source_height;
			}
			else {
				scrolly = scrolly % blit.source_height;
			}
			blit.clip_left = left;
			blit.clip_right = right;
			while( row < rows ){
				int cons = 1;
				int scrollx = rowscroll[row];
				/* count consecutive rows scrolled by the same amount */
				if( scrollx != TILE_LINE_DISABLED ){
					while( row + cons < rows &&	rowscroll[row + cons] == scrollx ) cons++;
					if( scrollx < 0){
						scrollx = blit.source_width - (-scrollx) % blit.source_width;
					}
					else {
						scrollx %= blit.source_width;
					}
					blit.clip_top = row * rowheight + scrolly;
					if (blit.clip_top < top) blit.clip_top = top;
					blit.clip_bottom = (row + cons) * rowheight + scrolly;
					if (blit.clip_bottom > bottom) blit.clip_bottom = bottom;
					for(
						xpos = scrollx - blit.source_width;
						xpos < blit.clip_right;
						xpos += blit.source_width
					){
						draw( xpos,scrolly );
					}
					blit.clip_top = row * rowheight + scrolly - blit.source_height;
					if (blit.clip_top < top) blit.clip_top = top;
					blit.clip_bottom = (row + cons) * rowheight + scrolly - blit.source_height;
					if (blit.clip_bottom > bottom) blit.clip_bottom = bottom;
					for(
						xpos = scrollx - blit.source_width;
						xpos < blit.clip_right;
						xpos += blit.source_width
					){
						draw( xpos,scrolly - blit.source_height );
					}
				}
				row += cons;
			}
		}
	}
profiler_mark(PROFILER_END);
}
