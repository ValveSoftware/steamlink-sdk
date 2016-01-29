#ifndef DECLARE

#include "driver.h"


/* LBO */
#ifdef LSB_FIRST
#define BL0 0
#define BL1 1
#define BL2 2
#define BL3 3
#define WL0 0
#define WL1 1
#else
#define BL0 3
#define BL1 2
#define BL2 1
#define BL3 0
#define WL0 1
#define WL1 0
#endif


UINT8 gfx_drawmode_table[256];
plot_pixel_proc plot_pixel;
read_pixel_proc read_pixel;
plot_box_proc plot_box;

static UINT8 is_raw[TRANSPARENCY_MODES];


#ifdef ALIGN_INTS /* GSL 980108 read/write nonaligned dword routine for ARM processor etc */
	#define write_dword_aligned(address,data) *(UINT32 *)address = data
	#ifdef LSB_FIRST
		#define write_dword_unaligned(address,data) \
			*(address) =    data; \
			*(address+1) = (data >> 8); \
			*(address+2) = (data >> 16); \
			*(address+3) = (data >> 24)
	#else
		#define write_dword_unaligned(address,data) \
			*(address+3) =  data; \
			*(address+2) = (data >> 8); \
			*(address+1) = (data >> 16); \
			*(address)   = (data >> 24)
	#endif
#else
#define write_dword(address,data) *address=data
#endif



INLINE int readbit(const UINT8 *src,int bitnum)
{
	return (src[bitnum / 8] >> (7 - bitnum % 8)) & 1;
}


void decodechar(struct GfxElement *gfx,int num,const UINT8 *src,const struct GfxLayout *gl)
{
	int plane,x,y;
	UINT8 *dp;
	int offs;


	offs = num * gl->charincrement;
	dp = gfx->gfxdata + num * gfx->char_modulo;
	for (y = 0;y < gfx->height;y++)
	{
		int yoffs;

		yoffs = y;
#ifdef PREROTATE_GFX
		if (Machine->orientation & ORIENTATION_FLIP_Y)
			yoffs = gfx->height-1 - yoffs;
#endif

		for (x = 0;x < gfx->width;x++)
		{
			int xoffs;

			xoffs = x;
#ifdef PREROTATE_GFX
			if (Machine->orientation & ORIENTATION_FLIP_X)
				xoffs = gfx->width-1 - xoffs;
#endif

			dp[x] = 0;
			if (Machine->orientation & ORIENTATION_SWAP_XY)
			{
				for (plane = 0;plane < gl->planes;plane++)
				{
					if (readbit(src,offs + gl->planeoffset[plane] + gl->yoffset[xoffs] + gl->xoffset[yoffs]))
						dp[x] |= (1 << (gl->planes-1-plane));
				}
			}
			else
			{
				for (plane = 0;plane < gl->planes;plane++)
				{
					if (readbit(src,offs + gl->planeoffset[plane] + gl->yoffset[yoffs] + gl->xoffset[xoffs]))
						dp[x] |= (1 << (gl->planes-1-plane));
				}
			}
		}
		dp += gfx->line_modulo;
	}


	if (gfx->pen_usage)
	{
		/* fill the pen_usage array with info on the used pens */
		gfx->pen_usage[num] = 0;

		dp = gfx->gfxdata + num * gfx->char_modulo;
		for (y = 0;y < gfx->height;y++)
		{
			for (x = 0;x < gfx->width;x++)
			{
				gfx->pen_usage[num] |= 1 << dp[x];
			}
			dp += gfx->line_modulo;
		}
	}
}


struct GfxElement *decodegfx(const UINT8 *src,const struct GfxLayout *gl)
{
	int c;
	struct GfxElement *gfx;


	if ((gfx = (GfxElement *) malloc(sizeof(struct GfxElement))) == 0)
		return 0;
	memset(gfx,0,sizeof(struct GfxElement));

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		gfx->width = gl->height;
		gfx->height = gl->width;
	}
	else
	{
		gfx->width = gl->width;
		gfx->height = gl->height;
	}

	gfx->line_modulo = gfx->width;
	gfx->char_modulo = gfx->line_modulo * gfx->height;
	if ((gfx->gfxdata = (unsigned char *) malloc(gl->total * gfx->char_modulo * sizeof(UINT8))) == 0)
	{
		free(gfx);
		return 0;
	}

	gfx->total_elements = gl->total;
	gfx->color_granularity = 1 << gl->planes;

	gfx->pen_usage = 0; /* need to make sure this is NULL if the next test fails) */
	if (gfx->color_granularity <= 32)	/* can't handle more than 32 pens */
		gfx->pen_usage = (unsigned int *) malloc(gfx->total_elements * sizeof(int));
		/* no need to check for failure, the code can work without pen_usage */

	for (c = 0;c < gl->total;c++)
		decodechar(gfx,c,src,gl);

	return gfx;
}


void freegfx(struct GfxElement *gfx)
{
	if (gfx)
	{
		free(gfx->pen_usage);
		free(gfx->gfxdata);
		free(gfx);
	}
}




INLINE void blockmove_NtoN_transpen_noremap8(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		UINT8 *dstdata,int dstmodulo,
		int transpen)
{
	UINT8 *end;
	int trans4;
	UINT32 *sd4;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	trans4 = transpen * 0x01010101;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = col;
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;

#ifdef ALIGN_INTS /* GSL 980108 read/write nonaligned dword routine for ARM processor etc */

		if ((long)dstdata & 3)
		{
			while (dstdata <= end - 4)
			{
				UINT32 col4;
	
				if ((col4 = *(sd4++)) != trans4)
				{
					UINT32 xod4;
	
					xod4 = col4 ^ trans4;
					if( (xod4&0x000000ff) && (xod4&0x0000ff00) &&
						(xod4&0x00ff0000) && (xod4&0xff000000) )
					{
						write_dword_unaligned(dstdata,col4);
					}
					else
					{
						if (xod4 & 0xff000000) dstdata[BL3] = col4 >> 24;
						if (xod4 & 0x00ff0000) dstdata[BL2] = col4 >> 16;
						if (xod4 & 0x0000ff00) dstdata[BL1] = col4 >>  8;
						if (xod4 & 0x000000ff) dstdata[BL0] = col4;
					}
				}
				dstdata += 4;
			}
		}
		else
		{
			while (dstdata <= end - 4)
			{
				UINT32 col4;
	
				if ((col4 = *(sd4++)) != trans4)
				{
					UINT32 xod4;
	
					xod4 = col4 ^ trans4;
					if( (xod4&0x000000ff) && (xod4&0x0000ff00) &&
						(xod4&0x00ff0000) && (xod4&0xff000000) )
					{
						write_dword_aligned((UINT32 *)dstdata,col4);
					}
					else
					{
						if (xod4 & 0xff000000) dstdata[BL3] = col4 >> 24;
						if (xod4 & 0x00ff0000) dstdata[BL2] = col4 >> 16;
						if (xod4 & 0x0000ff00) dstdata[BL1] = col4 >>  8;
						if (xod4 & 0x000000ff) dstdata[BL0] = col4;
					}
				}
				dstdata += 4;
			}
		}
#else
		while (dstdata <= end - 4)
		{
			UINT32 col4;

			if ((col4 = *(sd4++)) != trans4)
			{
				UINT32 xod4;

				xod4 = col4 ^ trans4;
				if( (xod4&0x000000ff) && (xod4&0x0000ff00) &&
					(xod4&0x00ff0000) && (xod4&0xff000000) )
				{
					write_dword((UINT32 *)dstdata,col4);
				}
				else
				{
					if (xod4 & 0xff000000) dstdata[BL3] = col4 >> 24;
					if (xod4 & 0x00ff0000) dstdata[BL2] = col4 >> 16;
					if (xod4 & 0x0000ff00) dstdata[BL1] = col4 >>  8;
					if (xod4 & 0x000000ff) dstdata[BL0] = col4;
				}
			}
			dstdata += 4;
		}
#endif
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
}

INLINE void blockmove_NtoN_transpen_noremap_flipx8(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		UINT8 *dstdata,int dstmodulo,
		int transpen)
{
	UINT8 *end;
	int trans4;
	UINT32 *sd4;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;
	srcdata -= 3;

	trans4 = transpen * 0x01010101;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (col != transpen) *dstdata = col;
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			UINT32 col4;

			if ((col4 = *(sd4--)) != trans4)
			{
				UINT32 xod4;

				xod4 = col4 ^ trans4;
				if (xod4 & 0x000000ff) dstdata[BL3] = col4;
				if (xod4 & 0x0000ff00) dstdata[BL2] = col4 >>  8;
				if (xod4 & 0x00ff0000) dstdata[BL1] = col4 >> 16;
				if (xod4 & 0xff000000) dstdata[BL0] = col4 >> 24;
			}
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (col != transpen) *dstdata = col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
}


INLINE void blockmove_NtoN_transpen_noremap16(
		const UINT16 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		UINT16 *dstdata,int dstmodulo,
		int transpen)
{
	UINT16 *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
}

INLINE void blockmove_NtoN_transpen_noremap_flipx16(
		const UINT16 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		UINT16 *dstdata,int dstmodulo,
		int transpen)
{
	UINT16 *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata--);
			if (col != transpen) *dstdata = col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
}


#define DATA_TYPE UINT8
#define DECLARE(function,args,body) INLINE void function##8 args body
#define BLOCKMOVE(function,flipx,args) \
	if (flipx) blockmove_##function##_flipx##8 args ; \
	else blockmove_##function##8 args
#include "drawgfx.cpp"
#undef DATA_TYPE
#undef DECLARE
#undef BLOCKMOVE

#define DATA_TYPE UINT16
#define DECLARE(function,args,body) INLINE void function##16 args body
#define BLOCKMOVE(function,flipx,args) \
	if (flipx) blockmove_##function##_flipx##16 args ; \
	else blockmove_##function##16 args
#include "drawgfx.cpp"
#undef DATA_TYPE
#undef DECLARE
#undef BLOCKMOVE


/***************************************************************************

  Draw graphic elements in the specified bitmap.

  transparency == TRANSPARENCY_NONE - no transparency.
  transparency == TRANSPARENCY_PEN - bits whose _original_ value is == transparent_color
                                     are transparent. This is the most common kind of
									 transparency.
  transparency == TRANSPARENCY_PENS - as above, but transparent_color is a mask of
  									 transparent pens.
  transparency == TRANSPARENCY_COLOR - bits whose _remapped_ palette index (taken from
                                     Machine->game_colortable) is == transparent_color
  transparency == TRANSPARENCY_THROUGH - if the _destination_ pixel is == transparent_color,
                                     the source pixel is drawn over it. This is used by
									 e.g. Jr. Pac Man to draw the sprites when the background
									 has priority over them.

  transparency == TRANSPARENCY_PEN_TABLE - the transparency condition is same as TRANSPARENCY_PEN
					A special drawing is done according to gfx_drawmode_table[source pixel].
					DRAWMODE_NONE      transparent
					DRAWMODE_SOURCE    normal, draw source pixel.
					DRAWMODE_SHADOW    destination is changed through palette_shadow_table[]

***************************************************************************/

INLINE void common_drawgfx(struct osd_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,
		struct osd_bitmap *pri_buffer,UINT32 pri_mask)
{
	struct rectangle myclip;

	if (!gfx)
	{
		usrintf_showmessage("drawgfx() gfx == 0");
		return;
	}
	if (!gfx->colortable && !is_raw[transparency])
	{
		usrintf_showmessage("drawgfx() gfx->colortable == 0");
		return;
	}

	code %= gfx->total_elements;
	if (!is_raw[transparency])
		color %= gfx->total_colors;

	if (gfx->pen_usage && (transparency == TRANSPARENCY_PEN || transparency == TRANSPARENCY_PENS))
	{
		int transmask = 0;

		if (transparency == TRANSPARENCY_PEN)
		{
			transmask = 1 << transparent_color;
		}
		else	/* transparency == TRANSPARENCY_PENS */
		{
			transmask = transparent_color;
		}

		if ((gfx->pen_usage[code] & ~transmask) == 0)
			/* character is totally transparent, no need to draw */
			return;
		else if ((gfx->pen_usage[code] & transmask) == 0)
			/* character is totally opaque, can disable transparency */
			transparency = TRANSPARENCY_NONE;
	}

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
#ifndef PREROTATE_GFX
		flipx = !flipx;
#endif
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
#ifndef PREROTATE_GFX
		flipy = !flipy;
#endif
	}

	if (dest->depth != 16)
		drawgfx_core8(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,pri_buffer,pri_mask);
	else
		drawgfx_core16(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,pri_buffer,pri_mask);
}

