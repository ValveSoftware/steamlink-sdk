/***************************************************************************

	Video Hardware for Blood Brothers

***************************************************************************/

#include "vidhrdw/generic.h"

#define NUM_SPRITES 128

unsigned char *textlayoutram;
static unsigned char *dirtybuffer2;
static struct osd_bitmap *tmpbitmap2;
extern unsigned char *dirtybuffer2;
unsigned char *bloodbro_videoram2;
unsigned char *bloodbro_scroll;
static struct sprite_list *sprite_list;


READ_HANDLER( bloodbro_background_r ){
	return READ_WORD (&videoram[offset]);
}

WRITE_HANDLER( bloodbro_background_w ){
	int oldword = READ_WORD(&videoram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	if( oldword != newword ){
		WRITE_WORD(&videoram[offset],newword);
		dirtybuffer[offset/2] = 1;
	}
}

READ_HANDLER( bloodbro_foreground_r ){
	return READ_WORD (&bloodbro_videoram2[offset]);
}

WRITE_HANDLER( bloodbro_foreground_w ){
	int oldword = READ_WORD(&bloodbro_videoram2[offset]);
	int newword = COMBINE_WORD(oldword,data);
	if( oldword != newword ){
		WRITE_WORD(&bloodbro_videoram2[offset],newword);
		dirtybuffer2[offset/2] = 1;
	}
}

/**************************************************************************/

void bloodbro_vh_stop(void) {
	if( tmpbitmap ) bitmap_free( tmpbitmap );
	if( tmpbitmap2 ) bitmap_free( tmpbitmap2 );
	free( dirtybuffer2 );
	free( dirtybuffer );
}

int bloodbro_vh_start(void) {
	tmpbitmap = bitmap_alloc(512,256);
	dirtybuffer = (unsigned char*)malloc(32*16);
	tmpbitmap2 = bitmap_alloc(512,256);
	dirtybuffer2 = (unsigned char*)malloc(32*16);
	sprite_list = sprite_list_create( NUM_SPRITES, SPRITE_LIST_FRONT_TO_BACK );

	if( tmpbitmap && tmpbitmap2 && dirtybuffer && dirtybuffer2 ){
		memset( dirtybuffer, 1, 32*16 );
		memset( dirtybuffer2, 1, 32*16 );
		sprite_list->transparent_pen = 0xf;
		sprite_list->max_priority = 1;
		sprite_list->sprite_type = SPRITE_TYPE_STACK;
		return 0;
	}
	bloodbro_vh_stop();
	return 1;
}

/**************************************************************************/

static void draw_text( struct osd_bitmap *bitmap ){
	const struct rectangle *clip = &Machine->visible_area;
	const UINT16 *source = (const UINT16 *)textlayoutram;
	int sx, sy;
	for( sy=0; sy<32; sy++ ){
		for( sx=0; sx<32; sx++ ){
			UINT16 data = *source++;

			drawgfx(bitmap,Machine->gfx[0],
					data&0xfff, /* tile number */
					data>>12, /* color */
					0,0,   /* no flip */
					8*sx,8*sy,
					clip,TRANSPARENCY_PEN,0xf);
		}
	}
}

static void draw_background( struct osd_bitmap *bitmap ) {
	const struct GfxElement *gfx = Machine->gfx[1];
	const UINT16 *source = (UINT16 *)videoram;
	int offs;
	for( offs=0; offs<32*16; offs++ ){
		if( dirtybuffer[offs] ){
			int sx = 16*(offs%32);
			int sy = 16*(offs/32);
			UINT16 data = source[offs];
			dirtybuffer[offs] = 0;

			drawgfx(tmpbitmap,gfx,
				data&0xfff, /* tile number */
				(data&0xf000)>>12, /* color */
				0,0, /* no flip */
				sx,sy,
				0,TRANSPARENCY_NONE,0);
		}
    }
    {
		int scrollx = -READ_WORD( &bloodbro_scroll[0x20] ); /** ? **/
		int scrolly = -READ_WORD( &bloodbro_scroll[0x22] ); /** ? **/

		copyscrollbitmap(bitmap,tmpbitmap,
			1,&scrollx,1,&scrolly,
			&Machine->visible_area,
              TRANSPARENCY_NONE,0);
	}
}

static void draw_foreground( struct osd_bitmap *bitmap ) {
	struct rectangle r;
	const struct GfxElement *gfx = Machine->gfx[2];
	const UINT16 *source = (UINT16 *)bloodbro_videoram2;
	int offs;
	for( offs=0; offs<32*16; offs++ ){
		if( dirtybuffer2[offs] ){
			int sx = 16*(offs%32);
			int sy = 16*(offs/32);
			UINT16 data = source[offs];
			dirtybuffer2[offs] = 0;

			/* Necessary to use TRANSPARENCY_PEN here */
			r.min_x = sx; r.max_x = sx+15;
			r.min_y = sy; r.max_y = sy+15;
			fillbitmap(tmpbitmap2,0xf,&r);
			/******************************************/

			drawgfx( tmpbitmap2,gfx,
				data&0xfff, /* tile number */
				(data&0xf000)>>12, /* color */
				0,0,
				sx,sy,
				0,TRANSPARENCY_PEN,0xf);
		}
	}
	{
		int scrollx = -READ_WORD( &bloodbro_scroll[0x24] );
		int scrolly = -READ_WORD( &bloodbro_scroll[0x26] );

		copyscrollbitmap(bitmap,tmpbitmap2,1,&scrollx,1,&scrolly,&Machine->visible_area,
                TRANSPARENCY_PEN,0xf );
	}
}

/* SPRITE INFO (8 bytes)

   --F???SS SSSSCCCC
   ---TTTTT TTTTTTTT
   -------X XXXXXXXX
   -------- YYYYYYYY */
static void get_sprite_info( void ){
	const struct GfxElement *gfx = Machine->gfx[3];
	const unsigned short *source = (const UINT16 *)spriteram;
	struct sprite *sprite = sprite_list->sprite;
	int count = NUM_SPRITES;

	int attributes, flags, number, color, vertical_size, horizontal_size, i;
	while( count-- ){
		attributes = source[0];
		flags = 0;
		if( (attributes&0x8000)==0 ){
			flags |= SPRITE_VISIBLE;
			horizontal_size = 1 + ((attributes>>7)&7);
			vertical_size = 1 + ((attributes>>4)&7);
			sprite->priority = (attributes>>11)&1;
			number = source[1]&0x1fff;
			sprite->x = source[2]&0x1ff;
			sprite->y = source[3]&0x1ff;

			/* wraparound - could be handled by Sprite Manager?*/
			if( sprite->x >= 256) sprite->x -= 512;
			if( sprite->y >= 256) sprite->y -= 512;

			sprite->total_width = 16*horizontal_size;
			sprite->total_height = 16*vertical_size;

			sprite->tile_width = 16;
			sprite->tile_height = 16;
			sprite->line_offset = 16;

			if( attributes&0x2000 ) flags |= SPRITE_FLIPX;
			if( attributes&0x4000 ) flags |= SPRITE_FLIPY; /* ? */
			color = attributes&0xf;

			sprite->pen_data = gfx->gfxdata + number * gfx->char_modulo;
			sprite->pal_data = &gfx->colortable[gfx->color_granularity * color];

			sprite->pen_usage = 0;
			for( i=0; i<vertical_size*horizontal_size; i++ ){
				sprite->pen_usage |= gfx->pen_usage[number++];
			}
		}
		sprite->flags = flags;

		sprite++;
		source+=4;
	}
}

static void bloodbro_mark_used_colors(void){
	int offs,i;
	int code, color, colmask[0x80];
	int pal_base = Machine->drv->gfxdecodeinfo[0].color_codes_start;

	/* Build the dynamic palette */
	palette_init_used_colors();

	/* Text layer */
	pal_base = Machine->drv->gfxdecodeinfo[0].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0;offs <0x800;offs += 2) {
		code = READ_WORD(&textlayoutram[offs]);
		color=code>>12;
		if ((code&0xfff)==0xd) continue;
		colmask[color] |= Machine->gfx[0]->pen_usage[code&0xfff];
	}
	for (color = 0;color < 16;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	/* Tiles - bottom layer */
	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;
	for (offs=0; offs<256; offs++)
		palette_used_colors[pal_base + offs] = PALETTE_COLOR_USED;

	/* Tiles - top layer */
	pal_base = Machine->drv->gfxdecodeinfo[2].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0x0000;offs <0x400;offs += 2 )
	{
		code = READ_WORD(&bloodbro_videoram2[offs]);
		color= code>>12;
		colmask[color] |= Machine->gfx[2]->pen_usage[code&0xfff];
	}

	for (color = 0;color < 16;color++){
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
		/* kludge */
		palette_used_colors[pal_base + 16 * color +15] = PALETTE_COLOR_TRANSPARENT;
		palette_change_color(pal_base + 16 * color +15 ,0,0,0);
	}
}

