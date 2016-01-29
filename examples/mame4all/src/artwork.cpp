/*********************************************************************

  artwork.c

  Generic backdrop/overlay functions.

  Created by Mike Balfour - 10/01/1998

  Added some overlay and backdrop functions
  for vector games. Mathis Rosenhauer - 10/09/1998

  MAB - 09 MAR 1999 - made some changes to artwork_create
  MLR - 29 MAR 1999 - added disks to artwork_create
  MLR - 24 MAR 2000 - support for true color artwork

*********************************************************************/

#include "driver.h"
#include "png.h"
#include "artwork.h"


/* Local variables */
static UINT8 isblack[256];

/* the backdrop instance */
struct artwork *artwork_backdrop = NULL;

/* the overlay instance */
struct artwork *artwork_overlay = NULL;
struct osd_bitmap *overlay_real_scrbitmap;

/*
 * finds closest color and returns the index (for 256 color)
 */

static UINT8 find_pen(UINT8 r,UINT8 g,UINT8 b)
{
	int i,bi,ii;
	long x,y,z,bc;
	ii = 32;
	bi = 256;
	bc = 0x01000000;

	do
	{
		for( i=0; i<256; i++ )
		{
			UINT8 r1,g1,b1;

			osd_get_pen(Machine->pens[i],&r1,&g1,&b1);
			if((x=(long)(abs(r1-r)+1)) > ii) continue;
			if((y=(long)(abs(g1-g)+1)) > ii) continue;
			if((z=(long)(abs(b1-b)+1)) > ii) continue;
			x = x*y*z;
			if (x < bc)
			{
				bc = x;
				bi = i;
			}
		}
		ii<<=1;
	} while (bi==256);

	return(bi);
}

void backdrop_refresh_tables (struct artwork *a)
{
	int i,j, k, tab_colors;
	UINT8 rgb1[3], rgb2[3], c[3];
	UINT16 *pens = Machine->pens;

	/* Calculate brightness of all colors */
	for (i = 0; i < Machine->drv->total_colors; i++)
	{
		osd_get_pen (pens[i], &rgb1[0], &rgb1[1], &rgb1[2]);
		a->brightness[pens[i]]=(222*rgb1[0]+707*rgb1[1]+71*rgb1[2])/1000;
	}

	if (Machine->scrbitmap->depth == 8)
	{
		tab_colors = MIN(256, Machine->drv->total_colors);

		/* Calculate mixed colors */

		for( i=0; i < tab_colors ;i++ )                /* color1 */
		{
			osd_get_pen(pens[i],&rgb1[0],&rgb1[1],&rgb1[2]);
			for( j=0; j < tab_colors ;j++ )               /* color2 */
			{
				osd_get_pen(pens[j],&rgb2[0],&rgb2[1],&rgb2[2]);

				for (k=0; k<3; k++)
					c[k] = MIN(255, rgb1[k]/4 + rgb2[k]);

				a->pTable[i * tab_colors + j] = find_pen(c[0],c[1],c[2]);
			}
		}
	}
}

/*********************************************************************
  backdrop_refresh

  This remaps the "original" palette indexes to the abstract OS indexes
  used by MAME.  This needs to be called every time palette_recalc
  returns a non-zero value, since the mappings will have changed.
 *********************************************************************/

void backdrop_refresh(struct artwork *a)
{
	int i, j, height,width, offset;
	struct osd_bitmap *back, *orig;

	offset = a->start_pen;
	back = a->artwork;
	orig = a->orig_artwork;
	height = a->artwork->height;
	width = a->artwork->width;

	if (back->depth == 8)
	{
		for ( j = 0; j < height; j++)
			for (i = 0; i < width; i++)
				back->line[j][i] = Machine->pens[orig->line[j][i] + offset];
	}
	else
	{
		for ( j = 0; j < height; j++)
			for (i = 0; i < width; i++)
				((UINT16 *)back->line[j])[i] = Machine->pens[((UINT16 *)orig->line[j])[i] + offset];
	}
}

/*********************************************************************
  backdrop_set_palette

  This sets the palette colors used by the backdrop to the new colors
  passed in as palette.  The values should be stored as one byte of red,
  one byte of blue, one byte of green.  This could hopefully be used
  for special effects, like lightening and darkening the backdrop.
 *********************************************************************/
void backdrop_set_palette(struct artwork *a, UINT8 *palette)
{
	int i;

	/* Load colors into the palette */
	if ((Machine->drv->video_attributes & VIDEO_MODIFIES_PALETTE))
	{
		for (i = 0; i < a->num_pens_used; i++)
			palette_change_color(i + a->start_pen, palette[i*3], palette[i*3+1], palette[i*3+2]);

		palette_recalc();
		backdrop_refresh(a);
	}
}

/*********************************************************************
  artwork_free

  Don't forget to clean up when you're done with the backdrop!!!
 *********************************************************************/

void artwork_free(struct artwork **a)
{
	if (*a)
	{
		if ((*a)->artwork)
			bitmap_free((*a)->artwork);
		if ((*a)->artwork1)
			bitmap_free((*a)->artwork1);
		if ((*a)->alpha)
			bitmap_free((*a)->alpha);
		if ((*a)->orig_artwork)
			bitmap_free((*a)->orig_artwork);
		if ((*a)->vector_bitmap)
			bitmap_free((*a)->vector_bitmap);
		if ((*a)->orig_palette)
			free((*a)->orig_palette);
		if ((*a)->transparency)
			free((*a)->transparency);
		if ((*a)->brightness)
			free((*a)->brightness);
		if ((*a)->rgb)
			free((*a)->rgb);
		if ((*a)->pTable)
			free((*a)->pTable);
		free(*a);

		*a = NULL;
	}
}

void overlay_free(void)
{
	if (artwork_overlay)
	{
		bitmap_free(Machine->scrbitmap);
		Machine->scrbitmap = overlay_real_scrbitmap;

		artwork_free(&artwork_overlay);
	}
}

void backdrop_free(void)
{
	if (artwork_backdrop)
		artwork_free(&artwork_backdrop);
}

/*********************************************************************
  backdrop_black_recalc

  If you use any of the experimental backdrop draw* blitters below,
  call this once per frame.  It will catch palette changes and mark
  every black as transparent.  If it returns a non-zero value, redraw
  the whole background.
 *********************************************************************/

int backdrop_black_recalc(void)
{
	UINT8 r,g,b;
	int i;
	int redraw = 0;

	/* Determine what colors can be overwritten */
	for (i=0; i<256; i++)
	{
		osd_get_pen(i,&r,&g,&b);

		if ((r==0) && (g==0) && (b==0))
		{
			if (isblack[i] != 1)
				redraw = 1;
			isblack[i] = 1;
		}
		else
		{
			if (isblack[i] != 0)
				redraw = 1;
			isblack[i] = 0;
		}
	}
	return redraw;
}

