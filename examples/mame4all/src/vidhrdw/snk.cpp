#include "driver.h"
#include "vidhrdw/generic.h"

int snk_bg_tilemap_baseaddr, gwar_sprite_placement;

#define MAX_VRAM_SIZE (64*64*2)

//static int k = 0; /*for debugging use */

static int shadows_visible = 0; /* toggles rapidly to fake translucency in ikari warriors */

#define GFX_CHARS			0
#define GFX_TILES			1
#define GFX_SPRITES			2
#define GFX_BIGSPRITES			3

void snk_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom){
	int i;
	int num_colors = 1024;
	for( i=0; i<num_colors; i++ ){
		int bit0,bit1,bit2,bit3;

		colortable[i] = i;

		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*palette++ = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (color_prom[num_colors] >> 0) & 0x01;
		bit1 = (color_prom[num_colors] >> 1) & 0x01;
		bit2 = (color_prom[num_colors] >> 2) & 0x01;
		bit3 = (color_prom[num_colors] >> 3) & 0x01;
		*palette++ = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (color_prom[2*num_colors] >> 0) & 0x01;
		bit1 = (color_prom[2*num_colors] >> 1) & 0x01;
		bit2 = (color_prom[2*num_colors] >> 2) & 0x01;
		bit3 = (color_prom[2*num_colors] >> 3) & 0x01;
		*palette++ = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}
}

void ikari_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom){
	int i;
	snk_vh_convert_color_prom( palette, colortable, color_prom);

	palette += 6*3;
	/*
		pen#6 is used for translucent shadows;
		we'll just make it dark grey for now
	*/
	for( i=0; i<256; i+=8 ){
		palette[i*3+0] = palette[i*3+1] = palette[i*3+2] = 14;
	}
}

int snk_vh_start( void ){
	dirtybuffer = (unsigned char*)malloc( MAX_VRAM_SIZE );
	if( dirtybuffer ){
		tmpbitmap = bitmap_alloc( 512, 512 );
		if( tmpbitmap ){
			memset( dirtybuffer, 0xff, MAX_VRAM_SIZE  );
			shadows_visible = 1;
			return 0;
		}
		free( dirtybuffer );
	}
	return 1;
}

void snk_vh_stop( void ){
	bitmap_free( tmpbitmap );
	tmpbitmap = 0;
	free( dirtybuffer );
	dirtybuffer = 0;
}

/**************************************************************************************/

static void tnk3_draw_background( struct osd_bitmap *bitmap, int scrollx, int scrolly ){
	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[GFX_TILES];
	int offs;
	for( offs=0; offs<64*64*2; offs+=2 ){
		int tile_number = videoram[offs];
		unsigned char attributes = videoram[offs+1];

		if( tile_number!=dirtybuffer[offs] || attributes != dirtybuffer[offs+1] ){
			int sy = ((offs/2)%64)*8;
			int sx = ((offs/2)/64)*8;
			int color = (attributes&0xf)^0x8;

			dirtybuffer[offs] = tile_number;
			dirtybuffer[offs+1] = attributes;

			tile_number += 256*((attributes>>4)&0x3);

			drawgfx( tmpbitmap,gfx,
				tile_number,
				color,
				0,0, /* no flip */
				sx,sy,
				0,TRANSPARENCY_NONE,0);
		}
	}
	{
		copyscrollbitmap(bitmap,tmpbitmap,
			1,&scrollx,1,&scrolly,
			clip,
			TRANSPARENCY_NONE,0);
	}
}

void tnk3_draw_text( struct osd_bitmap *bitmap, int bank, unsigned char *source ){
	const struct GfxElement *gfx = Machine->gfx[GFX_CHARS];
	int offs;

	bank*=256;

	for( offs = 0;offs <0x400; offs++ ){
		drawgfx( bitmap, gfx,
			source[offs]+bank,
			source[offs]>>5,
			0,0, /* no flip */
			16+(offs/32)*8,(offs%32)*8+8,
			0,
			TRANSPARENCY_PEN,15 );
	}
}

