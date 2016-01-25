/******************************************************************************
 *
 * vector.c
 *
 *
 * Copyright 1997,1998 by the M.A.M.E. Project
 *
 *        anti-alias code by Andrew Caldwell
 *        (still more to add)
 *
 * 980611 use translucent vectors. Thanks to Peter Hirschberg
 *        and Neil Bradley for the inspiration. BW
 * 980307 added cleverer dirty handling. BW, ASG
 *        fixed antialias table .ac
 * 980221 rewrote anti-alias line draw routine
 *        added inline assembly multiply fuction for 8086 based machines
 *        beam diameter added to draw routine
 *        beam diameter is accurate in anti-alias line draw (Tcosin)
 *        flicker added .ac
 * 980203 moved LBO's routines for drawing into a buffer of vertices
 *        from avgdvg.c to this location. Scaling is now initialized
 *        by calling vector_init(...). BW
 * 980202 moved out of msdos.c ASG
 * 980124 added anti-alias line draw routine
 *        modified avgdvg.c and sega.c to support new line draw routine
 *        added two new tables Tinten and Tmerge (for 256 color support)
 *        added find_color routine to build above tables .ac
 *
 **************************************************************************** */

/* GLmame and FXmame provide their own vector implementations */
#if !(defined xgl) && !(defined xfx) && !(defined svgafx)

#include <math.h>
#include "osinline.h"
#include "driver.h"
#include "osdepend.h"
#include "vector.h"
#include "artwork.h"

#define VCLEAN  0
#define VDIRTY  1
#define VCLIP   2

unsigned char *vectorram;
size_t vectorram_size;

int antialias;                            /* flag for anti-aliasing */
int beam;                                 /* size of vector beam    */
int flicker;                              /* beam flicker value     */
int translucency;

static int beam_diameter_is_one;		  /* flag that beam is one pixel wide */

static int vector_scale_x;                /* scaling to screen */
static int vector_scale_y;                /* scaling to screen */

static float gamma_correction = 1.2;
static float intensity_correction = 1.5;

/* The vectices are buffered here */
typedef struct
{
	int x; int y;
	int col;
	int intensity;
	int arg1; int arg2; /* start/end in pixel array or clipping info */
	int status;         /* for dirty and clipping handling */
} point;

static point *new_list;
static point *old_list;
static int new_index;
static int old_index;

/* coordinates of pixels are stored here for faster removal */
static unsigned int *pixel;
static int p_index=0;

static UINT32 *pTcosin;            /* adjust line width */
static UINT8  *pTinten;            /* intensity         */
static UINT16 *pTmerge;            /* mergeing pixels   */
static UINT16 *invpens;            /* maps OS colors to pens */

static UINT16 *pens;
static UINT16 total_colors;

#define Tcosin(x)   pTcosin[(x)]          /* adjust line width */
#define Tinten(x,y) pTinten[(x)*total_colors+(y)]  /* intensity         */
#define Tmerge(x,y) pTmerge[(x)*total_colors+(y)]  /* mergeing pixels   */

#define ANTIALIAS_GUNBIT  6             /* 6 bits per gun in vga (1-8 valid) */
#define ANTIALIAS_GUNNUM  (1<<ANTIALIAS_GUNBIT)

static UINT8 Tgamma[256];         /* quick gamma anti-alias table  */
static UINT8 Tgammar[256];        /* same as above, reversed order */

static struct osd_bitmap *vecbitmap;
static int vecwidth, vecheight;
static int vecshift;
static int xmin, ymin, xmax, ymax; /* clipping area */

static int vector_runs;	/* vector runs per refresh */

static void (*vector_pp)(struct osd_bitmap *bitmap,int x,int y,int pen);
static int  (*vector_rp)(struct osd_bitmap *bitmap,int x,int y);

/*
 * multiply and divide routines for drawing lines
 * can be be replaced by an assembly routine in osinline.h
 */
