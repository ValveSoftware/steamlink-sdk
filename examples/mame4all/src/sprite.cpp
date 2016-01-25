#include "driver.h"

#define SWAP(X,Y) { int temp = X; X = Y; Y = temp; }

/*
	Games currently using the Sprite Manager:
		Sega System16
		Blood Brothers
		Shoot Out
		Ninja Gaiden

	The Sprite Manager provides a service that would be difficult or impossible using drawgfx.
	It allows sprite-to-sprite priority to be orthogonal to sprite-to-tilemap priority.

	The sprite manager also abstract a nice chunk of generally useful functionality.

	Drivers making use of Sprite Manager will NOT necessarily be any faster than traditional
	drivers using drawgfx.

	Currently supported features include:
	- sprite layering order, FRONT_TO_BACK / BACK_TO_FRONT (can be easily switched from a driver)
	- priority masking (needed for Gaiden, Blood Brothers, and surely many others)
	  this allows sprite-to-sprite priority to be orthogonal to sprite-to-layer priority.
	- resource tracking (sprite_init and sprite_close must be called by mame.c)
	- flickering sprites
	- offscreen sprite skipping
	- palette usage tracking
	- screen orientation/clipping (preliminary!  rotation isn't working properly with
		SPRITE_TYPE_UNPACK and SPRITE_TYPE_STACK, yet)
	- support for graphics that aren't pre-rotated (i.e. System16)

There are three sprite types that a sprite_list may use.  With all three sprite types, sprite->x and sprite->y
are screenwise coordinates for the topleft pixel of a sprite, and sprite->total_width, sprite->total_width is the
sprite size in screen coordinates - the "footprint" of the sprite on the screen.  sprite->line_offset indicates
offset from one logical row of sprite pen data to the next.

	SPRITE_TYPE_UNPACK
		Often, it is possible to unpack a sprite at it's "largest" size, and draw a subset of it.
		Some games (i.e. Ninja Gaiden) have a size sprite attribute that defines a recursive arrangement
		of smaller (typically 8x8 pixel) tiles, i.e:

			0 1 4 5
			2 3 6 7
			8 9 c d
			a b e f

		If the above is the largest configuration, SPRITE_TYPE_UNPACK lets you specify tiny sprites (0,1,2,3,...)
		medium size sprites (0123, 4567, 89ab), or large sprites (the entire configuration: 012452367...)

		To unpack the sprite 1436 in the above example, the following fields would be used:

		size of sprite to be drawn
			sprite->total_width = 16
			sprite->total_height = 16

		size of largest possible sprite:
			sprite->tile_width = 32
			sprite->tile_height = 32

		relative offset of the sprite to be drawn:
			sprite->x_offset = 8
			sprite->y_offset = 0

	SPRITE_TYPE_STACK
		Sometimes a predetermined arrangement is not possible.  SPRITE_TYPE_STACK will draw a series of sprites,
		stacking them from top to bottom, and then from left to right.  This is used by Blood Brothers.
		The Sprite Manager will handle flipping and positioning of individual tiles.
		sprite->total_width must be a multiple of sprite->tile_width
		sprite->total_height must be a multiple of sprite->tile_height

	SPRITE_TYPE_ZOOM
		line_offset is pen skip to next line; tile_width and tile_height are logical sprite dimensions
		The sprite will be stretched to fit total_width, total_height, shrinking or magnifying as needed

	TBA:
	- GfxElement-oriented field-setting macros
	- cocktail support
	- "special" pen (hides pixels of previously drawn sprites) - for MCR games, Mr. Do's Castle, etc.
	- end-of-line marking pen (needed for Altered Beast, ESWAT)
*/

static int orientation, screen_width, screen_height;
static int screen_clip_left, screen_clip_top, screen_clip_right, screen_clip_bottom;
unsigned char *screen_baseaddr;
int screen_line_offset;

static struct sprite_list *first_sprite_list = NULL; /* used for resource tracking */
static int FlickeringInvisible;

static UINT16 *shade_table;

static void sprite_order_setup( struct sprite_list *sprite_list, int *first, int *last, int *delta ){
	if( sprite_list->flags&SPRITE_LIST_FRONT_TO_BACK ){
		*delta = -1;
		*first = sprite_list->num_sprites-1;
		*last = 0;
	}
	else {
		*delta = 1;
		*first = 0;
		*last = sprite_list->num_sprites-1;
	}
}

/*********************************************************************

	The mask buffer is a dynamically allocated resource
	it is recycled each frame.  Using this technique reduced the runttime
	memory requirements of the Gaiden from 512k (worst case) to approx 6K.

	Sprites use offsets instead of pointers directly to the mask data, since it
	is potentially reallocated.

*********************************************************************/
static unsigned char *mask_buffer = NULL;
static int mask_buffer_size = 0; /* actual size of allocated buffer */
static int mask_buffer_used = 0;

static void mask_buffer_reset( void ){
	mask_buffer_used = 0;
}
static void mask_buffer_dispose( void ){
	if (mask_buffer)
		free( mask_buffer );
	mask_buffer = NULL;
	mask_buffer_size = 0;
}
static long mask_buffer_alloc( long size ){
	long result = mask_buffer_used;
	long req_size = mask_buffer_used + size;
	if( req_size>mask_buffer_size ){
		mask_buffer = (unsigned char*)realloc( mask_buffer, req_size );
		mask_buffer_size = req_size;
		logerror("increased sprite mask buffer size to %d bytes.\n", mask_buffer_size );
		if( !mask_buffer ) logerror("Error! insufficient memory for mask_buffer_alloc\n" );
	}
	mask_buffer_used = req_size;
	memset( &mask_buffer[result], 0x00, size ); /* clear it */
	return result;
}

