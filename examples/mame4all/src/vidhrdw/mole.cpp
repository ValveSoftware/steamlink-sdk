/***************************************************************************
  vidhrdw/mole.c
  Functions to emulate the video hardware of Mole Attack!.
  Mole Attack's Video hardware is essentially two banks of 512 characters.
  The program uses a single byte to indicate which character goes in each location,
  and uses a control location (0x8400) to select the character sets
***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int tile_bank;
static UINT16 *tile_data;
#define NUM_ROWS 25
#define NUM_COLS 40
#define NUM_TILES (NUM_ROWS*NUM_COLS)

void moleattack_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom){
	int i;
	for( i=0; i<8; i++ ){
		colortable[i] = i;
		*palette++ = (i&1)?0xff:0x00;
		*palette++ = (i&4)?0xff:0x00;
		*palette++ = (i&2)?0xff:0x00;
	}
}

int moleattack_vh_start( void ){
	tile_data = (UINT16 *)malloc( NUM_TILES*sizeof(UINT16) );
	if( tile_data ){
		dirtybuffer = (unsigned char*)malloc( NUM_TILES );
		if( dirtybuffer ){
			memset( dirtybuffer, 1, NUM_TILES );
			return 0;
		}
		free( tile_data );
	}
	return 1; /* error */
}

void moleattack_vh_stop( void ){
	free( dirtybuffer );
	free( tile_data );
}

WRITE_HANDLER( moleattack_videoram_w ){
	if( offset<NUM_TILES ){
		if( tile_data[offset]!=data ){
			dirtybuffer[offset] = 1;
			tile_data[offset] = data | (tile_bank<<8);
		}
	}
	else if( offset==0x3ff ){ /* hack!  erase screen */
		memset( dirtybuffer, 1, NUM_TILES );
		memset( tile_data, 0, NUM_TILES*sizeof(UINT16) );
	}
}

WRITE_HANDLER( moleattack_tilesetselector_w ){
	tile_bank = data;
}

void moleattack_vh_screenrefresh( struct osd_bitmap *bitmap, int full_refresh ){
	int offs;

	if( full_refresh || palette_recalc() ){
		memset( dirtybuffer, 1, NUM_TILES );
	}

	for( offs=0; offs<NUM_TILES; offs++ ){
		if( dirtybuffer[offs] ){
			UINT16 code = tile_data[offs];
			drawgfx( bitmap, Machine->gfx[(code&0x200)?1:0],
				code&0x1ff,
				0, /* color */
				0,0, /* no flip */
				(offs%NUM_COLS)*8, /* xpos */
				(offs/NUM_COLS)*8, /* ypos */
				0, /* no clip */
				TRANSPARENCY_NONE,0 );

			dirtybuffer[offs] = 0;
		}
	}
}
