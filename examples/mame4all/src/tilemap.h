/* tilemap.h */

#ifndef TILEMAP_H
#define TILEMAP_H

#define ALL_TILEMAPS	0
/* ALL_TILEMAPS may be used with:
	tilemap_update, tilemap_render, tilemap_set_flip, tilemap_mark_all_pixels_dirty
*/

#define TILEMAP_OPAQUE				0x00
#define TILEMAP_TRANSPARENT			0x01
#define TILEMAP_SPLIT				0x02
#define TILEMAP_BITMASK				0x04
#define TILEMAP_TRANSPARENT_COLOR	0x08
/*
	TILEMAP_SPLIT should be used if the pixels from a single tile
	can appear in more than one plane.

	TILEMAP_BITMASK is needed for Namco SystemI
*/

#define TILEMAP_IGNORE_TRANSPARENCY		0x10
#define TILEMAP_BACK					0x20
#define TILEMAP_FRONT					0x40
/*
	when rendering a split layer, pass TILEMAP_FRONT or TILEMAP_BACK or'd with the
	tile_priority value to specify the part to draw.
*/

#define TILEMAP_BITMASK_TRANSPARENT (0)
#define TILEMAP_BITMASK_OPAQUE ((UINT8 *)~0)

struct cached_tile_info {
	const UINT8 *pen_data;
	const UINT16 *pal_data;
	UINT32 pen_usage;
	UINT32 flags;
};

extern struct tile_info {
	/*
		you must set tile_info.pen_data, tile_info.pal_data and tile_info.pen_usage
		in the callback.  You can use the SET_TILE_INFO() macro below to do this.
		tile_info.flags and tile_info.priority will be automatically preset to 0,
		games that don't need them don't need to explicitly set them to 0
	*/
	const UINT8 *pen_data;
	const UINT16 *pal_data;
	UINT32 pen_usage;
	UINT32 flags;

	UINT32 priority;
	UINT8 *mask_data;
} tile_info;

#define SET_TILE_INFO(GFX,CODE,COLOR) { \
	const struct GfxElement *gfx = Machine->gfx[(GFX)]; \
	int _code = (CODE) % gfx->total_elements; \
	tile_info.pen_data = gfx->gfxdata + _code*gfx->char_modulo; \
	tile_info.pal_data = &gfx->colortable[gfx->color_granularity * (COLOR)]; \
	tile_info.pen_usage = gfx->pen_usage?gfx->pen_usage[_code]:0; \
}

/* tile flags, set by get_tile_info callback */
#define TILE_FLIPX					0x01
#define TILE_FLIPY					0x02
#define TILE_SPLIT(T)				((T)<<2)
/* TILE_SPLIT is for use with TILEMAP_SPLIT layers.  It selects transparency type. */
#define TILE_IGNORE_TRANSPARENCY	0x10
/* TILE_IGNORE_TRANSPARENCY is used if you need an opaque tile in a transparent layer */

#define TILE_FLIPYX(YX)				(YX)
#define TILE_FLIPXY(XY)				((((XY)>>1)|((XY)<<1))&3)
/*
	TILE_FLIPYX is a shortcut that can be used by approx 80% of games,
	since yflip frequently occurs one bit higher than xflip within a
	tile attributes byte.
*/

#define TILE_LINE_DISABLED 0x80000000

#ifndef OSD_CPU_H
#include "osd_cpu.h"
#endif

extern struct osd_bitmap *priority_bitmap;

struct tilemap_mask {
	struct osd_bitmap *bitmask;
	int line_offset;
	UINT8 *data;
	UINT8 **data_row;
};

struct tilemap {
	UINT32 (*get_memory_offset)( UINT32 col, UINT32 row, UINT32 num_cols, UINT32 num_rows );
	int *memory_offset_to_cached_index;
	UINT32 *cached_index_to_memory_offset;
	int logical_flip_to_cached_flip[4];