#define BLIT \
if( sprite->flags&SPRITE_FLIPX ){ \
	source += screenx + flipx_adjust; \
	for( y=y1; y<y2; y++ ){ \
		for( x=x1; x<x2; x++ ){ \
			if( OPAQUE(-x) ) dest[x] = COLOR(-x); \
		} \
		source += source_dy; dest += blit.line_offset; \
		NEXTLINE \
	} \
} \
else { \
	source -= screenx; \
	for( y=y1; y<y2; y++ ){ \
		for( x=x1; x<x2; x++ ){ \
			if( OPAQUE(x) ) dest[x] = COLOR(x); \
			\
		} \
		source += source_dy; dest += blit.line_offset; \
		NEXTLINE \
	} \
}

static struct {
	int transparent_pen;
	int clip_left, clip_right, clip_top, clip_bottom;
	unsigned char *baseaddr;
	int line_offset;
	int write_to_mask;
	int origin_x, origin_y;
} blit;

static void do_blit_unpack( const struct sprite *sprite ){
	const unsigned short *pal_data = sprite->pal_data;
	int transparent_pen = blit.transparent_pen;

	int screenx = sprite->x - blit.origin_x;
	int screeny = sprite->y - blit.origin_y;
	int x1 = screenx;
	int y1 = screeny;
	int x2 = x1 + sprite->total_width;
	int y2 = y1 + sprite->total_height;
	int flipx_adjust = sprite->total_width-1;

	int source_dy;
	const unsigned char *baseaddr = sprite->pen_data;
	const unsigned char *source;
	unsigned char *dest;
	int x,y;

	source = baseaddr + sprite->line_offset*sprite->y_offset + sprite->x_offset;

	if( x1<blit.clip_left )		x1 = blit.clip_left;
	if( y1<blit.clip_top )		y1 = blit.clip_top;
	if( x2>blit.clip_right )	x2 = blit.clip_right;
	if( y2>blit.clip_bottom )	y2 = blit.clip_bottom;

	if( x1<x2 && y1<y2 ){
		dest = blit.baseaddr + y1*blit.line_offset;
		if( sprite->flags&SPRITE_FLIPY ){
			source_dy = -sprite->line_offset;
			source += (y2-1-screeny)*sprite->line_offset;
		}
		else {
			source_dy = sprite->line_offset;
			source += (y1-screeny)*sprite->line_offset;
		}
		if( blit.write_to_mask ){
			#define OPAQUE(X) (source[X]!=transparent_pen)
			#define COLOR(X) 0xff
			#define NEXTLINE
			BLIT
			#undef OPAQUE
			#undef COLOR
			#undef NEXTLINE
		}
		else if( sprite->mask_offset>=0 ){ /* draw a masked sprite */
			const unsigned char *mask = &mask_buffer[sprite->mask_offset] +
				(y1-sprite->y)*sprite->total_width-sprite->x;
			#define OPAQUE(X) (mask[x]==0 && source[X]!=transparent_pen)
			#define COLOR(X) (pal_data[source[X]])
			#define NEXTLINE mask+=sprite->total_width;
			BLIT
			#undef OPAQUE
			#undef COLOR
			#undef NEXTLINE
		}
		else if( sprite->flags&SPRITE_TRANSPARENCY_THROUGH ){
			int color = Machine->pens[palette_transparent_color];
			#define OPAQUE(X) (dest[x]==color && source[X]!=transparent_pen)
			#define COLOR(X) (pal_data[source[X]])
			#define NEXTLINE
			BLIT
			#undef OPAQUE
			#undef COLOR
			#undef NEXTLINE
		}
		else if( pal_data ){
			#define OPAQUE(X) (source[X]!=transparent_pen)
			#define COLOR(X) (pal_data[source[X]])
			#define NEXTLINE
			BLIT
			#undef OPAQUE
			#undef COLOR
			#undef NEXTLINE
		}
	}
}

static void do_blit_stack( const struct sprite *sprite ){
	const unsigned short *pal_data = sprite->pal_data;
	int transparent_pen = blit.transparent_pen;
	int flipx_adjust = sprite->tile_width-1;

	int xoffset, yoffset;
	int screenx, screeny;
	int x1, y1, x2, y2;
	int x,y;

	int source_dy;
	const unsigned char *baseaddr = sprite->pen_data;
	const unsigned char *source;
	unsigned char *dest;

	for( xoffset =0; xoffset<sprite->total_width; xoffset+=sprite->tile_width ){
		for( yoffset=0; yoffset<sprite->total_height; yoffset+=sprite->tile_height ){
			source = baseaddr;
			screenx = sprite->x - blit.origin_x;
			screeny = sprite->y - blit.origin_y;

			if( sprite->flags & SPRITE_FLIPX ){
				screenx += sprite->total_width - sprite->tile_width - xoffset;
			}
			else {
				screenx += xoffset;
			}

			if( sprite->flags & SPRITE_FLIPY ){
				screeny += sprite->total_height - sprite->tile_height - yoffset;
			}
			else {
				screeny += yoffset;
			}

			x1 = screenx;
			y1 = screeny;
			x2 = x1 + sprite->tile_width;
			y2 = y1 + sprite->tile_height;

			if( x1<blit.clip_left )		x1 = blit.clip_left;
			if( y1<blit.clip_top )		y1 = blit.clip_top;
			if( x2>blit.clip_right )	x2 = blit.clip_right;
			if( y2>blit.clip_bottom )	y2 = blit.clip_bottom;

			if( x1<x2 && y1<y2 ){
				dest = blit.baseaddr + y1*blit.line_offset;

				if( sprite->flags&SPRITE_FLIPY ){
					source_dy = -sprite->line_offset;
					source += (y2-1-screeny)*sprite->line_offset;
				}
				else {
					source_dy = sprite->line_offset;
					source += (y1-screeny)*sprite->line_offset;
				}

				if( blit.write_to_mask ){
					#define OPAQUE(X) (source[X]!=transparent_pen)
					#define COLOR(X) 0xff
					#define NEXTLINE
					BLIT
					#undef OPAQUE
					#undef COLOR
					#undef NEXTLINE
				}
				else if( sprite->mask_offset>=0 ){ /* draw a masked sprite */
					const unsigned char *mask = &mask_buffer[sprite->mask_offset] +
						(y1-sprite->y)*sprite->total_width-sprite->x;
					#define OPAQUE(X) (mask[x]==0 && source[X]!=transparent_pen)
					#define COLOR(X) (pal_data[source[X]])
					#define NEXTLINE mask+=sprite->total_width;
					BLIT
					#undef OPAQUE
					#undef COLOR
					#undef NEXTLINE
				}
				else if( sprite->flags&SPRITE_TRANSPARENCY_THROUGH ){
					int color = Machine->pens[palette_transparent_color];
					#define OPAQUE(X) (dest[x]==color && source[X]!=transparent_pen)
					#define COLOR(X) (pal_data[source[X]])
					#define NEXTLINE
					BLIT
					#undef OPAQUE
					#undef COLOR
					#undef NEXTLINE
				}
				else if( pal_data ){
					#define OPAQUE(X) (source[X]!=transparent_pen)
					#define COLOR(X) (pal_data[source[X]])
					#define NEXTLINE
					BLIT
					#undef OPAQUE
					#undef COLOR
					#undef NEXTLINE
				}
			} /* not totally clipped */
			baseaddr += sprite->tile_height*sprite->line_offset;
		} /* next yoffset */
	} /* next xoffset */
}

