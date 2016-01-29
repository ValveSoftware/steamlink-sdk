/*********************************************************************

  artwork.h

  Generic backdrop/overlay functions.

  Be sure to include driver.h before including this file.

  Created by Mike Balfour - 10/01/1998
*********************************************************************/

#ifndef ARTWORK_H

#define ARTWORK_H 1

/*********************************************************************
  artwork

  This structure is a generic structure used to hold both backdrops
  and overlays.
*********************************************************************/

struct artwork
{
	/* Publically accessible */
	struct osd_bitmap *artwork;
	struct osd_bitmap *artwork1;
	struct osd_bitmap *alpha;
	struct osd_bitmap *orig_artwork;   /* needed for palette recalcs */
	struct osd_bitmap *vector_bitmap;  /* needed to buffer the vector image in vg with overlays */
	UINT8 *orig_palette;               /* needed for restoring the colors after special effects? */
	int num_pens_used;
	UINT8 *transparency;
	int num_pens_trans;
	int start_pen;
	UINT8 *brightness;                 /* brightness of each palette entry */
	UINT64 *rgb;
	UINT8 *pTable;                     /* Conversion table usually used for mixing colors */
};


struct artwork_element
{
	struct rectangle box;
	UINT8 red,green,blue;
	UINT16 alpha;   /* 0x00-0xff or OVERLAY_DEFAULT_OPACITY */
};

#define OVERLAY_DEFAULT_OPACITY         0xffff

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif
#ifndef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#endif

extern struct artwork *artwork_backdrop;
extern struct artwork *artwork_overlay;
extern struct osd_bitmap *overlay_real_scrbitmap;

/*********************************************************************
  functions that apply to backdrops AND overlays
*********************************************************************/
void overlay_load(const char *filename, unsigned int start_pen, unsigned int max_pens);
void overlay_create(const struct artwork_element *ae, unsigned int start_pen, unsigned int max_pens);
void backdrop_load(const char *filename, unsigned int start_pen, unsigned int max_pens);
void artwork_load(struct artwork **a,const char *filename, unsigned int start_pen, unsigned int max_pens);
void artwork_load_size(struct artwork **a,const char *filename, unsigned int start_pen, unsigned int max_pens, int width, int height);
void artwork_elements_scale(struct artwork_element *ae, int width, int height);
void artwork_free(struct artwork **a);

/*********************************************************************
  functions that are backdrop-specific
*********************************************************************/
void backdrop_refresh(struct artwork *a);
void backdrop_refresh_tables (struct artwork *a);
void backdrop_set_palette(struct artwork *a, unsigned char *palette);
int backdrop_black_recalc(void);
void draw_backdrop(struct osd_bitmap *dest,const struct osd_bitmap *src,int sx,int sy,
		   const struct rectangle *clip);
void drawgfx_backdrop(struct osd_bitmap *dest,const struct GfxElement *gfx,
		      unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		      const struct rectangle *clip,const struct osd_bitmap *back);

/*********************************************************************
  functions that are overlay-specific
*********************************************************************/
int overlay_set_palette (unsigned char *palette, int num_shades);

#endif