void drawgfx(struct osd_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	profiler_mark(PROFILER_DRAWGFX);
	common_drawgfx(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,NULL,0);
	profiler_mark(PROFILER_END);
}

void pdrawgfx(struct osd_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,UINT32 priority_mask)
{
	profiler_mark(PROFILER_DRAWGFX);
	common_drawgfx(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,priority_bitmap,priority_mask);
	profiler_mark(PROFILER_END);
}


/***************************************************************************

  Use drawgfx() to copy a bitmap onto another at the given position.
  This function will very likely change in the future.

***************************************************************************/
void copybitmap(struct osd_bitmap *dest,struct osd_bitmap *src,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	/* translate to proper transparency here */
	if (transparency == TRANSPARENCY_NONE)
		transparency = TRANSPARENCY_NONE_RAW;
	else if (transparency == TRANSPARENCY_PEN)
		transparency = TRANSPARENCY_PEN_RAW;
	else if (transparency == TRANSPARENCY_COLOR)
	{
		transparent_color = Machine->pens[transparent_color];
		transparency = TRANSPARENCY_PEN_RAW;
	}
	else if (transparency == TRANSPARENCY_THROUGH)
		transparency = TRANSPARENCY_THROUGH_RAW;

	copybitmap_remap(dest,src,flipx,flipy,sx,sy,clip,transparency,transparent_color);
}


void copybitmap_remap(struct osd_bitmap *dest,struct osd_bitmap *src,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	struct rectangle myclip;


	profiler_mark(PROFILER_COPYBITMAP);

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

	if (dest->depth != 16)
		copybitmap_core8(dest,src,flipx,flipy,sx,sy,clip,transparency,transparent_color);
	else
		copybitmap_core16(dest,src,flipx,flipy,sx,sy,clip,transparency,transparent_color);

	profiler_mark(PROFILER_END);
}



/***************************************************************************

  Copy a bitmap onto another with scroll and wraparound.
  This function supports multiple independently scrolling rows/columns.
  "rows" is the number of indepentently scrolling rows. "rowscroll" is an
  array of integers telling how much to scroll each row. Same thing for
  "cols" and "colscroll".
  If the bitmap cannot scroll in one direction, set rows or columns to 0.
  If the bitmap scrolls as a whole, set rows and/or cols to 1.
  Bidirectional scrolling is, of course, supported only if the bitmap
  scrolls as a whole in at least one direction.

***************************************************************************/
void copyscrollbitmap(struct osd_bitmap *dest,struct osd_bitmap *src,
		int rows,const int *rowscroll,int cols,const int *colscroll,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	/* translate to proper transparency here */
	if (transparency == TRANSPARENCY_NONE)
		transparency = TRANSPARENCY_NONE_RAW;
	else if (transparency == TRANSPARENCY_PEN)
		transparency = TRANSPARENCY_PEN_RAW;
	else if (transparency == TRANSPARENCY_COLOR)
	{
		transparent_color = Machine->pens[transparent_color];
		transparency = TRANSPARENCY_PEN_RAW;
	}
	else if (transparency == TRANSPARENCY_THROUGH)
		transparency = TRANSPARENCY_THROUGH_RAW;

	copyscrollbitmap_remap(dest,src,rows,rowscroll,cols,colscroll,clip,transparency,transparent_color);
}

