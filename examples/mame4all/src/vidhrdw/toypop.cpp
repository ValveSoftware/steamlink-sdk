/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *toypop_sharedram_2;

unsigned char *bg_image;
static struct osd_bitmap *bgbitmap;
static unsigned char *dirtybackground;
static int flipscreen;

/***************************************************************************

  Convert the color PROMs into a more useable format.

  toypop has three 256x4 palette PROM and two 256x8 color lookup table PROMs
  (one for characters, one for sprites).


***************************************************************************/
void toypop_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;

	for (i = 0;i < 256;i++)
	{
		int bit0,bit1,bit2,bit3;

		// red component
		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		bit3 = (color_prom[i] >> 3) & 0x01;
		palette[3*i] = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		// green component
		bit0 = (color_prom[i+0x100] >> 0) & 0x01;
		bit1 = (color_prom[i+0x100] >> 1) & 0x01;
		bit2 = (color_prom[i+0x100] >> 2) & 0x01;
		bit3 = (color_prom[i+0x100] >> 3) & 0x01;
		palette[3*i + 1] = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		// blue component
		bit0 = (color_prom[i+0x200] >> 0) & 0x01;
		bit1 = (color_prom[i+0x200] >> 1) & 0x01;
		bit2 = (color_prom[i+0x200] >> 2) & 0x01;
		bit3 = (color_prom[i+0x200] >> 3) & 0x01;
		palette[3*i + 2] = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
	}

	// characters
	for (i = 0;i < 256;i++)
		colortable[i] = color_prom[i + 0x300] | 0xf0;

	// sprites
	for (i = 256;i < Machine->drv->color_table_len;i++)
		colortable[i] = color_prom[i + 0x400];	// 0x500-5ff
}

int toypop_vh_start(void)
{
	if ((dirtybuffer = (unsigned char*)malloc(videoram_size)) == 0)
		return 1;
	memset(dirtybuffer, 1, videoram_size);

	if ((dirtybackground = (unsigned char*)malloc(videoram_size)) == 0) {
		free(dirtybuffer);
		return 1;
	}
	memset(dirtybackground, 1, videoram_size);

	if ((tmpbitmap = bitmap_alloc(36*8,28*8)) == 0) {
		free(dirtybuffer);
		free(dirtybackground);
		return 1;
	}
	if ((bgbitmap = bitmap_alloc(36*8,28*8)) == 0) {
		bitmap_free(tmpbitmap);
		free(dirtybuffer);
		free(dirtybackground);
		return 1;
	}

	return 0;
}

void toypop_vh_stop(void)
{
	free(dirtybuffer);
	free(dirtybackground);
	bitmap_free(tmpbitmap);
	bitmap_free(bgbitmap);

	dirtybuffer = 0;
	dirtybackground = 0;
	tmpbitmap = 0;
	bgbitmap = 0;
}

READ_HANDLER( toypop_background_r )
{
	return READ_WORD(&bg_image[offset]);
}

WRITE_HANDLER( toypop_background_w )
{
	int oldword = READ_WORD(&bg_image[offset]);
	int newword = COMBINE_WORD(oldword,data);

	if (oldword != newword) {
		WRITE_WORD(&bg_image[offset],newword);
		dirtybackground[((offset % 288) >> 3) + ((offset / 2304) * 36)] = 1;
	}
}

WRITE_HANDLER( toypop_flipscreen_w )
{
	flipscreen = offset ^ 2;
}

void toypop_draw_sprite(struct osd_bitmap *dest,unsigned int code,unsigned int color,
	int flipx,int flipy,int sx,int sy)
{
	drawgfx(dest,Machine->gfx[1],code,color,flipx,flipy,sx,sy,&Machine->visible_area,
		TRANSPARENCY_PEN,0);
}