void tnk3_draw_status( struct osd_bitmap *bitmap, int bank, unsigned char *source ){
	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[GFX_CHARS];
	int offs;

	bank *= 256;

	for( offs = 0; offs<64; offs++ ){
		int tile_number = source[offs+30*32];
		int sy = (offs % 32)*8+8;
		int sx = (offs / 32)*8;

		drawgfx(bitmap,gfx,
			tile_number+bank,
			tile_number>>5,
			0,0, /* no flip */
			sx,sy,
			clip,TRANSPARENCY_NONE,0);

		tile_number = source[offs];
		sx += 34*8;

		drawgfx(bitmap,gfx,
			tile_number+bank,
			tile_number>>5,
			0,0, /* no flip */
			sx,sy,
			clip,TRANSPARENCY_NONE,0);
	}
}

void tnk3_draw_sprites( struct osd_bitmap *bitmap, int xscroll, int yscroll ){
	static int n = 50;
	const unsigned char *source = spriteram;
	const unsigned char *finish = source+n*4;
	struct rectangle clip = Machine->visible_area;
/*
if( keyboard_pressed( KEYCODE_J ) ){
	while( keyboard_pressed( KEYCODE_J ) ){}
	n--;
}
if( keyboard_pressed( KEYCODE_K ) ){
	while( keyboard_pressed( KEYCODE_K ) ){}
	n++;
}
*/

	while( source<finish ){
		int attributes = source[3]; /* YBBX.CCCC */
		int tile_number = source[1];
		int sy = source[0] + ((attributes&0x10)?256:0) - yscroll;
		int sx = source[2] + ((attributes&0x80)?256:0) - xscroll;
		int color = attributes&0xf;

		if( attributes&0x40 ) tile_number += 256;
		if( attributes&0x20 ) tile_number += 512;

		drawgfx(bitmap,Machine->gfx[GFX_SPRITES],
			tile_number,
			color,
			0,0,
			(256-sx)&0x1ff,sy&0x1ff,
			&clip,TRANSPARENCY_PEN,7);

		source+=4;
	}
}

void tnk3_vh_screenrefresh( struct osd_bitmap *bitmap, int full_refresh ){
	unsigned char *ram = memory_region(REGION_CPU1);
	int attributes = ram[0xc800];
	/*
		X-------
		-X------	character bank (for text layer)
		--X-----
		---X----	scrolly MSB (background)
		----X---	scrolly MSB (sprites)
		-----X--
		------X-	scrollx MSB (background)
		-------X	scrollx MSB (sprites)
	*/

	/* to be moved to memmap */
	spriteram = &ram[0xd000];
	videoram = &ram[0xd800];

	{
		int scrolly =  -8+ram[0xcb00]+((attributes&0x10)?256:0);
		int scrollx = -16+ram[0xcc00]+((attributes&0x02)?256:0);
		tnk3_draw_background( bitmap, -scrollx, -scrolly );
	}

	{
		int scrolly =  8+ram[0xc900] + ((attributes&0x08)?256:0);
		int scrollx = 30+ram[0xca00] + ((attributes&0x01)?256:0);
		tnk3_draw_sprites( bitmap, scrollx, scrolly );
	}

	{
		int bank = (attributes&0x40)?1:0;
		tnk3_draw_text( bitmap, bank, &ram[0xf800] );
		tnk3_draw_status( bitmap, bank, &ram[0xfc00] );
	}
}

/**************************************************************************************/

static void ikari_draw_background( struct osd_bitmap *bitmap, int xscroll, int yscroll ){
	const struct GfxElement *gfx = Machine->gfx[GFX_TILES];
	const unsigned char *source = &memory_region(REGION_CPU1)[snk_bg_tilemap_baseaddr];

	int offs;
	for( offs=0; offs<32*32*2; offs+=2 ){
		int tile_number = source[offs];
		unsigned char attributes = source[offs+1];

		if( tile_number!=dirtybuffer[offs] ||
			attributes != dirtybuffer[offs+1] ){

			int sy = ((offs/2)%32)*16;
			int sx = ((offs/2)/32)*16;

			dirtybuffer[offs] = tile_number;
			dirtybuffer[offs+1] = attributes;

			tile_number+=256*(attributes&0x3);

			drawgfx(tmpbitmap,gfx,
				tile_number,
				(attributes>>4), /* color */
				0,0, /* no flip */
				sx,sy,
				0,TRANSPARENCY_NONE,0);
		}
	}

	{
		struct rectangle clip = Machine->visible_area;
		clip.min_x += 16;
		clip.max_x -= 16;
		copyscrollbitmap(bitmap,tmpbitmap,
			1,&xscroll,1,&yscroll,
			&clip,
			TRANSPARENCY_NONE,0);
	}
}