void copyscrollbitmap_remap(struct osd_bitmap *dest,struct osd_bitmap *src,
		int rows,const int *rowscroll,int cols,const int *colscroll,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	int srcwidth,srcheight,destwidth,destheight;


	if (rows == 0 && cols == 0)
	{
		copybitmap(dest,src,0,0,0,0,clip,transparency,transparent_color);
		return;
	}

	profiler_mark(PROFILER_COPYBITMAP);

	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		srcwidth = src->height;
		srcheight = src->width;
		destwidth = dest->height;
		destheight = dest->width;
	}
	else
	{
		srcwidth = src->width;
		srcheight = src->height;
		destwidth = dest->width;
		destheight = dest->height;
	}

	if (rows == 0)
	{
		/* scrolling columns */
		int col,colwidth;
		struct rectangle myclip;


		colwidth = srcwidth / cols;

		myclip.min_y = clip->min_y;
		myclip.max_y = clip->max_y;

		col = 0;
		while (col < cols)
		{
			int cons,scroll;


			/* count consecutive columns scrolled by the same amount */
			scroll = colscroll[col];
			cons = 1;
			while (col + cons < cols &&	colscroll[col + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcheight - (-scroll) % srcheight;
			else scroll %= srcheight;

			myclip.min_x = col * colwidth;
			if (myclip.min_x < clip->min_x) myclip.min_x = clip->min_x;
			myclip.max_x = (col + cons) * colwidth - 1;
			if (myclip.max_x > clip->max_x) myclip.max_x = clip->max_x;

			copybitmap(dest,src,0,0,0,scroll,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,0,scroll - srcheight,&myclip,transparency,transparent_color);

			col += cons;
		}
	}
	else if (cols == 0)
	{
		/* scrolling rows */
		int row,rowheight;
		struct rectangle myclip;


		rowheight = srcheight / rows;

		myclip.min_x = clip->min_x;
		myclip.max_x = clip->max_x;

		row = 0;
		while (row < rows)
		{
			int cons,scroll;


			/* count consecutive rows scrolled by the same amount */
			scroll = rowscroll[row];
			cons = 1;
			while (row + cons < rows &&	rowscroll[row + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcwidth - (-scroll) % srcwidth;
			else scroll %= srcwidth;

			myclip.min_y = row * rowheight;
			if (myclip.min_y < clip->min_y) myclip.min_y = clip->min_y;
			myclip.max_y = (row + cons) * rowheight - 1;
			if (myclip.max_y > clip->max_y) myclip.max_y = clip->max_y;

			copybitmap(dest,src,0,0,scroll,0,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scroll - srcwidth,0,&myclip,transparency,transparent_color);

			row += cons;
		}
	}
	else if (rows == 1 && cols == 1)
	{
		/* XY scrolling playfield */
		int scrollx,scrolly,sx,sy;


		if (rowscroll[0] < 0) scrollx = srcwidth - (-rowscroll[0]) % srcwidth;
		else scrollx = rowscroll[0] % srcwidth;

		if (colscroll[0] < 0) scrolly = srcheight - (-colscroll[0]) % srcheight;
		else scrolly = colscroll[0] % srcheight;

		for (sx = scrollx - srcwidth;sx < destwidth;sx += srcwidth)
			for (sy = scrolly - srcheight;sy < destheight;sy += srcheight)
				copybitmap(dest,src,0,0,sx,sy,clip,transparency,transparent_color);
	}
	else if (rows == 1)
	{
		/* scrolling columns + horizontal scroll */
		int col,colwidth;
		int scrollx;
		struct rectangle myclip;


		if (rowscroll[0] < 0) scrollx = srcwidth - (-rowscroll[0]) % srcwidth;
		else scrollx = rowscroll[0] % srcwidth;

		colwidth = srcwidth / cols;

		myclip.min_y = clip->min_y;
		myclip.max_y = clip->max_y;

		col = 0;
		while (col < cols)
		{
			int cons,scroll;


			/* count consecutive columns scrolled by the same amount */
			scroll = colscroll[col];
			cons = 1;
			while (col + cons < cols &&	colscroll[col + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcheight - (-scroll) % srcheight;
			else scroll %= srcheight;

			myclip.min_x = col * colwidth + scrollx;
			if (myclip.min_x < clip->min_x) myclip.min_x = clip->min_x;
			myclip.max_x = (col + cons) * colwidth - 1 + scrollx;
			if (myclip.max_x > clip->max_x) myclip.max_x = clip->max_x;

			copybitmap(dest,src,0,0,scrollx,scroll,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scrollx,scroll - srcheight,&myclip,transparency,transparent_color);

			myclip.min_x = col * colwidth + scrollx - srcwidth;
			if (myclip.min_x < clip->min_x) myclip.min_x = clip->min_x;
			myclip.max_x = (col + cons) * colwidth - 1 + scrollx - srcwidth;
			if (myclip.max_x > clip->max_x) myclip.max_x = clip->max_x;

			copybitmap(dest,src,0,0,scrollx - srcwidth,scroll,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scrollx - srcwidth,scroll - srcheight,&myclip,transparency,transparent_color);

			col += cons;
		}
	}
	else if (cols == 1)
	{
		/* scrolling rows + vertical scroll */
		int row,rowheight;
		int scrolly;
		struct rectangle myclip;


		if (colscroll[0] < 0) scrolly = srcheight - (-colscroll[0]) % srcheight;
		else scrolly = colscroll[0] % srcheight;

		rowheight = srcheight / rows;

		myclip.min_x = clip->min_x;
		myclip.max_x = clip->max_x;

		row = 0;
		while (row < rows)
		{
			int cons,scroll;


			/* count consecutive rows scrolled by the same amount */
			scroll = rowscroll[row];
			cons = 1;
			while (row + cons < rows &&	rowscroll[row + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcwidth - (-scroll) % srcwidth;
			else scroll %= srcwidth;

			myclip.min_y = row * rowheight + scrolly;
			if (myclip.min_y < clip->min_y) myclip.min_y = clip->min_y;
			myclip.max_y = (row + cons) * rowheight - 1 + scrolly;
			if (myclip.max_y > clip->max_y) myclip.max_y = clip->max_y;

			copybitmap(dest,src,0,0,scroll,scrolly,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scroll - srcwidth,scrolly,&myclip,transparency,transparent_color);

			myclip.min_y = row * rowheight + scrolly - srcheight;
			if (myclip.min_y < clip->min_y) myclip.min_y = clip->min_y;
			myclip.max_y = (row + cons) * rowheight - 1 + scrolly - srcheight;
			if (myclip.max_y > clip->max_y) myclip.max_y = clip->max_y;

			copybitmap(dest,src,0,0,scroll,scrolly - srcheight,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scroll - srcwidth,scrolly - srcheight,&myclip,transparency,transparent_color);

			row += cons;
		}
	}

	profiler_mark(PROFILER_END);
}


/* notes:
   - startx and starty MUST be UINT32 for calculations to work correctly
   - srcbitmap->width and height are assumed to be a power of 2 to speed up wraparound
   */
void copyrozbitmap(struct osd_bitmap *dest,struct osd_bitmap *src,
		UINT32 startx,UINT32 starty,int incxx,int incxy,int incyx,int incyy,int wraparound,
		const struct rectangle *clip,int transparency,int transparent_color,UINT32 priority)
{
	profiler_mark(PROFILER_COPYBITMAP);

	/* cheat, the core doesn't support TRANSPARENCY_NONE yet */
	if (transparency == TRANSPARENCY_NONE)
	{
		transparency = TRANSPARENCY_PEN;
		transparent_color = -1;
	}

	/* if necessary, remap the transparent color */
	if (transparency == TRANSPARENCY_COLOR)
	{
		transparency = TRANSPARENCY_PEN;
		transparent_color = Machine->pens[transparent_color];
	}

	if (transparency != TRANSPARENCY_PEN)
	{
		usrintf_showmessage("copyrozbitmap unsupported trans %02x",transparency);
		return;
	}

	if (dest->depth != 16)
		copyrozbitmap_core8(dest,src,startx,starty,incxx,incxy,incyx,incyy,wraparound,clip,transparency,transparent_color,priority);
	else
		copyrozbitmap_core16(dest,src,startx,starty,incxx,incxy,incyx,incyy,wraparound,clip,transparency,transparent_color,priority);

	profiler_mark(PROFILER_END);
}



/* fill a bitmap using the specified pen */
void fillbitmap(struct osd_bitmap *dest,int pen,const struct rectangle *clip)
{
	int sx,sy,ex,ey,y;
	struct rectangle myclip;


	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		if (clip)
		{
			myclip.min_x = clip->min_y;
			myclip.max_x = clip->max_y;
			myclip.min_y = clip->min_x;
			myclip.max_y = clip->max_x;
			clip = &myclip;
		}
	}
	if (Machine->orientation & ORIENTATION_FLIP_X)
	{
		if (clip)
		{
			int temp;


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
		if (clip)
		{
			int temp;


			myclip.min_x = clip->min_x;
			myclip.max_x = clip->max_x;
			temp = clip->min_y;
			myclip.min_y = dest->height-1 - clip->max_y;
			myclip.max_y = dest->height-1 - temp;
			clip = &myclip;
		}
	}


	sx = 0;
	ex = dest->width - 1;
	sy = 0;
	ey = dest->height - 1;

	if (clip && sx < clip->min_x) sx = clip->min_x;
	if (clip && ex > clip->max_x) ex = clip->max_x;
	if (sx > ex) return;
	if (clip && sy < clip->min_y) sy = clip->min_y;
	if (clip && ey > clip->max_y) ey = clip->max_y;
	if (sy > ey) return;

	osd_mark_dirty (sx,sy,ex,ey,0);	/* ASG 971011 */

	/* ASG 980211 */
	if (dest->depth == 16)
	{
		if ((pen >> 8) == (pen & 0xff))
		{
			for (y = sy;y <= ey;y++)
				memset(&dest->line[y][sx*2],pen&0xff,(ex-sx+1)*2);
		}
		else
		{
			UINT16 *sp = (UINT16 *)dest->line[sy];
			int x;

			for (x = sx;x <= ex;x++)
				sp[x] = pen;
			sp+=sx;
			for (y = sy+1;y <= ey;y++)
				memcpy(&dest->line[y][sx*2],sp,(ex-sx+1)*2);
		}
	}
	else
	{
		for (y = sy;y <= ey;y++)
			memset(&dest->line[y][sx],pen,ex-sx+1);
	}
}


INLINE void common_drawgfxzoom( struct osd_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,
		int scalex, int scaley,struct osd_bitmap *pri_buffer,UINT32 pri_mask)
{
	struct rectangle myclip;


	if (!scalex || !scaley) return;

	if (scalex == 0x10000 && scaley == 0x10000)
	{
		common_drawgfx(dest_bmp,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,pri_buffer,pri_mask);
		return;
	}


	pri_mask |= (1<<31);

	if (transparency != TRANSPARENCY_PEN && transparency != TRANSPARENCY_PEN_RAW
			&& transparency != TRANSPARENCY_PENS && transparency != TRANSPARENCY_COLOR
			&& transparency != TRANSPARENCY_PEN_TABLE && transparency != TRANSPARENCY_PEN_TABLE_RAW)
	{
		usrintf_showmessage("drawgfxzoom unsupported trans %02x",transparency);
		return;
	}

	if (transparency == TRANSPARENCY_COLOR)
		transparent_color = Machine->pens[transparent_color];


	/*
	scalex and scaley are 16.16 fixed point numbers
	1<<15 : shrink to 50%
	1<<16 : uniform scale
	1<<17 : double to 200%
	*/


	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int temp;

		temp = sx;
		sx = sy;
		sy = temp;

		temp = flipx;
		flipx = flipy;
		flipy = temp;

		temp = scalex;
		scalex = scaley;
		scaley = temp;

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
		sx = dest_bmp->width - ((gfx->width * scalex + 0x7fff) >> 16) - sx;
		if (clip)
		{
			int temp;


			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_x;
			myclip.min_x = dest_bmp->width-1 - clip->max_x;
			myclip.max_x = dest_bmp->width-1 - temp;
			myclip.min_y = clip->min_y;
			myclip.max_y = clip->max_y;
			clip = &myclip;
		}
#ifndef PREROTATE_GFX
		flipx = !flipx;
#endif
	}
	if (Machine->orientation & ORIENTATION_FLIP_Y)
	{
		sy = dest_bmp->height - ((gfx->height * scaley + 0x7fff) >> 16) - sy;
		if (clip)
		{
			int temp;


			myclip.min_x = clip->min_x;
			myclip.max_x = clip->max_x;
			/* clip and myclip might be the same, so we need a temporary storage */
			temp = clip->min_y;
			myclip.min_y = dest_bmp->height-1 - clip->max_y;
			myclip.max_y = dest_bmp->height-1 - temp;
			clip = &myclip;
		}
#ifndef PREROTATE_GFX
		flipy = !flipy;
#endif
	}

	/* KW 991012 -- Added code to force clip to bitmap boundary */
	if(clip)
	{
		myclip.min_x = clip->min_x;
		myclip.max_x = clip->max_x;
		myclip.min_y = clip->min_y;
		myclip.max_y = clip->max_y;

		if (myclip.min_x < 0) myclip.min_x = 0;
		if (myclip.max_x >= dest_bmp->width) myclip.max_x = dest_bmp->width-1;
		if (myclip.min_y < 0) myclip.min_y = 0;
		if (myclip.max_y >= dest_bmp->height) myclip.max_y = dest_bmp->height-1;

		clip=&myclip;
	}


	/* ASG 980209 -- added 16-bit version */
	if (dest_bmp->depth != 16)
	{
		if( gfx && gfx->colortable )
		{
			const UINT16 *pal = &gfx->colortable[gfx->color_granularity * (color % gfx->total_colors)]; /* ASG 980209 */
			int source_base = (code % gfx->total_elements) * gfx->height;

			int sprite_screen_height = (scaley*gfx->height+0x8000)>>16;
			int sprite_screen_width = (scalex*gfx->width+0x8000)>>16;

			/* compute sprite increment per screen pixel */
			int dx = (gfx->width<<16)/sprite_screen_width;
			int dy = (gfx->height<<16)/sprite_screen_height;

			int ex = sx+sprite_screen_width;
			int ey = sy+sprite_screen_height;

			int x_index_base;
			int y_index;

			if( flipx )
			{
				x_index_base = (sprite_screen_width-1)*dx;
				dx = -dx;
			}
			else
			{
				x_index_base = 0;
			}

			if( flipy )
			{
				y_index = (sprite_screen_height-1)*dy;
				dy = -dy;
			}
			else
			{
				y_index = 0;
			}

			if( clip )
			{
				if( sx < clip->min_x)
				{ /* clip left */
					int pixels = clip->min_x-sx;
					sx += pixels;
					x_index_base += pixels*dx;
				}
				if( sy < clip->min_y )
				{ /* clip top */
					int pixels = clip->min_y-sy;
					sy += pixels;
					y_index += pixels*dy;
				}
				/* NS 980211 - fixed incorrect clipping */
				if( ex > clip->max_x+1 )
				{ /* clip right */
					int pixels = ex-clip->max_x-1;
					ex -= pixels;
				}
				if( ey > clip->max_y+1 )
				{ /* clip bottom */
					int pixels = ey-clip->max_y-1;
					ey -= pixels;
				}
			}

			if( ex>sx )
			{ /* skip if inner loop doesn't draw anything */
				int y;

				/* case 1: TRANSPARENCY_PEN */
				if (transparency == TRANSPARENCY_PEN)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
										dest[x] = pal[c];
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color ) dest[x] = pal[c];
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}

				/* case 1b: TRANSPARENCY_PEN_RAW */
				if (transparency == TRANSPARENCY_PEN_RAW)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
										dest[x] = color + c;
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color ) dest[x] = color + c;
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}

				/* case 2: TRANSPARENCY_PENS */
				if (transparency == TRANSPARENCY_PENS)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if (((1 << c) & transparent_color) == 0)
								{
									if (((1 << pri[x]) & pri_mask) == 0)
										dest[x] = pal[c];
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if (((1 << c) & transparent_color) == 0)
									dest[x] = pal[c];
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}

				/* case 3: TRANSPARENCY_COLOR */
				else if (transparency == TRANSPARENCY_COLOR)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = pal[source[x_index>>16]];
								if( c != transparent_color )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
										dest[x] = c;
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = pal[source[x_index>>16]];
								if( c != transparent_color ) dest[x] = c;
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}

				/* case 4: TRANSPARENCY_PEN_TABLE */
				if (transparency == TRANSPARENCY_PEN_TABLE)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											dest[x] = pal[c];
											break;
										case DRAWMODE_SHADOW:
											dest[x] = palette_shadow_table[dest[x]];
											break;
										}
									}
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									switch(gfx_drawmode_table[c])
									{
									case DRAWMODE_SOURCE:
										dest[x] = pal[c];
										break;
									case DRAWMODE_SHADOW:
										dest[x] = palette_shadow_table[dest[x]];
										break;
									}
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}

				/* case 4b: TRANSPARENCY_PEN_TABLE_RAW */
				if (transparency == TRANSPARENCY_PEN_TABLE_RAW)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											dest[x] = color + c;
											break;
										case DRAWMODE_SHADOW:
											dest[x] = palette_shadow_table[dest[x]];
											break;
										}
									}
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT8 *dest = dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									switch(gfx_drawmode_table[c])
									{
									case DRAWMODE_SOURCE:
										dest[x] = color + c;
										break;
									case DRAWMODE_SHADOW:
										dest[x] = palette_shadow_table[dest[x]];
										break;
									}
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}
			}
		}
	}

	/* ASG 980209 -- new 16-bit part */
	else
	{
		if( gfx && gfx->colortable )
		{
			const UINT16 *pal = &gfx->colortable[gfx->color_granularity * (color % gfx->total_colors)]; /* ASG 980209 */
			int source_base = (code % gfx->total_elements) * gfx->height;

			int sprite_screen_height = (scaley*gfx->height+0x8000)>>16;
			int sprite_screen_width = (scalex*gfx->width+0x8000)>>16;

			/* compute sprite increment per screen pixel */
			int dx = (gfx->width<<16)/sprite_screen_width;
			int dy = (gfx->height<<16)/sprite_screen_height;

			int ex = sx+sprite_screen_width;
			int ey = sy+sprite_screen_height;

			int x_index_base;
			int y_index;

			if( flipx )
			{
				x_index_base = (sprite_screen_width-1)*dx;
				dx = -dx;
			}
			else
			{
				x_index_base = 0;
			}

			if( flipy )
			{
				y_index = (sprite_screen_height-1)*dy;
				dy = -dy;
			}
			else
			{
				y_index = 0;
			}

			if( clip )
			{
				if( sx < clip->min_x)
				{ /* clip left */
					int pixels = clip->min_x-sx;
					sx += pixels;
					x_index_base += pixels*dx;
				}
				if( sy < clip->min_y )
				{ /* clip top */
					int pixels = clip->min_y-sy;
					sy += pixels;
					y_index += pixels*dy;
				}
				/* NS 980211 - fixed incorrect clipping */
				if( ex > clip->max_x+1 )
				{ /* clip right */
					int pixels = ex-clip->max_x-1;
					ex -= pixels;
				}
				if( ey > clip->max_y+1 )
				{ /* clip bottom */
					int pixels = ey-clip->max_y-1;
					ey -= pixels;
				}
			}

			if( ex>sx )
			{ /* skip if inner loop doesn't draw anything */
				int y;

				/* case 1: TRANSPARENCY_PEN */
				if (transparency == TRANSPARENCY_PEN)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
										dest[x] = pal[c];
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color ) dest[x] = pal[c];
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}

				/* case 1b: TRANSPARENCY_PEN_RAW */
				if (transparency == TRANSPARENCY_PEN_RAW)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
										dest[x] = color + c;
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color ) dest[x] = color + c;
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}

				/* case 2: TRANSPARENCY_PENS */
				if (transparency == TRANSPARENCY_PENS)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if (((1 << c) & transparent_color) == 0)
								{
									if (((1 << pri[x]) & pri_mask) == 0)
										dest[x] = pal[c];
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if (((1 << c) & transparent_color) == 0)
									dest[x] = pal[c];
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}

				/* case 3: TRANSPARENCY_COLOR */
				else if (transparency == TRANSPARENCY_COLOR)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = pal[source[x_index>>16]];
								if( c != transparent_color )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
										dest[x] = c;
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = pal[source[x_index>>16]];
								if( c != transparent_color ) dest[x] = c;
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}

				/* case 4: TRANSPARENCY_PEN_TABLE */
				if (transparency == TRANSPARENCY_PEN_TABLE)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											dest[x] = pal[c];
											break;
										case DRAWMODE_SHADOW:
											dest[x] = palette_shadow_table[dest[x]];
											break;
										}
									}
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									switch(gfx_drawmode_table[c])
									{
									case DRAWMODE_SOURCE:
										dest[x] = pal[c];
										break;
									case DRAWMODE_SHADOW:
										dest[x] = palette_shadow_table[dest[x]];
										break;
									}
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}

				/* case 4b: TRANSPARENCY_PEN_TABLE_RAW */
				if (transparency == TRANSPARENCY_PEN_TABLE_RAW)
				{
					if (pri_buffer)
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];
							UINT8 *pri = pri_buffer->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											dest[x] = color + c;
											break;
										case DRAWMODE_SHADOW:
											dest[x] = palette_shadow_table[dest[x]];
											break;
										}
									}
									pri[x] = 31;
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
					else
					{
						for( y=sy; y<ey; y++ )
						{
							UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
							UINT16 *dest = (UINT16 *)dest_bmp->line[y];

							int x, x_index = x_index_base;
							for( x=sx; x<ex; x++ )
							{
								int c = source[x_index>>16];
								if( c != transparent_color )
								{
									switch(gfx_drawmode_table[c])
									{
									case DRAWMODE_SOURCE:
										dest[x] = color + c;
										break;
									case DRAWMODE_SHADOW:
										dest[x] = palette_shadow_table[dest[x]];
										break;
									}
								}
								x_index += dx;
							}

							y_index += dy;
						}
					}
				}
			}
		}
	}
}

