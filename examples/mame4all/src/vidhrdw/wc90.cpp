#include "driver.h"
#include "vidhrdw/generic.h"

/* prototype */
static void wc90_draw_sprites( struct osd_bitmap *bitmap, int priority );

unsigned char *wc90_shared;
unsigned char *wc90_tile_colorram, *wc90_tile_videoram;
unsigned char *wc90_tile_colorram2, *wc90_tile_videoram2;
unsigned char *wc90_scroll0xlo, *wc90_scroll0xhi;
unsigned char *wc90_scroll1xlo, *wc90_scroll1xhi;
unsigned char *wc90_scroll2xlo, *wc90_scroll2xhi;
unsigned char *wc90_scroll0ylo, *wc90_scroll0yhi;
unsigned char *wc90_scroll1ylo, *wc90_scroll1yhi;
unsigned char *wc90_scroll2ylo, *wc90_scroll2yhi;

size_t wc90_tile_videoram_size;
size_t wc90_tile_videoram_size2;

static unsigned char *dirtybuffer1 = 0, *dirtybuffer2 = 0;
static struct osd_bitmap *tmpbitmap1 = 0,*tmpbitmap2 = 0;

int wc90_vh_start( void ) {

	if ( generic_vh_start() )
		return 1;

	if ( ( dirtybuffer1 = (unsigned char*)malloc( wc90_tile_videoram_size ) ) == 0 ) {
		return 1;
	}

	memset( dirtybuffer1, 1, wc90_tile_videoram_size );

	if ( ( tmpbitmap1 = bitmap_alloc(4*Machine->drv->screen_width,2*Machine->drv->screen_height) ) == 0 ){
		free( dirtybuffer1 );
		generic_vh_stop();
		return 1;
	}

	if ( ( dirtybuffer2 = (unsigned char*)malloc( wc90_tile_videoram_size2 ) ) == 0 ) {
		bitmap_free(tmpbitmap1);
		free(dirtybuffer1);
		generic_vh_stop();
		return 1;
	}

	memset( dirtybuffer2, 1, wc90_tile_videoram_size2 );

	if ( ( tmpbitmap2 = bitmap_alloc(4*Machine->drv->screen_width,2*Machine->drv->screen_height) ) == 0 ){
		bitmap_free(tmpbitmap1);
		free(dirtybuffer1);
		free(dirtybuffer2);
		generic_vh_stop();
		return 1;
	}

	// Free the generic bitmap and allocate one twice as wide
	free( tmpbitmap );

	if ( ( tmpbitmap = bitmap_alloc(2*Machine->drv->screen_width,Machine->drv->screen_height) ) == 0 ){
		bitmap_free(tmpbitmap1);
		bitmap_free(tmpbitmap2);
		free(dirtybuffer);
		free(dirtybuffer1);
		free(dirtybuffer2);
		generic_vh_stop();
		return 1;
	}

	return 0;
}

void wc90_vh_stop( void ) {
	free( dirtybuffer1 );
	free( dirtybuffer2 );
	bitmap_free( tmpbitmap1 );
	bitmap_free( tmpbitmap2 );
	generic_vh_stop();
}

READ_HANDLER( wc90_tile_videoram_r ) {
	return wc90_tile_videoram[offset];
}

WRITE_HANDLER( wc90_tile_videoram_w ) {
	if ( wc90_tile_videoram[offset] != data ) {
		dirtybuffer1[offset] = 1;
		wc90_tile_videoram[offset] = data;
	}
}

READ_HANDLER( wc90_tile_colorram_r ) {
	return wc90_tile_colorram[offset];
}

WRITE_HANDLER( wc90_tile_colorram_w ) {
	if ( wc90_tile_colorram[offset] != data ) {
		dirtybuffer1[offset] = 1;
		wc90_tile_colorram[offset] = data;
	}
}

READ_HANDLER( wc90_tile_videoram2_r ) {
	return wc90_tile_videoram2[offset];
}

WRITE_HANDLER( wc90_tile_videoram2_w ) {
	if ( wc90_tile_videoram2[offset] != data ) {
		dirtybuffer2[offset] = 1;
		wc90_tile_videoram2[offset] = data;
	}
}

READ_HANDLER( wc90_tile_colorram2_r ) {
	return wc90_tile_colorram2[offset];
}

