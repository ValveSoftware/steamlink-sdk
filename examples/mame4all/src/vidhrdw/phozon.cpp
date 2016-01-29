/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *phozon_spriteram;

/***************************************************************************

  Convert the color PROMs into a more useable format.

  The palette PROMs are connected to the RGB output this way:

  bit 3 -- 220 ohm resistor  -- RED/GREEN/BLUE
        -- 470 ohm resistor  -- RED/GREEN/BLUE
        -- 1  kohm resistor  -- RED/GREEN/BLUE
  bit 0 -- 2.2kohm resistor  -- RED/GREEN/BLUE

***************************************************************************/
void phozon_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom){
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])

	for (i = 0; i < Machine->drv->total_colors; i++){
		int bit0,bit1,bit2,bit3;

		/* red component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* green component */
		bit0 = (color_prom[Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* blue component */
		bit0 = (color_prom[2*Machine->drv->total_colors] >> 0) & 0x01;
		bit1 = (color_prom[2*Machine->drv->total_colors] >> 1) & 0x01;
		bit2 = (color_prom[2*Machine->drv->total_colors] >> 2) & 0x01;
		bit3 = (color_prom[2*Machine->drv->total_colors] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}

	color_prom += 2*Machine->drv->total_colors;
	/* color_prom now points to the beginning of the lookup table */

	/* characters */
	for (i = 0; i < TOTAL_COLORS(0); i++)
		COLOR(0,i) = (*(color_prom++) & 0x0f);
	/* sprites */
	for (i = 0; i < TOTAL_COLORS(2); i++)
		COLOR(2,i) = (*(color_prom++) & 0x0f) + 0x10;
}

int phozon_vh_start( void ) {
	/* set up spriteram area */
	spriteram_size = 0x80;
	spriteram = &phozon_spriteram[0x780];
	spriteram_2 = &phozon_spriteram[0x780+0x800];
	spriteram_3 = &phozon_spriteram[0x780+0x800+0x800];

	return generic_vh_start();
}

void phozon_vh_stop( void ) {

	generic_vh_stop();
}

void phozon_draw_sprite(struct osd_bitmap *dest,unsigned int code,unsigned int color,
	int flipx,int flipy,int sx,int sy)
{
	drawgfx(dest,Machine->gfx[2],code,color,flipx,flipy,sx,sy,&Machine->visible_area,
		TRANSPARENCY_PEN,0);
}

void phozon_draw_sprite8(struct osd_bitmap *dest,unsigned int code,unsigned int color,
	int flipx,int flipy,int sx,int sy)
{
	drawgfx(dest,Machine->gfx[3],code,color,flipx,flipy,sx,sy,&Machine->visible_area,
		TRANSPARENCY_PEN,0);
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void phozon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* for every character in the video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,mx,my;


			dirtybuffer[offs] = 0;

			/* Even if Phozon screen is 28x36, the memory layout is 32x32. We therefore
			have to convert the memory coordinates into screen coordinates.
			Note that 32*32 = 1024, while 28*36 = 1008: therefore 16 bytes of Video RAM
			don't map to a screen position. We don't check that here, however: range
			checking is performed by drawgfx(). */

			mx = offs % 32;
			my = offs / 32;

			if (my <= 1)
			{       /* bottom screen characters */
				sx = my + 34;
				sy = mx - 2;
			}
			else if (my >= 30)
			{	/* top screen characters */
				sx = my - 30;
				sy = mx - 2;
			}
			else
			{               /* middle screen characters */
				sx = mx + 2;
				sy = my - 2;
			}

			drawgfx(tmpbitmap,Machine->gfx[(colorram[offs] & 0x80) ? 1 : 0],
					videoram[offs],
					colorram[offs] & 0x3f,
					0,0,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
			}
	}

	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* Draw the sprites. */
	for (offs = 0;offs < spriteram_size;offs += 2){
		/* is it on? */
		if ((spriteram_3[offs+1] & 2) == 0){
			int sprite = spriteram[offs];
			int color = spriteram[offs+1];
			int x = (spriteram_2[offs+1]-69) + 0x100*(spriteram_3[offs+1] & 1);
			int y = ( Machine->drv->screen_height ) - spriteram_2[offs] - 8;
			int flipx = spriteram_3[offs] & 1;
			int flipy = spriteram_3[offs] & 2;

			switch (spriteram_3[offs] & 0x3c)
			{
				case 0x00:		/* 16x16 */
					phozon_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y);
					break;

				case 0x14:		/* 8x8 */
					sprite = (sprite << 2) | ((spriteram_3[offs] & 0xc0) >> 6);
					phozon_draw_sprite8(bitmap,sprite,color,flipx,flipy,x,y+8);
					break;

				case 0x04:		/* 8x16 */
					sprite = (sprite << 2) | ((spriteram_3[offs] & 0xc0) >> 6);
					if (!flipy){
						phozon_draw_sprite8(bitmap,2+sprite,color,flipx,flipy,x,y+8);
						phozon_draw_sprite8(bitmap,sprite,color,flipx,flipy,x,y);
					}
					else{
						phozon_draw_sprite8(bitmap,2+sprite,color,flipx,flipy,x,y);
						phozon_draw_sprite8(bitmap,sprite,color,flipx,flipy,x,y+8);
					}
					break;

				case 0x24:		/* 8x32 */
					sprite = (sprite << 2) | ((spriteram_3[offs] & 0xc0) >> 6);
					if (!flipy){
						phozon_draw_sprite8(bitmap,10+sprite,color,flipx,flipy,x,y+8);
						phozon_draw_sprite8(bitmap,8+sprite,color,flipx,flipy,x,y);
						phozon_draw_sprite8(bitmap,2+sprite,color,flipx,flipy,x,y-8);
						phozon_draw_sprite8(bitmap,sprite,color,flipx,flipy,x,y-16);
					}
					else{
						phozon_draw_sprite8(bitmap,10+sprite,color,flipx,flipy,x,y-16);
						phozon_draw_sprite8(bitmap,8+sprite,color,flipx,flipy,x,y-8);
						phozon_draw_sprite8(bitmap,2+sprite,color,flipx,flipy,x,y);
						phozon_draw_sprite8(bitmap,sprite,color,flipx,flipy,x,y+8);
					}
					break;

				default:
					phozon_draw_sprite(bitmap,rand(),color,flipx,flipy,x,y);
					break;
			}
		}
	}


	/* redraw high priority chars */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (colorram[offs] & 0x40)
		{
			int sx,sy,mx,my;


			/* Even if Phozon screen is 28x36, the memory layout is 32x32. We therefore
			have to convert the memory coordinates into screen coordinates.
			Note that 32*32 = 1024, while 28*36 = 1008: therefore 16 bytes of Video RAM
			don't map to a screen position. We don't check that here, however: range
			checking is performed by drawgfx(). */

			mx = offs % 32;
			my = offs / 32;

			if (my <= 1)
			{       /* bottom screen characters */
				sx = my + 34;
				sy = mx - 2;
			}
			else if (my >= 30)
			{	/* top screen characters */
				sx = my - 30;
				sy = mx - 2;
			}
			else
			{               /* middle screen characters */
				sx = mx + 2;
				sy = my - 2;
			}

			drawgfx(bitmap,Machine->gfx[(colorram[offs] & 0x80) ? 1 : 0],
					videoram[offs],
					colorram[offs] & 0x3f,
					0,0,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
			}
	}
}