void drawgfxzoom( struct osd_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,int scalex, int scaley)
{
	profiler_mark(PROFILER_DRAWGFX);
	common_drawgfxzoom(dest_bmp,gfx,code,color,flipx,flipy,sx,sy,
			clip,transparency,transparent_color,scalex,scaley,NULL,0);
	profiler_mark(PROFILER_END);
}

void pdrawgfxzoom( struct osd_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,int scalex, int scaley,
		UINT32 priority_mask)
{
	profiler_mark(PROFILER_DRAWGFX);
	common_drawgfxzoom(dest_bmp,gfx,code,color,flipx,flipy,sx,sy,
			clip,transparency,transparent_color,scalex,scaley,priority_bitmap,priority_mask);
	profiler_mark(PROFILER_END);
}


void plot_pixel2(struct osd_bitmap *bitmap1,struct osd_bitmap *bitmap2,int x,int y,int pen)
{
	plot_pixel(bitmap1, x, y, pen);
	plot_pixel(bitmap2, x, y, pen);
}

static void pp_8_nd(struct osd_bitmap *b,int x,int y,int p)  { b->line[y][x] = p; }
static void pp_8_nd_fx(struct osd_bitmap *b,int x,int y,int p)  { b->line[y][b->width-1-x] = p; }
static void pp_8_nd_fy(struct osd_bitmap *b,int x,int y,int p)  { b->line[b->height-1-y][x] = p; }
static void pp_8_nd_fxy(struct osd_bitmap *b,int x,int y,int p)  { b->line[b->height-1-y][b->width-1-x] = p; }
static void pp_8_nd_s(struct osd_bitmap *b,int x,int y,int p)  { b->line[x][y] = p; }
static void pp_8_nd_fx_s(struct osd_bitmap *b,int x,int y,int p)  { b->line[x][b->width-1-y] = p; }
static void pp_8_nd_fy_s(struct osd_bitmap *b,int x,int y,int p)  { b->line[b->height-1-x][y] = p; }
static void pp_8_nd_fxy_s(struct osd_bitmap *b,int x,int y,int p)  { b->line[b->height-1-x][b->width-1-y] = p; }

static void pp_8_d(struct osd_bitmap *b,int x,int y,int p)  { b->line[y][x] = p; osd_mark_dirty (x,y,x,y,0); }
static void pp_8_d_fx(struct osd_bitmap *b,int x,int y,int p)  { x = b->width-1-x;  b->line[y][x] = p; osd_mark_dirty (x,y,x,y,0); }
static void pp_8_d_fy(struct osd_bitmap *b,int x,int y,int p)  { y = b->height-1-y; b->line[y][x] = p; osd_mark_dirty (x,y,x,y,0); }
static void pp_8_d_fxy(struct osd_bitmap *b,int x,int y,int p)  { x = b->width-1-x; y = b->height-1-y; b->line[y][x] = p; osd_mark_dirty (x,y,x,y,0); }
static void pp_8_d_s(struct osd_bitmap *b,int x,int y,int p)  { b->line[x][y] = p; osd_mark_dirty (y,x,y,x,0); }
static void pp_8_d_fx_s(struct osd_bitmap *b,int x,int y,int p)  { y = b->width-1-y; b->line[x][y] = p; osd_mark_dirty (y,x,y,x,0); }
static void pp_8_d_fy_s(struct osd_bitmap *b,int x,int y,int p)  { x = b->height-1-x; b->line[x][y] = p; osd_mark_dirty (y,x,y,x,0); }
static void pp_8_d_fxy_s(struct osd_bitmap *b,int x,int y,int p)  { x = b->height-1-x; y = b->width-1-y; b->line[x][y] = p; osd_mark_dirty (y,x,y,x,0); }

static void pp_16_nd(struct osd_bitmap *b,int x,int y,int p)  { ((UINT16 *)b->line[y])[x] = p; }
static void pp_16_nd_fx(struct osd_bitmap *b,int x,int y,int p)  { ((UINT16 *)b->line[y])[b->width-1-x] = p; }
static void pp_16_nd_fy(struct osd_bitmap *b,int x,int y,int p)  { ((UINT16 *)b->line[b->height-1-y])[x] = p; }
static void pp_16_nd_fxy(struct osd_bitmap *b,int x,int y,int p)  { ((UINT16 *)b->line[b->height-1-y])[b->width-1-x] = p; }
static void pp_16_nd_s(struct osd_bitmap *b,int x,int y,int p)  { ((UINT16 *)b->line[x])[y] = p; }
static void pp_16_nd_fx_s(struct osd_bitmap *b,int x,int y,int p)  { ((UINT16 *)b->line[x])[b->width-1-y] = p; }
static void pp_16_nd_fy_s(struct osd_bitmap *b,int x,int y,int p)  { ((UINT16 *)b->line[b->height-1-x])[y] = p; }
static void pp_16_nd_fxy_s(struct osd_bitmap *b,int x,int y,int p)  { ((UINT16 *)b->line[b->height-1-x])[b->width-1-y] = p; }

static void pp_16_d(struct osd_bitmap *b,int x,int y,int p)  { ((UINT16 *)b->line[y])[x] = p; osd_mark_dirty (x,y,x,y,0); }
static void pp_16_d_fx(struct osd_bitmap *b,int x,int y,int p)  { x = b->width-1-x;  ((UINT16 *)b->line[y])[x] = p; osd_mark_dirty (x,y,x,y,0); }
static void pp_16_d_fy(struct osd_bitmap *b,int x,int y,int p)  { y = b->height-1-y; ((UINT16 *)b->line[y])[x] = p; osd_mark_dirty (x,y,x,y,0); }
static void pp_16_d_fxy(struct osd_bitmap *b,int x,int y,int p)  { x = b->width-1-x; y = b->height-1-y; ((UINT16 *)b->line[y])[x] = p; osd_mark_dirty (x,y,x,y,0); }
static void pp_16_d_s(struct osd_bitmap *b,int x,int y,int p)  { ((UINT16 *)b->line[x])[y] = p; osd_mark_dirty (y,x,y,x,0); }
static void pp_16_d_fx_s(struct osd_bitmap *b,int x,int y,int p)  { y = b->width-1-y; ((UINT16 *)b->line[x])[y] = p; osd_mark_dirty (y,x,y,x,0); }
static void pp_16_d_fy_s(struct osd_bitmap *b,int x,int y,int p)  { x = b->height-1-x; ((UINT16 *)b->line[x])[y] = p; osd_mark_dirty (y,x,y,x,0); }
static void pp_16_d_fxy_s(struct osd_bitmap *b,int x,int y,int p)  { x = b->height-1-x; y = b->width-1-y; ((UINT16 *)b->line[x])[y] = p; osd_mark_dirty (y,x,y,x,0); }


static int rp_8(struct osd_bitmap *b,int x,int y)  { return b->line[y][x]; }
static int rp_8_fx(struct osd_bitmap *b,int x,int y)  { return b->line[y][b->width-1-x]; }
static int rp_8_fy(struct osd_bitmap *b,int x,int y)  { return b->line[b->height-1-y][x]; }
static int rp_8_fxy(struct osd_bitmap *b,int x,int y)  { return b->line[b->height-1-y][b->width-1-x]; }
static int rp_8_s(struct osd_bitmap *b,int x,int y)  { return b->line[x][y]; }
static int rp_8_fx_s(struct osd_bitmap *b,int x,int y)  { return b->line[x][b->width-1-y]; }
static int rp_8_fy_s(struct osd_bitmap *b,int x,int y)  { return b->line[b->height-1-x][y]; }
static int rp_8_fxy_s(struct osd_bitmap *b,int x,int y)  { return b->line[b->height-1-x][b->width-1-y]; }

static int rp_16(struct osd_bitmap *b,int x,int y)  { return ((UINT16 *)b->line[y])[x]; }
static int rp_16_fx(struct osd_bitmap *b,int x,int y)  { return ((UINT16 *)b->line[y])[b->width-1-x]; }
static int rp_16_fy(struct osd_bitmap *b,int x,int y)  { return ((UINT16 *)b->line[b->height-1-y])[x]; }
static int rp_16_fxy(struct osd_bitmap *b,int x,int y)  { return ((UINT16 *)b->line[b->height-1-y])[b->width-1-x]; }
static int rp_16_s(struct osd_bitmap *b,int x,int y)  { return ((UINT16 *)b->line[x])[y]; }
static int rp_16_fx_s(struct osd_bitmap *b,int x,int y)  { return ((UINT16 *)b->line[x])[b->width-1-y]; }
static int rp_16_fy_s(struct osd_bitmap *b,int x,int y)  { return ((UINT16 *)b->line[b->height-1-x])[y]; }
static int rp_16_fxy_s(struct osd_bitmap *b,int x,int y)  { return ((UINT16 *)b->line[b->height-1-x])[b->width-1-y]; }


static void pb_8_nd(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; while(h-->0){ int c=w; x=t; while(c-->0){ b->line[y][x] = p; x++; } y++; } }
static void pb_8_nd_fx(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->width-1-x; while(h-->0){ int c=w; x=t; while(c-->0){ b->line[y][x] = p; x--; } y++; } }
static void pb_8_nd_fy(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; y = b->height-1-y; while(h-->0){ int c=w; x=t; while(c-->0){ b->line[y][x] = p; x++; } y--; } }
static void pb_8_nd_fxy(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->width-1-x; y = b->height-1-y; while(h-->0){ int c=w; x=t; while(c-->0){ b->line[y][x] = p; x--; } y--; } }
static void pb_8_nd_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; while(h-->0){ int c=w; x=t; while(c-->0){ b->line[x][y] = p; x++; } y++; } }
static void pb_8_nd_fx_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; y = b->width-1-y; while(h-->0){ int c=w; x=t; while(c-->0){ b->line[x][y] = p; x++; } y--; } }
static void pb_8_nd_fy_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->height-1-x; while(h-->0){ int c=w; x=t; while(c-->0){ b->line[x][y] = p; x--; } y++; } }
static void pb_8_nd_fxy_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->height-1-x; y = b->width-1-y; while(h-->0){ int c=w; x=t; while(c-->0){ b->line[x][y] = p; x--; } y--; } }

static void pb_8_d(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; osd_mark_dirty (t,y,t+w-1,y+h-1,0); while(h-->0){ int c=w; x=t; while(c-->0){ b->line[y][x] = p; x++; } y++; } }
static void pb_8_d_fx(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->width-1-x;  osd_mark_dirty (t-w+1,y,t,y+h-1,0); while(h-->0){ int c=w; x=t; while(c-->0){ b->line[y][x] = p; x--; } y++; } }
static void pb_8_d_fy(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; y = b->height-1-y; osd_mark_dirty (t,y-h+1,t+w-1,y,0); while(h-->0){ int c=w; x=t; while(c-->0){ b->line[y][x] = p; x++; } y--; } }
static void pb_8_d_fxy(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->width-1-x; y = b->height-1-y; osd_mark_dirty (t-w+1,y-h+1,t,y,0); while(h-->0){ int c=w; x=t; while(c-->0){ b->line[y][x] = p; x--; } y--; } }
static void pb_8_d_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; osd_mark_dirty (y,t,y+h-1,t+w-1,0); while(h-->0){ int c=w; x=t; while(c-->0){ b->line[x][y] = p; x++; } y++; } }
static void pb_8_d_fx_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; y = b->width-1-y;  osd_mark_dirty (y-h+1,t,y,t+w-1,0); while(h-->0){ int c=w; x=t; while(c-->0){ b->line[x][y] = p; x++; } y--; } }
static void pb_8_d_fy_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->height-1-x; osd_mark_dirty (y,t-w+1,y+h-1,t,0); while(h-->0){ int c=w; x=t; while(c-->0){ b->line[x][y] = p; x--; } y++; } }
static void pb_8_d_fxy_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->height-1-x; y = b->width-1-y; osd_mark_dirty (y-h+1,t-w+1,y,t,0); while(h-->0){ int c=w; x=t; while(c-->0){ b->line[x][y] = p; x--; } y--; } }