static void ikari_draw_text( struct osd_bitmap *bitmap ){
	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[GFX_CHARS];
	const unsigned char *source = &memory_region(REGION_CPU1)[0xf800];

	int offs;
	for( offs = 0;offs <0x400; offs++ ){
		int tile_number = source[offs];
		int sy = (offs % 32)*8+8;
		int sx = (offs / 32)*8+16;

		drawgfx(bitmap,gfx,
			tile_number,
			8, /* color - vreg needs mapped! */
			0,0, /* no flip */
			sx,sy,
			clip,TRANSPARENCY_PEN,15);
	}
}

static void ikari_draw_status( struct osd_bitmap *bitmap ){
	/*	this is drawn above and below the main display */

	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[GFX_CHARS];
	const unsigned char *source = &memory_region(REGION_CPU1)[0xfc00];

	int offs;
	for( offs = 0; offs<64; offs++ ){
		int tile_number = source[offs+30*32];
		int sy = 20+(offs % 32)*8 - 16;
		int sx = (offs / 32)*8;

		drawgfx(bitmap,gfx,
			tile_number,
			8, /* color */
			0,0, /* no flip */
			sx,sy,
			clip,TRANSPARENCY_NONE,0);

		tile_number = source[offs];
		sx += 34*8;

		drawgfx(bitmap,gfx,
			tile_number,
			8, /* color */
			0,0, /* no flip */
			sx,sy,
			clip,TRANSPARENCY_NONE,0);
	}
}

static void ikari_draw_sprites_16x16( struct osd_bitmap *bitmap, int start, int quantity, int xscroll, int yscroll )
{
	int transp_mode  = shadows_visible ? TRANSPARENCY_PEN : TRANSPARENCY_PENS;
	int transp_param = shadows_visible ? 7 : ((1<<7) | (1<<6));

	int which;
	const unsigned char *source = &memory_region(REGION_CPU1)[0xe800];

	struct rectangle clip = Machine->visible_area;
	clip.min_x += 16;
	clip.max_x -= 16;

	for( which = start*4; which < (start+quantity)*4; which+=4 )
	{
		int attributes = source[which+3]; /* YBBX.CCCC */
		int tile_number = source[which+1] + ((attributes&0x60)<<3);
		int sy = - yscroll + source[which]  +((attributes&0x10)?256:0);
		int sx =   xscroll - source[which+2]+((attributes&0x80)?0:256);

		drawgfx(bitmap,Machine->gfx[GFX_SPRITES],
			tile_number,
			attributes&0xf, /* color */
			0,0, /* flip */
			-16+(sx & 0x1ff), -16+(sy & 0x1ff),
			&clip,transp_mode,transp_param);
	}
}

static void ikari_draw_sprites_32x32( struct osd_bitmap *bitmap, int start, int quantity, int xscroll, int yscroll )
{
	int transp_mode  = shadows_visible ? TRANSPARENCY_PEN : TRANSPARENCY_PENS;
	int transp_param = shadows_visible ? 7 : ((1<<7) | (1<<6));

	int which;
	const unsigned char *source = &memory_region(REGION_CPU1)[0xe000];

	struct rectangle clip = Machine->visible_area;
	clip.min_x += 16;
	clip.max_x -= 16;

	for( which = start*4; which < (start+quantity)*4; which+=4 )
	{
		int attributes = source[which+3];
		int tile_number = source[which+1];
		int sy = - yscroll + source[which] + ((attributes&0x10)?256:0);
		int sx = xscroll - source[which+2] + ((attributes&0x80)?0:256);
		if( attributes&0x40 ) tile_number += 256;

		drawgfx( bitmap,Machine->gfx[GFX_BIGSPRITES],
			tile_number,
			attributes&0xf, /* color */
			0,0, /* flip */
			-16+(sx & 0x1ff), -16+(sy & 0x1ff),
			&clip,transp_mode,transp_param );

	}
}