WRITE_HANDLER( wc90_tile_colorram2_w ) {
	if ( wc90_tile_colorram2[offset] != data ) {
		dirtybuffer2[offset] = 1;
		wc90_tile_colorram2[offset] = data;
	}
}

READ_HANDLER( wc90_shared_r ) {
	return wc90_shared[offset];
}

WRITE_HANDLER( wc90_shared_w ) {
	wc90_shared[offset] = data;
}

void wc90_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs, i;
	int scrollx, scrolly;


	/* compute palette usage */
	{
		unsigned short palette_map[4 * 16];
		int tile, cram;

		memset (palette_map, 0, sizeof (palette_map));

		for ( offs = wc90_tile_videoram_size2 - 1; offs >= 0; offs-- )
		{
			cram = wc90_tile_colorram2[offs];
			tile = wc90_tile_videoram2[offs] + 256 * (( cram & 3 ) + ( ( cram >> 1 ) & 4 ));
			palette_map[3*16 + (cram >> 4)] |= Machine->gfx[2]->pen_usage[tile];
		}
		for ( offs = wc90_tile_videoram_size - 1; offs >= 0; offs-- )
		{
			cram = wc90_tile_colorram[offs];
			tile = wc90_tile_videoram[offs] + 256 * (( cram & 3 ) + ( ( cram >> 1 ) & 4 ));
			palette_map[2*16 + (cram >> 4)] |= Machine->gfx[1]->pen_usage[tile];
		}
		for ( offs = videoram_size - 1; offs >= 0; offs-- )
		{
			cram = colorram[offs];
			tile = videoram[offs] + ( ( cram & 0x07 ) << 8 );
			palette_map[1*16 + (cram >> 4)] |= Machine->gfx[0]->pen_usage[tile];
		}
		for (offs = 0;offs < spriteram_size;offs += 16){
			int bank = spriteram[offs+0];

			if ( bank & 4 ) { /* visible */
				int flags = spriteram[offs+4];
				palette_map[0*16 + (flags >> 4)] |= 0xfffe;
			}
		}

		/* expand the results */
		for (i = 0; i < 4*16; i++)
		{
			int usage = palette_map[i], j;
			if (usage)
			{
				palette_used_colors[i * 16 + 0] = PALETTE_COLOR_TRANSPARENT;
				for (j = 1; j < 16; j++)
					if (usage & (1 << j))
						palette_used_colors[i * 16 + j] = PALETTE_COLOR_USED;
					else
						palette_used_colors[i * 16 + j] = PALETTE_COLOR_UNUSED;
			}
			else
				memset (&palette_used_colors[i * 16 + 0], PALETTE_COLOR_UNUSED, 16);
		}

		if (palette_recalc ())
		{
			memset( dirtybuffer,  1, videoram_size );
			memset( dirtybuffer1, 1, wc90_tile_videoram_size );
			memset( dirtybuffer2, 1, wc90_tile_videoram_size2 );
		}
	}