	/* callback to interpret video VRAM for the tilemap */
	void (*tile_get_info)( int memory_offset );

	UINT32 max_memory_offset;
	int num_tiles;
	int num_logical_rows, num_logical_cols;
	int num_cached_rows, num_cached_cols;
	int cached_tile_width, cached_tile_height, cached_width, cached_height;

	struct cached_tile_info *cached_tile_info;

	int dx, dx_if_flipped;
	int dy, dy_if_flipped;
	int scrollx_delta, scrolly_delta;

	int enable;
	int attributes;

	int type;
	int transparent_pen;
	unsigned int transmask[4];

	void (*draw)( int, int );
	void (*draw_opaque)( int, int );

	UINT8 *priority,	/* priority for each tile */
		**priority_row;

	UINT8 *visible; /* boolean flag for each tile */

	UINT8 *dirty_vram; /* boolean flag for each tile */

	UINT8 *dirty_pixels;

	int scroll_rows, scroll_cols;
	int *rowscroll, *colscroll;

	int orientation;
	int clip_left,clip_right,clip_top,clip_bottom;

	/* cached color data */
	struct osd_bitmap *pixmap;
	int pixmap_line_offset;

	struct tilemap_mask *foreground;
	/* for transparent layers, or the front half of a split layer */

	struct tilemap_mask *background;
	/* for the back half of a split layer */

	struct tilemap *next; /* resource tracking */
} __attribute__ ((__aligned__ (32)));

/* don't call these from drivers - they are called from mame.c */
int tilemap_init( void );
void tilemap_close( void );

struct tilemap *tilemap_create(
	void (*tile_get_info)( int memory_offset ),
	UINT32 (*get_memory_offset)( UINT32 col, UINT32 row, UINT32 num_cols, UINT32 num_rows ),
	int type,
	int tile_width, int tile_height, /* in pixels */
	int num_cols, int num_rows /* in tiles */
);

void tilemap_dispose( struct tilemap *tilemap );
/*	you shouldn't call this in vh_close; all tilemaps will be automatically
	disposed.  tilemap_dispose is supplied for games that need to change
	tile size or cols/rows dynamically.
*/

void tilemap_set_transparent_pen( struct tilemap *tilemap, int pen );
void tilemap_set_scroll_cols( struct tilemap *tilemap, int scroll_cols );
void tilemap_set_scroll_rows( struct tilemap *tilemap, int scroll_rows );
/* scroll_rows and scroll_cols default to 1 for XY scrolling */

void tilemap_mark_tile_dirty( struct tilemap *tilemap, int memory_offset );
void tilemap_mark_all_tiles_dirty( struct tilemap *tilemap );
void tilemap_mark_all_pixels_dirty( struct tilemap *tilemap );

void tilemap_set_scrollx( struct tilemap *tilemap, int row, int value );
void tilemap_set_scrolly( struct tilemap *tilemap, int col, int value );

void tilemap_set_scrolldx( struct tilemap *tilemap, int dx, int dx_if_flipped );
void tilemap_set_scrolldy( struct tilemap *tilemap, int dy, int dy_if_flipped );

#define TILEMAP_FLIPX 0x1
#define TILEMAP_FLIPY 0x2
void tilemap_set_flip( struct tilemap *tilemap, int attributes );
void tilemap_set_clip( struct tilemap *tilemap, const struct rectangle *clip );
void tilemap_set_enable( struct tilemap *tilemap, int enable );

void tilemap_update( struct tilemap *tilemap );
void tilemap_render( struct tilemap *tilemap );
void tilemap_draw( struct osd_bitmap *dest, struct tilemap *tilemap, UINT32 priority );

/*********************************************************************/

UINT32 tilemap_scan_cols( UINT32 col, UINT32 row, UINT32 num_cols, UINT32 num_rows );
UINT32 tilemap_scan_rows( UINT32 col, UINT32 row, UINT32 num_cols, UINT32 num_rows );

#endif