static void pb_16_nd(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[y])[x] = p; x++; } y++; } }
static void pb_16_nd_fx(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->width-1-x; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[y])[x] = p; x--; } y++; } }
static void pb_16_nd_fy(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; y = b->height-1-y; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[y])[x] = p; x++; } y--; } }
static void pb_16_nd_fxy(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->width-1-x; y = b->height-1-y; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[y])[x] = p; x--; } y--; } }
static void pb_16_nd_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[x])[y] = p; x++; } y++; } }
static void pb_16_nd_fx_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; y = b->width-1-y; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[x])[y] = p; x++; } y--; } }
static void pb_16_nd_fy_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->height-1-x; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[x])[y] = p; x--; } y++; } }
static void pb_16_nd_fxy_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->height-1-x; y = b->width-1-y; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[x])[y] = p; x--; } y--; } }

static void pb_16_d(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; osd_mark_dirty (t,y,t+w-1,y+h-1,0); while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[y])[x] = p; x++; } y++; } }
static void pb_16_d_fx(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->width-1-x;  osd_mark_dirty (t-w+1,y,t,y+h-1,0); while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[y])[x] = p; x--; } y++; } }
static void pb_16_d_fy(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; y = b->height-1-y; osd_mark_dirty (t,y-h+1,t+w-1,y,0); while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[y])[x] = p; x++; } y--; } }
static void pb_16_d_fxy(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->width-1-x; y = b->height-1-y; osd_mark_dirty (t-w+1,y-h+1,t,y,0); while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[y])[x] = p; x--; } y--; } }
static void pb_16_d_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; osd_mark_dirty (y,t,y+h-1,t+w-1,0); while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[x])[y] = p; x++; } y++; } }
static void pb_16_d_fx_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=x; y = b->width-1-y; osd_mark_dirty (y-h+1,t,y,t+w-1,0); while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[x])[y] = p; x++; } y--; } }
static void pb_16_d_fy_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->height-1-x; osd_mark_dirty (y,t-w+1,y+h-1,t,0); while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[x])[y] = p; x--; } y++; } }
static void pb_16_d_fxy_s(struct osd_bitmap *b,int x,int y,int w,int h,int p)  { int t=b->height-1-x; y = b->width-1-y; osd_mark_dirty (y-h+1,t-w+1,y,t,0); while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[x])[y] = p; x--; } y--; } }


static plot_pixel_proc pps_8_nd[] =
		{ pp_8_nd, 	 pp_8_nd_fx,   pp_8_nd_fy, 	 pp_8_nd_fxy,
		  pp_8_nd_s, pp_8_nd_fx_s, pp_8_nd_fy_s, pp_8_nd_fxy_s };

static plot_pixel_proc pps_8_d[] =
		{ pp_8_d, 	pp_8_d_fx,   pp_8_d_fy,	  pp_8_d_fxy,
		  pp_8_d_s, pp_8_d_fx_s, pp_8_d_fy_s, pp_8_d_fxy_s };

static plot_pixel_proc pps_16_nd[] =
		{ pp_16_nd,   pp_16_nd_fx,   pp_16_nd_fy, 	pp_16_nd_fxy,
		  pp_16_nd_s, pp_16_nd_fx_s, pp_16_nd_fy_s, pp_16_nd_fxy_s };

static plot_pixel_proc pps_16_d[] =
		{ pp_16_d,   pp_16_d_fx,   pp_16_d_fy, 	 pp_16_d_fxy,
		  pp_16_d_s, pp_16_d_fx_s, pp_16_d_fy_s, pp_16_d_fxy_s };


static read_pixel_proc rps_8[] =
		{ rp_8,	  rp_8_fx,   rp_8_fy,	rp_8_fxy,
		  rp_8_s, rp_8_fx_s, rp_8_fy_s, rp_8_fxy_s };

static read_pixel_proc rps_16[] =
		{ rp_16,   rp_16_fx,   rp_16_fy,   rp_16_fxy,
		  rp_16_s, rp_16_fx_s, rp_16_fy_s, rp_16_fxy_s };


static plot_box_proc pbs_8_nd[] =
		{ pb_8_nd, 	 pb_8_nd_fx,   pb_8_nd_fy, 	 pb_8_nd_fxy,
		  pb_8_nd_s, pb_8_nd_fx_s, pb_8_nd_fy_s, pb_8_nd_fxy_s };

static plot_box_proc pbs_8_d[] =
		{ pb_8_d, 	pb_8_d_fx,   pb_8_d_fy,	  pb_8_d_fxy,
		  pb_8_d_s, pb_8_d_fx_s, pb_8_d_fy_s, pb_8_d_fxy_s };

static plot_box_proc pbs_16_nd[] =
		{ pb_16_nd,   pb_16_nd_fx,   pb_16_nd_fy, 	pb_16_nd_fxy,
		  pb_16_nd_s, pb_16_nd_fx_s, pb_16_nd_fy_s, pb_16_nd_fxy_s };

static plot_box_proc pbs_16_d[] =
		{ pb_16_d,   pb_16_d_fx,   pb_16_d_fy, 	 pb_16_d_fxy,
		  pb_16_d_s, pb_16_d_fx_s, pb_16_d_fy_s, pb_16_d_fxy_s };


void set_pixel_functions(void)
{
	if (Machine->color_depth == 8)
	{
		read_pixel = rps_8[Machine->orientation];

		if (Machine->drv->video_attributes & VIDEO_SUPPORTS_DIRTY)
		{
			plot_pixel = pps_8_d[Machine->orientation];
			plot_box = pbs_8_d[Machine->orientation];
		}
		else
		{
			plot_pixel = pps_8_nd[Machine->orientation];
			plot_box = pbs_8_nd[Machine->orientation];
		}
	}
	else
	{
		read_pixel = rps_16[Machine->orientation];

		if (Machine->drv->video_attributes & VIDEO_SUPPORTS_DIRTY)
		{
			plot_pixel = pps_16_d[Machine->orientation];
			plot_box = pbs_16_d[Machine->orientation];
		}
		else
		{
			plot_pixel = pps_16_nd[Machine->orientation];
			plot_box = pbs_16_nd[Machine->orientation];
		}
	}

	/* while we're here, fill in the raw drawing mode table as well */
	is_raw[TRANSPARENCY_NONE_RAW]      = 1;
	is_raw[TRANSPARENCY_PEN_RAW]       = 1;
	is_raw[TRANSPARENCY_PENS_RAW]      = 1;
	is_raw[TRANSPARENCY_THROUGH_RAW]   = 1;
	is_raw[TRANSPARENCY_PEN_TABLE_RAW] = 1;
	is_raw[TRANSPARENCY_BLEND_RAW]     = 1;
}

#else /* DECLARE */

/* -------------------- included inline section --------------------- */

/* don't put this file in the makefile, it is #included by common.c to */
/* generate 8-bit and 16-bit versions                                  */

DECLARE(blockmove_8toN_opaque,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			dstdata[0] = paldata[srcdata[0]];
			dstdata[1] = paldata[srcdata[1]];
			dstdata[2] = paldata[srcdata[2]];
			dstdata[3] = paldata[srcdata[3]];
			dstdata[4] = paldata[srcdata[4]];
			dstdata[5] = paldata[srcdata[5]];
			dstdata[6] = paldata[srcdata[6]];
			dstdata[7] = paldata[srcdata[7]];
			dstdata += 8;
			srcdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) = paldata[*(srcdata++)];

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_opaque_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			dstdata[0] = paldata[srcdata[8]];
			dstdata[1] = paldata[srcdata[7]];
			dstdata[2] = paldata[srcdata[6]];
			dstdata[3] = paldata[srcdata[5]];
			dstdata[4] = paldata[srcdata[4]];
			dstdata[5] = paldata[srcdata[3]];
			dstdata[6] = paldata[srcdata[2]];
			dstdata[7] = paldata[srcdata[1]];
			dstdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) = paldata[*(srcdata--)];

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_opaque_pri,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,UINT8 *pridata,UINT32 pmask),
{
	DATA_TYPE *end;

	pmask |= (1<<31);

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			if (((1 << pridata[0]) & pmask) == 0) dstdata[0] = paldata[srcdata[0]];
			if (((1 << pridata[1]) & pmask) == 0) dstdata[1] = paldata[srcdata[1]];
			if (((1 << pridata[2]) & pmask) == 0) dstdata[2] = paldata[srcdata[2]];
			if (((1 << pridata[3]) & pmask) == 0) dstdata[3] = paldata[srcdata[3]];
			if (((1 << pridata[4]) & pmask) == 0) dstdata[4] = paldata[srcdata[4]];
			if (((1 << pridata[5]) & pmask) == 0) dstdata[5] = paldata[srcdata[5]];
			if (((1 << pridata[6]) & pmask) == 0) dstdata[6] = paldata[srcdata[6]];
			if (((1 << pridata[7]) & pmask) == 0) dstdata[7] = paldata[srcdata[7]];
			memset(pridata,31,8);
			srcdata += 8;
			dstdata += 8;
			pridata += 8;
		}
		while (dstdata < end)
		{
			if (((1 << *pridata) & pmask) == 0)
				*dstdata = paldata[*srcdata];
			*pridata = 31;
			srcdata++;
			dstdata++;
			pridata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		pridata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_opaque_pri_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,UINT8 *pridata,UINT32 pmask),
{
	DATA_TYPE *end;

	pmask |= (1<<31);

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			if (((1 << pridata[0]) & pmask) == 0) dstdata[0] = paldata[srcdata[8]];
			if (((1 << pridata[1]) & pmask) == 0) dstdata[1] = paldata[srcdata[7]];
			if (((1 << pridata[2]) & pmask) == 0) dstdata[2] = paldata[srcdata[6]];
			if (((1 << pridata[3]) & pmask) == 0) dstdata[3] = paldata[srcdata[5]];
			if (((1 << pridata[4]) & pmask) == 0) dstdata[4] = paldata[srcdata[4]];
			if (((1 << pridata[5]) & pmask) == 0) dstdata[5] = paldata[srcdata[3]];
			if (((1 << pridata[6]) & pmask) == 0) dstdata[6] = paldata[srcdata[2]];
			if (((1 << pridata[7]) & pmask) == 0) dstdata[7] = paldata[srcdata[1]];
			memset(pridata,31,8);
			dstdata += 8;
			pridata += 8;
		}
		while (dstdata < end)
		{
			if (((1 << *pridata) & pmask) == 0)
				*dstdata = paldata[*srcdata];
			*pridata = 31;
			srcdata--;
			dstdata++;
			pridata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		pridata += dstmodulo;
		srcheight--;
	}
})