/* commented out -- if we copyscrollbitmap below with TRANSPARENCY_NONE, we shouldn't waste our
   time here:
	wc90_draw_sprites( bitmap, 3 );
*/

	for ( offs = wc90_tile_videoram_size2-1; offs >= 0; offs-- ) {
		int sx, sy, tile;

		if ( dirtybuffer2[offs] ) {

			dirtybuffer2[offs] = 0;

			sx = ( offs % 64 );
			sy = ( offs / 64 );

			tile = wc90_tile_videoram2[offs] +
					256 * (( wc90_tile_colorram2[offs] & 3 ) + ( ( wc90_tile_colorram2[offs] >> 1 ) & 4 ));

			drawgfx(tmpbitmap2,Machine->gfx[2],
					tile,
					wc90_tile_colorram2[offs] >> 4,
					0,0,
					sx*16,sy*16,
					0,TRANSPARENCY_NONE,0);
		}
	}

	scrollx = -wc90_scroll2xlo[0] - 256 * ( wc90_scroll2xhi[0] & 3 );
	scrolly = -wc90_scroll2ylo[0] - 256 * ( wc90_scroll2yhi[0] & 1 );

	copyscrollbitmap(bitmap,tmpbitmap2,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_NONE,0);

	wc90_draw_sprites( bitmap, 2 );

	for ( offs = wc90_tile_videoram_size-1; offs >= 0; offs-- ) {
		int sx, sy, tile;

		if ( dirtybuffer1[offs] ) {

			dirtybuffer1[offs] = 0;

			sx = ( offs % 64 );
			sy = ( offs / 64 );

			tile = wc90_tile_videoram[offs] +
					256 * (( wc90_tile_colorram[offs] & 3 ) + ( ( wc90_tile_colorram[offs] >> 1 ) & 4 ));

			drawgfx(tmpbitmap1,Machine->gfx[1],
					tile,
					wc90_tile_colorram[offs] >> 4,
					0,0,
					sx*16,sy*16,
					0,TRANSPARENCY_NONE,0);
		}
	}

	scrollx = -wc90_scroll1xlo[0] - 256 * ( wc90_scroll1xhi[0] & 3 );
	scrolly = -wc90_scroll1ylo[0] - 256 * ( wc90_scroll1yhi[0] & 1 );

	copyscrollbitmap(bitmap,tmpbitmap1,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);

	wc90_draw_sprites( bitmap, 1 );

	for ( offs = videoram_size - 1; offs >= 0; offs-- ) {
		if ( dirtybuffer[offs] ) {
			int sx, sy;

			dirtybuffer[offs] = 0;

			sx = (offs % 64);
			sy = (offs / 64);

			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs] + ( ( colorram[offs] & 0x07 ) << 8 ),
					colorram[offs] >> 4,
					0,0,
					sx*8,sy*8,
					0,TRANSPARENCY_NONE,0);
		}
	}

	scrollx = -wc90_scroll0xlo[0] - 256 * ( wc90_scroll0xhi[0] & 1 );
	scrolly = -wc90_scroll0ylo[0] - 256 * ( wc90_scroll0yhi[0] & 1 );

	copyscrollbitmap(bitmap,tmpbitmap,1,&scrollx,1,&scrolly,&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen);

	wc90_draw_sprites( bitmap, 0 );
}

#define WC90_DRAW_SPRITE( code, sx, sy ) \
					drawgfx( bitmap, Machine->gfx[3], code, flags >> 4, \
					bank&1, bank&2, sx, sy, &Machine->visible_area, TRANSPARENCY_PEN, 0 )

static char pos32x32[] = { 0, 1, 2, 3 };
static char pos32x32x[] = { 1, 0, 3, 2 };
static char pos32x32y[] = { 2, 3, 0, 1 };
static char pos32x32xy[] = { 3, 2, 1, 0 };