static void _do_blit_zoom( const struct sprite *sprite ){
	/*	assumes SPRITE_LIST_RAW_DATA flag is set */

	int x1,x2, y1,y2, dx,dy;
	int xcount0 = 0, ycount0 = 0;

	if( sprite->flags & SPRITE_FLIPX ){
		x2 = sprite->x;
		x1 = x2+sprite->total_width;
		dx = -1;
		if( x2<blit.clip_left ) x2 = blit.clip_left;
		if( x1>blit.clip_right ){
			xcount0 = (x1-blit.clip_right)*sprite->tile_width;
			x1 = blit.clip_right;
		}
		if( x2>=x1 ) return;
		x1--; x2--;
	}
	else {
		x1 = sprite->x;
		x2 = x1+sprite->total_width;
		dx = 1;
		if( x1<blit.clip_left ){
			xcount0 = (blit.clip_left-x1)*sprite->tile_width;
			x1 = blit.clip_left;
		}
		if( x2>blit.clip_right ) x2 = blit.clip_right;
		if( x1>=x2 ) return;
	}
	if( sprite->flags & SPRITE_FLIPY ){
		y2 = sprite->y;
		y1 = y2+sprite->total_height;
		dy = -1;
		if( y2<blit.clip_top ) y2 = blit.clip_top;
		if( y1>blit.clip_bottom ){
			ycount0 = (y1-blit.clip_bottom)*sprite->tile_height;
			y1 = blit.clip_bottom;
		}
		if( y2>=y1 ) return;
		y1--; y2--;
	}
	else {
		y1 = sprite->y;
		y2 = y1+sprite->total_height;
		dy = 1;
		if( y1<blit.clip_top ){
			ycount0 = (blit.clip_top-y1)*sprite->tile_height;
			y1 = blit.clip_top;
		}
		if( y2>blit.clip_bottom ) y2 = blit.clip_bottom;
		if( y1>=y2 ) return;
	}

	if(!(sprite->flags & (SPRITE_SHADOW | SPRITE_PARTIAL_SHADOW)))
	{
		const unsigned char *pen_data = sprite->pen_data;
		const unsigned short *pal_data = sprite->pal_data;
		int x,y;
		unsigned int pen;
		int pitch = blit.line_offset*dy;
		unsigned char *dest = blit.baseaddr + blit.line_offset*y1;
		int ycount = ycount0;

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
			int xcount = xcount0;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				unsigned char *dest1;

				ycount = ycount0;
				while( xcount>=sprite->total_width ){
					xcount -= sprite->total_width;
					pen_data+=sprite->line_offset;
				}
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					while( ycount>=sprite->total_height ){
						ycount -= sprite->total_height;
						source ++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
                    if( pen ) *dest1 = pal_data[pen];
					ycount+= sprite->tile_height;
					dest1 += pitch;
				}
				xcount += sprite->tile_width;
			}
		}
		else {
			for( y=y1; y!=y2; y+=dy ){
				int xcount = xcount0;
				const unsigned char *source;
				while( ycount>=sprite->total_height ){
					ycount -= sprite->total_height;
					pen_data += sprite->line_offset;
				}
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					while( xcount>=sprite->total_width ){
						xcount -= sprite->total_width;
						source++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
                    if( pen ) dest[x] = pal_data[pen];
					xcount += sprite->tile_width;
				}
				ycount += sprite->tile_height;
				dest += pitch;
			}
		}
	}
	else if(sprite->flags & SPRITE_PARTIAL_SHADOW)
	{
		const unsigned char *pen_data = sprite->pen_data;
		const unsigned short *pal_data = sprite->pal_data;
		int x,y;
		unsigned int pen;
		int pitch = blit.line_offset*dy;
		unsigned char *dest = blit.baseaddr + blit.line_offset*y1;
		int ycount = ycount0;

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
			int xcount = xcount0;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				unsigned char *dest1;

				ycount = ycount0;
				while( xcount>=sprite->total_width ){
					xcount -= sprite->total_width;
					pen_data+=sprite->line_offset;
				}
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					while( ycount>=sprite->total_height ){
						ycount -= sprite->total_height;
						source ++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen==sprite->shadow_pen ) *dest1 = shade_table[*dest1];
					else if( pen ) *dest1 = pal_data[pen];
					ycount+= sprite->tile_height;
					dest1 += pitch;
				}
				xcount += sprite->tile_width;
			}
		}
		else {
			for( y=y1; y!=y2; y+=dy ){
				int xcount = xcount0;
				const unsigned char *source;
				while( ycount>=sprite->total_height ){
					ycount -= sprite->total_height;
					pen_data += sprite->line_offset;
				}
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					while( xcount>=sprite->total_width ){
						xcount -= sprite->total_width;
						source++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen==sprite->shadow_pen ) dest[x] = shade_table[dest[x]];
					else if( pen ) dest[x] = pal_data[pen];
					xcount += sprite->tile_width;
				}
				ycount += sprite->tile_height;
				dest += pitch;
			}
		}
	}
	else
	{	// Shadow Sprite
		const unsigned char *pen_data = sprite->pen_data;
		int x,y;
		unsigned int pen;
		int pitch = blit.line_offset*dy;
		unsigned char *dest = blit.baseaddr + blit.line_offset*y1;
		int ycount = ycount0;

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
			int xcount = xcount0;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				unsigned char *dest1;

				ycount = ycount0;
				while( xcount>=sprite->total_width ){
					xcount -= sprite->total_width;
					pen_data+=sprite->line_offset;
				}
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					while( ycount>=sprite->total_height ){
						ycount -= sprite->total_height;
						source ++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen ) *dest1 = shade_table[*dest1];
					ycount+= sprite->tile_height;
					dest1 += pitch;
				}
				xcount += sprite->tile_width;
			}
		}
		else {
			for( y=y1; y!=y2; y+=dy ){
				int xcount = xcount0;
				const unsigned char *source;
				while( ycount>=sprite->total_height ){
					ycount -= sprite->total_height;
					pen_data += sprite->line_offset;
				}
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					while( xcount>=sprite->total_width ){
						xcount -= sprite->total_width;
						source++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen ) dest[x] = shade_table[dest[x]];
					xcount += sprite->tile_width;
				}
				ycount += sprite->tile_height;
				dest += pitch;
			}
		}
	}

}