DECLARE(blockmove_8toN_opaque_raw,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		unsigned int colorbase),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			dstdata[0] = colorbase + srcdata[0];
			dstdata[1] = colorbase + srcdata[1];
			dstdata[2] = colorbase + srcdata[2];
			dstdata[3] = colorbase + srcdata[3];
			dstdata[4] = colorbase + srcdata[4];
			dstdata[5] = colorbase + srcdata[5];
			dstdata[6] = colorbase + srcdata[6];
			dstdata[7] = colorbase + srcdata[7];
			dstdata += 8;
			srcdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) = colorbase + *(srcdata++);

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_opaque_raw_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		unsigned int colorbase),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			dstdata[0] = colorbase + srcdata[8];
			dstdata[1] = colorbase + srcdata[7];
			dstdata[2] = colorbase + srcdata[6];
			dstdata[3] = colorbase + srcdata[5];
			dstdata[4] = colorbase + srcdata[4];
			dstdata[5] = colorbase + srcdata[3];
			dstdata[6] = colorbase + srcdata[2];
			dstdata[7] = colorbase + srcdata[1];
			dstdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) = colorbase + *(srcdata--);

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transpen,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transpen),
{
	DATA_TYPE *end;
	int trans4;
	UINT32 *sd4;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	trans4 = transpen * 0x01010101;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = paldata[col];
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			UINT32 col4;

			if ((col4 = *(sd4++)) != trans4)
			{
				UINT32 xod4;

				xod4 = col4 ^ trans4;
				if (xod4 & 0x000000ff) dstdata[BL0] = paldata[(col4) & 0xff];
				if (xod4 & 0x0000ff00) dstdata[BL1] = paldata[(col4 >>  8) & 0xff];
				if (xod4 & 0x00ff0000) dstdata[BL2] = paldata[(col4 >> 16) & 0xff];
				if (xod4 & 0xff000000) dstdata[BL3] = paldata[col4 >> 24];
			}
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = paldata[col];
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transpen_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transpen),
{
	DATA_TYPE *end;
	int trans4;
	UINT32 *sd4;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;
	srcdata -= 3;

	trans4 = transpen * 0x01010101;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (col != transpen) *dstdata = paldata[col];
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			UINT32 col4;

			if ((col4 = *(sd4--)) != trans4)
			{
				UINT32 xod4;

				xod4 = col4 ^ trans4;
				if (xod4 & 0xff000000) dstdata[BL0] = paldata[col4 >> 24];
				if (xod4 & 0x00ff0000) dstdata[BL1] = paldata[(col4 >> 16) & 0xff];
				if (xod4 & 0x0000ff00) dstdata[BL2] = paldata[(col4 >>  8) & 0xff];
				if (xod4 & 0x000000ff) dstdata[BL3] = paldata[col4 & 0xff];
			}
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (col != transpen) *dstdata = paldata[col];
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transpen_pri,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transpen,UINT8 *pridata,UINT32 pmask),
{
	DATA_TYPE *end;
	int trans4;
	UINT32 *sd4;

	pmask |= (1<<31);

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	trans4 = transpen * 0x01010101;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = *(srcdata++);
			if (col != transpen)
			{
				if (((1 << *pridata) & pmask) == 0)
					*dstdata = paldata[col];
				*pridata = 31;
			}
			dstdata++;
			pridata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			UINT32 col4;

			if ((col4 = *(sd4++)) != trans4)
			{
				UINT32 xod4;

				xod4 = col4 ^ trans4;
				if (xod4 & 0x000000ff)
				{
					if (((1 << pridata[BL0]) & pmask) == 0)
						dstdata[BL0] = paldata[(col4) & 0xff];
					pridata[BL0] = 31;
				}
				if (xod4 & 0x0000ff00)
				{
					if (((1 << pridata[BL1]) & pmask) == 0)
						dstdata[BL1] = paldata[(col4 >>  8) & 0xff];
					pridata[BL1] = 31;
				}
				if (xod4 & 0x00ff0000)
				{
					if (((1 << pridata[BL2]) & pmask) == 0)
						dstdata[BL2] = paldata[(col4 >> 16) & 0xff];
					pridata[BL2] = 31;
				}
				if (xod4 & 0xff000000)
				{
					if (((1 << pridata[BL3]) & pmask) == 0)
						dstdata[BL3] = paldata[col4 >> 24];
					pridata[BL3] = 31;
				}
			}
			dstdata += 4;
			pridata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (col != transpen)
			{
				if (((1 << *pridata) & pmask) == 0)
					*dstdata = paldata[col];
				*pridata = 31;
			}
			dstdata++;
			pridata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		pridata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transpen_pri_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transpen,UINT8 *pridata,UINT32 pmask),
{
	DATA_TYPE *end;
	int trans4;
	UINT32 *sd4;

	pmask |= (1<<31);

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;
	srcdata -= 3;

	trans4 = transpen * 0x01010101;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (col != transpen)
			{
				if (((1 << *pridata) & pmask) == 0)
					*dstdata = paldata[col];
				*pridata = 31;
			}
			dstdata++;
			pridata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			UINT32 col4;

			if ((col4 = *(sd4--)) != trans4)
			{
				UINT32 xod4;

				xod4 = col4 ^ trans4;
				if (xod4 & 0xff000000)
				{
					if (((1 << pridata[BL0]) & pmask) == 0)
						dstdata[BL0] = paldata[col4 >> 24];
					pridata[BL0] = 31;
				}
				if (xod4 & 0x00ff0000)
				{
					if (((1 << pridata[BL1]) & pmask) == 0)
						dstdata[BL1] = paldata[(col4 >> 16) & 0xff];
					pridata[BL1] = 31;
				}
				if (xod4 & 0x0000ff00)
				{
					if (((1 << pridata[BL2]) & pmask) == 0)
						dstdata[BL2] = paldata[(col4 >>  8) & 0xff];
					pridata[BL2] = 31;
				}
				if (xod4 & 0x000000ff)
				{
					if (((1 << pridata[BL3]) & pmask) == 0)
						dstdata[BL3] = paldata[col4 & 0xff];
					pridata[BL3] = 31;
				}
			}
			dstdata += 4;
			pridata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (col != transpen)
			{
				if (((1 << *pridata) & pmask) == 0)
					*dstdata = paldata[col];
				*pridata = 31;
			}
			dstdata++;
			pridata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		pridata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transpen_raw,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		unsigned int colorbase,int transpen),
{
	DATA_TYPE *end;
	int trans4;
	UINT32 *sd4;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	trans4 = transpen * 0x01010101;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = colorbase + col;
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			UINT32 col4;

			if ((col4 = *(sd4++)) != trans4)
			{
				UINT32 xod4;

				xod4 = col4 ^ trans4;
				if (xod4 & 0x000000ff) dstdata[BL0] = colorbase + ((col4) & 0xff);
				if (xod4 & 0x0000ff00) dstdata[BL1] = colorbase + ((col4 >>  8) & 0xff);
				if (xod4 & 0x00ff0000) dstdata[BL2] = colorbase + ((col4 >> 16) & 0xff);
				if (xod4 & 0xff000000) dstdata[BL3] = colorbase + (col4 >> 24);
			}
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = colorbase + col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transpen_raw_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		unsigned int colorbase, int transpen),
{
	DATA_TYPE *end;
	int trans4;
	UINT32 *sd4;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;
	srcdata -= 3;

	trans4 = transpen * 0x01010101;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (col != transpen) *dstdata = colorbase + col;
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			UINT32 col4;

			if ((col4 = *(sd4--)) != trans4)
			{
				UINT32 xod4;

				xod4 = col4 ^ trans4;
				if (xod4 & 0xff000000) dstdata[BL0] = colorbase + (col4 >> 24);
				if (xod4 & 0x00ff0000) dstdata[BL1] = colorbase + ((col4 >> 16) & 0xff);
				if (xod4 & 0x0000ff00) dstdata[BL2] = colorbase + ((col4 >>  8) & 0xff);
				if (xod4 & 0x000000ff) dstdata[BL3] = colorbase + (col4 & 0xff);
			}
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (col != transpen) *dstdata = colorbase + col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

#define PEN_IS_OPAQUE ((1<<col)&transmask) == 0

DECLARE(blockmove_8toN_transmask,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transmask),
{
	DATA_TYPE *end;
	UINT32 *sd4;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = *(srcdata++);
			if (PEN_IS_OPAQUE) *dstdata = paldata[col];
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			int col;
			UINT32 col4;

			col4 = *(sd4++);
			col = (col4 >>  0) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL0] = paldata[col];
			col = (col4 >>  8) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL1] = paldata[col];
			col = (col4 >> 16) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL2] = paldata[col];
			col = (col4 >> 24) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL3] = paldata[col];
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (PEN_IS_OPAQUE) *dstdata = paldata[col];
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transmask_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transmask),
{
	DATA_TYPE *end;
	UINT32 *sd4;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;
	srcdata -= 3;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (PEN_IS_OPAQUE) *dstdata = paldata[col];
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			int col;
			UINT32 col4;

			col4 = *(sd4--);
			col = (col4 >> 24) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL0] = paldata[col];
			col = (col4 >> 16) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL1] = paldata[col];
			col = (col4 >>  8) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL2] = paldata[col];
			col = (col4 >>  0) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL3] = paldata[col];
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (PEN_IS_OPAQUE) *dstdata = paldata[col];
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transmask_pri,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transmask,UINT8 *pridata,UINT32 pmask),
{
	DATA_TYPE *end;
	UINT32 *sd4;

	pmask |= (1<<31);

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = *(srcdata++);
			if (PEN_IS_OPAQUE)
			{
				if (((1 << *pridata) & pmask) == 0)
					*dstdata = paldata[col];
				*pridata = 31;
			}
			dstdata++;
			pridata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			int col;
			UINT32 col4;

			col4 = *(sd4++);
			col = (col4 >>  0) & 0xff;
			if (PEN_IS_OPAQUE)
			{
				if (((1 << pridata[BL0]) & pmask) == 0)
					dstdata[BL0] = paldata[col];
				pridata[BL0] = 31;
			}
			col = (col4 >>  8) & 0xff;
			if (PEN_IS_OPAQUE)
			{
				if (((1 << pridata[BL1]) & pmask) == 0)
					dstdata[BL1] = paldata[col];
				pridata[BL1] = 31;
			}
			col = (col4 >> 16) & 0xff;
			if (PEN_IS_OPAQUE)
			{
				if (((1 << pridata[BL2]) & pmask) == 0)
					dstdata[BL2] = paldata[col];
				pridata[BL2] = 31;
			}
			col = (col4 >> 24) & 0xff;
			if (PEN_IS_OPAQUE)
			{
				if (((1 << pridata[BL3]) & pmask) == 0)
					dstdata[BL3] = paldata[col];
				pridata[BL3] = 31;
			}
			dstdata += 4;
			pridata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (PEN_IS_OPAQUE)
			{
				if (((1 << *pridata) & pmask) == 0)
					*dstdata = paldata[col];
				*pridata = 31;
			}
			dstdata++;
			pridata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		pridata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transmask_pri_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transmask,UINT8 *pridata,UINT32 pmask),
{
	DATA_TYPE *end;
	UINT32 *sd4;

	pmask |= (1<<31);

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;
	srcdata -= 3;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (PEN_IS_OPAQUE)
			{
				if (((1 << *pridata) & pmask) == 0)
					*dstdata = paldata[col];
				*pridata = 31;
			}
			dstdata++;
			pridata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			int col;
			UINT32 col4;

			col4 = *(sd4--);
			col = (col4 >> 24) & 0xff;
			if (PEN_IS_OPAQUE)
			{
				if (((1 << pridata[BL0]) & pmask) == 0)
					dstdata[BL0] = paldata[col];
				pridata[BL0] = 31;
			}
			col = (col4 >> 16) & 0xff;
			if (PEN_IS_OPAQUE)
			{
				if (((1 << pridata[BL1]) & pmask) == 0)
					dstdata[BL1] = paldata[col];
				pridata[BL1] = 31;
			}
			col = (col4 >>  8) & 0xff;
			if (PEN_IS_OPAQUE)
			{
				if (((1 << pridata[BL2]) & pmask) == 0)
					dstdata[BL2] = paldata[col];
				pridata[BL2] = 31;
			}
			col = (col4 >>  0) & 0xff;
			if (PEN_IS_OPAQUE)
			{
				if (((1 << pridata[BL3]) & pmask) == 0)
					dstdata[BL3] = paldata[col];
				pridata[BL3] = 31;
			}
			dstdata += 4;
			pridata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (PEN_IS_OPAQUE)
			{
				if (((1 << *pridata) & pmask) == 0)
					*dstdata = paldata[col];
				*pridata = 31;
			}
			dstdata++;
			pridata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		pridata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transmask_raw,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		unsigned int colorbase,int transmask),
{
	DATA_TYPE *end;
	UINT32 *sd4;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = *(srcdata++);
			if (PEN_IS_OPAQUE) *dstdata = colorbase + col;
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			int col;
			UINT32 col4;

			col4 = *(sd4++);
			col = (col4 >>  0) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL0] = colorbase + col;
			col = (col4 >>  8) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL1] = colorbase + col;
			col = (col4 >> 16) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL2] = colorbase + col;
			col = (col4 >> 24) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL3] = colorbase + col;
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (PEN_IS_OPAQUE) *dstdata = colorbase + col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transmask_raw_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		unsigned int colorbase,int transmask),
{
	DATA_TYPE *end;
	UINT32 *sd4;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;
	srcdata -= 3;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (PEN_IS_OPAQUE) *dstdata = colorbase + col;
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			int col;
			UINT32 col4;

			col4 = *(sd4--);
			col = (col4 >> 24) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL0] = colorbase + col;
			col = (col4 >> 16) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL1] = colorbase + col;
			col = (col4 >>  8) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL2] = colorbase + col;
			col = (col4 >>  0) & 0xff;
			if (PEN_IS_OPAQUE) dstdata[BL3] = colorbase + col;
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (PEN_IS_OPAQUE) *dstdata = colorbase + col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})