#ifndef vec_mult
INLINE int vec_mult(int parm1, int parm2)
{
	int temp,result;

	temp     = abs(parm1);
	result   = (temp&0x0000ffff) * (parm2&0x0000ffff);
	result >>= 16;
	result  += (temp&0x0000ffff) * (parm2>>16       );
	result  += (temp>>16       ) * (parm2&0x0000ffff);
	result >>= 16;
	result  += (temp>>16       ) * (parm2>>16       );

	if( parm1 < 0 )
		return(-result);
	else
		return( result);
}
#endif

/* can be be replaced by an assembly routine in osinline.h */
#ifndef vec_div
INLINE int vec_div(int parm1, int parm2)
{
	if( (parm2>>12) )
	{
		parm1 = (parm1<<4) / (parm2>>12);
		if( parm1 > 0x00010000 )
			return( 0x00010000 );
		if( parm1 < -0x00010000 )
			return( -0x00010000 );
		return( parm1 );
	}
	return( 0x00010000 );
}
#endif

static void vector_pp_8(struct osd_bitmap *b,int x,int y,int p)  { b->line[y][x] = p; }
static void vector_pp_16(struct osd_bitmap *b,int x,int y,int p)  { ((unsigned short *)b->line[y])[x] = p; }
static int vector_rp_8(struct osd_bitmap *b,int x,int y)  { return b->line[y][x]; }
static int vector_rp_16(struct osd_bitmap *b,int x,int y)  { return ((unsigned short *)b->line[y])[x]; }

/*
 * finds closest color and returns the index (for 256 color)
 */