static void _do_blit_zoom_noscale( const struct sprite *sprite ){
	/*	assumes SPRITE_LIST_RAW_DATA flag is set */

	int x1,x2, y1,y2, dx,dy;
	int xcount0 = 0, ycount0 = 0;

	if( sprite->flags & SPRITE_FLIPX ){
		x2 = sprite->x;
		x1 = x2+sprite->total_width;
		dx = -1;
		if( x2<blit.clip_left ) x2 = blit.clip_left;
		if( x1>blit.clip_right ){
			xcount0 = x1-blit.clip_right;
			x1 = blit.clip_right;
		}
		if( x2>=x1 ) return;
		x1--; x2--;
	}
	else {
		x1 = sprite->x;
		x2 = x1+sprite->total_width;
		dx = 1;
		if( x1<blit.clip_left ){
			xcount0 = blit.clip_left-x1;
			x1 = blit.clip_left;
		}
		if( x2>blit.clip_right ) x2 = blit.clip_right;
		if( x1>=x2 ) return;
	}
	if( sprite->flags & SPRITE_FLIPY ){
		y2 = sprite->y;
		y1 = y2+sprite->total_height;
		dy = -1;
		if( y2<blit.clip_top ) y2 = blit.clip_top;
		if( y1>blit.clip_bottom ){
			ycount0 = y1-blit.clip_bottom;
			y1 = blit.clip_bottom;
		}
		if( y2>=y1 ) return;
		y1--; y2--;
	}
	else {
		y1 = sprite->y;
		y2 = y1+sprite->total_height;
		dy = 1;
		if( y1<blit.clip_top ){
			ycount0 = blit.clip_top-y1;
			y1 = blit.clip_top;
		}
		if( y2>blit.clip_bottom ) y2 = blit.clip_bottom;
		if( y1>=y2 ) return;
	}

	if(!(sprite->flags & (SPRITE_SHADOW | SPRITE_PARTIAL_SHADOW)))
	{
		int x,y;
		unsigned int pen;
		int pitch = blit.line_offset*dy;
		unsigned char *dest = blit.baseaddr + blit.line_offset*y1;

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
    		const unsigned char *pen_data = sprite->pen_data+xcount0*sprite->line_offset+ycount0;
	    	const unsigned short *pal_data = sprite->pal_data;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				unsigned char *dest1;
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
                    if( pen ) *dest1 = pal_data[pen];
					dest1 += pitch;
				}
    			pen_data+=sprite->line_offset;
			}
		}
		else {
    		const unsigned char *pen_data = sprite->pen_data+xcount0+ycount0*sprite->line_offset;
	    	const unsigned short *pal_data = sprite->pal_data;
			for( y=y1; y!=y2; y+=dy ){
				const unsigned char *source;
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
                    if( pen ) dest[x] = pal_data[pen];
				}
				pen_data += sprite->line_offset;
				dest += pitch;
			}
		}
	}
	else if(sprite->flags & SPRITE_PARTIAL_SHADOW)
	{
		int x,y;
		unsigned int pen;
		int pitch = blit.line_offset*dy;
		unsigned char *dest = blit.baseaddr + blit.line_offset*y1;

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
    		const unsigned char *pen_data = sprite->pen_data+xcount0*sprite->line_offset+ycount0;
	    	const unsigned short *pal_data = sprite->pal_data;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				unsigned char *dest1;

				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen==sprite->shadow_pen ) *dest1 = shade_table[*dest1];
					else if( pen ) *dest1 = pal_data[pen];
					dest1 += pitch;
				}
				pen_data+=sprite->line_offset;
			}
		}
		else {
    		const unsigned char *pen_data = sprite->pen_data+xcount0+ycount0*sprite->line_offset;
	    	const unsigned short *pal_data = sprite->pal_data;
			for( y=y1; y!=y2; y+=dy ){
				const unsigned char *source;
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen==sprite->shadow_pen ) dest[x] = shade_table[dest[x]];
					else if( pen ) dest[x] = pal_data[pen];
				}
				pen_data += sprite->line_offset;
				dest += pitch;
			}
		}
	}
	else
	{	// Shadow Sprite
		int x,y;
		unsigned int pen;
		int pitch = blit.line_offset*dy;
		unsigned char *dest = blit.baseaddr + blit.line_offset*y1;

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
    		const unsigned char *pen_data = sprite->pen_data+xcount0*sprite->line_offset+ycount0;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				unsigned char *dest1;
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen ) *dest1 = shade_table[*dest1];
					dest1 += pitch;
				}
				pen_data+=sprite->line_offset;
			}
		}
		else {
    		const unsigned char *pen_data = sprite->pen_data+xcount0+ycount0*sprite->line_offset;
			for( y=y1; y!=y2; y+=dy ){
				const unsigned char *source;
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen ) dest[x] = shade_table[dest[x]];
				}
				pen_data += sprite->line_offset;
				dest += pitch;
			}
		}
	}

}