void toypop_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	register int offs;
	struct rectangle box;

	/* check if the background image has been modified */
	/* since last time and update it accordingly. */
	// (The background image changes only few times, so this loop is very fast)
	for (offs = 0;offs < 1008;offs++) {
		if (dirtybackground[offs]) {
			int bx, by, palette_index;

			dirtybackground[offs] = 0;
			box.min_x = (offs % 36) << 3;
			box.max_x = box.min_x + 7;
			box.min_y = (offs / 36) << 3;
			box.max_y = box.min_y + 7;
			// copy the background image from memory to bitmap
			palette_index = (toypop_sharedram_2[0x102] ? 0xe0 : 0x60);
			for (by = box.min_y;by <= box.max_y;by++)
				for (bx = box.min_x;bx <= box.max_x;bx++)
					bgbitmap->line[by][bx] = Machine->pens[bg_image[(bx + by * 288) ^ 1] | palette_index];
			// mark the matching character as dirty
			// so the next loop will draw everything right
			switch ((offs >> 1) % 18) {
				case 0:	// the 2 columns at left
					dirtybuffer[(((offs % 36) + 30) << 5) + (offs / 36 + 2)] = 1;
					break;
				case 17:	// the 2 columns at right
					dirtybuffer[(((offs % 36) - 34) << 5) + (offs / 36 + 2)] = 1;
					break;
				default:	// the rest of the screen
					dirtybuffer[(offs % 36) - 2 + ((offs / 36 + 2) << 5)] = 1;
			}
		}
	}

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--) {
		if (dirtybuffer[offs]) {
			int sx,sy;

			dirtybuffer[offs] = 0;
			if (offs >= videoram_size - 64) {
				// Draw the 2 columns at left
				sx = ((offs >> 5) - 30) << 3;
				sy = ((offs & 0x1f) - 2) << 3;
			} else if (offs < 64) {
				// Draw the 2 columns at right
				sx = ((offs >> 5) + 34) << 3;
				sy = ((offs & 0x1f) - 2) << 3;
			} else {
				// draw the rest of the screen
				sx = ((offs & 0x1f) + 2) << 3;
				sy = ((offs >> 5) - 2) << 3;
			}
			// draw the background
			box.min_x = sx;
			box.max_x = sx+7;
			box.min_y = sy;
			box.max_y = sy+7;
			copybitmap(tmpbitmap,bgbitmap,0,0,0,0,&box,TRANSPARENCY_NONE,0);
			// draw the character
			drawgfx(tmpbitmap,Machine->gfx[0],
					videoram[offs],
					colorram[offs],
					0,0,sx,sy,
					0,TRANSPARENCY_PEN,0);
		}
	}

	/* copy the temporary bitmap to the screen */
	copybitmap(bitmap,tmpbitmap,flipscreen,flipscreen,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);

	/* Draw the sprites. */
	for (offs = 0;offs < spriteram_size;offs += 2) {
		/* is it on? */
		if ((spriteram_2[offs]) != 0xe9) {
			int sprite = spriteram[offs];
			int color = spriteram[offs+1];
			int x = 343 - spriteram_2[offs+1] - 0x100 * (spriteram_3[offs+1] & 1);
			int y = spriteram_2[offs] - 9;
			int flipx = spriteram_3[offs] & 1;
			int flipy = spriteram_3[offs] & 2;

			if (flipscreen) {
				flipx = !flipx;
				flipy = !flipy;
			}

			switch (spriteram_3[offs] & 0x0c)
			{
				case 0:		/* normal size */
					toypop_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y);
					break;

				case 4:		/* 2x vertical */
					sprite &= ~1;
					if (!flipy)
					{
						toypop_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y);
						toypop_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x,16+y);
					}
					else
					{
						toypop_draw_sprite(bitmap,sprite,color,flipx,flipy,x,16+y);
						toypop_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x,y);
					}
					break;

				case 8:		/* 2x horizontal */
					sprite &= ~2;
					if (!flipx)
					{
						toypop_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x,y);
						toypop_draw_sprite(bitmap,sprite,color,flipx,flipy,16+x,y);
					}
					else
					{
						toypop_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y);
						toypop_draw_sprite(bitmap,2+sprite,color,flipx,flipy,16+x,y);
					}
					break;

				case 12:		/* 2x both ways */
					sprite &= ~3;
					if (!flipy && !flipx)
					{
						toypop_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x-16,y+16);
						toypop_draw_sprite(bitmap,3+sprite,color,flipx,flipy,x,16+y);
						toypop_draw_sprite(bitmap,sprite,color,flipx,flipy,x-16,y);
						toypop_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x,y);
					}
					else if (flipy && flipx)
					{
						toypop_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x-16,y+16);
						toypop_draw_sprite(bitmap,sprite,color,flipx,flipy,x,16+y);
						toypop_draw_sprite(bitmap,3+sprite,color,flipx,flipy,x-16,y);
						toypop_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x,y);
					}
					else if (flipx)
					{
						toypop_draw_sprite(bitmap,3+sprite,color,flipx,flipy,x-16,y+16);
						toypop_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x,16+y);
						toypop_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x-16,y);
						toypop_draw_sprite(bitmap,sprite,color,flipx,flipy,x,y);
					}
					else /* flipy */
					{
						toypop_draw_sprite(bitmap,sprite,color,flipx,flipy,x-16,y+16);
						toypop_draw_sprite(bitmap,1+sprite,color,flipx,flipy,x,16+y);
						toypop_draw_sprite(bitmap,2+sprite,color,flipx,flipy,x-16,y);
						toypop_draw_sprite(bitmap,3+sprite,color,flipx,flipy,x,y);
					}
					break;
			}
		}
	}
}
