#ifndef SPRITE_H
#define SPRITE_H

#define SPRITE_FLIPX					0x01
#define SPRITE_FLIPY					0x02
#define SPRITE_FLICKER					0x04
#define SPRITE_VISIBLE					0x08
#define SPRITE_TRANSPARENCY_THROUGH		0x10
#define SPRITE_SPECIAL					0x20

#define SPRITE_SHADOW					0x40
#define SPRITE_PARTIAL_SHADOW			0x80

#define SPRITE_TYPE_STACK 0
#define SPRITE_TYPE_UNPACK 1
#define SPRITE_TYPE_ZOOM 2

struct sprite {
	int priority, flags;

	const UINT8 *pen_data;	/* points to top left corner of tile data */
	int line_offset;

	const UINT16 *pal_data;
	UINT32 pen_usage;

	int x_offset, y_offset;
	int tile_width, tile_height;
	int total_width, total_height;	/* in screen coordinates */
	int x, y;

	int shadow_pen;

	/* private */ const struct sprite *next;
	/* private */ long mask_offset;
} __attribute__ ((__aligned__ (32)));

/* sprite list flags */
#define SPRITE_LIST_BACK_TO_FRONT	0x0
#define SPRITE_LIST_FRONT_TO_BACK	0x1
#define SPRITE_LIST_RAW_DATA		0x2
#define SPRITE_LIST_FLIPX			0x4
#define SPRITE_LIST_FLIPY			0x8

struct sprite_list {
	int sprite_type;
	int num_sprites;
	int flags;
	int max_priority;
	int transparent_pen;
	int special_pen;

	struct sprite *sprite;
	struct sprite_list *next; /* resource tracking */
};

void sprite_init( void );	/* called by core - don't call this in drivers */
void sprite_close( void );	/* called by core - don't call this in drivers */

struct sprite_list *sprite_list_create( int num_sprites, int flags );
void sprite_update( void );
void sprite_draw( struct sprite_list *sprite_list, int priority );

void sprite_set_shade_table(UINT16 *table);

#endif
