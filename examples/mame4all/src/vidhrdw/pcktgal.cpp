#include "driver.h"
#include "vidhrdw/generic.h"

static int flipscreen;

WRITE_HANDLER( pcktgal_flipscreen_w )
{
	static int last_flip;
	flipscreen = (data&0x80) ? 1 : 0;
	if (last_flip!=flipscreen)
		memset(dirtybuffer,1,0x800);
	last_flip=flipscreen;
}

void pcktgal_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;

	/* Draw character tiles */
	for (offs = videoram_size - 2;offs >= 0;offs -= 2)
	{
		if (dirtybuffer[offs] || dirtybuffer[offs+1])
		{
			int sx,sy,fx=0,fy=0;

			dirtybuffer[offs] = dirtybuffer[offs+1] = 0;

			sx = (offs/2) % 32;
			sy = (offs/2) / 32;
			if (flipscreen) {
				sx=31-sx;
				sy=31-sy;
				fx=1;
				fy=1;
			}

	        drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs+1] + ((videoram[offs] & 0x0f) << 8),
					videoram[offs] >> 4,
					fx,fy,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
		}
	}

	/* copy the character mapped graphics */
	copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* Sprites */
	for (offs = 0;offs < spriteram_size;offs += 4)
	{
		if (spriteram[offs] != 0xf8)
		{
			int sx,sy,flipx,flipy;


			sx = 240 - spriteram[offs+2];
			sy = 240 - spriteram[offs];

			flipx = spriteram[offs+1] & 0x04;
			flipy = spriteram[offs+1] & 0x02;
			if (flipscreen) {
				sx=240-sx;
				sy=240-sy;
				if (flipx) flipx=0; else flipx=1;
				if (flipy) flipy=0; else flipy=1;
			}

			drawgfx(bitmap,Machine->gfx[1],
					spriteram[offs+3] + ((spriteram[offs+1] & 1) << 8),
					(spriteram[offs+1] & 0x70) >> 4,
					flipx,flipy,
					sx,sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		}
	}
}