static void do_blit_zoom( const struct sprite *sprite ){
    if ((sprite->tile_width==sprite->total_width) && (sprite->tile_height==sprite->total_height))
    {
        _do_blit_zoom_noscale(sprite);
    }
    else
    {
        _do_blit_zoom(sprite);
    }
}

static void _do_blit_zoom16( const struct sprite *sprite ){
	/*	assumes SPRITE_LIST_RAW_DATA flag is set */

	int x1,x2, y1,y2, dx,dy;
	int xcount0 = 0, ycount0 = 0;

	if( sprite->flags & SPRITE_FLIPX ){
		x2 = sprite->x;
		x1 = x2+sprite->total_width;
		dx = -1;
		if( x2<blit.clip_left ) x2 = blit.clip_left;
		if( x1>blit.clip_right ){
			xcount0 = (x1-blit.clip_right)*sprite->tile_width;
			x1 = blit.clip_right;
		}
		if( x2>=x1 ) return;
		x1--; x2--;
	}
	else {
		x1 = sprite->x;
		x2 = x1+sprite->total_width;
		dx = 1;
		if( x1<blit.clip_left ){
			xcount0 = (blit.clip_left-x1)*sprite->tile_width;
			x1 = blit.clip_left;
		}
		if( x2>blit.clip_right ) x2 = blit.clip_right;
		if( x1>=x2 ) return;
	}
	if( sprite->flags & SPRITE_FLIPY ){
		y2 = sprite->y;
		y1 = y2+sprite->total_height;
		dy = -1;
		if( y2<blit.clip_top ) y2 = blit.clip_top;
		if( y1>blit.clip_bottom ){
			ycount0 = (y1-blit.clip_bottom)*sprite->tile_height;
			y1 = blit.clip_bottom;
		}
		if( y2>=y1 ) return;
		y1--; y2--;
	}
	else {
		y1 = sprite->y;
		y2 = y1+sprite->total_height;
		dy = 1;
		if( y1<blit.clip_top ){
			ycount0 = (blit.clip_top-y1)*sprite->tile_height;
			y1 = blit.clip_top;
		}
		if( y2>blit.clip_bottom ) y2 = blit.clip_bottom;
		if( y1>=y2 ) return;
	}

	if(!(sprite->flags & (SPRITE_SHADOW | SPRITE_PARTIAL_SHADOW)))
	{
		const unsigned char *pen_data = sprite->pen_data;
		const unsigned short *pal_data = sprite->pal_data;
		int x,y;
		unsigned int pen;
		int pitch = blit.line_offset*dy/2;
		UINT16 *dest = (UINT16 *)(blit.baseaddr + blit.line_offset*y1);
		int ycount = ycount0;

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
			int xcount = xcount0;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				UINT16 *dest1;

				ycount = ycount0;
				while( xcount>=sprite->total_width ){
					xcount -= sprite->total_width;
					pen_data+=sprite->line_offset;
				}
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					while( ycount>=sprite->total_height ){
						ycount -= sprite->total_height;
						source ++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
                    if( pen ) *dest1 = pal_data[pen];
					ycount+= sprite->tile_height;
					dest1 += pitch;
				}
				xcount += sprite->tile_width;
			}
		}
		else {
			for( y=y1; y!=y2; y+=dy ){
				int xcount = xcount0;
				const unsigned char *source;
				while( ycount>=sprite->total_height ){
					ycount -= sprite->total_height;
					pen_data += sprite->line_offset;
				}
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					while( xcount>=sprite->total_width ){
						xcount -= sprite->total_width;
						source++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
                    if( pen ) dest[x] = pal_data[pen];
					xcount += sprite->tile_width;
				}
				ycount += sprite->tile_height;
				dest += pitch;
			}
		}
	}
	else if(sprite->flags & SPRITE_PARTIAL_SHADOW)
	{
		const unsigned char *pen_data = sprite->pen_data;
		const unsigned short *pal_data = sprite->pal_data;
		int x,y;
		unsigned int pen;
		int pitch = (blit.line_offset*dy)>>1;
		UINT16 *dest = (UINT16 *)(blit.baseaddr + blit.line_offset*y1);
		int ycount = ycount0;

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
			int xcount = xcount0;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				UINT16 *dest1;

				ycount = ycount0;
				while( xcount>=sprite->total_width ){
					xcount -= sprite->total_width;
					pen_data+=sprite->line_offset;
				}
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					while( ycount>=sprite->total_height ){
						ycount -= sprite->total_height;
						source ++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen==sprite->shadow_pen ) *dest1 = shade_table[*dest1];
					else if( pen ) *dest1 = pal_data[pen];
					ycount+= sprite->tile_height;
					dest1 += pitch;
				}
				xcount += sprite->tile_width;
			}
		}
		else {
			for( y=y1; y!=y2; y+=dy ){
				int xcount = xcount0;
				const unsigned char *source;
				while( ycount>=sprite->total_height ){
					ycount -= sprite->total_height;
					pen_data += sprite->line_offset;
				}
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					while( xcount>=sprite->total_width ){
						xcount -= sprite->total_width;
						source++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen==sprite->shadow_pen ) dest[x] = shade_table[dest[x]];
					else if( pen ) dest[x] = pal_data[pen];
					xcount += sprite->tile_width;
				}
				ycount += sprite->tile_height;
				dest += pitch;
			}
		}
	}
	else
	{	// Shadow Sprite
		const unsigned char *pen_data = sprite->pen_data;
		int x,y;
		unsigned int pen;
		int pitch = (blit.line_offset*dy)>>1;
		UINT16 *dest = (UINT16 *)(blit.baseaddr + blit.line_offset*y1);
		int ycount = ycount0;

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
			int xcount = xcount0;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				UINT16 *dest1;

				ycount = ycount0;
				while( xcount>=sprite->total_width ){
					xcount -= sprite->total_width;
					pen_data+=sprite->line_offset;
				}
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					while( ycount>=sprite->total_height ){
						ycount -= sprite->total_height;
						source ++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen ) *dest1 = shade_table[*dest1];
					ycount+= sprite->tile_height;
					dest1 += pitch;
				}
				xcount += sprite->tile_width;
			}
		}
		else {
			for( y=y1; y!=y2; y+=dy ){
				int xcount = xcount0;
				const unsigned char *source;
				while( ycount>=sprite->total_height ){
					ycount -= sprite->total_height;
					pen_data += sprite->line_offset;
				}
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					while( xcount>=sprite->total_width ){
						xcount -= sprite->total_width;
						source++;
					}
					pen = *source;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen ) dest[x] = shade_table[dest[x]];
					xcount += sprite->tile_width;
				}
				ycount += sprite->tile_height;
				dest += pitch;
			}
		}
	}

}