DECLARE(blockmove_8toN_transcolor,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transcolor),
{
	DATA_TYPE *end;
	const UINT16 *lookupdata = Machine->game_colortable + (paldata - Machine->remapped_colortable);

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			if (lookupdata[*srcdata] != transcolor) *dstdata = paldata[*srcdata];
			srcdata++;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transcolor_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transcolor),
{
	DATA_TYPE *end;
	const UINT16 *lookupdata = Machine->game_colortable + (paldata - Machine->remapped_colortable);

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			if (lookupdata[*srcdata] != transcolor) *dstdata = paldata[*srcdata];
			srcdata--;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transcolor_pri,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transcolor,UINT8 *pridata,UINT32 pmask),
{
	DATA_TYPE *end;
	const UINT16 *lookupdata = Machine->game_colortable + (paldata - Machine->remapped_colortable);

	pmask |= (1<<31);

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			if (lookupdata[*srcdata] != transcolor)
			{
				if (((1 << *pridata) & pmask) == 0)
					*dstdata = paldata[*srcdata];
				*pridata = 31;
			}
			srcdata++;
			dstdata++;
			pridata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		pridata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transcolor_pri_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transcolor,UINT8 *pridata,UINT32 pmask),
{
	DATA_TYPE *end;
	const UINT16 *lookupdata = Machine->game_colortable + (paldata - Machine->remapped_colortable);

	pmask |= (1<<31);

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			if (lookupdata[*srcdata] != transcolor)
			{
				if (((1 << *pridata) & pmask) == 0)
					*dstdata = paldata[*srcdata];
				*pridata = 31;
			}
			srcdata--;
			dstdata++;
			pridata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		pridata += dstmodulo;
		srcheight--;
	}
})


DECLARE(blockmove_8toN_transthrough,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transcolor),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			if (*dstdata == transcolor) *dstdata = paldata[*srcdata];
			srcdata++;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transthrough_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transcolor),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			if (*dstdata == transcolor) *dstdata = paldata[*srcdata];
			srcdata--;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transthrough_raw,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		unsigned int colorbase,int transcolor),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			if (*dstdata == transcolor) *dstdata = colorbase + *srcdata;
			srcdata++;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_transthrough_raw_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		unsigned int colorbase,int transcolor),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			if (*dstdata == transcolor) *dstdata = colorbase + *srcdata;
			srcdata--;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_pen_table,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transcolor),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (col != transcolor)
			{
				switch(gfx_drawmode_table[col])
				{
				case DRAWMODE_SOURCE:
					*dstdata = paldata[col];
					break;
				case DRAWMODE_SHADOW:
					*dstdata = palette_shadow_table[*dstdata];
					break;
				}
			}
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_pen_table_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int transcolor),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata--);
			if (col != transcolor)
			{
				switch(gfx_drawmode_table[col])
				{
				case DRAWMODE_SOURCE:
					*dstdata = paldata[col];
					break;
				case DRAWMODE_SHADOW:
					*dstdata = palette_shadow_table[*dstdata];
					break;
				}
			}
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_pen_table_raw,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		unsigned int colorbase,int transcolor),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (col != transcolor)
			{
				switch(gfx_drawmode_table[col])
				{
				case DRAWMODE_SOURCE:
					*dstdata = colorbase + col;
					break;
				case DRAWMODE_SHADOW:
					*dstdata = palette_shadow_table[*dstdata];
					break;
				}
			}
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_8toN_pen_table_raw_flipx,(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		unsigned int colorbase,int transcolor),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata--);
			if (col != transcolor)
			{
				switch(gfx_drawmode_table[col])
				{
				case DRAWMODE_SOURCE:
					*dstdata = colorbase + col;
					break;
				case DRAWMODE_SHADOW:
					*dstdata = palette_shadow_table[*dstdata];
					break;
				}
			}
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_opaque_noremap,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo),
{
	while (srcheight)
	{
		memcpy(dstdata,srcdata,srcwidth * sizeof(DATA_TYPE));
		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_opaque_noremap_flipx,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			dstdata[0] = srcdata[8];
			dstdata[1] = srcdata[7];
			dstdata[2] = srcdata[6];
			dstdata[3] = srcdata[5];
			dstdata[4] = srcdata[4];
			dstdata[5] = srcdata[3];
			dstdata[6] = srcdata[2];
			dstdata[7] = srcdata[1];
			dstdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) = *(srcdata--);

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_opaque_remap,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			dstdata[0] = paldata[srcdata[0]];
			dstdata[1] = paldata[srcdata[1]];
			dstdata[2] = paldata[srcdata[2]];
			dstdata[3] = paldata[srcdata[3]];
			dstdata[4] = paldata[srcdata[4]];
			dstdata[5] = paldata[srcdata[5]];
			dstdata[6] = paldata[srcdata[6]];
			dstdata[7] = paldata[srcdata[7]];
			dstdata += 8;
			srcdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) = paldata[*(srcdata++)];

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_opaque_remap_flipx,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			dstdata[0] = paldata[srcdata[8]];
			dstdata[1] = paldata[srcdata[7]];
			dstdata[2] = paldata[srcdata[6]];
			dstdata[3] = paldata[srcdata[5]];
			dstdata[4] = paldata[srcdata[4]];
			dstdata[5] = paldata[srcdata[3]];
			dstdata[6] = paldata[srcdata[2]];
			dstdata[7] = paldata[srcdata[1]];
			dstdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) = paldata[*(srcdata--)];

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})


DECLARE(blockmove_NtoN_transthrough_noremap,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		int transcolor),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			if (*dstdata == transcolor) *dstdata = *srcdata;
			srcdata++;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_transthrough_noremap_flipx,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		int transcolor),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			if (*dstdata == transcolor) *dstdata = *srcdata;
			srcdata--;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_blend_noremap,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		int srcshift),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			dstdata[0] |= srcdata[0] << srcshift;
			dstdata[1] |= srcdata[1] << srcshift;
			dstdata[2] |= srcdata[2] << srcshift;
			dstdata[3] |= srcdata[3] << srcshift;
			dstdata[4] |= srcdata[4] << srcshift;
			dstdata[5] |= srcdata[5] << srcshift;
			dstdata[6] |= srcdata[6] << srcshift;
			dstdata[7] |= srcdata[7] << srcshift;
			dstdata += 8;
			srcdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) |= *(srcdata++) << srcshift;

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_blend_noremap_flipx,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		int srcshift),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			dstdata[0] |= srcdata[8] << srcshift;
			dstdata[1] |= srcdata[7] << srcshift;
			dstdata[2] |= srcdata[6] << srcshift;
			dstdata[3] |= srcdata[5] << srcshift;
			dstdata[4] |= srcdata[4] << srcshift;
			dstdata[5] |= srcdata[3] << srcshift;
			dstdata[6] |= srcdata[2] << srcshift;
			dstdata[7] |= srcdata[1] << srcshift;
			dstdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) |= *(srcdata--) << srcshift;

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_blend_remap,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int srcshift),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			dstdata[0] = paldata[dstdata[0] | (srcdata[0] << srcshift)];
			dstdata[1] = paldata[dstdata[1] | (srcdata[1] << srcshift)];
			dstdata[2] = paldata[dstdata[2] | (srcdata[2] << srcshift)];
			dstdata[3] = paldata[dstdata[3] | (srcdata[3] << srcshift)];
			dstdata[4] = paldata[dstdata[4] | (srcdata[4] << srcshift)];
			dstdata[5] = paldata[dstdata[5] | (srcdata[5] << srcshift)];
			dstdata[6] = paldata[dstdata[6] | (srcdata[6] << srcshift)];
			dstdata[7] = paldata[dstdata[7] | (srcdata[7] << srcshift)];
			dstdata += 8;
			srcdata += 8;
		}
		while (dstdata < end)
		{
			*dstdata = paldata[*dstdata | (*(srcdata++) << srcshift)];
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_blend_remap_flipx,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const UINT16 *paldata,int srcshift),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	//srcdata += srcwidth-1;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			dstdata[0] = paldata[dstdata[0] | (srcdata[8] << srcshift)];
			dstdata[1] = paldata[dstdata[1] | (srcdata[7] << srcshift)];
			dstdata[2] = paldata[dstdata[2] | (srcdata[6] << srcshift)];
			dstdata[3] = paldata[dstdata[3] | (srcdata[5] << srcshift)];
			dstdata[4] = paldata[dstdata[4] | (srcdata[4] << srcshift)];
			dstdata[5] = paldata[dstdata[5] | (srcdata[3] << srcshift)];
			dstdata[6] = paldata[dstdata[6] | (srcdata[2] << srcshift)];
			dstdata[7] = paldata[dstdata[7] | (srcdata[1] << srcshift)];
			dstdata += 8;
		}
		while (dstdata < end)
		{
			*dstdata = paldata[*dstdata | (*(srcdata--) << srcshift)];
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})


DECLARE(drawgfx_core,(
		struct osd_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,
		struct osd_bitmap *pri_buffer,UINT32 pri_mask),
{
	int ox;
	int oy;
	int ex;
	int ey;


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

	{
		UINT8 *sd = gfx->gfxdata + code * gfx->char_modulo;		/* source data */
		int sw = ex-sx+1;										/* source width */
		int sh = ey-sy+1;										/* source height */
		int sm = gfx->line_modulo;								/* source modulo */
		DATA_TYPE *dd = ((DATA_TYPE *)dest->line[sy]) + sx;		/* dest data */
		int dm = ((DATA_TYPE *)dest->line[1])-((DATA_TYPE *)dest->line[0]);	/* dest modulo */
		const UINT16 *paldata = &gfx->colortable[gfx->color_granularity * color];
		UINT8 *pribuf = (pri_buffer) ? pri_buffer->line[sy] + sx : NULL;

		if (flipx)
		{
			//if ((sx-ox) == 0) sd += gfx->width - sw;
			sd += gfx->width -1 -(sx-ox);
		}
		else
			sd += (sx-ox);

		if (flipy)
		{
			//if ((sy-oy) == 0) sd += sm * (gfx->height - sh);
			//dd += dm * (sh - 1);
			//dm = -dm;
			sd += sm * (gfx->height -1 -(sy-oy));
			sm = -sm;
		}
		else
			sd += sm * (sy-oy);

		switch (transparency)
		{
			case TRANSPARENCY_NONE:
				if (pribuf)
					BLOCKMOVE(8toN_opaque_pri,flipx,(sd,sw,sh,sm,dd,dm,paldata,pribuf,pri_mask));
				else
					BLOCKMOVE(8toN_opaque,flipx,(sd,sw,sh,sm,dd,dm,paldata));
				break;

			case TRANSPARENCY_PEN:
				if (pribuf)
					BLOCKMOVE(8toN_transpen_pri,flipx,(sd,sw,sh,sm,dd,dm,paldata,transparent_color,pribuf,pri_mask));
				else
					BLOCKMOVE(8toN_transpen,flipx,(sd,sw,sh,sm,dd,dm,paldata,transparent_color));
				break;

			case TRANSPARENCY_PENS:
				if (pribuf)
					BLOCKMOVE(8toN_transmask_pri,flipx,(sd,sw,sh,sm,dd,dm,paldata,transparent_color,pribuf,pri_mask));
				else
					BLOCKMOVE(8toN_transmask,flipx,(sd,sw,sh,sm,dd,dm,paldata,transparent_color));
				break;

			case TRANSPARENCY_COLOR:
				if (pribuf)
					BLOCKMOVE(8toN_transcolor_pri,flipx,(sd,sw,sh,sm,dd,dm,paldata,transparent_color,pribuf,pri_mask));
				else
					BLOCKMOVE(8toN_transcolor,flipx,(sd,sw,sh,sm,dd,dm,paldata,transparent_color));
				break;

			case TRANSPARENCY_THROUGH:
				if (pribuf)
usrintf_showmessage("pdrawgfx TRANS_THROUGH not supported");
//					BLOCKMOVE(8toN_transthrough,flipx,(sd,sw,sh,sm,dd,dm,paldata,transparent_color));
				else
					BLOCKMOVE(8toN_transthrough,flipx,(sd,sw,sh,sm,dd,dm,paldata,transparent_color));
				break;

			case TRANSPARENCY_PEN_TABLE:
				if (pribuf)
usrintf_showmessage("pdrawgfx TRANS_PEN_TABLE not supported");
//					BLOCKMOVE(8toN_pen_table_pri,flipx,(sd,sw,sh,sm,dd,dm,paldata,transparent_color));
				else
					BLOCKMOVE(8toN_pen_table,flipx,(sd,sw,sh,sm,dd,dm,paldata,transparent_color));
				break;

			case TRANSPARENCY_PEN_TABLE_RAW:
				if (pribuf)
usrintf_showmessage("pdrawgfx TRANS_PEN_TABLE_RAW not supported");
//					BLOCKMOVE(8toN_pen_table_pri_raw,flipx,(sd,sw,sh,sm,dd,dm,color,transparent_color));
				else
					BLOCKMOVE(8toN_pen_table_raw,flipx,(sd,sw,sh,sm,dd,dm,color,transparent_color));
				break;

			case TRANSPARENCY_NONE_RAW:
				if (pribuf)
usrintf_showmessage("pdrawgfx TRANS_NONE_RAW not supported");
//					BLOCKMOVE(8toN_opaque_pri_raw,flipx,(sd,sw,sh,sm,dd,dm,paldata,pribuf,pri_mask));
				else
					BLOCKMOVE(8toN_opaque_raw,flipx,(sd,sw,sh,sm,dd,dm,color));
				break;

			case TRANSPARENCY_PEN_RAW:
				if (pribuf)
usrintf_showmessage("pdrawgfx TRANS_PEN_RAW not supported");
//					BLOCKMOVE(8toN_transpen_pri_raw,flipx,(sd,sw,sh,sm,dd,dm,paldata,pribuf,pri_mask));
				else
					BLOCKMOVE(8toN_transpen_raw,flipx,(sd,sw,sh,sm,dd,dm,color,transparent_color));
				break;

			case TRANSPARENCY_PENS_RAW:
				if (pribuf)
usrintf_showmessage("pdrawgfx TRANS_PENS_RAW not supported");
//					BLOCKMOVE(8toN_transmask_pri_raw,flipx,(sd,sw,sh,sm,dd,dm,paldata,pribuf,pri_mask));
				else
					BLOCKMOVE(8toN_transmask_raw,flipx,(sd,sw,sh,sm,dd,dm,color,transparent_color));
				break;

			case TRANSPARENCY_THROUGH_RAW:
				if (pribuf)
usrintf_showmessage("pdrawgfx TRANS_PEN_RAW not supported");
//					BLOCKMOVE(8toN_transpen_pri_raw,flipx,(sd,sw,sh,sm,dd,dm,paldata,pribuf,pri_mask));
				else
					BLOCKMOVE(8toN_transthrough_raw,flipx,(sd,sw,sh,sm,dd,dm,color,transparent_color));
				break;

			default:
				if (pribuf)
					usrintf_showmessage("pdrawgfx pen mode not supported");
				else
					usrintf_showmessage("drawgfx pen mode not supported");
				break;
		}
	}
})

