/*******************************************************************************

	Karnov - Bryan McPhail, mish@tendril.co.uk

*******************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static unsigned char *karnov_foreground,*dirty_f;
static struct osd_bitmap *bitmap_f;
int karnov_scroll[4];

/***************************************************************************

  Convert the color PROMs into a more useable format.

  Karnov has two 1024x8 palette PROM.
  I don't know the exact values of the resistors between the RAM and the
  RGB output. I assumed these values (the same as Commando)

  bit 7 -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 2.2kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
        -- 1  kohm resistor  -- RED
  bit 0 -- 2.2kohm resistor  -- RED

  bit 7 -- unused
        -- unused
        -- unused
        -- unused
        -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 1  kohm resistor  -- BLUE
  bit 0 -- 2.2kohm resistor  -- BLUE

***************************************************************************/
void karnov_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;

	for (i = 0;i < Machine->drv->total_colors;i++)
	{
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
}

/******************************************************************************/

void karnov_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int my,mx,offs,color,tile;
	int scrollx=READ_WORD (&karnov_scroll[0]);
	int scrolly=READ_WORD (&karnov_scroll[2]);

	/* 1st area is stored along X-axis... */
	mx=-1; my=0;
	for (offs = 0;offs < 0x800; offs += 2) {
		mx++;
		if (mx==32) {mx=0; my++;}

		if (!dirty_f[offs]) continue; else dirty_f[offs]=0;

		tile=READ_WORD (&karnov_foreground[offs]);
		color = tile >> 12;
		tile = tile&0x7ff;

		drawgfx(bitmap_f,Machine->gfx[1],tile,
			color, 0,0, 16*mx,16*my,
		 	0,TRANSPARENCY_NONE,0);
	}

	/* 2nd area is stored along Y-axis... */
	mx=0; my=-1;
	for (offs = 0x800 ;offs < 0x1000; offs += 2) {
		my++;
		if (my==32) {my=0; mx++;}

		if (!dirty_f[offs]) continue; else dirty_f[offs]=0;

		tile=READ_WORD (&karnov_foreground[offs]);
		color = tile >> 12;
		tile=tile&0x7ff;

		drawgfx(bitmap_f,Machine->gfx[1],tile,
			color, 0,0, 16*mx,16*my,
		 	0,TRANSPARENCY_NONE,0);
	}

	scrolly=-scrolly;
	scrollx=-scrollx;
	copyscrollbitmap(bitmap,bitmap_f,1,&scrollx,1,&scrolly,0,TRANSPARENCY_NONE,0);

	/* Sprites */
	for (offs = 0;offs <0x800;offs += 8) {
		int x,y,sprite,sprite2,colour,fx,fy,extra;

	    y=READ_WORD (&buffered_spriteram[offs]);
	    if (!(y&0x8000)) continue;

	    y=y&0x1ff;
	    sprite=READ_WORD (&buffered_spriteram[offs+6]);
	    colour=sprite>>12;
	    sprite=sprite&0xfff;
	    x=READ_WORD (&buffered_spriteram[offs+4])&0x1ff;

		fx=READ_WORD (&buffered_spriteram[offs+2]);
	    if ((fx&0x10)) extra=1; else extra=0;
		fy=fx&0x2;
		fx=fx&0x4;

		if (extra) y=y+16;

	    /* Convert the co-ords..*/
		x=(x+16)%0x200;
		y=(y+16)%0x200;
		x=256 - x;
		y=256 - y;

		/* Y Flip determines order of multi-sprite */
		if (extra && fy) {
			sprite2=sprite;
			sprite++;
		}
		else sprite2=sprite+1;

		drawgfx(bitmap,Machine->gfx[2],
				sprite,
				colour,fx,fy,x,y,
				0,TRANSPARENCY_PEN,0);

    	/* 1 more sprite drawn underneath */
    	if (extra)
    		drawgfx(bitmap,Machine->gfx[2],
				sprite2,
				colour,fx,fy,x,y+16,
				0,TRANSPARENCY_PEN,0);
	}

	/* Draw character tiles */
	for (offs = videoram_size - 2;offs >= 0;offs -= 2) {
		tile=READ_WORD (&videoram[offs]);
		if (!tile) continue;
		color=tile>>14;
		tile=tile&0xfff;
		mx = (offs/2) % 32;
		my = (offs/2) / 32;
		drawgfx(bitmap,Machine->gfx[0],
			tile,color,0,0,8*mx,8*my,
			&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

void wndrplnt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int my,mx,offs,color,tile;
	int scrollx=READ_WORD (&karnov_scroll[0]);
	int scrolly=READ_WORD (&karnov_scroll[2]);

	/* 1st area is stored along X-axis... */
	mx=-1; my=0;
	for (offs = 0;offs < 0x800; offs += 2) {
		mx++;
		if (mx==32) {mx=0; my++;}

		if (!dirty_f[offs]) continue; else dirty_f[offs]=0;

		tile=READ_WORD (&karnov_foreground[offs]);
		color = tile >> 12;
		tile = tile&0x7ff;

		drawgfx(bitmap_f,Machine->gfx[1],tile,
			color, 0,0, 16*mx,16*my,
		 	0,TRANSPARENCY_NONE,0);
	}

	/* 2nd area is stored along Y-axis... */
	mx=0; my=-1;
	for (offs = 0x800 ;offs < 0x1000; offs += 2) {
		my++;
		if (my==32) {my=0; mx++;}

		if (!dirty_f[offs]) continue; else dirty_f[offs]=0;

		tile=READ_WORD (&karnov_foreground[offs]);
		color = tile >> 12;
		tile=tile&0x7ff;

		drawgfx(bitmap_f,Machine->gfx[1],tile,
			color, 0,0, 16*mx,16*my,
		 	0,TRANSPARENCY_NONE,0);
	}

	scrolly=-scrolly;
	scrollx=-scrollx;
	copyscrollbitmap(bitmap,bitmap_f,1,&scrollx,1,&scrolly,0,TRANSPARENCY_NONE,0);

	/* Sprites */
	for (offs = 0;offs <0x800;offs += 8) {
		int x,y,sprite,sprite2,colour,fx,fy,extra;

	    y=READ_WORD (&buffered_spriteram[offs]);
	    if (!(y&0x8000)) continue;

	    y=y&0x1ff;
	    sprite=READ_WORD (&buffered_spriteram[offs+6]);
	    colour=sprite>>12;
	    sprite=sprite&0xfff;
	    x=READ_WORD (&buffered_spriteram[offs+4])&0x1ff;

		fx=READ_WORD (&buffered_spriteram[offs+2]);
	    if ((fx&0x10)) extra=1; else extra=0;
		fy=fx&0x2;
		fx=fx&0x4;

		if (extra) y=y+16;

	    /* Convert the co-ords..*/
		x=(x+16)%0x200;
		y=(y+16)%0x200;
		x=256 - x;
		y=256 - y;

		/* Y Flip determines order of multi-sprite */
		if (extra && fy) {
			sprite2=sprite;
			sprite++;
		}
		else sprite2=sprite+1;

		drawgfx(bitmap,Machine->gfx[2],
				sprite,
				colour,fx,fy,x,y,
				0,TRANSPARENCY_PEN,0);

    	/* 1 more sprite drawn underneath */
    	if (extra)
    		drawgfx(bitmap,Machine->gfx[2],
				sprite2,
				colour,fx,fy,x,y+16,
				0,TRANSPARENCY_PEN,0);
	}

	/* Draw character tiles */
	for (offs = videoram_size - 2;offs >= 0;offs -= 2) {
		tile=READ_WORD (&videoram[offs]);
		if (!tile) continue;
		color=tile>>14;
		tile=tile&0xfff;
		my = (offs/2) % 32;
		mx = (offs/2) / 32;
		drawgfx(bitmap,Machine->gfx[0],
			tile,color,0,0,8*mx,8*my,
			&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

/******************************************************************************/

WRITE_HANDLER( karnov_foreground_w )
{
	COMBINE_WORD_MEM(&karnov_foreground[offset],data);
	dirty_f[offset] = 1;
}

/******************************************************************************/

void karnov_vh_stop (void)
{
	if (dirty_f) free(dirty_f);
	if (karnov_foreground) free(karnov_foreground);
	if (bitmap_f) bitmap_free(bitmap_f);
}

int karnov_vh_start (void)
{
	/* Allocate bitmaps */
	if ((bitmap_f = bitmap_alloc(512,512)) == 0) {
		karnov_vh_stop ();
		return 1;
	}

	dirty_f=(unsigned char*)malloc(0x1000);
	karnov_foreground=(unsigned char*)malloc(0x1000);
	memset(karnov_foreground,0,0x1000);
	memset(dirty_f,1,0x1000);

	return 0;
}

/******************************************************************************/