void ikari_vh_screenrefresh( struct osd_bitmap *bitmap, int full_refresh){
	const unsigned char *ram = memory_region(REGION_CPU1);

	shadows_visible = !shadows_visible;

	{
		int attributes = ram[0xc900];
		int scrolly =  8-ram[0xc800] - ((attributes&0x01) ? 256:0);
		int scrollx = 13-ram[0xc880] - ((attributes&0x02) ? 256:0);
		ikari_draw_background( bitmap, scrollx, scrolly );
	}
	{
		int attributes = ram[0xcd00];

		int sp16_scrolly = -7 + ram[0xca00] + ((attributes&0x04) ? 256:0);
		int sp16_scrollx = 44 + ram[0xca80] + ((attributes&0x10) ? 256:0);

		int sp32_scrolly =  9 + ram[0xcb00] + ((attributes&0x08) ? 256:0);
		int sp32_scrollx = 28 + ram[0xcb80] + ((attributes&0x20) ? 256:0);

		ikari_draw_sprites_16x16( bitmap,  0, 25, sp16_scrollx, sp16_scrolly );
		ikari_draw_sprites_32x32( bitmap,  0, 25, sp32_scrollx, sp32_scrolly );
		ikari_draw_sprites_16x16( bitmap, 25, 25, sp16_scrollx, sp16_scrolly );
	}
	ikari_draw_text( bitmap );
	ikari_draw_status( bitmap );
}

/**************************************************************/

static void tdfever_draw_background( struct osd_bitmap *bitmap,
		int xscroll, int yscroll )
{
	const struct GfxElement *gfx = Machine->gfx[GFX_TILES];
	const unsigned char *source = &memory_region(REGION_CPU1)[0xd000]; //d000

	int offs;
	for( offs=0; offs<32*32*2; offs+=2 ){
		int tile_number = source[offs];
		unsigned char attributes = source[offs+1];

		if( tile_number!=dirtybuffer[offs] ||
			attributes != dirtybuffer[offs+1] ){

			int sy = ((offs/2)%32)*16;
			int sx = ((offs/2)/32)*16;

			int color = (attributes>>4); /* color */

			dirtybuffer[offs] = tile_number;
			dirtybuffer[offs+1] = attributes;

			tile_number+=256*(attributes&0xf);

			drawgfx(tmpbitmap,gfx,
				tile_number,
				color,
				0,0, /* no flip */
				sx,sy,
				0,TRANSPARENCY_NONE,0);
		}
	}

	{
		struct rectangle clip = Machine->visible_area;
		copyscrollbitmap(bitmap,tmpbitmap,
			1,&xscroll,1,&yscroll,
			&clip,
			TRANSPARENCY_NONE,0);
	}
}

static void tdfever_draw_sprites( struct osd_bitmap *bitmap, int xscroll, int yscroll ){
	int transp_mode  = shadows_visible ? TRANSPARENCY_PEN : TRANSPARENCY_PENS;
	int transp_param = shadows_visible ? 15 : ((1<<15) | (1<<14));

	const struct GfxElement *gfx = Machine->gfx[GFX_SPRITES];
	const unsigned char *source = &memory_region(REGION_CPU1)[0xe000];

	int which;

	struct rectangle clip = Machine->visible_area;

	for( which = 0; which < 32*4; which+=4 ){
		int attributes = source[which+3];
		int tile_number = source[which+1] + 8*(attributes&0x60);

		int sy = - yscroll + source[which];
		int sx = xscroll - source[which+2];
		if( attributes&0x10 ) sy += 256;
		if( attributes&0x80 ) sx -= 256;

		sx &= 0x1ff; if( sx>512-32 ) sx-=512;
		sy &= 0x1ff; if( sy>512-32 ) sy-=512;

		drawgfx(bitmap,gfx,
			tile_number,
			(attributes&0xf), /* color */
			0,0, /* no flip */
			sx,sy,
			&clip,transp_mode,transp_param);
	}
}