void bloodbro_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh ){
	get_sprite_info();

	bloodbro_mark_used_colors();
	sprite_update();

	if (palette_recalc()) {
		memset(dirtybuffer,1,32*16);
		memset(dirtybuffer2,1,32*16);
	}

	draw_background( bitmap );
	sprite_draw( sprite_list, 1 );
	draw_foreground( bitmap );
	sprite_draw( sprite_list, 0 );
	draw_text( bitmap );
}

/* SPRITE INFO (8 bytes)

   -------- YYYYYYYY
   ---TTTTT TTTTTTTT
   CCCC--F? -?--????  Priority??
   -------X XXXXXXXX
*/

static void weststry_draw_sprites( struct osd_bitmap *bitmap, int priority) {
	int offs;
	for( offs = 0x800-8; offs > 0; offs-=8 ){
		int data = READ_WORD( &spriteram[offs+4] );
		int data0 = READ_WORD( &spriteram[offs+0] );
		int tile_number = READ_WORD( &spriteram[offs+2] )&0x1fff;
		int sx = READ_WORD( &spriteram[offs+6] )&0xff;
		int sy = 0xf0-(data0&0xff);
		int flipx = (data&0x200)>>9;
		int datax = (READ_WORD( &spriteram[offs+6] )&0x100);
		int color = (data&0xf000)>>12;

		/* Remap sprites */
		switch(tile_number&0x1800) {
			case 0x0000: break;
			case 0x0800: tile_number = (tile_number&0x7ff) | 0x1000; break;
			case 0x1000: tile_number = (tile_number&0x7ff) | 0x800; break;
			case 0x1800: break;
		}

		if ((!(data0&0x8000)) && (!datax)) {
			drawgfx(bitmap,Machine->gfx[3],
				tile_number, color, flipx,0,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0xf);
		}
	}
}

