/*********************************************************************

  drawgfx.h

  Generic graphic functions.

*********************************************************************/

#ifndef DRAWGFX_H
#define DRAWGFX_H

#define MAX_GFX_PLANES 8
#define MAX_GFX_SIZE 64

#define RGN_FRAC(num,den) (0x80000000 | (((num) & 0x0f) << 27) | (((den) & 0x0f) << 23))
#define IS_FRAC(offset) ((offset) & 0x80000000)
#define FRAC_NUM(offset) (((offset) >> 27) & 0x0f)
#define FRAC_DEN(offset) (((offset) >> 23) & 0x0f)
#define FRAC_OFFSET(offset) ((offset) & 0x007fffff)

#define STEP4(START,STEP)  (START),(START)+1*(STEP),(START)+2*(STEP),(START)+3*(STEP)
#define STEP8(START,STEP)  STEP4(START,STEP),STEP4((START)+4*(STEP),STEP)
#define STEP16(START,STEP) STEP8(START,STEP),STEP8((START)+8*(STEP),STEP)


struct GfxLayout
{
	UINT16 width,height; /* width and height (in pixels) of chars/sprites */
	UINT32 total; /* total numer of chars/sprites in the rom */
	UINT16 planes; /* number of bitplanes */
	UINT32 planeoffset[MAX_GFX_PLANES]; /* start of every bitplane (in bits) */
	UINT32 xoffset[MAX_GFX_SIZE]; /* position of the bit corresponding to the pixel */
	UINT32 yoffset[MAX_GFX_SIZE]; /* of the given coordinates */
	UINT16 charincrement; /* distance between two consecutive characters/sprites (in bits) */
};

struct GfxElement
{
	int width,height;

	unsigned int total_elements;	/* total number of characters/sprites */
	int color_granularity;	/* number of colors for each color code */
							/* (for example, 4 for 2 bitplanes gfx) */
	unsigned short *colortable;	/* map color codes to screen pens */
	int total_colors;
	unsigned int *pen_usage;	/* an array of total_elements ints. */
								/* It is a table of the pens each character uses */
								/* (bit 0 = pen 0, and so on). This is used by */
								/* drawgfgx() to do optimizations like skipping */
								/* drawing of a totally transparent characters */
	unsigned char *gfxdata;	/* pixel data */
	int line_modulo;	/* amount to add to get to the next line (usually = width) */
	int char_modulo;	/* = line_modulo * height */
} __attribute__ ((__aligned__ (32)));

struct GfxDecodeInfo
{
	int memory_region;	/* memory region where the data resides (usually 1) */
						/* -1 marks the end of the array */
	int start;	/* beginning of data to decode */
	struct GfxLayout *gfxlayout;
	int color_codes_start;	/* offset in the color lookup table where color codes start */
	int total_color_codes;	/* total number of color codes */
};


struct rectangle
{
	int min_x,max_x;
	int min_y,max_y;
};


enum
{
	TRANSPARENCY_NONE,			/* opaque with remapping */
	TRANSPARENCY_NONE_RAW,		/* opaque with no remapping */
	TRANSPARENCY_PEN,			/* single pen transparency with remapping */
	TRANSPARENCY_PEN_RAW,		/* single pen transparency with no remapping */
	TRANSPARENCY_PENS,			/* multiple pen transparency with remapping */
	TRANSPARENCY_PENS_RAW,		/* multiple pen transparency with no remapping */
	TRANSPARENCY_COLOR,			/* single remapped pen transparency with remapping */
	TRANSPARENCY_THROUGH,		/* destination pixel overdraw with remapping */
	TRANSPARENCY_THROUGH_RAW,	/* destination pixel overdraw with no remapping */
	TRANSPARENCY_PEN_TABLE,		/* special pen remapping modes (see DRAWMODE_xxx below) with remapping */
	TRANSPARENCY_PEN_TABLE_RAW,	/* special pen remapping modes (see DRAWMODE_xxx below) with no remapping */
	TRANSPARENCY_BLEND,			/* blend two bitmaps, shifting the source and ORing to the dest with remapping */
	TRANSPARENCY_BLEND_RAW,		/* blend two bitmaps, shifting the source and ORing to the dest with no remapping */