static void _do_blit_zoom16_noscale( const struct sprite *sprite ){
	/*	assumes SPRITE_LIST_RAW_DATA flag is set */

	int x1,x2, y1,y2, dx,dy;
	int xcount0 = 0, ycount0 = 0;

	if( sprite->flags & SPRITE_FLIPX ){
		x2 = sprite->x;
		x1 = x2+sprite->total_width;
		dx = -1;
		if( x2<blit.clip_left ) x2 = blit.clip_left;
		if( x1>blit.clip_right ){
			xcount0 = x1-blit.clip_right;
			x1 = blit.clip_right;
		}
		if( x2>=x1 ) return;
		x1--; x2--;
	}
	else {
		x1 = sprite->x;
		x2 = x1+sprite->total_width;
		dx = 1;
		if( x1<blit.clip_left ){
			xcount0 = blit.clip_left-x1;
			x1 = blit.clip_left;
		}
		if( x2>blit.clip_right ) x2 = blit.clip_right;
		if( x1>=x2 ) return;
	}
	if( sprite->flags & SPRITE_FLIPY ){
		y2 = sprite->y;
		y1 = y2+sprite->total_height;
		dy = -1;
		if( y2<blit.clip_top ) y2 = blit.clip_top;
		if( y1>blit.clip_bottom ){
			ycount0 = y1-blit.clip_bottom;
			y1 = blit.clip_bottom;
		}
		if( y2>=y1 ) return;
		y1--; y2--;
	}
	else {
		y1 = sprite->y;
		y2 = y1+sprite->total_height;
		dy = 1;
		if( y1<blit.clip_top ){
			ycount0 = blit.clip_top-y1;
			y1 = blit.clip_top;
		}
		if( y2>blit.clip_bottom ) y2 = blit.clip_bottom;
		if( y1>=y2 ) return;
	}

	if(!(sprite->flags & (SPRITE_SHADOW | SPRITE_PARTIAL_SHADOW)))
	{
		int x,y;
		unsigned int pen;
		int pitch = (blit.line_offset*dy)>>1;
		UINT16 *dest = (UINT16 *)(blit.baseaddr + blit.line_offset*y1);
		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
    		const unsigned char *pen_data = sprite->pen_data+xcount0*sprite->line_offset+ycount0;
    		const unsigned short *pal_data = sprite->pal_data;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				UINT16 *dest1;
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
                    if( pen ) *dest1 = pal_data[pen];
					dest1 += pitch;
				}
				pen_data+=sprite->line_offset;
			}
		}
		else {
    		const unsigned char *pen_data = sprite->pen_data+xcount0+ycount0*sprite->line_offset;
    		const unsigned short *pal_data = sprite->pal_data;
			for( y=y1; y!=y2; y+=dy ){
				const unsigned char *source;
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
                    if( pen ) dest[x] = pal_data[pen];
				}
				pen_data += sprite->line_offset;
				dest += pitch;
			}
		}
	}
	else if(sprite->flags & SPRITE_PARTIAL_SHADOW)
	{
		int x,y;
		unsigned int pen;
		int pitch = (blit.line_offset*dy)>>1;
		UINT16 *dest = (UINT16 *)(blit.baseaddr + blit.line_offset*y1);

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
    		const unsigned char *pen_data = sprite->pen_data+xcount0*sprite->line_offset+ycount0;
    		const unsigned short *pal_data = sprite->pal_data;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				UINT16 *dest1;
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen==sprite->shadow_pen ) *dest1 = shade_table[*dest1];
					else if( pen ) *dest1 = pal_data[pen];
					dest1 += pitch;
				}
				pen_data+=sprite->line_offset;
			}
		}
		else {
    		const unsigned char *pen_data = sprite->pen_data+xcount0+ycount0*sprite->line_offset;
    		const unsigned short *pal_data = sprite->pal_data;
			for( y=y1; y!=y2; y+=dy ){
				const unsigned char *source;
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen==sprite->shadow_pen ) dest[x] = shade_table[dest[x]];
					else if( pen ) dest[x] = pal_data[pen];
				}
				pen_data += sprite->line_offset;
				dest += pitch;
			}
		}
	}
	else
	{	// Shadow Sprite
		int x,y;
		unsigned int pen;
		int pitch = blit.line_offset*dy/2;
		UINT16 *dest = (UINT16 *)(blit.baseaddr + blit.line_offset*y1);

		if( orientation & ORIENTATION_SWAP_XY ){ /* manually rotate the sprite graphics */
    		const unsigned char *pen_data = sprite->pen_data+xcount0*sprite->line_offset+ycount0;
			for( x=x1; x!=x2; x+=dx ){
				const unsigned char *source;
				UINT16 *dest1;
				source = pen_data;
				dest1 = &dest[x];
				for( y=y1; y!=y2; y+=dy ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen ) *dest1 = shade_table[*dest1];
					dest1 += pitch;
				}
				pen_data+=sprite->line_offset;
			}
		}
		else {
    		const unsigned char *pen_data = sprite->pen_data+xcount0+ycount0*sprite->line_offset;
			for( y=y1; y!=y2; y+=dy ){
				const unsigned char *source;
				source = pen_data;
				for( x=x1; x!=x2; x+=dx ){
					pen = *source++;
					if( pen==0xff ) break; /* marker for right side of sprite; needed for AltBeast, ESwat */
					if( pen ) dest[x] = shade_table[dest[x]];
				}
				pen_data += sprite->line_offset;
				dest += pitch;
			}
		}
	}

}

