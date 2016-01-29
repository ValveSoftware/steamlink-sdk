#include "driver.h"


unsigned char *lsasquad_scrollram,*lsasquad_videoram,*lsasquad_spriteram;
size_t lsasquad_spriteram_size;


void lsasquad_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1,bit2,bit3;


		/* red component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		bit3 = (color_prom[0] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* green component */
		bit0 = (color_prom[0x400] >> 0) & 0x01;
		bit1 = (color_prom[0x400] >> 1) & 0x01;
		bit2 = (color_prom[0x400] >> 2) & 0x01;
		bit3 = (color_prom[0x400] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		/* blue component */
		bit0 = (color_prom[2*0x400] >> 0) & 0x01;
		bit1 = (color_prom[2*0x400] >> 1) & 0x01;
		bit2 = (color_prom[2*0x400] >> 2) & 0x01;
		bit3 = (color_prom[2*0x400] >> 3) & 0x01;
		*(palette++) = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		color_prom++;
	}


	/* no color PROMs here, only RAM, but the gfx data is inverted so we */
	/* cannot use the default lookup table */
	for (i = 0;i < Machine->drv->color_table_len;i++)
		colortable[i] = i ^ 0x0f;
}



static void draw_layer(struct osd_bitmap *bitmap,unsigned char *scrollram)
{
	int offs,scrollx,scrolly;


	scrollx = scrollram[3];
	scrolly = -scrollram[0];

	for (offs = 0;offs < 0x080;offs += 4)
	{
		int base,y,sx,sy,code,color;

		base = 64 * scrollram[offs+1];
		sx = 8*(offs/4) + scrollx;
		if (flip_screen) sx = 248 - sx;
		sx &= 0xff;

		for (y = 0;y < 32;y++)
		{
			int attr;

			sy = 8*y + scrolly;
			if (flip_screen) sy = 248 - sy;
			sy &= 0xff;

			attr = lsasquad_videoram[base + 2*y + 1];
			code = lsasquad_videoram[base + 2*y] + ((attr & 0x0f) << 8);
			color = attr >> 4;

			drawgfx(bitmap,Machine->gfx[0],
					code,
					color,
					flip_screen,flip_screen,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
			if (sx > 248)	/* wraparound */
				drawgfx(bitmap,Machine->gfx[0],
						code,
						color,
						flip_screen,flip_screen,
						sx-256,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}
}

static void draw_sprites(struct osd_bitmap *bitmap)
{
	int offs;

	for (offs = lsasquad_spriteram_size-4;offs >= 0;offs -= 4)
	{
		int sx,sy,attr,code,color,flipx,flipy;

		sx = lsasquad_spriteram[offs+3];
		sy = 240 - lsasquad_spriteram[offs];
		attr = lsasquad_spriteram[offs+1];
		code = lsasquad_spriteram[offs+2] + ((attr & 0x30) << 4);
		color = attr & 0x0f;
		flipx = attr & 0x40;
		flipy = attr & 0x80;

		if (flip_screen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		drawgfx(bitmap,Machine->gfx[1],
				code,
				color,
				flipx,flipy,
				sx,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
		/* wraparound */
		drawgfx(bitmap,Machine->gfx[1],
				code,
				color,
				flipx,flipy,
				sx-256,sy,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

void lsasquad_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	fillbitmap(bitmap,Machine->pens[511],&Machine->visible_area);

	draw_layer(bitmap,lsasquad_scrollram + 0x000);
	draw_layer(bitmap,lsasquad_scrollram + 0x080);
	draw_sprites(bitmap);
	draw_layer(bitmap,lsasquad_scrollram + 0x100);
}
