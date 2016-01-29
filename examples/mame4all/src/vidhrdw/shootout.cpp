/*
	Video Hardware for Shoot Out
	prom GB09.K6 may be related to background tile-sprite priority
*/

#include "driver.h"
#include "vidhrdw/generic.h"

#define NUM_SPRITES 128

extern unsigned char *shootout_textram;
static struct sprite_list *sprite_list;


int shootout_vh_start( void ){
	if( generic_vh_start()==0 ){
		sprite_list = sprite_list_create( NUM_SPRITES, SPRITE_LIST_BACK_TO_FRONT );
		if( sprite_list ){
			int i;
			sprite_list->sprite_type = SPRITE_TYPE_STACK;

			for( i=0; i<NUM_SPRITES; i++ ){
				struct sprite *sprite = &sprite_list->sprite[i];
				sprite->pal_data = Machine->gfx[1]->colortable;
				sprite->tile_width = 16;
				sprite->tile_height = 16;
				sprite->total_width = 16;
				sprite->line_offset = 16;
			}
			sprite_list->max_priority = 1;

			return 0;
		}
		generic_vh_stop();
	}
	return 1; /* error */
}

static void get_sprite_info( void ){
	const struct GfxElement *gfx = Machine->gfx[1];
	const UINT8 *source = spriteram;
	struct sprite *sprite = sprite_list->sprite;
	int count = NUM_SPRITES;

	int attributes, flags, number;

	while( count-- ){
		flags = 0;
		attributes = source[1];
		/*
		    76543210
			xxx			bank
			   x		vertical size
			    x		priority
			     x		horizontal flip
			      x		flicker
			       x	enable
		*/
		if ( attributes & 0x01 ){ /* enabled */
			flags |= SPRITE_VISIBLE;
			sprite->priority = (attributes&0x08)?1:0;
			sprite->x = (240 - source[2])&0xff;
			sprite->y = (240 - source[0])&0xff;

			number = source[3] + ((attributes&0xe0)<<3);
			if( attributes & 0x04 ) flags |= SPRITE_FLIPX;
			if( attributes & 0x02 ) flags |= SPRITE_FLICKER; /* ? */

			if( attributes & 0x10 ){ /* double height */
				number = number&(~1);
				sprite->y -= 16;
				sprite->total_height = 32;
			}
			else {
				sprite->total_height = 16;
			}
			sprite->pen_data = gfx->gfxdata + number * gfx->char_modulo;
		}
		sprite->flags = flags;
		sprite++;
		source += 4;
	}
}

static void get_sprite_info2( void ){
	const struct GfxElement *gfx = Machine->gfx[1];
	const UINT8 *source = spriteram;
	struct sprite *sprite = sprite_list->sprite;
	int count = NUM_SPRITES;

	int attributes, flags, number;

	while( count-- ){
		flags = 0;
		attributes = source[1];
		if ( attributes & 0x01 ){ /* enabled */
			flags |= SPRITE_VISIBLE;
			sprite->priority = (attributes&0x08)?1:0;
			sprite->x = (240 - source[2])&0xff;
			sprite->y = (240 - source[0])&0xff;

			number = source[3] + ((attributes&0xc0)<<2);
			if( attributes & 0x04 ) flags |= SPRITE_FLIPX;
			if( attributes & 0x02 ) flags |= SPRITE_FLICKER; /* ? */

			if( attributes & 0x10 ){ /* double height */
				number = number&(~1);
				sprite->y -= 16;
				sprite->total_height = 32;
			}
			else {
				sprite->total_height = 16;
			}
			sprite->pen_data = gfx->gfxdata + number * gfx->char_modulo;
		}
		sprite->flags = flags;
		sprite++;
		source += 4;
	}
}

static void draw_background( struct osd_bitmap *bitmap ){
	const struct rectangle *clip = &Machine->visible_area;
	int offs;
	for( offs=0; offs<videoram_size; offs++ ){
		if( dirtybuffer[offs] ){
			int sx = (offs%32)*8;
			int sy = (offs/32)*8;
			int attributes = colorram[offs]; /* CCCC -TTT */
			int tile_number = videoram[offs] + 256*(attributes&7);
			int color = attributes>>4;

			drawgfx(tmpbitmap,Machine->gfx[2],
					tile_number&0x7ff,
					color,
					0,0,
					sx,sy,
					clip,TRANSPARENCY_NONE,0);

			dirtybuffer[offs] = 0;
		}
	}
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
}

static void draw_foreground( struct osd_bitmap *bitmap ){
	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[0];
	int sx,sy;

	unsigned char *source = shootout_textram;

	for( sy=0; sy<256; sy+=8 ){
		for( sx=0; sx<256; sx+=8 ){
			int attributes = *(source+videoram_size); /* CCCC --TT */
			int tile_number = 256*(attributes&0x3) + *source++;
			int color = attributes>>4;
			drawgfx(bitmap,gfx,
				tile_number, /* 0..1024 */
				color,
				0,0,
				sx,sy,
				clip,TRANSPARENCY_PEN,0);
		}
	}
}

void shootout_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh){
	get_sprite_info();
	sprite_update();
	draw_background( bitmap );
	sprite_draw( sprite_list, 1);
	draw_foreground( bitmap );
	sprite_draw( sprite_list, 0);
}

void shootouj_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh){
	get_sprite_info2();
	sprite_update();
	draw_background( bitmap );
	sprite_draw( sprite_list, 1);
	draw_foreground( bitmap );
	sprite_draw( sprite_list, 0);
}