static char pos32x64[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static char pos32x64x[] = { 5, 4, 7, 6, 1, 0, 3, 2 };
static char pos32x64y[] = { 2, 3, 0, 1,	6, 7, 4, 5 };
static char pos32x64xy[] = { 7, 6, 5, 4, 3, 2, 1, 0 };

static char pos64x32[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static char pos64x32x[] = { 1, 0, 3, 2, 5, 4, 7, 6 };
static char pos64x32y[] = { 6, 7, 4, 5, 2, 3, 0, 1 };
static char pos64x32xy[] = { 7, 6, 5, 4, 3, 2, 1, 0 };

static char pos64x64[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
static char pos64x64x[] = { 5, 4, 7, 6, 1, 0, 3, 2, 13, 12, 15, 14, 9, 8, 11, 10 };
static char pos64x64y[] = { 10, 11, 8, 9, 14, 15, 12, 13, 2, 3, 0, 1, 6, 7,	4, 5 };
static char pos64x64xy[] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

static char* p32x32[4] = {
	pos32x32,
	pos32x32x,
	pos32x32y,
	pos32x32xy
};

static char* p32x64[4] = {
	pos32x64,
	pos32x64x,
	pos32x64y,
	pos32x64xy
};

static char* p64x32[4] = {
	pos64x32,
	pos64x32x,
	pos64x32y,
	pos64x32xy
};

static char* p64x64[4] = {
	pos64x64,
	pos64x64x,
	pos64x64y,
	pos64x64xy
};

static void drawsprite_16x16( struct osd_bitmap *bitmap, int code,
							  int sx, int sy, int bank, int flags ) {
	WC90_DRAW_SPRITE( code, sx, sy );
}

static void drawsprite_16x32( struct osd_bitmap *bitmap, int code,
							  int sx, int sy, int bank, int flags ) {
	if ( bank & 2 ) {
		WC90_DRAW_SPRITE( code+1, sx, sy+16 );
		WC90_DRAW_SPRITE( code, sx, sy );
	} else {
		WC90_DRAW_SPRITE( code, sx, sy );
		WC90_DRAW_SPRITE( code+1, sx, sy+16 );
	}
}

static void drawsprite_16x64( struct osd_bitmap *bitmap, int code,
							  int sx, int sy, int bank, int flags ) {
	if ( bank & 2 ) {
		WC90_DRAW_SPRITE( code+3, sx, sy+48 );
		WC90_DRAW_SPRITE( code+2, sx, sy+32 );
		WC90_DRAW_SPRITE( code+1, sx, sy+16 );
		WC90_DRAW_SPRITE( code, sx, sy );
	} else {
		WC90_DRAW_SPRITE( code, sx, sy );
		WC90_DRAW_SPRITE( code+1, sx, sy+16 );
		WC90_DRAW_SPRITE( code+2, sx, sy+32 );
		WC90_DRAW_SPRITE( code+3, sx, sy+48 );
	}
}

static void drawsprite_32x16( struct osd_bitmap *bitmap, int code,
							  int sx, int sy, int bank, int flags ) {
	if ( bank & 1 ) {
		WC90_DRAW_SPRITE( code+1, sx+16, sy );
		WC90_DRAW_SPRITE( code, sx, sy );
	} else {
		WC90_DRAW_SPRITE( code, sx, sy );
		WC90_DRAW_SPRITE( code+1, sx+16, sy );
	}
}

static void drawsprite_32x32( struct osd_bitmap *bitmap, int code,
							  int sx, int sy, int bank, int flags ) {

	char *p = p32x32[ bank&3 ];

	WC90_DRAW_SPRITE( code+p[0], sx, sy );
	WC90_DRAW_SPRITE( code+p[1], sx+16, sy );
	WC90_DRAW_SPRITE( code+p[2], sx, sy+16 );
	WC90_DRAW_SPRITE( code+p[3], sx+16, sy+16 );
}

static void drawsprite_32x64( struct osd_bitmap *bitmap, int code,
							  int sx, int sy, int bank, int flags ) {

	char *p = p32x64[ bank&3 ];

	WC90_DRAW_SPRITE( code+p[0], sx, sy );
	WC90_DRAW_SPRITE( code+p[1], sx+16, sy );
	WC90_DRAW_SPRITE( code+p[2], sx, sy+16 );
	WC90_DRAW_SPRITE( code+p[3], sx+16, sy+16 );
	WC90_DRAW_SPRITE( code+p[4], sx, sy+32 );
	WC90_DRAW_SPRITE( code+p[5], sx+16, sy+32 );
	WC90_DRAW_SPRITE( code+p[6], sx, sy+48 );
	WC90_DRAW_SPRITE( code+p[7], sx+16, sy+48 );
}

static void drawsprite_64x16( struct osd_bitmap *bitmap, int code,
							  int sx, int sy, int bank, int flags ) {
	if ( bank & 1 ) {
		WC90_DRAW_SPRITE( code+3, sx+48, sy );
		WC90_DRAW_SPRITE( code+2, sx+32, sy );
		WC90_DRAW_SPRITE( code+1, sx+16, sy );
		WC90_DRAW_SPRITE( code, sx, sy );
	} else {
		WC90_DRAW_SPRITE( code, sx, sy );
		WC90_DRAW_SPRITE( code+1, sx+16, sy );
		WC90_DRAW_SPRITE( code+2, sx+32, sy );
		WC90_DRAW_SPRITE( code+3, sx+48, sy );
	}
}

static void drawsprite_64x32( struct osd_bitmap *bitmap, int code,
							  int sx, int sy, int bank, int flags ) {

	char *p = p64x32[ bank&3 ];

	WC90_DRAW_SPRITE( code+p[0], sx, sy );
	WC90_DRAW_SPRITE( code+p[1], sx+16, sy );
	WC90_DRAW_SPRITE( code+p[2], sx, sy+16 );
	WC90_DRAW_SPRITE( code+p[3], sx+16, sy+16 );
	WC90_DRAW_SPRITE( code+p[4], sx+32, sy );
	WC90_DRAW_SPRITE( code+p[5], sx+48, sy );
	WC90_DRAW_SPRITE( code+p[6], sx+32, sy+16 );
	WC90_DRAW_SPRITE( code+p[7], sx+48, sy+16 );
}

static void drawsprite_64x64( struct osd_bitmap *bitmap, int code,
							  int sx, int sy, int bank, int flags ) {

	char *p = p64x64[ bank&3 ];

	WC90_DRAW_SPRITE( code+p[0], sx, sy );
	WC90_DRAW_SPRITE( code+p[1], sx+16, sy );
	WC90_DRAW_SPRITE( code+p[2], sx, sy+16 );
	WC90_DRAW_SPRITE( code+p[3], sx+16, sy+16 );
	WC90_DRAW_SPRITE( code+p[4], sx+32, sy );
	WC90_DRAW_SPRITE( code+p[5], sx+48, sy );
	WC90_DRAW_SPRITE( code+p[6], sx+32, sy+16 );
	WC90_DRAW_SPRITE( code+p[7], sx+48, sy+16 );

	WC90_DRAW_SPRITE( code+p[8], sx, sy+32 );
	WC90_DRAW_SPRITE( code+p[9], sx+16, sy+32 );
	WC90_DRAW_SPRITE( code+p[10], sx, sy+48 );
	WC90_DRAW_SPRITE( code+p[11], sx+16, sy+48 );
	WC90_DRAW_SPRITE( code+p[12], sx+32, sy+32 );
	WC90_DRAW_SPRITE( code+p[13], sx+48, sy+32 );
	WC90_DRAW_SPRITE( code+p[14], sx+32, sy+48 );
	WC90_DRAW_SPRITE( code+p[15], sx+48, sy+48 );
}

static void drawsprite_invalid( struct osd_bitmap *bitmap, int code,
											int sx, int sy, int bank, int flags ) {
	logerror("8 pixel sprite size not supported\n" );
}

typedef void (*drawsprites_procdef)( struct osd_bitmap *, int, int, int, int, int );

static drawsprites_procdef drawsprites_proc[16] = {
	drawsprite_invalid,		/* 0000 = 08x08 */
	drawsprite_invalid,		/* 0001 = 16x08 */
	drawsprite_invalid,		/* 0010 = 32x08 */
	drawsprite_invalid,		/* 0011 = 64x08 */
	drawsprite_invalid,		/* 0100 = 08x16 */
	drawsprite_16x16,		/* 0101 = 16x16 */
	drawsprite_32x16,		/* 0110 = 32x16 */
	drawsprite_64x16,		/* 0111 = 64x16 */
	drawsprite_invalid,		/* 1000 = 08x32 */
	drawsprite_16x32,		/* 1001 = 16x32 */
	drawsprite_32x32,		/* 1010 = 32x32 */
	drawsprite_64x32,		/* 1011 = 64x32 */
	drawsprite_invalid,		/* 1100 = 08x64 */
	drawsprite_16x64,		/* 1101 = 16x64 */
	drawsprite_32x64,		/* 1110 = 32x64 */
	drawsprite_64x64		/* 1111 = 64x64 */
};

static void wc90_draw_sprites( struct osd_bitmap *bitmap, int priority ){
	int offs, sx,sy, flags, which;

	/* draw all visible sprites of specified priority */
	for (offs = 0;offs < spriteram_size;offs += 16){
		int bank = spriteram[offs+0];

		if ( ( bank >> 4 ) == priority ) {

			if ( bank & 4 ) { /* visible */
				which = ( spriteram[offs+2] >> 2 ) + ( spriteram[offs+3] << 6 );

				sx = spriteram[offs + 8] + ( (spriteram[offs + 9] & 1 ) << 8 );
				sy = spriteram[offs + 6] + ( (spriteram[offs + 7] & 1 ) << 8 );

				flags = spriteram[offs+4];
				( *( drawsprites_proc[ flags & 0x0f ] ) )( bitmap, which, sx, sy, bank, flags );
			}
		}
	}
}

#undef WC90_DRAW_SPRITE