static void do_blit_zoom16( const struct sprite *sprite ){
    if ((sprite->tile_width==sprite->total_width) && (sprite->tile_height==sprite->total_height))
    {
        _do_blit_zoom16_noscale(sprite);
    }
    else
    {
        _do_blit_zoom16(sprite);
    }
}

/*********************************************************************/

void sprite_init( void ){
	const struct rectangle *clip = &Machine->visible_area;
	int left = clip->min_x;
	int top = clip->min_y;
	int right = clip->max_x+1;
	int bottom = clip->max_y+1;

	struct osd_bitmap *bitmap = Machine->scrbitmap;
	screen_baseaddr = bitmap->line[0];
	screen_line_offset = bitmap->line[1]-bitmap->line[0];

	orientation = Machine->orientation;
	screen_width = Machine->scrbitmap->width;
	screen_height = Machine->scrbitmap->height;

	if( orientation & ORIENTATION_SWAP_XY ){
		SWAP(left,top)
		SWAP(right,bottom)
	}
	if( orientation & ORIENTATION_FLIP_X ){
		SWAP(left,right)
		left = screen_width-left;
		right = screen_width-right;
	}
	if( orientation & ORIENTATION_FLIP_Y ){
		SWAP(top,bottom)
		top = screen_height-top;
		bottom = screen_height-bottom;
	}

	screen_clip_left = left;
	screen_clip_right = right;
	screen_clip_top = top;
	screen_clip_bottom = bottom;
}

void sprite_close( void ){
	struct sprite_list *sprite_list = first_sprite_list;
	mask_buffer_dispose();

	while( sprite_list ){
		struct sprite_list *next = sprite_list->next;
		free( sprite_list->sprite );
		free( sprite_list );
		sprite_list = next;
	}
	first_sprite_list = NULL;
}

struct sprite_list *sprite_list_create( int num_sprites, int flags ){
	struct sprite *sprite = (struct sprite *)calloc( num_sprites, sizeof(struct sprite) );
	struct sprite_list *sprite_list = (struct sprite_list *)calloc( 1, sizeof(struct sprite_list) );

	sprite_list->num_sprites = num_sprites;
	sprite_list->special_pen = -1;
	sprite_list->sprite = sprite;
	sprite_list->flags = flags;

	/* resource tracking */
	sprite_list->next = first_sprite_list;
	first_sprite_list = sprite_list;

	return sprite_list; /* warning: no error checking! */
}

