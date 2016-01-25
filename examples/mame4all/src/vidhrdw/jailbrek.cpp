#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *jailbrek_scroll_x;

void jailbrek_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom) {
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])
	int i;

	for ( i = 0; i < Machine->drv->total_colors; i++ ) {
		int bit0,bit1,bit2,bit3;


		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (color_prom[0] >> 4) & 0x01;
		bit1 = (color_prom[0] >> 5) & 0x01;
		bit2 = (color_prom[0] >> 6) & 0x01;
		bit3 = (color_prom[0] >> 7) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (color_prom[Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}

	color_prom += Machine->drv->total_colors;

	for ( i = 0; i < TOTAL_COLORS(0); i++ )
		COLOR(0,i) = ( *color_prom++ ) + 0x10;

	for ( i = 0; i < TOTAL_COLORS(1); i++ )
		COLOR(1,i) = *color_prom++;
}

int jailbrek_vh_start( void ) {

	if ( ( dirtybuffer = (unsigned char*)malloc( videoram_size ) ) == 0 )
		return 1;
	memset( dirtybuffer, 1, videoram_size );

	if ( ( tmpbitmap = bitmap_alloc(Machine->drv->screen_width * 2,Machine->drv->screen_height) ) == 0 ) {
		free(dirtybuffer);
		return 1;
	}

	return 0;
}

void jailbrek_vh_stop( void ) {

	free( dirtybuffer );
	bitmap_free( tmpbitmap );
}

static void drawsprites( struct osd_bitmap *bitmap ) {
	int i;

	for ( i = 0; i < spriteram_size; i += 4 ) {
		int tile, color, sx, sy, flipx, flipy;

		/* attributes = ?tyxcccc */

		sx = spriteram[i+2] - ( ( spriteram[i+1] & 0x80 ) << 1 );
		sy = spriteram[i+3];
		tile = spriteram[i] + ( ( spriteram[i+1] & 0x40 ) << 2 );
		flipx = spriteram[i+1] & 0x10;
		flipy = spriteram[i+1] & 0x20;
		color = spriteram[i+1] & 0x0f;

		drawgfx(bitmap,Machine->gfx[1],
				tile,color,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_COLOR,0);
	}
}

void jailbrek_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh) {
	int i;

	if ( full_refresh )
		memset( dirtybuffer, 1, videoram_size );

	for ( i = 0; i < videoram_size; i++ ) {
		if ( dirtybuffer[i] ) {
			int sx,sy, code;

			dirtybuffer[i] = 0;

			sx = ( i % 64 );
			sy = ( i / 64 );

			code = videoram[i] + ( ( colorram[i] & 0xc0 ) << 2 );

			drawgfx(tmpbitmap,Machine->gfx[0],
					code,
					colorram[i] & 0x0f,
					0,0,
					sx*8,sy*8,
					0,TRANSPARENCY_NONE,0);
		}
	}

	{
		int scrollx[32];

		for ( i = 0; i < 32; i++ )
			scrollx[i] = -( ( jailbrek_scroll_x[i+32] << 8 ) + jailbrek_scroll_x[i] );

		copyscrollbitmap(bitmap,tmpbitmap,32,scrollx,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}

	drawsprites( bitmap );
}