/*********************************************************************
  draw_backdrop

  This is an experimental backdrop blitter.  How to use:
  1)  Draw the dirty background video game graphic with no transparency.
  2)  Call draw_backdrop with a clipping rectangle containing the location
      of the dirty_graphic.

  draw_backdrop will fill in everything that's colored black with the
  backdrop.
 *********************************************************************/

void draw_backdrop(struct osd_bitmap *dest,const struct osd_bitmap *src,int sx,int sy,
					const struct rectangle *clip)
{
	int ox,oy,ex,ey,y,start,dy;
	/*  int col;
		int *sd4;
		int trans4,col4;*/
	struct rectangle myclip;

	if (!src) return;
	if (!dest) return;

    /* Rotate and swap as necessary... */
	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;

		temp = sx;
		sx = sy;
		sy = temp;

		if (clip)
		{
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = clip->min_y;
			myclip.min_y = temp;
			temp = clip->max_x;
			myclip.max_x = clip->max_y;
			myclip.max_y = temp;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_X)
	{
		sx = dest->width - src->width - sx;
		if (clip)
		{
			int temp;


			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = dest->width-1 - clip->max_x;
			myclip.max_x = dest->width-1 - temp;
			myclip.min_y = clip->min_y;
			myclip.max_y = clip->max_y;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_Y)
	{
		sy = dest->height - src->height - sy;
		if (clip)
		{
			int temp;


			myclip.min_x = clip->min_x;
			myclip.max_x = clip->max_x;
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_y;
			myclip.min_y = dest->height-1 - clip->max_y;
			myclip.max_y = dest->height-1 - temp;
			clip = &myclip;
		}
	}


	/* check bounds */
	ox = sx;
	oy = sy;
	ex = sx + src->width-1;
	if (sx < 0) sx = 0;
	if (clip && sx < clip->min_x) sx = clip->min_x;
	if (ex >= dest->width) ex = dest->width-1;
	if (clip && ex > clip->max_x) ex = clip->max_x;
	if (sx > ex) return;
	ey = sy + src->height-1;
	if (sy < 0) sy = 0;
	if (clip && sy < clip->min_y) sy = clip->min_y;
	if (ey >= dest->height) ey = dest->height-1;
	if (clip && ey > clip->max_y) ey = clip->max_y;
	if (sy > ey) return;

    /* VERY IMPORTANT to mark this rectangle as dirty! :) - MAB */
	osd_mark_dirty (sx,sy,ex,ey,0);

	start = sy-oy;
	dy = 1;

	if (dest->depth == 8)
	{
		for (y = sy;y <= ey;y++)
		{
			const UINT8 *sd;
			UINT8 *bm,*bme;

			bm = dest->line[y];
			bme = bm + ex;
			sd = src->line[start] + (sx-ox);
			for( bm = bm+sx ; bm <= bme ; bm++ )
			{
				if (isblack[*bm])
					*bm = *sd;
				sd++;
			}
			start+=dy;
		}
	}
	else
	{
		for (y = sy;y <= ey;y++)
		{
			const UINT16 *sd;
			UINT16 *bm,*bme;

			bm = (UINT16 *)dest->line[y];
			bme = bm + ex;
			sd = ((UINT16 *)src->line[start]) + (sx-ox);
			for( bm = bm+sx ; bm <= bme ; bm++ )
			{
				if (isblack[*bm])
					*bm = *sd;
				sd++;
			}
			start+=dy;
		}
	}
}


/*********************************************************************
  drawgfx_backdrop

  This is an experimental backdrop blitter.  How to use:

  Every time you want to draw a background tile, instead of calling
  drawgfx, call this and pass in the backdrop bitmap.  Wherever the
  tile is black, the backdrop will be drawn.
 *********************************************************************/
void drawgfx_backdrop(struct osd_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,const struct osd_bitmap *back)
{
	int ox,oy,ex,ey,y,start,dy;
	const UINT8 *sd;
	/*int col;
	  int *sd4;
	  int trans4,col4;*/
	struct rectangle myclip;

	if (!gfx) return;

	code %= gfx->total_elements;
	color %= gfx->total_colors;

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;

		temp = sx;
		sx = sy;
		sy = temp;

		temp = flipx;
		flipx = flipy;
		flipy = temp;

		if (clip)
		{
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = clip->min_y;
			myclip.min_y = temp;
			temp = clip->max_x;
			myclip.max_x = clip->max_y;
			myclip.max_y = temp;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_X)
	{
		sx = dest->width - gfx->width - sx;
		if (clip)
		{
			int temp;


			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = dest->width-1 - clip->max_x;
			myclip.max_x = dest->width-1 - temp;
			myclip.min_y = clip->min_y;
			myclip.max_y = clip->max_y;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_Y)
	{
		sy = dest->height - gfx->height - sy;
		if (clip)
		{
			int temp;


			myclip.min_x = clip->min_x;
			myclip.max_x = clip->max_x;
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_y;
			myclip.min_y = dest->height-1 - clip->max_y;
			myclip.max_y = dest->height-1 - temp;
			clip = &myclip;
		}
	}


	/* check bounds */
	ox = sx;
	oy = sy;
	ex = sx + gfx->width-1;
	if (sx < 0) sx = 0;
	if (clip && sx < clip->min_x) sx = clip->min_x;
	if (ex >= dest->width) ex = dest->width-1;
	if (clip && ex > clip->max_x) ex = clip->max_x;
	if (sx > ex) return;
	ey = sy + gfx->height-1;
	if (sy < 0) sy = 0;
	if (clip && sy < clip->min_y) sy = clip->min_y;
	if (ey >= dest->height) ey = dest->height-1;
	if (clip && ey > clip->max_y) ey = clip->max_y;
	if (sy > ey) return;

	osd_mark_dirty (sx,sy,ex,ey,0);	/* ASG 971011 */

	/* start = code * gfx->height; */
	if (flipy)	/* Y flop */
	{
		start = code * gfx->height + gfx->height-1 - (sy-oy);
		dy = -1;
	}
	else		/* normal */
	{
		start = code * gfx->height + (sy-oy);
		dy = 1;
	}


	if (dest->depth == 8)
	{
		UINT8 *bm,*bme;
		const UINT8 *sb;

		if (gfx->colortable)	/* remap colors */
		{
			const UINT16 *paldata;	/* ASG 980209 */

			paldata = &gfx->colortable[gfx->color_granularity * color];

			if (flipx)	/* X flip */
			{
				for (y = sy;y <= ey;y++)
				{
					bm  = dest->line[y];
					bme = bm + ex;
					sd = gfx->gfxdata + start * gfx->line_modulo + gfx->width-1 - (sx-ox);
    	            sb = back->line[y] + sx;
					for( bm += sx ; bm <= bme ; bm++ )
					{
						if (isblack[paldata[*sd]])
    	                    *bm = *sb;
    	                else
							*bm = paldata[*sd];
						sd--;
    	                sb++;
					}
					start+=dy;
				}
			}
			else		/* normal */
			{
				for (y = sy;y <= ey;y++)
				{
					bm  = dest->line[y];
					bme = bm + ex;
					sd = gfx->gfxdata + start * gfx->line_modulo + (sx-ox);
    	            sb = back->line[y] + sx;
					for( bm += sx ; bm <= bme ; bm++ )
					{
						if (isblack[paldata[*sd]])
    	                    *bm = *sb;
    	                else
							*bm = paldata[*sd];
						sd++;
    	                sb++;
					}
					start+=dy;
				}
			}
		}
		else
		{
			if (flipx)	/* X flip */
			{
				for (y = sy;y <= ey;y++)
				{
					bm = dest->line[y];
					bme = bm + ex;
					sd = gfx->gfxdata + start * gfx->line_modulo + gfx->width-1 - (sx-ox);
    	            sb = back->line[y] + sx;
					for( bm = bm+sx ; bm <= bme ; bm++ )
					{
						if (isblack[*sd])
    	                    *bm = *sb;
    	                else
							*bm = *sd;
						sd--;
    	                sb++;
					}
					start+=dy;
				}
			}
			else		/* normal */
			{
				for (y = sy;y <= ey;y++)
				{
					bm = dest->line[y];
					bme = bm + ex;
					sd = gfx->gfxdata + start * gfx->line_modulo + (sx-ox);
    	            sb = back->line[y] + sx;
					for( bm = bm+sx ; bm <= bme ; bm++ )
					{
						if (isblack[*sb])
    	                    *bm = *sb;
    	                else
							*bm = *sd;
						sd++;
    	                sb++;
					}
					start+=dy;
				}
			}
		}
	}
	else
	{
		UINT16 *bm,*bme;
		const UINT16 *sb;

		if (gfx->colortable)	/* remap colors */
		{
			const UINT16 *paldata;	/* ASG 980209 */

			paldata = &gfx->colortable[gfx->color_granularity * color];

			if (flipx)	/* X flip */
			{
				for (y = sy;y <= ey;y++)
				{
					bm  = (UINT16 *)dest->line[y];
					bme = bm + ex;
					sd = gfx->gfxdata + start * gfx->line_modulo + gfx->width-1 - (sx-ox);
    	            sb = ((UINT16 *)back->line[y]) + sx;
					for( bm += sx ; bm <= bme ; bm++ )
					{
						if (isblack[paldata[*sd]])
    	                    *bm = *sb;
    	                else
							*bm = paldata[*sd];
						sd--;
    	                sb++;
					}
					start+=dy;
				}
			}
			else		/* normal */
			{
				for (y = sy;y <= ey;y++)
				{
					bm  = (UINT16 *)dest->line[y];
					bme = bm + ex;
					sd = gfx->gfxdata + start * gfx->line_modulo + (sx-ox);
    	            sb = ((UINT16 *)back->line[y]) + sx;
					for( bm += sx ; bm <= bme ; bm++ )
					{
						if (isblack[paldata[*sd]])
    	                    *bm = *sb;
    	                else
							*bm = paldata[*sd];
						sd++;
    	                sb++;
					}
					start+=dy;
				}
			}
		}
		else
		{
			if (flipx)	/* X flip */
			{
				for (y = sy;y <= ey;y++)
				{
					bm = (UINT16 *)dest->line[y];
					bme = bm + ex;
					sd = gfx->gfxdata + start * gfx->line_modulo + gfx->width-1 - (sx-ox);
    	            sb = ((UINT16 *)back->line[y]) + sx;
					for( bm = bm+sx ; bm <= bme ; bm++ )
					{
						if (isblack[*sd])
    	                    *bm = *sb;
    	                else
							*bm = *sd;
						sd--;
    	                sb++;
					}
					start+=dy;
				}
			}
			else		/* normal */
			{
				for (y = sy;y <= ey;y++)
				{
					bm = (UINT16 *)dest->line[y];
					bme = bm + ex;
					sd = gfx->gfxdata + start * gfx->line_modulo + (sx-ox);
    	            sb = ((UINT16 *)back->line[y]) + sx;
					for( bm = bm+sx ; bm <= bme ; bm++ )
					{
						if (isblack[*sb])
    	                    *bm = *sb;
    	                else
							*bm = *sd;
						sd++;
    	                sb++;
					}
					start+=dy;
				}
			}
		}
	}
}


/*********************************************************************
  overlay_draw

  This is an experimental backdrop blitter.  How to use:
  1)  Refresh all of your tmpbitmap.
  2)  Call overlay_draw with the bitmap, tmpbitmap and the overlay.

  Not so tough, is it? :)

  Note: we don't have to worry about marking dirty rectangles here,
  because we should only have color changes in redrawn sections, which
  should already be marked as dirty by the original blitter.

  Supports different levels of intensity on the screen and different
  levels of transparancy of the overlay (only in 16 bpp modes).
 *********************************************************************/

void overlay_draw(struct osd_bitmap *dest, struct osd_bitmap *source)
{
	int i,j;
	int height,width;

	height = artwork_overlay->artwork->height;
	width = artwork_overlay->artwork->width;

	if (dest->depth == 8)
	{
		if (Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
		{
			UINT8 *dst, *ovr, *src;
			UINT8 *bright = artwork_overlay->brightness;
			UINT8 *tab = artwork_overlay->pTable;
			int bp;

			copybitmap(dest, artwork_overlay->artwork ,0,0,0,0,NULL,TRANSPARENCY_NONE,0);
			for ( j = 0; j < height; j++)
			{
				dst = dest->line[j];
				src = source->line[j];
				ovr = artwork_overlay->orig_artwork->line[j];
				for (i = 0; i < width; i++)
				{
					bp = bright[*src++];
					if (bp > 0)
						dst[i] = Machine->pens[tab[(ovr[i] << 8) + bp]];
				}
			}
		}
		else
		{
			UINT8 *dst, *ovr, *src;
			int black = Machine->pens[0];

			for ( j = 0; j < height; j++)
			{
				dst = dest->line[j];
				src = source->line[j];
				ovr = artwork_overlay->artwork->line[j];
				for (i = width; i > 0; i--)
				{
					if (*src!=black)
						*dst = *ovr;
					else
						*dst = black;
					dst++;
					src++;
					ovr++;
				}
			}
		}
	}
	else
	{
		if (artwork_overlay->start_pen == 2)
		{
			/* fast version */
			UINT16 *dst, *bg, *fg, *src;
			int black = Machine->pens[0];

			height = artwork_overlay->artwork->height;
			width = artwork_overlay->artwork->width;

			for ( j = 0; j < height; j++)
			{
				dst = (UINT16 *)dest->line[j];
				src = (UINT16 *)source->line[j];
				bg = (UINT16 *)artwork_overlay->artwork->line[j];
				fg = (UINT16 *)artwork_overlay->artwork1->line[j];
				for (i = width; i > 0; i--)
				{
					if (*src!=black)
						*dst = *fg;
					else
						*dst = *bg;
					dst++;
					src++;
					fg++;
					bg++;
				}
			}
		}
		else
		{
			/* slow version */
			UINT16 *src, *dst;
			UINT64 *rgb = artwork_overlay->rgb;
			UINT8 *bright = artwork_overlay->brightness;
			unsigned short *pens = &Machine->pens[artwork_overlay->start_pen];

			copybitmap(dest, artwork_overlay->artwork ,0,0,0,0,NULL,TRANSPARENCY_NONE,0);

			for ( j = 0; j < height; j++)
			{
				dst = (UINT16 *)dest->line[j];
				src = (UINT16 *)source->line[j];
				for (i = width; i > 0; i--)
				{
					int bp = bright[*src++];
					if (bp > 0)
					{
						if (*rgb & 0x00ffffff)
						{
							int v = *rgb >> 32;
							int vn =(*rgb >> 24) & 0xff;
							UINT8 r = *rgb >> 16;
							UINT8 g = *rgb >> 8;
							UINT8 b = *rgb;

							vn += ((255 - vn) * bp) / 255;
							r = (r * vn) / v;
							g = (g * vn) / v;
							b = (b * vn) / v;
							*dst = pens[(((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3))];
						}
						else
						{
							int vn =(*rgb >> 24) & 0xff;

							vn += ((255 - vn) * bp) / 255;
							*dst = pens[(((vn & 0xf8) << 7) | ((vn & 0xf8) << 2) | (vn >> 3))];
						}
					}
					dst++;
					rgb++;
				}
			}
		}
	}
}


/*********************************************************************
  RGBtoHSV and HSVtoRGB

  This is overkill for now but maybe they come in handy later
  (Stolen from Foley's book)
 *********************************************************************/
static void RGBtoHSV( float r, float g, float b, float *h, float *s, float *v )
{
	float min, max, delta;

	min = MIN( r, MIN( g, b ));
	max = MAX( r, MAX( g, b ));
	*v = max;

	delta = max - min;

	if( delta > 0  )
		*s = delta / max;
	else {
		*s = 0;
		*h = 0;
		return;
	}

	if( r == max )
		*h = ( g - b ) / delta;
	else if( g == max )
		*h = 2 + ( b - r ) / delta;
	else
		*h = 4 + ( r - g ) / delta;

	*h *= 60;
	if( *h < 0 )
		*h += 360;
}

static void HSVtoRGB( float *r, float *g, float *b, float h, float s, float v )
{
	int i;
	float f, p, q, t;

	if( s == 0 ) {
		*r = *g = *b = v;
		return;
	}

	h /= 60;
	i = h;
	f = h - i;
	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );

	switch( i ) {
	case 0: *r = v; *g = t; *b = p; break;
	case 1: *r = q; *g = v; *b = p; break;
	case 2: *r = p; *g = v; *b = t; break;
	case 3: *r = p; *g = q; *b = v; break;
	case 4: *r = t; *g = p; *b = v; break;
	default: *r = v; *g = p; *b = q; break;
	}

}

/*********************************************************************
  transparency_hist

  Calculates a histogram of all transparent pixels in the overlay.
  The function returns a array of ints with the number of shades
  for each transparent color based on the color histogram.
 *********************************************************************/
static unsigned int *transparency_hist (struct artwork *a, int num_shades)
{
	int i, j;
	unsigned int *hist;
	int num_pix=0, min_shades;
	UINT8 pen;

	if ((hist = (unsigned int *)malloc(a->num_pens_trans*sizeof(unsigned int)))==NULL)
	{
		logerror("Not enough memory!\n");
		return NULL;
	}
	memset (hist, 0, a->num_pens_trans*sizeof(int));

	if (a->orig_artwork->depth == 8)
	{
		for ( j=0; j<a->orig_artwork->height; j++)
			for (i=0; i<a->orig_artwork->width; i++)
			{
				pen = a->orig_artwork->line[j][i];
				if (pen < a->num_pens_trans)
				{
					hist[pen]++;
					num_pix++;
				}
			}
	}
	else
	{
		for ( j=0; j<a->orig_artwork->height; j++)
			for (i=0; i<a->orig_artwork->width; i++)
			{
				pen = ((UINT16 *)a->orig_artwork->line[j])[i];
				if (pen < a->num_pens_trans)
				{
					hist[pen]++;
					num_pix++;
				}
			}
	}

	/* we try to get at least 3 shades per transparent color */
	min_shades = ((num_shades-a->num_pens_used-3*a->num_pens_trans) < 0) ? 0 : 3;

	if (min_shades==0)
		logerror("Too many colors in overlay. Vector colors may be wrong.\n");

	num_pix /= num_shades-(a->num_pens_used-a->num_pens_trans)
		-min_shades*a->num_pens_trans;

	if (num_pix)
		for (i=0; i < a->num_pens_trans; i++)
			hist[i] = hist[i]/num_pix + min_shades;

	return hist;
}

/*********************************************************************
  overlay_set_palette

  Generates a palette for vector games with an overlay.

  The 'glowing' effect is simulated by alpha blending the transparent
  colors with a black (the screen) background. Then different shades
  of each transparent color are calculated by alpha blending this
  color with different levels of brightness (values in HSV) of the
  transparent color from v=0 to v=1. This doesn't work very well with
  blue. The number of shades is proportional to the number of pixels of
  that color. A look up table is also generated to map beam
  brightness and overlay colors to pens. If you have a beam brightness
  of 128 under a transparent pixel of pen 7 then
     Table (7,128)
  returns the pen of the resulting color. The table is usually
  converted to OS colors later.
 *********************************************************************/
int overlay_set_palette (UINT8 *palette, int num_shades)
{
	unsigned int i,j, shades = 0, step;
	unsigned int *hist;
	float h, s, v, r, g, b;

	/* adjust palette start */

	palette += 3 * artwork_overlay->start_pen;

	if (Machine->scrbitmap->depth == 8)
	{
		if((hist = transparency_hist (artwork_overlay, num_shades))==NULL)
			return 0;

		/* Copy all artwork colors to the palette */
		memcpy (palette, artwork_overlay->orig_palette, 3 * artwork_overlay->num_pens_used);

		/* Fill the palette with shades of the transparent colors */
		for (i = 0; i < artwork_overlay->num_pens_trans; i++)
		{
			RGBtoHSV( artwork_overlay->orig_palette[i * 3]/255.0,
					  artwork_overlay->orig_palette[i * 3 + 1] / 255.0,
					  artwork_overlay->orig_palette[i * 3 + 2] / 255.0, &h, &s, &v );

			/* blend transparent entries with black background */
			/* we don't need the original palette entry any more */
			HSVtoRGB ( &r, &g, &b, h, s, v*artwork_overlay->transparency[i]/255.0);
			palette [i * 3] = r * 255.0;
			palette [i * 3 + 1] = g * 255.0;
			palette [i * 3 + 2] = b * 255.0;
			if (hist[i] > 1)
			{
				for (j = 0; j < hist[i] - 1; j++)
				{
					/* we start from 1 because the 0 level is already in the palette */
					HSVtoRGB ( &r, &g, &b, h, s, v * artwork_overlay->transparency[i]/255.0 +
							   ((1.0-(v*artwork_overlay->transparency[i]/255.0))*(j+1))/(hist[i]-1));
					palette [(artwork_overlay->num_pens_used + shades + j) * 3] = r * 255.0;
					palette [(artwork_overlay->num_pens_used + shades + j) * 3 + 1] = g * 255.0;
					palette [(artwork_overlay->num_pens_used + shades + j) * 3 + 2] = b * 255.0;
				}

				/* create alpha LUT for quick alpha blending */
				for (j = 0; j < 256; j++)
				{
					step = hist[i] * j / 256.0;
					if (step == 0)
						/* no beam, just overlay over black screen */
						artwork_overlay->pTable[i * 256 + j] = i + artwork_overlay->start_pen;
					else
						artwork_overlay->pTable[i * 256 + j] = artwork_overlay->num_pens_used +
															   shades + step - 1 + artwork_overlay->start_pen;
				}
				shades += hist[i] - 1;
			}
		}
	}
	else
		memcpy (palette, artwork_overlay->orig_palette, 3 * artwork_overlay->num_pens_used);
	return 1;
}

/*********************************************************************
  overlay_remap

  This remaps the "original" palette indexes to the abstract OS indexes
  used by MAME. This has to be called during startup after the
  OS colors have been initialized (vh_start).
 *********************************************************************/
void overlay_remap(void)
{
	int i,j;
	UINT8 r,g,b;
	float h, s, v, rf, gf, bf;
	int offset, height, width;
	struct osd_bitmap *overlay, *overlay1, *orig;

	if (!artwork_overlay)  return;

	offset = artwork_overlay->start_pen;
	height = artwork_overlay->artwork->height;
	width = artwork_overlay->artwork->width;
	overlay = artwork_overlay->artwork;
	overlay1 = artwork_overlay->artwork1;
	orig = artwork_overlay->orig_artwork;

	if (overlay->depth == 8)
	{
		for ( j=0; j<height; j++)
			for (i=0; i<width; i++)
				overlay->line[j][i] = Machine->pens[orig->line[j][i]+offset];
	}
	else
	{
		if (artwork_overlay->alpha)
		{
			for ( j=0; j<height; j++)
				for (i=0; i<width; i++)
				{
					UINT64 v1,v2;
					UINT16 alpha = ((UINT16 *)artwork_overlay->alpha->line[j])[i];

					osd_get_pen (Machine->pens[((UINT16 *)orig->line[j])[i]+offset], &r, &g, &b);
					v1 = MAX(r, MAX(g, b));
					v2 = (v1 * alpha) / 255;
					artwork_overlay->rgb[j*width+i] = (v1 << 32) | (v2 << 24) | ((UINT64)r << 16) |
													  ((UINT64)g << 8) | (UINT64)b;

					RGBtoHSV( r/255.0, g/255.0, b/255.0, &h, &s, &v );

					HSVtoRGB( &rf, &gf, &bf, h, s, v * alpha/255.0);
					r = rf*255; g = gf*255; b = bf*255;
					((UINT16 *)overlay->line[j])[i] = Machine->pens[(((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3)) + artwork_overlay->start_pen];

					HSVtoRGB( &rf, &gf, &bf, h, s, 1);
					r = rf*255; g = gf*255; b = bf*255;
					((UINT16 *)overlay1->line[j])[i] = Machine->pens[(((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3)) + artwork_overlay->start_pen];
				}
		}
		else
		{
			for ( j=0; j<height; j++)
				for (i=0; i<width; i++)
					((UINT16 *)overlay->line[j])[i] = Machine->pens[((UINT16 *)orig->line[j])[i]+offset];
		}
	}

	/* Calculate brightness of all colors */

	for (i = 0; i < Machine->drv->total_colors; i++)
	{
		osd_get_pen (Machine->pens[i], &r, &g, &b);
		artwork_overlay->brightness[Machine->pens[i]]=(222*r+707*g+71*b)/1000;
	}

	/* Erase vector bitmap same way as in vector.c */
	if (artwork_overlay->vector_bitmap)
		fillbitmap(artwork_overlay->vector_bitmap,Machine->pens[0],0);
}

/*********************************************************************
  allocate_artwork_mem

  Allocates memory for all the bitmaps.
 *********************************************************************/
static void allocate_artwork_mem (int width, int height, struct artwork **a)
{
	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;

		temp = height;
		height = width;
		width = temp;
	}

	*a = (struct artwork *)malloc(sizeof(struct artwork));
	if (*a == 0)
	{
		logerror("Not enough memory for artwork!\n");
		return;
	}

	(*a)->transparency = NULL;
	(*a)->orig_palette = NULL;
	(*a)->pTable = NULL;
	(*a)->brightness = NULL;
	(*a)->vector_bitmap = NULL;

	if (((*a)->orig_artwork = bitmap_alloc(width, height)) == 0)
	{
		logerror("Not enough memory for artwork!\n");
		artwork_free(a);
		return;
	}
	fillbitmap((*a)->orig_artwork,0,0);

	if (((*a)->alpha = bitmap_alloc(width, height)) == 0)
	{
		logerror("Not enough memory for artwork!\n");
		artwork_free(a);
		return;
	}
	fillbitmap((*a)->alpha,0,0);

	if (((*a)->artwork = bitmap_alloc(width,height)) == 0)
	{
		logerror("Not enough memory for artwork!\n");
		artwork_free(a);
		return;
	}

	if (((*a)->artwork1 = bitmap_alloc(width,height)) == 0)
	{
		logerror("Not enough memory for artwork!\n");
		artwork_free(a);
		return;
	}

	if (((*a)->pTable = (UINT8*)malloc(256*256))==0)
	{
		logerror("Not enough memory.\n");
		artwork_free(a);
		return;
	}

	if (((*a)->brightness = (UINT8*)malloc(256*256))==0)
	{
		logerror("Not enough memory.\n");
		artwork_free(a);
		return;
	}
	memset ((*a)->brightness, 0, 256*256);

	if (((*a)->rgb = (UINT64*)malloc(width*height*sizeof(UINT64)))==0)
	{
		logerror("Not enough memory.\n");
		artwork_free(a);
		return;
	}

	/* Create bitmap for the vector screen */
	if (Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
	{
		if (((*a)->vector_bitmap = bitmap_alloc(width,height)) == 0)
		{
			logerror("Not enough memory for artwork!\n");
			artwork_free(a);
			return;
		}
		fillbitmap((*a)->vector_bitmap,0,0);
	}
}

static UINT8 *create_15bit_palette ( void )
{
	int r, g, b;
	UINT8 *palette, *tmp;

	if ((palette = (UINT8*)malloc(3 * 32768)) == 0)
		return 0;

	tmp = palette;
	for (r = 0; r < 32; r++)
		for (g = 0; g < 32; g++)
			for (b = 0; b < 32; b++)
			{
				*tmp++ = r << 3;
				*tmp++ = g << 3;
				*tmp++ = b << 3;
			}
	return palette;
}

/*********************************************************************

  Reads a PNG for a artwork struct and checks if it has the right
  format.

 *********************************************************************/
static int artwork_read_bitmap(const char *file_name, struct osd_bitmap **bitmap, struct osd_bitmap **alpha, struct png_info *p)
{
	UINT8 *tmp;
	int x, y, pen;
	void *fp;
	int file_name_len;
	char file_name2[256];

	/* check for .png */
	strcpy(file_name2, file_name);
	file_name_len = strlen(file_name2);
	if ((file_name_len < 4) || strcasecmp(&file_name2[file_name_len - 4], ".png"))
	{
		strcat(file_name2, ".png");
	}

	if (!(fp = osd_fopen(Machine->gamedrv->name, file_name2, OSD_FILETYPE_ARTWORK, 0)))
	{
		logerror("Unable to open PNG %s\n", file_name);
		return 0;
	}

	if (!png_read_file(fp, p))
	{
		osd_fclose (fp);
		return 0;
	}
	osd_fclose (fp);

	if (p->bit_depth > 8)
	{
		logerror("Unsupported bit depth %i (8 bit max.)\n", p->bit_depth);
		return 0;
	}

	if (p->interlace_method != 0)
	{
		logerror("Interlace unsupported\n");
		return 0;
	}

	if (Machine->scrbitmap->depth == 8 && p->color_type != 3)
	{
		logerror("Use 8bit artwork for 8bpp modes. Artwork disabled.\n");
		return 0;
	}

	switch (p->color_type)
	{
	case 3:
		/* Convert to 8 bit */
		png_expand_buffer_8bit (p);

		png_delete_unused_colors (p);

		if ((*bitmap = bitmap_alloc(p->width,p->height)) == 0)
		{
			logerror("Unable to allocate memory for artwork\n");
			return 0;
		}

		tmp = p->image;
		if ((*bitmap)->depth == 8)
		{
			for (y=0; y<p->height; y++)
				for (x=0; x<p->width; x++)
				{
					plot_pixel(*bitmap, x, y, *tmp++);
				}
		}
		else
		{
			/* convert to 15bit */
			if (p->num_trans > 0)
				if ((*alpha = bitmap_alloc(p->width,p->height)) == 0)
				{
					logerror("Unable to allocate memory for artwork\n");
					return 0;
				}

			for (y=0; y<p->height; y++)
				for (x=0; x<p->width; x++)
				{
					pen = ((p->palette[*tmp * 3] & 0xf8) << 7) | ((p->palette[*tmp * 3 + 1] & 0xf8) << 2) | (p->palette[*tmp * 3 + 2] >> 3);
					plot_pixel(*bitmap, x, y, pen);

					if (p->num_trans > 0)
					{
						if (*tmp < p->num_trans)
							plot_pixel(*alpha, x, y, p->trans[*tmp]);
						else
							plot_pixel(*alpha, x, y, 255);
					}
					tmp++;
				}

			free(p->palette);

			/* create 15 bit palette */
			if ((p->palette = create_15bit_palette()) == 0)
			{
				logerror("Unable to allocate memory for artwork\n");
				return 0;
			}
			p->num_palette = 32768;
		}
		break;

	case 6:
		if ((*alpha = bitmap_alloc(p->width,p->height)) == 0)
		{
			logerror("Unable to allocate memory for artwork\n");
			return 0;
		}

	case 2:
		if ((*bitmap = bitmap_alloc(p->width,p->height)) == 0)
		{
			logerror("Unable to allocate memory for artwork\n");
			return 0;
		}

		/* create 15 bit palette */
		if ((p->palette = create_15bit_palette()) == 0)
		{
			logerror("Unable to allocate memory for artwork\n");
			return 0;
		}

		p->num_palette = 32768;
		p->trans = NULL;
		p->num_trans = 0;

		/* reduce true color to 15 bit */
		tmp = p->image;
		for (y=0; y<p->height; y++)
			for (x=0; x<p->width; x++)
			{
				pen = ((tmp[0] & 0xf8) << 7) | ((tmp[1] & 0xf8) << 2) | (tmp[2] >> 3);
				plot_pixel(*bitmap, x, y, pen);

				if (p->color_type == 6)
				{
					plot_pixel(*alpha, x, y, tmp[3]);
					tmp += 4;
				}
				else
					tmp += 3;
			}

		break;

	default:
		logerror("Unsupported color type %i \n", p->color_type);
		return 0;
		break;
	}
	free(p->image);
	return 1;
}

/*********************************************************************
  artwork_load(_size)

  This is what loads your backdrop in from disk.
  start_pen = the first pen available for the backdrop to use
  max_pens = the number of pens the backdrop can use
  So, for example, suppose you want to load "dotron.png", have it
  start with color 192, and only use 48 pens.  You would call
  backdrop = backdrop_load("dotron.png",192,48);
 *********************************************************************/

static void artwork_load_size_common(const char *filename, unsigned int start_pen, unsigned int max_pens,
					   				 int width, int height, struct artwork **a)
{
	struct osd_bitmap *picture = 0, *alpha = 0;
	struct png_info p;
	int scalex, scaley;

	/* If the user turned artwork off, bail */
	if (!options.use_artwork) return;

	allocate_artwork_mem(width, height, a);

	if (*a==NULL)
		return;

	(*a)->start_pen = start_pen;

	if (!artwork_read_bitmap(filename, &picture, &alpha, &p))
	{
		artwork_free(a);
		return;
	}

	(*a)->num_pens_used = p.num_palette;
	(*a)->num_pens_trans = p.num_trans;
	(*a)->orig_palette = p.palette;
	(*a)->transparency = p.trans;

	/* Make sure we don't have too many colors */
	if ((*a)->num_pens_used > max_pens)
	{
		logerror("Too many colors in artwork.\n");
		logerror("Colors found: %d  Max Allowed: %d\n",
				      (*a)->num_pens_used,max_pens);
		artwork_free(a);
		bitmap_free(picture);
		return;
	}

	/* Scale the original picture to be the same size as the visible area */
	scalex = 0x10000 * picture->width  / (*a)->orig_artwork->width;
	scaley = 0x10000 * picture->height / (*a)->orig_artwork->height;
	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int tmp;
		tmp = scalex;
		scalex = scaley;
		scaley = tmp;
	}

	copyrozbitmap((*a)->orig_artwork, picture, 0, 0, scalex, 0, 0, scaley, 0, 0, TRANSPARENCY_NONE, 0, 0);
	/* We don't need the original any more */
	bitmap_free(picture);

	if (alpha)
	{
		copyrozbitmap((*a)->alpha, alpha, 0, 0, scalex, 0, 0, scaley, 0, 0, TRANSPARENCY_NONE, 0, 0);
		bitmap_free(alpha);
	}

	/* If the game uses dynamic colors, we assume that it's safe
	   to init the palette and remap the colors now */
	if (Machine->drv->video_attributes & VIDEO_MODIFIES_PALETTE)
		backdrop_set_palette(*a,(*a)->orig_palette);
}

static void artwork_load_common(const char *filename, unsigned int start_pen, unsigned int max_pens, struct artwork **a)
{
	artwork_load_size_common(filename, start_pen, max_pens, Machine->scrbitmap->width, Machine->scrbitmap->height, a);
}

void overlay_load(const char *filename, unsigned int start_pen, unsigned int max_pens)
{
	int width, height;

	/* replace the real display with a fake one, this way drivers can access Machine->scrbitmap
	   the same way as before */

	width = Machine->scrbitmap->width;
	height = Machine->scrbitmap->height;

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;

		temp = height;
		height = width;
		width = temp;
	}

	artwork_load_common(filename, start_pen, max_pens, &artwork_overlay);

	if (artwork_overlay)
	{
		overlay_real_scrbitmap = Machine->scrbitmap;

		if ((Machine->scrbitmap = bitmap_alloc(width, height)) == 0)
		{
			overlay_free();
			logerror("Not enough memory for artwork!\n");
			return;
		}
	}
}

void backdrop_load(const char *filename, unsigned int start_pen, unsigned int max_pens)
{
	artwork_load_common(filename, start_pen, max_pens, &artwork_backdrop);
}

void artwork_load(struct artwork **a, const char *filename, unsigned int start_pen, unsigned int max_pens)
{
	artwork_load_common(filename, start_pen, max_pens, a);
}

void artwork_load_size(struct artwork **a, const char *filename, unsigned int start_pen, unsigned int max_pens,
					   int width, int height)
{
	artwork_load_size_common(filename, start_pen, max_pens, width, height, a);
}

/*********************************************************************
  create_disk

  Creates a disk with radius r in the color of pen. A new bitmap
  is allocated for the disk.

*********************************************************************/
static struct osd_bitmap *create_disk (int r, int fg, int bg)
{
	struct osd_bitmap *disk;

	int x = 0, twox = 0;
	int y = r;
	int twoy = r+r;
	int p = 1 - r;
	int i;

	if ((disk = bitmap_alloc(twoy, twoy)) == 0)
	{
		logerror("Not enough memory for artwork!\n");
		return NULL;
	}

	/* background */
	fillbitmap (disk, bg, 0);

	while (x < y)
	{
		x++;
		twox +=2;
		if (p < 0)
			p += twox + 1;
		else
		{
			y--;
			twoy -= 2;
			p += twox - twoy + 1;
		}

		for (i = 0; i < twox; i++)
		{
			plot_pixel(disk, r-x+i, r-y  , fg);
			plot_pixel(disk, r-x+i, r+y-1, fg);
		}

		for (i = 0; i < twoy; i++)
		{
			plot_pixel(disk, r-y+i, r-x  , fg);
			plot_pixel(disk, r-y+i, r+x-1, fg);
		}
	}
	return disk;
}

/*********************************************************************
  artwork_elements scale

  scales an array of artwork elements to width and height. The first
  element (which has to be a box) is used as reference. This is useful
  for atwork with disks.

*********************************************************************/

void artwork_elements_scale(struct artwork_element *ae, int width, int height)
{
	int scale_w, scale_h;

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		scale_w = (height << 16)/(ae->box.max_x + 1);
		scale_h = (width << 16)/(ae->box.max_y + 1);
	}
	else
	{
		scale_w = (width << 16)/(ae->box.max_x + 1);
		scale_h = (height << 16)/(ae->box.max_y + 1);
	}
	while (ae->box.min_x >= 0)
	{
		ae->box.min_x = (ae->box.min_x * scale_w) >> 16;
		ae->box.max_x = (ae->box.max_x * scale_w) >> 16;
		ae->box.min_y = (ae->box.min_y * scale_h) >> 16;
		if (ae->box.max_y >= 0)
			ae->box.max_y = (ae->box.max_y * scale_h) >> 16;
		ae++;
	}
}

static int artwork_newpen (struct artwork *a, int r, int g, int b, int alpha)
{
	int pen;

	/* look if the color is already in the palette */
	if (Machine->scrbitmap->depth == 8)
	{
		pen =0;
		while ((pen < a->num_pens_used) &&
			   ((r != a->orig_palette[3*pen]) ||
				(g != a->orig_palette[3*pen+1]) ||
				(b != a->orig_palette[3*pen+2]) ||
				((alpha < 255) && (alpha != a->transparency[pen]))))
			pen++;

		if (pen == a->num_pens_used)
		{
			a->orig_palette[3*pen]=r;
			a->orig_palette[3*pen+1]=g;
			a->orig_palette[3*pen+2]=b;
			a->num_pens_used++;
			if (alpha < 255)
			{
				a->transparency[pen]=alpha;
				a->num_pens_trans++;
			}
		}
	}
	else
		pen = ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3);

	return pen;
}

static void merge_cmy(struct artwork *a, struct osd_bitmap *source, struct osd_bitmap *source_alpha,int sx, int sy)
{
	int c1, c2, m1, m2, y1, y2, pen1, pen2, max, alpha;
	int x, y, w, h;
	struct osd_bitmap *dest, *dest_alpha;

	dest = a->orig_artwork;
	dest_alpha = a->alpha;

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		w = source->height;
		h = source->width;
	}
	else
	{
		h = source->height;
		w = source->width;
	}

	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++)
		{
			pen1 = read_pixel(dest, sx + x, sy + y);

			c1 = 0xff - a->orig_palette[3*pen1];
			m1 = 0xff - a->orig_palette[3*pen1+1];
			y1 = 0xff - a->orig_palette[3*pen1+2];

			pen2 = read_pixel(source, x, y);
			c2 = 0xff - a->orig_palette[3*pen2] + c1;
			m2 = 0xff - a->orig_palette[3*pen2+1] + m1;
			y2 = 0xff - a->orig_palette[3*pen2+2] + y1;

			max = MAX(c2, MAX(m2, y2));
			if (max > 0xff)
			{
				c2 = (c2 * 0xf8) / max;
				m2 = (m2 * 0xf8) / max;
				y2 = (y2 * 0xf8) / max;
			}

			alpha = MIN (0xff, read_pixel(source_alpha, x, y)
						 + read_pixel(dest_alpha, sx + x, sy + y));
			plot_pixel(dest, sx + x, sy + y, artwork_newpen(a, 0xff - c2, 0xff - m2, 0xff - y2, alpha));
			plot_pixel(dest_alpha, sx + x, sy + y, alpha);
		}
}