	TRANSPARENCY_MODES			/* total number of modes; must be last */
};

/* drawing mode case TRANSPARENCY_PEN_TABLE */
extern UINT8 gfx_drawmode_table[256];
enum
{
	DRAWMODE_NONE,
	DRAWMODE_SOURCE,
	DRAWMODE_SHADOW
};


typedef void (*plot_pixel_proc)(struct osd_bitmap *bitmap,int x,int y,int pen);
typedef int  (*read_pixel_proc)(struct osd_bitmap *bitmap,int x,int y);
typedef void (*plot_box_proc)(struct osd_bitmap *bitmap,int x,int y,int width,int height,int pen);

/* pointers to pixel functions.  They're set based on orientation, depthness and weather
   dirty rectangle handling is enabled */
extern plot_pixel_proc plot_pixel;
extern read_pixel_proc read_pixel;
extern plot_box_proc plot_box;


void decodechar(struct GfxElement *gfx,int num,const unsigned char *src,const struct GfxLayout *gl);
struct GfxElement *decodegfx(const unsigned char *src,const struct GfxLayout *gl);
void set_pixel_functions(void);
void freegfx(struct GfxElement *gfx);
void drawgfx(struct osd_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color);
void pdrawgfx(struct osd_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,
		UINT32 priority_mask);
void copybitmap(struct osd_bitmap *dest,struct osd_bitmap *src,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color);
void copybitmap_remap(struct osd_bitmap *dest,struct osd_bitmap *src,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color);
void copyscrollbitmap(struct osd_bitmap *dest,struct osd_bitmap *src,
		int rows,const int *rowscroll,int cols,const int *colscroll,
		const struct rectangle *clip,int transparency,int transparent_color);
void copyscrollbitmap_remap(struct osd_bitmap *dest,struct osd_bitmap *src,
		int rows,const int *rowscroll,int cols,const int *colscroll,
		const struct rectangle *clip,int transparency,int transparent_color);

/*
  Copy a bitmap applying rotation, zooming, and arbitrary distortion.
  This function works in a way that mimics some real hardware like the Konami
  051316, so it requires little or no further processing on the caller side.

  Two 16.16 fixed point counters are used to keep track of the position on
  the source bitmap. startx and starty are the initial values of those counters,
  indicating the source pixel that will be drawn at coordinates (0,0) in the
  destination bitmap. The destination bitmap is scanned left to right, top to
  bottom; every time the cursor moves one pixel to the right, incxx is added
  to startx and incxy is added to starty. Every time the curso moves to the
  next line, incyx is added to startx and incyy is added to startyy.

  What this means is that if incxy and incyx are both 0, the bitmap will be
  copied with only zoom and no rotation. If e.g. incxx and incyy are both 0x8000,
  the source bitmap will be doubled.

  Rotation is performed this way:
  incxx = 0x10000 * cos(theta)
  incxy = 0x10000 * -sin(theta)
  incyx = 0x10000 * sin(theta)
  incyy = 0x10000 * cos(theta)
  this will perform a rotation around (0,0), you'll have to adjust startx and
  starty to move the center of rotation elsewhere.

  Optionally the bitmap can be tiled across the screen instead of doing a single
  copy. This is obtained by setting the wraparound parameter to true.
 */
void copyrozbitmap(struct osd_bitmap *dest,struct osd_bitmap *src,
		UINT32 startx,UINT32 starty,int incxx,int incxy,int incyx,int incyy,int wraparound,
		const struct rectangle *clip,int transparency,int transparent_color,UINT32 priority);

void fillbitmap(struct osd_bitmap *dest,int pen,const struct rectangle *clip);
void plot_pixel2(struct osd_bitmap *bitmap1,struct osd_bitmap *bitmap2,int x,int y,int pen);
void drawgfxzoom( struct osd_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,int scalex,int scaley);
void pdrawgfxzoom( struct osd_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,int scalex,int scaley,
		UINT32 priority_mask);

#endif
