/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"
#include "ctype.h"



extern unsigned char *tnzs_objram;
extern unsigned char *tnzs_vdcram;
extern unsigned char *tnzs_scrollram;


static struct osd_bitmap *tnzs_column[16];
static int tnzs_dirty_map[32][16];
static int tnzs_screenflip, old_tnzs_screenflip;

/***************************************************************************

  The New Zealand Story doesn't have a color PROM. It uses 1024 bytes of RAM
  to dynamically create the palette. Each couple of bytes defines one
  color (15 bits per pixel; the top bit of the second byte is unused).
  Since the graphics use 4 bitplanes, hence 16 colors, this makes for 32
  different color codes.

***************************************************************************/


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Arkanoid has a two 512x8 palette PROMs. The two bytes joined together
  form 512 xRRRRRGGGGGBBBBB color values.

***************************************************************************/
void arkanoi2_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i,col;

	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		col = (color_prom[i]<<8)+color_prom[i+512];
		*(palette++) =  (col & 0x7c00)>>7;	/* Red */
		*(palette++) =  (col & 0x03e0)>>2;	/* Green */
		*(palette++) =  (col & 0x001f)<<3;	/* Blue */
	}
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int tnzs_vh_start(void)
{
	int column,x,y;
	for (column=0;column<16;column++)
	{
		if ((tnzs_column[column] = bitmap_alloc(32,256)) == 0)
		{
			/* Free all the columns */
			for (column--;column;column--)
				bitmap_free(tnzs_column[column]);
			return 1;
		}
	}

	for (x=0;x<32;x++)
	{
		for (y=0;y<16;y++)
		{
			tnzs_dirty_map[x][y] = -1;
		}
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void tnzs_vh_stop(void)
{
	int column;

	/* Free all the columns */
	for (column=0;column<16;column++)
		bitmap_free(tnzs_column[column]);
}



/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/

void tnzs_vh_draw_background(struct osd_bitmap *bitmap,unsigned char *m)
{
	int i,x,y,column,tot;
	int scrollx, scrolly;
	unsigned int upperbits;

	/* The screen is split into 16 columns.
	   So first, update the tiles. */
	for (i=0,column=0;column<16;column++)
	{
		for (y=0;y<16;y++)
		{
			for (x=0;x<2;x++,i++)
			{
				int tile;

				/* Construct unique identifier for this tile/color */
				tile = (m[i + 0x1200] << 16) | (m[i + 0x1000] << 8) | m[i];

				if (tnzs_dirty_map[column*2+x][y] != tile)
				{
					int code,color,flipx,flipy,sx,sy;


					tnzs_dirty_map[column*2+x][y] = tile;

					code = m[i] + ((m[i + 0x1000] & 0x1f) << 8);
					color = (m[i + 0x1200] & 0xf8) >> 3; /* colours at d600-d7ff */
					sx = x*16;
					sy = y*16;
					flipx = m[i + 0x1000] & 0x80;
					flipy = m[i + 0x1000] & 0x40;
					if (tnzs_screenflip)
					{
						sy = 240 - sy;
						flipx = !flipx;
						flipy = !flipy;
					}

					drawgfx(tnzs_column[column],Machine->gfx[0],
							code,
							color,
							flipx,flipy,
							sx,sy,
							0,TRANSPARENCY_NONE,0);
				}
			}
		}
	}

	/* If the byte at f301 has bit 0 clear, then don't draw the
	   background tiles -WRONG- */

	/* The byte at f200 is the y-scroll value for the first column.
	   The byte at f204 is the LSB of x-scroll value for the first column.

	   The other columns follow at 16-byte intervals.

	   The 9th bit of each x-scroll value is combined into 2 bytes
	   at f302-f303 */

	/* f301 seems to control how many columns are drawn but it's not clear how. */
	/* Arkanoid 2 also uses f381, which TNZS always leaves at 00. */
	/* Maybe it's a background / foreground thing? In Arkanoid 2, f381 contains */
	/* the value we expect for the background stars (2E vs. 2A), while f301 the */
	/* one we expect at the beginning of a level (2C vs. 2A). */
	x = tnzs_scrollram[0x101] & 0xf;
	if (x == 1) x = 16;
	y = tnzs_scrollram[0x181] & 0xf;
	if (y == 1) y = 16;
	/* let's just pick the larger value... */
	tot = x;
	if (y > tot) tot = y;

	upperbits = tnzs_scrollram[0x102] + tnzs_scrollram[0x103] * 256;
	/* again, it's not clear why there are two areas, but Arkanoid 2 uses these */
	/* for the end of game animation */
	upperbits |= tnzs_scrollram[0x182] + tnzs_scrollram[0x183] * 256;

	for (column = 0;column < tot;column++)
	{
		scrollx = tnzs_scrollram[column*16+4] - ((upperbits & 0x01) * 256);
		if (tnzs_screenflip)
			scrolly = tnzs_scrollram[column*16] + 1 - 256;
		else
			scrolly = -tnzs_scrollram[column*16] + 1;

		copybitmap(bitmap,tnzs_column[column^8],0,0,scrollx,scrolly,
				   &Machine->visible_area,TRANSPARENCY_COLOR,0);
		copybitmap(bitmap,tnzs_column[column^8],0,0,scrollx,scrolly+(16*16),
				   &Machine->visible_area,TRANSPARENCY_COLOR,0);

		upperbits >>= 1;
	}
}

void tnzs_vh_draw_foreground(struct osd_bitmap *bitmap,
							 unsigned char *char_pointer,
							 unsigned char *x_pointer,
							 unsigned char *y_pointer,
							 unsigned char *ctrl_pointer,
							 unsigned char *color_pointer)
{
	int i;


	/* Draw all 512 sprites */
	for (i=0x1ff;i >= 0;i--)
	{
		int code,color,sx,sy,flipx,flipy;

		code = char_pointer[i] + ((ctrl_pointer[i] & 0x1f) << 8);
		color = (color_pointer[i] & 0xf8) >> 3;
		sx = x_pointer[i] - ((color_pointer[i] & 1) << 8);
		sy = 240 - y_pointer[i];
		flipx = ctrl_pointer[i] & 0x80;
		flipy = ctrl_pointer[i] & 0x40;
		if (tnzs_screenflip)
		{
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
			/* hack to hide Chuka Taisens grey line, top left corner */
			if ((sy == 0) && (code == 0)) sy += 240;
		}

		drawgfx(bitmap,Machine->gfx[0],
				code,
				color,
				flipx,flipy,
				sx,sy+2,
				&Machine->visible_area,TRANSPARENCY_PEN,0);
	}
}

void arkanoi2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int x,y;

	/* If the byte at f300 has bit 6 set, flip the screen
	   (I'm not 100% sure about this) */
	tnzs_screenflip = (tnzs_scrollram[0x100] & 0x40) >> 6;
	if (old_tnzs_screenflip != tnzs_screenflip)
	{
		for (x=0;x<32;x++)
		{
			for (y=0;y<16;y++)
			{
				tnzs_dirty_map[x][y] = -1;
			}
		}
	}
	old_tnzs_screenflip = tnzs_screenflip;


	/* Blank the background */
	fillbitmap(bitmap, Machine->pens[0], &Machine->visible_area);

	/* Redraw the background tiles (c400-c5ff) */
	tnzs_vh_draw_background(bitmap, tnzs_objram + 0x400);

	/* Draw the sprites on top */
	tnzs_vh_draw_foreground(bitmap,
							tnzs_objram + 0x0000, /*  chars : c000 */
							tnzs_objram + 0x0200, /*	  x : c200 */
							tnzs_vdcram + 0x0000, /*	  y : f000 */
							tnzs_objram + 0x1000, /*   ctrl : d000 */
							tnzs_objram + 0x1200); /* color : d200 */
}

void tnzs_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int color,code,i,offs,x,y;
	int colmask[32];

	/* Remap dynamic palette */
	palette_init_used_colors();

	for (color = 0;color < 32;color++) colmask[color] = 0;

	/* See what colours the tiles need */
	for (offs=32*16 - 1;offs >= 0;offs--)
	{
		code = tnzs_objram[offs + 0x400]
			 + 0x100 * (tnzs_objram[offs + 0x1400] & 0x1f);
		color = tnzs_objram[offs + 0x1600] >> 3;

		colmask[color] |= Machine->gfx[0]->pen_usage[code];
	}

	/* See what colours the sprites need */
	for (offs=0x1ff;offs >= 0;offs--)
	{
		code = tnzs_objram[offs]
			 + 0x100 * (tnzs_objram[offs + 0x1000] & 0x1f);
		color = tnzs_objram[offs + 0x1200] >> 3;

		colmask[color] |= Machine->gfx[0]->pen_usage[code];
	}

	/* Construct colour usage table */
	for (color=0;color<32;color++)
	{
		if (colmask[color] & (1 << 0))
			palette_used_colors[16 * color] = PALETTE_COLOR_TRANSPARENT;
		for (i=1;i<16;i++)
		{
			if (colmask[color] & (1 << i))
				palette_used_colors[16 * color + i] = PALETTE_COLOR_USED;
		}
	}

	if (palette_recalc())
	{
		for (x=0;x<32;x++)
		{
			for (y=0;y<16;y++)
			{
				tnzs_dirty_map[x][y] = -1;
			}
		}
	}


	arkanoi2_vh_screenrefresh(bitmap,full_refresh);
}