DECLARE(copybitmap_core,(
		struct osd_bitmap *dest,struct osd_bitmap *src,
		int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color),
{
	int ox;
	int oy;
	int ex;
	int ey;


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

	{
		DATA_TYPE *sd = ((DATA_TYPE *)src->line[0]);							/* source data */
		int sw = ex-sx+1;														/* source width */
		int sh = ey-sy+1;														/* source height */
		int sm = ((DATA_TYPE *)src->line[1])-((DATA_TYPE *)src->line[0]);		/* source modulo */
		DATA_TYPE *dd = ((DATA_TYPE *)dest->line[sy]) + sx;						/* dest data */
		int dm = ((DATA_TYPE *)dest->line[1])-((DATA_TYPE *)dest->line[0]);		/* dest modulo */

		if (flipx)
		{
			//if ((sx-ox) == 0) sd += gfx->width - sw;
			sd += src->width -1 -(sx-ox);
		}
		else
			sd += (sx-ox);

		if (flipy)
		{
			//if ((sy-oy) == 0) sd += sm * (gfx->height - sh);
			//dd += dm * (sh - 1);
			//dm = -dm;
			sd += sm * (src->height -1 -(sy-oy));
			sm = -sm;
		}
		else
			sd += sm * (sy-oy);

		switch (transparency)
		{
			case TRANSPARENCY_NONE:
				BLOCKMOVE(NtoN_opaque_remap,flipx,(sd,sw,sh,sm,dd,dm,Machine->pens));
				break;

			case TRANSPARENCY_NONE_RAW:
				BLOCKMOVE(NtoN_opaque_noremap,flipx,(sd,sw,sh,sm,dd,dm));
				break;

			case TRANSPARENCY_PEN_RAW:
				BLOCKMOVE(NtoN_transpen_noremap,flipx,(sd,sw,sh,sm,dd,dm,transparent_color));
				break;

			case TRANSPARENCY_THROUGH_RAW:
				BLOCKMOVE(NtoN_transthrough_noremap,flipx,(sd,sw,sh,sm,dd,dm,transparent_color));
				break;

			case TRANSPARENCY_BLEND:
				BLOCKMOVE(NtoN_blend_remap,flipx,(sd,sw,sh,sm,dd,dm,Machine->pens,transparent_color));
				break;

			case TRANSPARENCY_BLEND_RAW:
				BLOCKMOVE(NtoN_blend_noremap,flipx,(sd,sw,sh,sm,dd,dm,transparent_color));
				break;

			default:
				usrintf_showmessage("copybitmap pen mode not supported");
				break;
		}
	}
})

DECLARE(copyrozbitmap_core,(struct osd_bitmap *bitmap,struct osd_bitmap *srcbitmap,
		UINT32 startx,UINT32 starty,int incxx,int incxy,int incyx,int incyy,int wraparound,
		const struct rectangle *clip,int transparency,int transparent_color,UINT32 priority),
{
	UINT32 cx;
	UINT32 cy;
	int x;
	int sx;
	int sy;
	int ex;
	int ey;
	const int xmask = srcbitmap->width-1;
	const int ymask = srcbitmap->height-1;
	const int widthshifted = srcbitmap->width << 16;
	const int heightshifted = srcbitmap->height << 16;
	DATA_TYPE *dest;


	if (clip)
	{
		startx += clip->min_x * incxx + clip->min_y * incyx;
		starty += clip->min_x * incxy + clip->min_y * incyy;

		sx = clip->min_x;
		sy = clip->min_y;
		ex = clip->max_x;
		ey = clip->max_y;
	}
	else
	{
		sx = 0;
		sy = 0;
		ex = bitmap->width-1;
		ey = bitmap->height-1;
	}


	if (Machine->orientation & ORIENTATION_SWAP_XY)
	{
		int t;

		t = startx; startx = starty; starty = t;
		t = sx; sx = sy; sy = t;
		t = ex; ex = ey; ey = t;
		t = incxx; incxx = incyy; incyy = t;
		t = incxy; incxy = incyx; incyx = t;
	}

	if (Machine->orientation & ORIENTATION_FLIP_X)
	{
		int w = ex - sx;

		incxy = -incxy;
		incyx = -incyx;
		startx = widthshifted - startx - 1;
		startx -= incxx * w;
		starty -= incxy * w;

		w = sx;
		sx = bitmap->width-1 - ex;
		ex = bitmap->width-1 - w;
	}

	if (Machine->orientation & ORIENTATION_FLIP_Y)
	{
		int h = ey - sy;

		incxy = -incxy;
		incyx = -incyx;
		starty = heightshifted - starty - 1;
		startx -= incyx * h;
		starty -= incyy * h;

		h = sy;
		sy = bitmap->height-1 - ey;
		ey = bitmap->height-1 - h;
	}

	if (incxy == 0 && incyx == 0 && !wraparound)
	{
		/* optimized loop for the not rotated case */

		if (incxx == 0x10000)
		{
			/* optimized loop for the not zoomed case */

			/* startx is unsigned */
			startx = ((INT32)startx) >> 16;

			if (startx >= srcbitmap->width)
			{
				sx += -startx;
				startx = 0;
			}

			if (sx <= ex)
			{
				while (sy <= ey)
				{
					if (starty < heightshifted)
					{
						x = sx;
						cx = startx;
						cy = starty >> 16;
						dest = ((DATA_TYPE *)bitmap->line[sy]) + sx;
						if (priority)
						{
							UINT8 *pri = &priority_bitmap->line[sy][sx];
							DATA_TYPE *src = (DATA_TYPE *)srcbitmap->line[cy];

							while (x <= ex && cx < srcbitmap->width)
							{
								int c = src[cx];

								if (c != transparent_color)
								{
									*dest = c;
									*pri |= priority;
								}

								cx++;
								x++;
								dest++;
								pri++;
							}
						}
						else
						{
							DATA_TYPE *src = (DATA_TYPE *)srcbitmap->line[cy];

							while (x <= ex && cx < srcbitmap->width)
							{
								int c = src[cx];

								if (c != transparent_color)
									*dest = c;

								cx++;
								x++;
								dest++;
							}
						}
					}
					starty += incyy;
					sy++;
				}
			}
		}
		else
		{
			while (startx >= widthshifted && sx <= ex)
			{
				startx += incxx;
				sx++;
			}

			if (sx <= ex)
			{
				while (sy <= ey)
				{
					if (starty < heightshifted)
					{
						x = sx;
						cx = startx;
						cy = starty >> 16;
						dest = ((DATA_TYPE *)bitmap->line[sy]) + sx;
						if (priority)
						{
							UINT8 *pri = &priority_bitmap->line[sy][sx];
							DATA_TYPE *src = (DATA_TYPE *)srcbitmap->line[cy];

							while (x <= ex && cx < widthshifted)
							{
								int c = src[cx >> 16];

								if (c != transparent_color)
								{
									*dest = c;
									*pri |= priority;
								}

								cx += incxx;
								x++;
								dest++;
								pri++;
							}
						}
						else
						{
							DATA_TYPE *src = (DATA_TYPE *)srcbitmap->line[cy];

							while (x <= ex && cx < widthshifted)
							{
								int c = src[cx >> 16];

								if (c != transparent_color)
									*dest = c;

								cx += incxx;
								x++;
								dest++;
							}
						}
					}
					starty += incyy;
					sy++;
				}
			}
		}
	}
	else
	{
		if (wraparound)
		{
			/* plot with wraparound */
			while (sy <= ey)
			{
				x = sx;
				cx = startx;
				cy = starty;
				dest = ((DATA_TYPE *)bitmap->line[sy]) + sx;
				if (priority)
				{
					UINT8 *pri = &priority_bitmap->line[sy][sx];

					while (x <= ex)
					{
						int c = ((DATA_TYPE *)srcbitmap->line[(cy >> 16) & xmask])[(cx >> 16) & ymask];

						if (c != transparent_color)
						{
							*dest = c;
							*pri |= priority;
						}

						cx += incxx;
						cy += incxy;
						x++;
						dest++;
						pri++;
					}
				}
				else
				{
					while (x <= ex)
					{
						int c = ((DATA_TYPE *)srcbitmap->line[(cy >> 16) & xmask])[(cx >> 16) & ymask];

						if (c != transparent_color)
							*dest = c;

						cx += incxx;
						cy += incxy;
						x++;
						dest++;
					}
				}
				startx += incyx;
				starty += incyy;
				sy++;
			}
		}
		else
		{
			while (sy <= ey)
			{
				x = sx;
				cx = startx;
				cy = starty;
				dest = ((DATA_TYPE *)bitmap->line[sy]) + sx;
				if (priority)
				{
					UINT8 *pri = &priority_bitmap->line[sy][sx];

					while (x <= ex)
					{
						if (cx < widthshifted && cy < heightshifted)
						{
							int c = ((DATA_TYPE *)srcbitmap->line[cy >> 16])[cx >> 16];

							if (c != transparent_color)
							{
								*dest = c;
								*pri |= priority;
							}
						}

						cx += incxx;
						cy += incxy;
						x++;
						dest++;
						pri++;
					}
				}
				else
				{
					while (x <= ex)
					{
						if (cx < widthshifted && cy < heightshifted)
						{
							int c = ((DATA_TYPE *)srcbitmap->line[cy >> 16])[cx >> 16];

							if (c != transparent_color)
								*dest = c;
						}

						cx += incxx;
						cy += incxy;
						x++;
						dest++;
					}
				}
				startx += incyx;
				starty += incyy;
				sy++;
			}
		}
	}
})

#endif /* DECLARE */