static UINT16 find_pen(unsigned char r,unsigned char g,unsigned char b)
{
	int i,bi,ii;
	long x,y,z,bc;
	ii = 32;
	bi = 256;
	bc = 0x01000000;

	do
	{
		for( i = 0; i < total_colors; i++ )
		{
			unsigned char r1,g1,b1;

			osd_get_pen(pens[i],&r1,&g1,&b1);
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

/* MLR 990316 new gamma handling added */
void vector_set_gamma(float _gamma)
{
	int i, h;

	gamma_correction = _gamma;

	for (i = 0; i < 256; i++)
	{
		h = 255.0*pow(i/255.0, 1.0/gamma_correction);
		if( h > 255) h = 255;
		Tgamma[i] = Tgammar[255-i] = h;
	}
}

float vector_get_gamma(void)
{
	return gamma_correction;
}

void vector_set_intensity(float _intensity)
{
	intensity_correction = _intensity;
}

float vector_get_intensity(void)
{
	return intensity_correction;
}

/*
 * Initializes vector game video emulation
 */

int vector_vh_start (void)
{
	int h,i,j,k,c[3];

	/* Grab the settings for this session */
	antialias = options.antialias;
	translucency = options.translucency;
	flicker = options.flicker;
	beam = options.beam;

	pens = Machine->pens;
	total_colors = MIN(256, Machine->drv->total_colors);

	if (Machine->color_depth == 8)
	{
		vector_pp = vector_pp_8;
		vector_rp = vector_rp_8;
	}
	else
	{
		vector_pp = vector_pp_16;
		vector_rp = vector_rp_16;
	}

	if (beam == 0x00010000)
		beam_diameter_is_one = 1;
	else
		beam_diameter_is_one = 0;

	p_index = 0;

	new_index = 0;
	old_index = 0;
	vector_runs = 0;

	/* allocate memory for tables */
	pTcosin = (UINT32*)malloc( (2048+1) * sizeof(INT32));   /* yes! 2049 is correct */
	pTinten = (UINT8*)malloc( total_colors * 256 * sizeof(UINT8));
	pTmerge = (UINT16*)malloc(total_colors * total_colors * sizeof(UINT32));
	invpens = (UINT16*)malloc(65536 * sizeof(UINT16));
	pixel = (unsigned int*)malloc(MAX_PIXELS * sizeof (UINT32));
	old_list = (point*)malloc(MAX_POINTS * sizeof (point));
	new_list = (point*)malloc(MAX_POINTS * sizeof (point));

	/* did we get the requested memory? */
	if (!(pTcosin && pTinten && pTmerge && invpens && pixel && old_list && new_list))
	{
		/* vector_vh_stop should better be called by the main engine */
		/* if vector_vh_start fails */
		vector_vh_stop();
		return 1;
	}

	/* build cosine table for fixing line width in antialias */
	for (i=0; i<=2048; i++)
	{
		Tcosin(i) = (int)((double)(1.0/cos(atan((double)(i)/2048.0)))*0x10000000 + 0.5);
	}

	memset (invpens, 0, 65536 * sizeof(unsigned short));
	for( i = 0; i < total_colors ;i++ )
		invpens[Machine->pens[i]] = i;

	/* build anti-alias table */
	h = 256 / ANTIALIAS_GUNNUM;           /* to generate table faster */
	for (i = 0; i < 256; i += h )               /* intensity */
	{
		for (j = 0; j < total_colors; j++)               /* color */
		{
			UINT8 r1,g1,b1,pen,n;
			osd_get_pen(pens[j],&r1,&g1,&b1);
			pen = find_pen( (r1*(i+1))>>8, (g1*(i+1))>>8, (b1*(i+1))>>8 );
			for (n = 0; n < h; n++ )
			{
				Tinten(i + n, j) = pen;
			}
		}
	}

	/* build merge color table */
	for( i = 0; i < total_colors ;i++ )                /* color1 */
	{
		unsigned char rgb1[3],rgb2[3];

		osd_get_pen(pens[i],&rgb1[0],&rgb1[1],&rgb1[2]);
		for( j = 0; j <= i ;j++ )               /* color2 */
		{
			osd_get_pen(pens[j],&rgb2[0],&rgb2[1],&rgb2[2]);

			for (k = 0; k < 3; k++)
			if (translucency) /* add gun values */
			{
				int tmp;
				tmp = rgb1[k] + rgb2[k];
				if (tmp > 255)
					c[k] = 255;
				else
					c[k] = tmp;
			}
			else /* choose highest gun value */
			{
				if (rgb1[k] > rgb2[k])
					c[k] = rgb1[k];
				else
					c[k] = rgb2[k];
			}
			Tmerge(i,j) = Tmerge(j,i) = find_pen(c[0],c[1],c[2]);
		}
	}

	/* build gamma correction table */
	vector_set_gamma (gamma_correction);

	return 0;
}


/*
 * Setup scaling. Currently the Sega games are stuck at VECSHIFT 15
 * and the the AVG games at VECSHIFT 16
 */
void vector_set_shift (int shift)
{
	vecshift = shift;
}

/*
 * Clear the old bitmap. Delete pixel for pixel, this is faster than memset.
 */
static void vector_clear_pixels (void)
{
	unsigned char bg=pens[0];
	int i;
	int coords;


	for (i=p_index-1; i>=0; i--)
	{
		coords = pixel[i];
		vector_pp (vecbitmap, coords >> 16, coords & 0x0000ffff, bg);
	}

	p_index=0;

}

/*
 * Stop the vector video hardware emulation. Free memory.
 */
void vector_vh_stop (void)
{
	if (pTcosin)
		free(pTcosin);
	pTcosin = NULL;
	if (pTinten)
		free(pTinten);
	pTinten = NULL;
	if (pTmerge)
		free(pTmerge);
	pTmerge = NULL;
	if (pixel)
		free(pixel);
	pixel = NULL;
	if (old_list)
		free(old_list);
	old_list = NULL;
	if (new_list)
		free(new_list);
	new_list = NULL;
}

/*
 * draws an anti-aliased pixel (blends pixel with background)
 */
INLINE void vector_draw_aa_pixel (int x, int y, int col, int dirty)
{
	if (x < xmin || x >= xmax)
		return;
	if (y < ymin || y >= ymax)
		return;

	vector_pp (vecbitmap, x, y, pens[Tmerge(invpens[vector_rp(vecbitmap, x, y)], col)]);

	if (p_index<MAX_PIXELS)
	{
		pixel[p_index] = y | (x << 16);
		p_index++;
	}

	/* Mark this pixel as dirty */
	if (dirty)
		osd_mark_vector_dirty (x, y);
}


/*
 * draws a line
 *
 * input:   x2  16.16 fixed point
 *          y2  16.16 fixed point
 *         col  0-255 indexed color (8 bit)
 *   intensity  0-255 intensity
 *       dirty  bool  mark the pixels as dirty while plotting them
 *
 * written by Andrew Caldwell
 */

void vector_draw_to (int x2, int y2, int col, int intensity, int dirty)
{
	unsigned char a1;
	int orientation;
	int dx,dy,sx,sy,cx,cy,width;
	static int x1,yy1;
	int xx,yy;

#if 0
	logerror("line:%d,%d nach %d,%d color %d\n",x1,yy1,x2,y2,col);
#endif

	/* [1] scale coordinates to display */

	x2 = vec_mult(x2<<4,vector_scale_x);
	y2 = vec_mult(y2<<4,vector_scale_y);

	/* [2] fix display orientation */

	orientation = Machine->orientation;
	if (orientation & ORIENTATION_SWAP_XY)
	{
		int temp;
		temp = x2;
		x2 = y2;
		y2 = temp;
	}
	if (orientation & ORIENTATION_FLIP_X)
		x2 = ((vecwidth-1)<<16)-x2;
	if (orientation & ORIENTATION_FLIP_Y)
		y2 = ((vecheight-1)<<16)-y2;

	/* [3] adjust cords if needed */

	if (antialias)
	{
		if(beam_diameter_is_one)
		{
			x2 = (x2+0x8000)&0xffff0000;
			y2 = (y2+0x8000)&0xffff0000;
		}
	}
	else /* noantialiasing */
	{
		x2 >>= 16;
		y2 >>= 16;
	}

	/* [4] handle color and intensity */

	if (intensity == 0) goto end_draw;

	col = Tinten(intensity,col);

	/* [5] draw line */

	if (antialias)
	{
		/* draw an anti-aliased line */
		dx=abs(x1-x2);
		dy=abs(yy1-y2);
		if (dx>=dy)
		{
			sx = ((x1 <= x2) ? 1:-1);
			sy = vec_div(y2-yy1,dx);
			if (sy<0)
				dy--;
			x1 >>= 16;
			xx = x2>>16;
			width = vec_mult(beam<<4,Tcosin(abs(sy)>>5));
			if (!beam_diameter_is_one)
				yy1-= width>>1; /* start back half the diameter */
			for (;;)
			{
				dx = width;    /* init diameter of beam */
				dy = yy1>>16;
				vector_draw_aa_pixel(x1,dy++,Tinten(Tgammar[0xff&(yy1>>8)],col), dirty);
				dx -= 0x10000-(0xffff & yy1); /* take off amount plotted */
				a1 = Tgamma[(dx>>8)&0xff];   /* calc remainder pixel */
				dx >>= 16;                   /* adjust to pixel (solid) count */
				while (dx--)                 /* plot rest of pixels */
					vector_draw_aa_pixel(x1,dy++,col, dirty);
				vector_draw_aa_pixel(x1,dy,Tinten(a1,col), dirty);
				if (x1 == xx) break;
				x1+=sx;
				yy1+=sy;
			}
		}
		else
		{
			sy = ((yy1 <= y2) ? 1:-1);
			sx = vec_div(x2-x1,dy);
			if (sx<0)
				dx--;
			yy1 >>= 16;
			yy = y2>>16;
			width = vec_mult(beam<<4,Tcosin(abs(sx)>>5));
			if( !beam_diameter_is_one )
				x1-= width>>1; /* start back half the width */
			for (;;)
			{
				dy = width;    /* calc diameter of beam */
				dx = x1>>16;
				vector_draw_aa_pixel(dx++,yy1,Tinten(Tgammar[0xff&(x1>>8)],col), dirty);
				dy -= 0x10000-(0xffff & x1); /* take off amount plotted */
				a1 = Tgamma[(dy>>8)&0xff];   /* remainder pixel */
				dy >>= 16;                   /* adjust to pixel (solid) count */
				while (dy--)                 /* plot rest of pixels */
					vector_draw_aa_pixel(dx++,yy1,col, dirty);
				vector_draw_aa_pixel(dx,yy1,Tinten(a1,col), dirty);
				if (yy1 == yy) break;
				yy1+=sy;
				x1+=sx;
			}
		}
	}
	else /* use good old Bresenham for non-antialiasing 980317 BW */
	{
		dx = abs(x1-x2);
		dy = abs(yy1-y2);
		sx = (x1 <= x2) ? 1: -1;
		sy = (yy1 <= y2) ? 1: -1;
		cx = dx/2;
		cy = dy/2;

		if (dx>=dy)
		{
			for (;;)
			{
				vector_draw_aa_pixel (x1, yy1, col, dirty);
				if (x1 == x2) break;
				x1 += sx;
				cx -= dy;
				if (cx < 0)
				{
					yy1 += sy;
					cx += dx;
				}
			}
		}
		else
		{
			for (;;)
			{
				vector_draw_aa_pixel (x1, yy1, col, dirty);
				if (yy1 == y2) break;
				yy1 += sy;
				cy -= dx;
				if (cy < 0)
				{
					x1 += sx;
					cy += dy;
				}
			}
		}
	}

end_draw:

	x1=x2;
	yy1=y2;
}


/*
 * Adds a line end point to the vertices list. The vector processor emulation
 * needs to call this.
 */
void vector_add_point (int x, int y, int color, int intensity)
{
	point *_new;

	intensity *= intensity_correction;
	if (intensity > 0xff)
		intensity = 0xff;

	if (flicker && (intensity > 0))
	{
		intensity += (intensity * (0x80-(rand()&0xff)) * flicker)>>16;
		if (intensity < 0)
			intensity = 0;
		if (intensity > 0xff)
			intensity = 0xff;
	}
	_new = &new_list[new_index];
	_new->x = x;
	_new->y = y;
	_new->col = color;
	_new->intensity = intensity;
	_new->status = VDIRTY; /* mark identical lines as clean later */

	new_index++;
	if (new_index >= MAX_POINTS)
	{
		new_index--;
		//logerror("*** Warning! Vector list overflow!\n");
	}
}

/*
 * Add new clipping info to the list
 */
void vector_add_clip (int x1, int yy1, int x2, int y2)
{
	point *_new;

	_new = &new_list[new_index];
	_new->x = x1;
	_new->y = yy1;
	_new->arg1 = x2;
	_new->arg2 = y2;
	_new->status = VCLIP;

	new_index++;
	if (new_index >= MAX_POINTS)
	{
		new_index--;
		//logerror("*** Warning! Vector list overflow!\n");
	}
}


/*
 * Set the clipping area
 */
void vector_set_clip (int x1, int yy1, int x2, int y2)
{
	int orientation;
	int tmp;

	/* failsafe */
	if ((x1 >= x2) || (yy1 >= y2))
	{
		//logerror("Error in clipping parameters.\n");
		xmin = 0;
		ymin = 0;
		xmax = vecwidth;
		ymax = vecheight;
		return;
	}

	/* scale coordinates to display */
	x1 = vec_mult(x1<<4,vector_scale_x);
	yy1 = vec_mult(yy1<<4,vector_scale_y);
	x2 = vec_mult(x2<<4,vector_scale_x);
	y2 = vec_mult(y2<<4,vector_scale_y);

	/* fix orientation */
	orientation = Machine->orientation;
	/* swapping x/y coordinates will still have the minima in x1,yy1 */
	if (orientation & ORIENTATION_SWAP_XY)
	{
		tmp = x1; x1 = yy1; yy1 = tmp;
		tmp = x2; x2 = y2; y2 = tmp;
	}
	/* don't forget to swap x1,x2, since x2 becomes the minimum */
	if (orientation & ORIENTATION_FLIP_X)
	{
		x1 = ((vecwidth-1)<<16)-x1;
		x2 = ((vecwidth-1)<<16)-x2;
		tmp = x1; x1 = x2; x2 = tmp;
	}
	/* don't forget to swap yy1,y2, since y2 becomes the minimum */
	if (orientation & ORIENTATION_FLIP_Y)
	{
		yy1 = ((vecheight-1)<<16)-yy1;
		y2 = ((vecheight-1)<<16)-y2;
		tmp = yy1; yy1 = y2; y2 = tmp;
	}

	xmin = x1 >> 16;
	ymin = yy1 >> 16;
	xmax = x2 >> 16;
	ymax = y2 >> 16;

	/* Make it foolproof by trapping rounding errors */
	if (xmin < 0) xmin = 0;
	if (ymin < 0) ymin = 0;
	if (xmax > vecwidth) xmax = vecwidth;
	if (ymax > vecheight) ymax = vecheight;
}


/*
 * The vector CPU creates a new display list. We save the old display list,
 * but only once per refresh.
 */
void vector_clear_list (void)
{
	point *tmp;

	if (vector_runs == 0)
	{
		old_index = new_index;
		tmp = old_list; old_list = new_list; new_list = tmp;
	}

	new_index = 0;
	vector_runs++;
}


/*
 * By comparing with the last drawn list, we can prevent that identical
 * vectors are marked dirty which appeared at the same list index in the
 * previous frame. BW 19980307
 */
static void clever_mark_dirty (void)
{
	int i, j, min_index, last_match = 0;
	unsigned int *coords;
	point *_new, *old;
	point newclip, oldclip;
	int clips_match = 1;

	if (old_index < new_index)
		min_index = old_index;
	else
		min_index = new_index;

	/* Reset the active clips to invalid values */
	memset (&newclip, 0, sizeof (newclip));
	memset (&oldclip, 0, sizeof (oldclip));

	/* Mark vectors which are not the same in both lists as dirty */
	_new = new_list;
	old = old_list;

	for (i = min_index; i > 0; i--, old++, _new++)
	{
		/* If this is a clip, we need to determine if the clip regions still match */
		if (old->status == VCLIP || _new->status == VCLIP)
		{
			if (old->status == VCLIP)
				oldclip = *old;
			if (_new->status == VCLIP)
				newclip = *_new;
			clips_match = (newclip.x == oldclip.x) && (newclip.y == oldclip.y) && (newclip.arg1 == oldclip.arg1) && (newclip.arg2 == oldclip.arg2);
			if (!clips_match)
				last_match = 0;

			/* fall through to erase the old line if this is not a clip */
			if (old->status == VCLIP)
				continue;
		}

		/* If the clips match and the vectors match, update */
		else if (clips_match && (_new->x == old->x) && (_new->y == old->y) &&
			(_new->col == old->col) && (_new->intensity == old->intensity))
		{
			if (last_match)
			{
				_new->status = VCLEAN;
				continue;
			}
			last_match = 1;
		}

		/* If nothing matches, remember it */
		else
			last_match = 0;

		/* mark the pixels of the old vector dirty */
		coords = &pixel[old->arg1];
		for (j = (old->arg2 - old->arg1); j > 0; j--)
		{
			osd_mark_vector_dirty (*coords >> 16, *coords & 0x0000ffff);
			coords++;
		}
	}

	/* all old vector with index greater new_index are dirty */
	/* old = &old_list[min_index] here! */
	for (i = (old_index-min_index); i > 0; i--, old++)
	{
		/* skip old clips */
		if (old->status == VCLIP)
			continue;

		/* mark the pixels of the old vector dirty */
		coords = &pixel[old->arg1];
		for (j = (old->arg2 - old->arg1); j > 0; j--)
		{
			osd_mark_vector_dirty (*coords >> 16, *coords & 0x0000ffff);
			coords++;
		}
	}
}

void vector_vh_update(struct osd_bitmap *bitmap,int full_refresh)
{
	int i;
	int temp_x, temp_y;
	point *_new;

	/* copy parameters */
	vecbitmap = bitmap;
	vecwidth  = bitmap->width;
	vecheight = bitmap->height;

	/* setup scaling */
	temp_x = (1<<(44-vecshift)) / (Machine->visible_area.max_x - Machine->visible_area.min_x);
	temp_y = (1<<(44-vecshift)) / (Machine->visible_area.max_y - Machine->visible_area.min_y);

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		vector_scale_x = temp_x * vecheight;
		vector_scale_y = temp_y * vecwidth;
	}
	else
	{
		vector_scale_x = temp_x * vecwidth;
		vector_scale_y = temp_y * vecheight;
	}
	/* reset clipping area */
	xmin = 0; xmax = vecwidth; ymin = 0; ymax = vecheight;

	/* next call to vector_clear_list() is allowed to swap the lists */
	vector_runs = 0;

	/* mark pixels which are not idential in newlist and oldlist dirty */
	/* the old pixels which get removed are marked dirty immediately,  */
	/* new pixels are recognized by setting new->dirty                 */
	clever_mark_dirty();

	/* clear ALL pixels in the hidden map */
	vector_clear_pixels();

	/* Draw ALL lines into the hidden map. Mark only those lines with */
	/* new->dirty = 1 as dirty. Remember the pixel start/end indices  */
	_new = new_list;
	for (i = 0; i < new_index; i++)
	{
		if (_new->status == VCLIP)
			vector_set_clip (_new->x, _new->y, _new->arg1, _new->arg2);
		else
		{
			_new->arg1 = p_index;
			vector_draw_to (_new->x, _new->y, _new->col, Tgamma[_new->intensity], _new->status);

			_new->arg2 = p_index;
		}
		_new++;
	}
}

/*********************************************************************
  Artwork functions.
 *********************************************************************/

/*********************************************************************
  Restore the old bitmap with the artwork (backdrop or overlay).
  MLR 100598
 *********************************************************************/

static void vector_restore_artwork (struct osd_bitmap *bitmap, struct artwork *a, int full_refresh)
{
	int i, x, y;
	struct osd_bitmap *artwork;

	if (full_refresh)
	{
		copybitmap(bitmap, a->artwork ,0,0,0,0,NULL,TRANSPARENCY_NONE,0);
		osd_mark_dirty (0, 0, bitmap->width, bitmap->height, 0);
	}
	else if (pixel)
	{
		artwork = a->artwork;

		for (i=p_index-1; i>=0; i--)
		{
			x = pixel[i] >> 16;
			y = pixel[i] & 0x0000ffff;
			vector_pp (bitmap, x, y, vector_rp(artwork, x, y));
		}
	}
}

/*********************************************************************
  vector_vh_update_backdrop

  This draws a vector screen with backdrop.

  First the backdrop is restored. Then the new vector screen
  is recalculated with vector_vh_update. Changed pixels are
  then merged with the backdrop.
 *********************************************************************/

static void vector_vh_update_backdrop(struct osd_bitmap *bitmap, struct artwork *a, int full_refresh)
{
	int i, x, y;
	unsigned int coords;
	int newcol, bdcol;
	UINT8 r, g, b, rb, gb, bb;
	struct osd_bitmap *vb = a->vector_bitmap;
	struct osd_bitmap *ob = a->orig_artwork;
	struct osd_bitmap *ab = a->artwork;
	UINT8 *tab = a->pTable;
	UINT8 *brightness = a->brightness;
	vector_restore_artwork(bitmap, a, full_refresh);
	vector_vh_update(vb, full_refresh);

	if (bitmap->depth == 8)
		for (i = p_index - 1; i >= 0; i--)
		{
			coords = pixel[i];
			x = coords >> 16;
			y = coords & 0x0000ffff;
			newcol = pens[tab[vector_rp(ob, x, y) * total_colors + invpens[vector_rp(vb, x, y)]]];
			if (brightness[newcol] > brightness[vector_rp(ab, x, y)])
				vector_pp (bitmap, x, y, newcol);
		}
	else
		for (i = p_index - 1; i >= 0; i--)
		{
			coords = pixel[i];
			x = coords >> 16;
			y = coords & 0x0000ffff;

			osd_get_pen (vector_rp(vb, x, y), &r, &g, &b);
			bdcol = vector_rp(ab, x, y);
			osd_get_pen (bdcol, &rb, &gb, &bb);
			r = MIN (255, r + rb / 4);
			g = MIN (255, g + gb / 4);
			b = MIN (255, b + bb / 4);
			newcol = Machine->pens[(((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3)) + a->start_pen];
			if (brightness[newcol] > brightness[bdcol])
				vector_pp (bitmap, x, y, newcol);
		}
}

void vector_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	if (artwork_backdrop)
		vector_vh_update_backdrop(bitmap, artwork_backdrop, full_refresh);
	else
		vector_vh_update(bitmap, full_refresh);
}

#endif /* if !(defined xgl) && !(defined xfx) && !(defined svgafx) */