static void tdfever_draw_text( struct osd_bitmap *bitmap, int attributes, int dx, int dy, int base ){
	int bank = attributes>>4;
	int color = attributes&0xf;

	const struct rectangle *clip = &Machine->visible_area;
	const struct GfxElement *gfx = Machine->gfx[GFX_CHARS];

	const unsigned char *source = &memory_region(REGION_CPU1)[base];

	int offs;

	int bank_offset = bank*256;

	for( offs = 0;offs <0x800; offs++ ){
		int tile_number = source[offs];
		int sy = dx+(offs % 32)*8;
		int sx = dy+(offs / 32)*8;

		if( source[offs] != 0x20 ){
			drawgfx(bitmap,gfx,
				tile_number + bank_offset,
				color,
				0,0, /* no flip */
				sx,sy,
				clip,TRANSPARENCY_PEN,15);
		}
	}
}

void tdfever_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh ){
	const unsigned char *ram = memory_region(REGION_CPU1);
	shadows_visible = !shadows_visible;

	{
		unsigned char bg_attributes = ram[0xc880];
		int bg_scroll_y = -30 - ram[0xc800] - ((bg_attributes&0x01)?256:0);
		int bg_scroll_x = 141 - ram[0xc840] - ((bg_attributes&0x02)?256:0);
		tdfever_draw_background( bitmap, bg_scroll_x, bg_scroll_y );
	}

	{
		unsigned char sprite_attributes = ram[0xc900];
		int sprite_scroll_y =   65 + ram[0xc980] + ((sprite_attributes&0x80)?256:0);
		int sprite_scroll_x = -135 + ram[0xc9c0] + ((sprite_attributes&0x40)?256:0);
		tdfever_draw_sprites( bitmap, sprite_scroll_x, sprite_scroll_y );
	}

	{
		unsigned char text_attributes = ram[0xc8c0];
		tdfever_draw_text( bitmap, text_attributes, 0,0, 0xf800 );
	}
}

void ftsoccer_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh ){
	const unsigned char *ram = memory_region(REGION_CPU1);
	shadows_visible = !shadows_visible;
	{
		unsigned char bg_attributes = ram[0xc880];
		int bg_scroll_y = - ram[0xc800] - ((bg_attributes&0x01)?256:0);
		int bg_scroll_x = 16 - ram[0xc840] - ((bg_attributes&0x02)?256:0);
		tdfever_draw_background( bitmap, bg_scroll_x, bg_scroll_y );
	}

	{
		unsigned char sprite_attributes = ram[0xc900];
		int sprite_scroll_y =  31 + ram[0xc980] + ((sprite_attributes&0x80)?256:0);
		int sprite_scroll_x = -40 + ram[0xc9c0] + ((sprite_attributes&0x40)?256:0);
		tdfever_draw_sprites( bitmap, sprite_scroll_x, sprite_scroll_y );
	}

	{
		unsigned char text_attributes = ram[0xc8c0];
		tdfever_draw_text( bitmap, text_attributes, 0,0, 0xf800 );
	}
}

static void gwar_draw_sprites_16x16( struct osd_bitmap *bitmap, int xscroll, int yscroll ){
	const struct GfxElement *gfx = Machine->gfx[GFX_SPRITES];
	const unsigned char *source = &memory_region(REGION_CPU1)[0xe800];

	const struct rectangle *clip = &Machine->visible_area;

	int which;
	for( which=0; which<(64)*4; which+=4 )
	{
		int attributes = source[which+3]; /* YBBX.BCCC */
		int tile_number = source[which+1];
		int sy = -xscroll + source[which];
		int sx =  yscroll - source[which+2];
		if( attributes&0x10 ) sy += 256;
		if( attributes&0x80 ) sx -= 256;

		if( attributes&0x08 ) tile_number += 256;
		if( attributes&0x20 ) tile_number += 512;
		if( attributes&0x40 ) tile_number += 1024;

		sy &= 0x1ff; if( sy>512-16 ) sy-=512;
		sx = (-sx)&0x1ff; if( sx>512-16 ) sx-=512;

		drawgfx(bitmap,gfx,
			tile_number,
			(attributes&7), /* color */
			0,0, /* flip */
			sx,sy,
			clip,TRANSPARENCY_PEN,15 );
	}
}

