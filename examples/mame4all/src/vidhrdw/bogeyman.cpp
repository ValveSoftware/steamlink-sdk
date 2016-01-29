#include "driver.h"
#include "vidhrdw/generic.h"

static int flipscreen;
unsigned char *bogeyman_videoram;




void bogeyman_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;

	palette += 3*16;	/* first 16 colors are RAM */

	for (i = 0;i < 256;i++)
	{
		int bit0,bit1,bit2;

		/* red component */
		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[0] >> 1) & 0x01;
		bit2 = (color_prom[0] >> 2) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* green component */
		bit0 = (color_prom[0] >> 3) & 0x01;
		bit1 = (color_prom[256] >> 0) & 0x01;
		bit2 = (color_prom[256] >> 1) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (color_prom[256] >> 2) & 0x01;
		bit2 = (color_prom[256] >> 3) & 0x01;
		*(palette++) = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		color_prom++;
	}
}

int bogeyman_vh_start(void)
{
	dirtybuffer = (unsigned char*)malloc(videoram_size);
	memset(dirtybuffer,1,videoram_size);
	tmpbitmap = bitmap_alloc(256,256);

	return 0;
}

void bogeyman_vh_stop(void)
{
	free(dirtybuffer);
	bitmap_free(tmpbitmap);
}

/******************************************************************************/

WRITE_HANDLER( bogeyman_paletteram_w )
{
	/* RGB output is inverted */
	paletteram_BBGGGRRR_w(offset,~data);
}

WRITE_HANDLER( bogeyman_videoram_w )
{
	bogeyman_videoram[offset]=data;
	dirtybuffer[offset]=1;
}

void bogeyman_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int mx,my,offs,color,tile,bank,sx,sy,flipx,flipy,multi;

	if (palette_recalc())
		memset(dirtybuffer,1,videoram_size);

	flipscreen=0; /* For now */

	for (offs = 0; offs<0x100; offs++)
	{
		if (dirtybuffer[offs] | dirtybuffer[offs+0x100])
		{
			dirtybuffer[offs] = dirtybuffer[offs+0x100] = 0;

			sx=offs%16;
			sy=offs/16;

			bank=((bogeyman_videoram[offs+0x100] & 0x01) << 8) | bogeyman_videoram[offs];
			bank=(bank/0x80)+3;
			color=bogeyman_videoram[offs+0x100]>>1;

			drawgfx(tmpbitmap,Machine->gfx[bank],
					bogeyman_videoram[offs]&0x7f,
					color&7,
					flipscreen,flipscreen,
					16*sx,16*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}

	copyscrollbitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* Sprites */
	for (offs = 0;offs < spriteram_size;offs += 4)
	{
		if (spriteram[offs] & 0x01)
		{
			sx = 240 - spriteram[offs+3];
			sy = (240 - spriteram[offs+2]) & 0xff;
			flipx = spriteram[offs] & 0x04;
			flipy = 0;//spriteram[offs] & 0x02;
			multi=spriteram[offs] & 0x10;
			if (multi) sy-=16;

			if (1/*flipscreen*/)
			{
				sx = 240 - sx;
				//sy = 240 - sy;
				flipx = !flipx;
				//flipy = !flipy;
			}

			drawgfx(bitmap,Machine->gfx[2],
					spriteram[offs+1] + ((spriteram[offs] & 0x40) << 2),	// Modified by T.Nogi 1999/10/25
					(spriteram[offs] & 0x08) >> 3,	// Modified by T.Nogi 1999/10/26
					flipx,flipy,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
			if (multi)
				drawgfx(bitmap,Machine->gfx[2],
					spriteram[offs+1]+ 1 + ((spriteram[offs] & 0x40) << 2),	// Modified by T.Nogi 1999/10/25
					(spriteram[offs] & 0x08) >> 3,	// Modified by T.Nogi 1999/10/26
					flipx,flipy,
					sx,sy+16,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}

	/* Draw character tiles */
	for (offs = 0;offs < 0x400; offs ++)
	{
		mx=offs%32;
		my=offs/32;
		if (flipscreen) {mx=31-mx; my=31-my;}
		tile=videoram[offs] | ((videoram[offs+0x400]&3)<<8);
		bank=tile/0x200;
		if (!tile) continue;
#if 0
		color=0;//(videoram[offs+0x400]&2)>>1;	// Modified by T.Nogi 1999/10/26
#else
		color = 0;
		if (!bank)
		{
			if ((tile >= 0x002) && (tile < 0x036)) color = 1;
			if ((tile >= 0x10b) && (tile < 0x15b)) color = 1;
			if ((tile >= 0x178) && (tile < 0x1c7)) color = 1;
			if ((tile >= 0x1e0) && (tile < 0x1f3)) color = 1;
		} else {
			if ((tile >= (0x010+0x200)) && (tile < (0x17f+0x200))) color = 1;
		}
#endif
		drawgfx(bitmap,Machine->gfx[bank],
				tile&0x1ff,
				color,
				flipscreen,flipscreen,
				8*mx,8*my,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}
