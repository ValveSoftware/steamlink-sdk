/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static int gfx_bank;

WRITE_HANDLER( exctsccr_gfx_bank_w ) {
	gfx_bank = data & 1;
}

void *exctsccr_fm_timer;

static void exctsccr_fm_callback( int param ) {
	cpu_cause_interrupt( 1, 0xff );
}

int exctsccr_vh_start( void ) {
	exctsccr_fm_timer = timer_pulse( TIME_IN_HZ( 75.0 ), 0, exctsccr_fm_callback ); /* updates fm */

	return generic_vh_start();
}

void exctsccr_vh_stop( void ) {
	if ( exctsccr_fm_timer ) {
		timer_remove( exctsccr_fm_timer );
		exctsccr_fm_timer = 0;
	}

	generic_vh_stop();
}

/***************************************************************************

  Convert the color PROMs into a more useable format.

***************************************************************************/
void exctsccr_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i,idx;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])

	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2;

		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		palette[3*i] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		palette[3*i + 1] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		palette[3*i + 2] = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
	}

	color_prom += Machine->drv->total_colors;

	/* characters */
	idx = 0;
	for (i = 0;i < 32;i++)
	{
		COLOR(0,idx++) = color_prom[256+0+(i*4)];
		COLOR(0,idx++) = color_prom[256+1+(i*4)];
		COLOR(0,idx++) = color_prom[256+2+(i*4)];
		COLOR(0,idx++) = color_prom[256+3+(i*4)];
		COLOR(0,idx++) = color_prom[256+128+0+(i*4)];
		COLOR(0,idx++) = color_prom[256+128+1+(i*4)];
		COLOR(0,idx++) = color_prom[256+128+2+(i*4)];
		COLOR(0,idx++) = color_prom[256+128+3+(i*4)];
	}

	/* sprites */

	idx=0;

	for (i = 0;i < 15*16;i++)
	{
		if ( (i%16) < 8 )
		{
			COLOR(2,idx) = color_prom[i]+16;
			idx++;
		}
	}
	for (i = 15*16;i < 16*16;i++)
	{
		if ( (i%16) > 7 )
		{
			COLOR(2,idx) = color_prom[i]+16;
			idx++;
		}
	}
	for (i = 16;i < 32;i++)
	{
		COLOR(2,idx++) = color_prom[256+0+(i*4)]+16;
		COLOR(2,idx++) = color_prom[256+1+(i*4)]+16;
		COLOR(2,idx++) = color_prom[256+2+(i*4)]+16;
		COLOR(2,idx++) = color_prom[256+3+(i*4)]+16;
		COLOR(2,idx++) = color_prom[256+128+0+(i*4)]+16;
		COLOR(2,idx++) = color_prom[256+128+1+(i*4)]+16;
		COLOR(2,idx++) = color_prom[256+128+2+(i*4)]+16;
		COLOR(2,idx++) = color_prom[256+128+3+(i*4)]+16;
	}

	/* Patch for goalkeeper */
	COLOR(2,29*8+7) = 16;

}

static void exctsccr_drawsprites( struct osd_bitmap *bitmap ) {
	int offs;
	unsigned char *OBJ1, *OBJ2;

	OBJ1 = videoram;
	OBJ2 = &(spriteram[0x20]);

	for ( offs = 0x0e; offs >= 0; offs -= 2 ) {
		int sx,sy,code,bank,flipx,flipy,color;

		sx = 256 - OBJ2[offs+1];
		sy = OBJ2[offs] - 16;

		code = ( OBJ1[offs] >> 2 ) & 0x3f;
		flipx = ( OBJ1[offs] ) & 0x01;
		flipy = ( OBJ1[offs] ) & 0x02;
		color = ( OBJ1[offs+1] ) & 0x1f;
		bank = 2;
		bank += ( ( OBJ1[offs+1] >> 4 ) & 1 );

		drawgfx(bitmap,Machine->gfx[bank],
				code,
				color,
				flipx, flipy,
				sx,sy,
				&Machine->visible_area,
				TRANSPARENCY_PEN,0);
	}

	OBJ1 = &(memory_region(REGION_CPU1)[0x8800]);
	OBJ2 = spriteram;

	for ( offs = 0x0e; offs >= 0; offs -= 2 ) {
		int sx,sy,code,bank,flipx,flipy,color;

		sx = 256 - OBJ2[offs+1];
		sy = OBJ2[offs] - 16;

		code = ( OBJ1[offs] >> 2 ) & 0x3f;
		flipx = ( OBJ1[offs] ) & 0x01;
		flipy = ( OBJ1[offs] ) & 0x02;
		color = ( OBJ1[offs+1] ) & 0x1f;
		bank = 3;

		if ( color == 0 )
			continue;

		if ( color < 0x10 )
			bank++;

		if ( color > 0x10 && color < 0x17 )
		{
			drawgfx(bitmap,Machine->gfx[4],
				code,
				0x0e,
				flipx, flipy,
				sx,sy,
				&Machine->visible_area,
				TRANSPARENCY_PEN,0);

			color += 6;
		}
		if ( color==0x1d && gfx_bank==1 )
		{
			drawgfx(bitmap,Machine->gfx[3],
				code,
				color,
				flipx, flipy,
				sx,sy,
				&Machine->visible_area,
				TRANSPARENCY_PEN,0);
			drawgfx(bitmap,Machine->gfx[4],
				code,
				color,
				flipx, flipy,
				sx,sy,
				&Machine->visible_area,
				TRANSPARENCY_COLOR, 16);

		} else
		{
		drawgfx(bitmap,Machine->gfx[bank],
				code,
				color,
				flipx, flipy,
				sx,sy,
				&Machine->visible_area,
				TRANSPARENCY_PEN,0);
		}
	}
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void exctsccr_vh_screenrefresh( struct osd_bitmap *bitmap, int full_refresh ) {
	int offs;

	/* background chars */
	for (offs = 0;offs < ( videoram_size - 0x10 );offs++) {

		if ( dirtybuffer[offs] ) {
			int sx,sy,code;

			dirtybuffer[offs] = 0;

			sx = 8 * (offs % 32);
			sy = 8 * (offs / 32);

			code = videoram[offs];

			drawgfx(tmpbitmap,Machine->gfx[gfx_bank],
					code,
					( colorram[offs] ) & 0x1f,
					0, 0,
					sx,sy,
					&Machine->visible_area,
					TRANSPARENCY_NONE,0);
		}
	}

	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* draw sprites */
	exctsccr_drawsprites( bitmap );

}