static void weststry_mark_used_colors(void){
	int offs,i;
	int colmask[0x80],code,pal_base,color;

 	/* Build the dynamic palette */
	palette_init_used_colors();

	/* Text layer */
	pal_base = Machine->drv->gfxdecodeinfo[0].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0;offs <0x800;offs += 2) {
		code = READ_WORD(&textlayoutram[offs]);
		color=code>>12;
		if ((code&0xfff)==0xd) continue;
		colmask[color] |= Machine->gfx[0]->pen_usage[code&0xfff];
	}
	for (color = 0;color < 16;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	/* Tiles - bottom layer */
	pal_base = Machine->drv->gfxdecodeinfo[1].color_codes_start;
	for (offs=0; offs<256; offs++)
		palette_used_colors[pal_base + offs] = PALETTE_COLOR_USED;

	/* Tiles - top layer */
	pal_base = Machine->drv->gfxdecodeinfo[2].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0x0000;offs <0x400;offs += 2 )
	{
		code = READ_WORD(&bloodbro_videoram2[offs]);
		color= code>>12;
		colmask[color] |= Machine->gfx[2]->pen_usage[code&0xfff];
	}
	for (color = 0;color < 16;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}

                /* kludge */
                palette_used_colors[pal_base + 16 * color +15] = PALETTE_COLOR_TRANSPARENT;
                palette_change_color(pal_base + 16 * color +15 ,0,0,0);
	}

	/* Sprites */
	pal_base = Machine->drv->gfxdecodeinfo[3].color_codes_start;
	for (color = 0;color < 16;color++) colmask[color] = 0;
	for (offs = 0;offs <0x800;offs += 8 )
	{
		color = READ_WORD(&spriteram[offs+4])>>12;
		code = READ_WORD(&spriteram[offs+2])&0x1fff;
                /* Remap code 0x800 <-> 0x1000 */
                code = (code&0x7ff) | ((code&0x800)<<1) | ((code&0x1000)>>1);

		colmask[color] |= Machine->gfx[3]->pen_usage[code];
	}
	for (color = 0;color < 16;color++)
	{
		for (i = 0;i < 15;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[pal_base + 16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	if (palette_recalc()) {
                memset(dirtybuffer,1,32*16);
                memset(dirtybuffer2,1,32*16);
	}
}

void weststry_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh ){
	weststry_mark_used_colors();

	draw_background( bitmap );
	//weststry_draw_sprites(bitmap,0);
	draw_foreground( bitmap );
	weststry_draw_sprites(bitmap,1);
	draw_text( bitmap );
}