void gwar_draw_sprites_32x32( struct osd_bitmap *bitmap, int xscroll, int yscroll ){
	const struct GfxElement *gfx = Machine->gfx[GFX_BIGSPRITES];
	const unsigned char *source = &memory_region(REGION_CPU1)[0xe000];

	const struct rectangle *clip = &Machine->visible_area;

	int which;
	for( which=0; which<(32)*4; which+=4 )
	{
		int attributes = source[which+3];
		int tile_number = source[which+1] + 8*(attributes&0x60);

		int sy = - xscroll + source[which];
		int sx = yscroll - source[which+2];
		if( attributes&0x10 ) sy += 256;
		if( attributes&0x80 ) sx -= 256;

		sy = (sy&0x1ff);
		sx = ((-sx)&0x1ff);
		if( sy>512-32 ) sy-=512;
		if( sx>512-32 ) sx-=512;

		drawgfx(bitmap,gfx,
			tile_number,
			(attributes&0xf), /* color */
			0,0, /* no flip */
			sx,sy,
			clip,TRANSPARENCY_PEN,15);
	}
}

void gwar_vh_screenrefresh( struct osd_bitmap *bitmap, int full_refresh ){
	const unsigned char *ram = memory_region(REGION_CPU1);
	unsigned char bg_attributes, sp_attributes;

	{
		int bg_scroll_y, bg_scroll_x;

		if( gwar_sprite_placement==2 ) { /* Gwar alternate */
			bg_attributes = ram[0xf880];
			sp_attributes = ram[0xfa80];
			bg_scroll_y = - ram[0xf800] - ((bg_attributes&0x01)?256:0);
			bg_scroll_x  = 16 - ram[0xf840] - ((bg_attributes&0x02)?256:0);
		} else {
			bg_attributes = ram[0xc880];
			sp_attributes = ram[0xcac0];
			bg_scroll_y = - ram[0xc800] - ((bg_attributes&0x01)?256:0);
			bg_scroll_x  = 16 - ram[0xc840] - ((bg_attributes&0x02)?256:0);
		}
		tdfever_draw_background( bitmap, bg_scroll_x, bg_scroll_y );
	}

	{
		int sp16_y = ram[0xc900]+15;
		int sp16_x = ram[0xc940]+8;
		int sp32_y = ram[0xc980]+31;
		int sp32_x = ram[0xc9c0]+8;

		if( gwar_sprite_placement ) /* gwar */
		{
			if( bg_attributes&0x10 ) sp16_y += 256;
			if( bg_attributes&0x40 ) sp16_x += 256;
			if( bg_attributes&0x20 ) sp32_y += 256;
			if( bg_attributes&0x80 ) sp32_x += 256;
		}
		else{ /* psychos, bermudet, chopper1... */
			unsigned char spp_attributes = ram[0xca80];
			if( spp_attributes&0x04 ) sp16_y += 256;
			if( spp_attributes&0x08 ) sp32_y += 256;
			if( spp_attributes&0x10 ) sp16_x += 256;
			if( spp_attributes&0x20 ) sp32_x += 256;
		}
		if (sp_attributes & 0x20)
		{
			gwar_draw_sprites_16x16( bitmap, sp16_y, sp16_x );
			gwar_draw_sprites_32x32( bitmap, sp32_y, sp32_x );
		}
		else {
			gwar_draw_sprites_32x32( bitmap, sp32_y, sp32_x );
			gwar_draw_sprites_16x16( bitmap, sp16_y, sp16_x );
		}
	}

	{
		if( gwar_sprite_placement==2) { /* Gwar alternate */
			unsigned char text_attributes = ram[0xf8c0];
			tdfever_draw_text( bitmap, text_attributes,0,0, 0xc800 );
		}
		else {
			unsigned char text_attributes = ram[0xc8c0];
			tdfever_draw_text( bitmap, text_attributes,0,0, 0xf800 );
		}
	}
}