static void sprite_update_helper( struct sprite_list *sprite_list ){
	struct sprite *sprite_table = sprite_list->sprite;

	/* initialize constants */
	blit.transparent_pen = sprite_list->transparent_pen;
	blit.write_to_mask = 1;
	blit.clip_left = 0;
	blit.clip_top = 0;

	/* make a pass to adjust for screen orientation */
	if( orientation & ORIENTATION_SWAP_XY ){
		struct sprite *sprite = sprite_table;
		const struct sprite *finish = &sprite[sprite_list->num_sprites];
		while( sprite<finish ){
			SWAP(sprite->x, sprite->y)
			SWAP(sprite->total_height,sprite->total_width)
			SWAP(sprite->tile_width,sprite->tile_height)
			SWAP(sprite->x_offset,sprite->y_offset)

			/* we must also swap the flipx and flipy bits (if they aren't identical) */
			if( sprite->flags&SPRITE_FLIPX ){
				if( !(sprite->flags&SPRITE_FLIPY) ){
					sprite->flags = (sprite->flags&~SPRITE_FLIPX)|SPRITE_FLIPY;
				}
			}
			else {
				if( sprite->flags&SPRITE_FLIPY ){
					sprite->flags = (sprite->flags&~SPRITE_FLIPY)|SPRITE_FLIPX;
				}
			}
			sprite++;
		}
	}
	if( orientation & ORIENTATION_FLIP_X ){
		struct sprite *sprite = sprite_table;
		const struct sprite *finish = &sprite[sprite_list->num_sprites];
#ifndef PREROTATE_GFX
		int toggle_bit = SPRITE_FLIPX;
#else
		int toggle_bit = (sprite_list->flags & SPRITE_LIST_RAW_DATA)?SPRITE_FLIPX:0;
#endif
		while( sprite<finish ){
			sprite->x = screen_width - (sprite->x+sprite->total_width);
			sprite->flags ^= toggle_bit;

			/* extra processing for packed sprites */
			sprite->x_offset = sprite->tile_width - (sprite->x_offset+sprite->total_width);
			sprite++;
		}
	}
	if( orientation & ORIENTATION_FLIP_Y ){
		struct sprite *sprite = sprite_table;
		const struct sprite *finish = &sprite[sprite_list->num_sprites];
#ifndef PREROTATE_GFX
		int toggle_bit = SPRITE_FLIPY;
#else
		int toggle_bit = (sprite_list->flags & SPRITE_LIST_RAW_DATA)?SPRITE_FLIPY:0;
#endif
		while( sprite<finish ){
			sprite->y = screen_height - (sprite->y+sprite->total_height);
			sprite->flags ^= toggle_bit;

			/* extra processing for packed sprites */
			sprite->y_offset = sprite->tile_height - (sprite->y_offset+sprite->total_height);
			sprite++;
		}
	}
	{ /* visibility check */
		struct sprite *sprite = sprite_table;
		const struct sprite *finish = &sprite[sprite_list->num_sprites];
		while( sprite<finish ){
			if( (FlickeringInvisible && (sprite->flags & SPRITE_FLICKER)) ||
				sprite->total_width<=0 || sprite->total_height<=0 ||
				sprite->x + sprite->total_width<=0 || sprite->x>=screen_width ||
				sprite->y + sprite->total_height<=0 || sprite->y>=screen_height ){
				sprite->flags &= (~SPRITE_VISIBLE);
			}
			sprite++;
		}
	}
	{
		int j,i, dir, last;
		void (*do_blit)( const struct sprite * );

		switch( sprite_list->sprite_type ){
			case SPRITE_TYPE_ZOOM:
			do_blit = do_blit_zoom;
			return;
			break;

			case SPRITE_TYPE_STACK:
			do_blit = do_blit_stack;
			break;

			case SPRITE_TYPE_UNPACK:
			default:
			do_blit = do_blit_unpack;
			break;
		}

		sprite_order_setup( sprite_list, &i, &last, &dir );

		for(;;){ /* process each sprite */
			struct sprite *sprite = &sprite_table[i];
			sprite->mask_offset = -1;

			if( sprite->flags & SPRITE_VISIBLE ){
				int priority = sprite->priority;

				if( palette_used_colors ){
					UINT32 pen_usage = sprite->pen_usage;
					int indx = sprite->pal_data - Machine->remapped_colortable;
					while( pen_usage ){
						if( pen_usage&1 ) palette_used_colors[indx] = PALETTE_COLOR_USED;
						pen_usage>>=1;
						indx++;
					}
				}

				if( i!=last && priority<sprite_list->max_priority ){
					blit.origin_x = sprite->x;
					blit.origin_y = sprite->y;
					/* clip_left and clip_right are always zero */
					blit.clip_right = sprite->total_width;
					blit.clip_bottom = sprite->total_height;
				/*
					The following loop ensures that even though we are drawing all priority 3
					sprites before drawing the priority 2 sprites, and priority 2 sprites before the
					priority 1 sprites, that the sprite order as a whole still dictates
					sprite-to-sprite priority when sprite pixels overlap and aren't obscured by a
					background.  Checks are done to avoid special handling for the cases where
					masking isn't needed.

					max priority sprites are always drawn first, so we don't need to do anything
					special to cause them to be obscured by other sprites
				*/
					j = i+dir;
					for(;;){
						struct sprite *front = &sprite_table[j];
						if( (front->flags&SPRITE_VISIBLE) && front->priority>priority ){

							if( front->x < sprite->x+sprite->total_width &&
								front->y < sprite->y+sprite->total_height &&
								front->x+front->total_width > sprite->x &&
								front->y+front->total_height > sprite->y )
							{
								/* uncomment the following line to see which sprites are corrected */
								//sprite->pal_data = Machine->remapped_colortable+(rand()&0xff);

								if( sprite->mask_offset<0 ){ /* first masking? */
									sprite->mask_offset = mask_buffer_alloc( sprite->total_width * sprite->total_height );
									blit.line_offset = sprite->total_width;
									blit.baseaddr = &mask_buffer[sprite->mask_offset];
								}
								do_blit( front );
							}
						}
						if( j==last ) break;
						j += dir;
					} /* next j */
				} /* priority<SPRITE_MAX_PRIORITY */
			} /* visible */
			if( i==last ) break;
			i += dir;
		} /* next i */
	}
}

void sprite_update( void ){
	struct sprite_list *sprite_list = first_sprite_list;
	mask_buffer_reset();
	FlickeringInvisible = !FlickeringInvisible;
	while( sprite_list ){
		sprite_update_helper( sprite_list );
		sprite_list = sprite_list->next;
	}
}

void sprite_draw( struct sprite_list *sprite_list, int priority ){
	const struct sprite *sprite_table = sprite_list->sprite;


	{ /* set constants */
		blit.origin_x = 0;
		blit.origin_y = 0;

		blit.baseaddr = screen_baseaddr;
		blit.line_offset = screen_line_offset;
		blit.transparent_pen = sprite_list->transparent_pen;
		blit.write_to_mask = 0;

		blit.clip_left = screen_clip_left;
		blit.clip_top = screen_clip_top;
		blit.clip_right = screen_clip_right;
		blit.clip_bottom = screen_clip_bottom;
	}

	{
		int i, dir, last;
		void (*do_blit)( const struct sprite * );

		switch( sprite_list->sprite_type ){
			case SPRITE_TYPE_ZOOM:
			if (Machine->scrbitmap->depth == 16) /* 16 bit */
			{
				do_blit = do_blit_zoom16;
//				return;
			}
			else
				do_blit = do_blit_zoom;
			break;

			case SPRITE_TYPE_STACK:
			do_blit = do_blit_stack;
			break;

			case SPRITE_TYPE_UNPACK:
			default:
			do_blit = do_blit_unpack;
			break;
		}

		sprite_order_setup( sprite_list, &i, &last, &dir );
		for(;;){
			const struct sprite *sprite = &sprite_table[i];
			if( (sprite->flags&SPRITE_VISIBLE) && (sprite->priority==priority) ) do_blit( sprite );
			if( i==last ) break;
			i+=dir;
		}
	}
}


void sprite_set_shade_table(UINT16 *table)
{
	shade_table=table;
}