/*********************************************************************
  overlay_create

  This works similar to artwork_load but generates artwork from
  an array of artwork_element. This is useful for very simple artwork
  like the overlay in the Space invaders series of games.  The overlay
  is defined to be the same size as the screen.
  The end of the array is marked by an entry with negative coordinates.
  Boxes and disks are supported. Disks are marked max_y == -1,
  min_x == x coord. of center, min_y == y coord. of center, max_x == radius.
  If there are transparent and opaque overlay elements, the opaque ones
  have to be at the end of the list to stay compatible with the PNG
  artwork.
 *********************************************************************/
void overlay_create(const struct artwork_element *ae, unsigned int start_pen, unsigned int max_pens)
{
	struct osd_bitmap *disk, *disk_alpha, *box, *box_alpha;
	int pen, transparent_pen = -1, disk_type, white_pen;
	int width, height;

	allocate_artwork_mem(Machine->scrbitmap->width, Machine->scrbitmap->height, &artwork_overlay);

	if (artwork_overlay==NULL)
		return;

	/* replace the real display with a fake one, this way drivers can access Machine->scrbitmap
	   the same way as before */

	width = Machine->scrbitmap->width;
	height = Machine->scrbitmap->height;

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;

		temp = height;
		height = width;
		width = temp;
	}

	overlay_real_scrbitmap = Machine->scrbitmap;

	if ((Machine->scrbitmap = bitmap_alloc(width, height)) == 0)
	{
		overlay_free();
		logerror("Not enough memory for artwork!\n");
		return;
	}

	artwork_overlay->start_pen = start_pen;

	if (Machine->scrbitmap->depth == 8)
	{
		if ((artwork_overlay->orig_palette = (UINT8 *)malloc(256*3)) == NULL)
		{
			logerror("Not enough memory for overlay!\n");
			overlay_free();
			return;
		}

		if ((artwork_overlay->transparency = (UINT8 *)malloc(256)) == NULL)
		{
			logerror("Not enough memory for overlay!\n");
			overlay_free();
			return;
		}

		transparent_pen = 255;
		/* init with transparent white */
		memset (artwork_overlay->orig_palette, 255, 3);
		artwork_overlay->transparency[0]=0;
		artwork_overlay->num_pens_used = 1;
		artwork_overlay->num_pens_trans = 1;
		white_pen = 0;
		fillbitmap (artwork_overlay->orig_artwork, 0, 0);
		fillbitmap (artwork_overlay->alpha, 0, 0);
	}
	else
	{
		if ((artwork_overlay->orig_palette = create_15bit_palette()) == 0)
		{
			logerror("Unable to allocate memory for artwork\n");
			overlay_free();
			return;
		}
		artwork_overlay->num_pens_used = 32768;
		transparent_pen = 0xffff;
		white_pen = 0x7fff;
		fillbitmap (artwork_overlay->orig_artwork, white_pen, 0);
		fillbitmap (artwork_overlay->alpha, 0, 0);
	}

	while (ae->box.min_x >= 0)
	{
		int alpha = ae->alpha;

		if (alpha == OVERLAY_DEFAULT_OPACITY)
		{
			alpha = 0x18;
		}

		pen = artwork_newpen (artwork_overlay, ae->red, ae->green, ae->blue, alpha);
		if (ae->box.max_y < 0) /* disk */
		{
			int r = ae->box.max_x;
			disk_type = ae->box.max_y;

			switch (disk_type)
			{
			case -1: /* disk overlay */
				if ((disk = create_disk (r, pen, white_pen)) == NULL)
				{
					overlay_free();
					return;
				}
				if ((disk_alpha = create_disk (r, alpha, 0)) == NULL)
				{
					overlay_free();
					return;
				}
				merge_cmy (artwork_overlay, disk, disk_alpha, ae->box.min_x - r, ae->box.min_y - r);
				bitmap_free(disk_alpha);
				bitmap_free(disk);
				break;

			case -2: /* punched disk */
				if ((disk = create_disk (r, pen, transparent_pen)) == NULL)
				{
					overlay_free();
					return;
				}
				copybitmap(artwork_overlay->orig_artwork,disk,0, 0,
						   ae->box.min_x - r,
						   ae->box.min_y - r,
						   0,TRANSPARENCY_PEN, transparent_pen);
				/* alpha */
				if ((disk_alpha = create_disk (r, alpha, transparent_pen)) == NULL)
				{
					overlay_free();
					return;
				}
				copybitmap(artwork_overlay->alpha,disk_alpha,0, 0,
						   ae->box.min_x - r,
						   ae->box.min_y - r,
						   0,TRANSPARENCY_PEN, transparent_pen);
				bitmap_free(disk_alpha);
				bitmap_free(disk);
				break;

			}
		}
		else
		{
			if ((box = bitmap_alloc(ae->box.max_x - ae->box.min_x + 1,
										 ae->box.max_y - ae->box.min_y + 1)) == 0)
			{
				logerror("Not enough memory for artwork!\n");
				overlay_free();
				return;
			}
			if ((box_alpha = bitmap_alloc(ae->box.max_x - ae->box.min_x + 1,
										 ae->box.max_y - ae->box.min_y + 1)) == 0)
			{
				logerror("Not enough memory for artwork!\n");
				overlay_free();
				return;
			}
			fillbitmap (box, pen, 0);
			fillbitmap (box_alpha, alpha, 0);
			merge_cmy (artwork_overlay, box, box_alpha, ae->box.min_x, ae->box.min_y);
			bitmap_free(box);
			bitmap_free(box_alpha);
		}
		ae++;
	}

	/* Make sure we don't have too many colors */
	if (artwork_overlay->num_pens_used > max_pens)
	{
		logerror("Too many colors in overlay.\n");
		logerror("Colors found: %d  Max Allowed: %d\n",
				      artwork_overlay->num_pens_used,max_pens);
		overlay_free();
		return;
	}

	/* If the game uses dynamic colors, we assume that it's safe
	   to init the palette and remap the colors now */
	if (Machine->drv->video_attributes & VIDEO_MODIFIES_PALETTE)
		backdrop_set_palette(artwork_overlay,artwork_overlay->orig_palette);
}
