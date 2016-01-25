/*************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

**************************************************************************/

#include "driver.h"
#include "cpu/tms34010/tms34010.h"



/* compile-time options */
#define FAST_DMA			1		/* DMAs complete immediately; reduces number of CPU switches */
#define LOG_DMA				0		/* DMAs are logged if the 'L' key is pressed */


/* constants for the DMA chip */
enum
{
	DMA_COMMAND = 0,
	DMA_ROWBYTES,
	DMA_OFFSETLO,
	DMA_OFFSETHI,
	DMA_XSTART,
	DMA_YSTART,
	DMA_WIDTH,
	DMA_HEIGHT,
	DMA_PALETTE,
	DMA_COLOR
};



/* CMOS-related variables */
extern UINT8 *	wms_cmos_ram;
extern UINT32	wms_cmos_page;

/* graphics-related variables */
       struct rectangle wms_visible_area;
       UINT8 *	wms_gfx_rom;
       size_t	wms_gfx_rom_size;
static UINT8	autoerase_enable;

/* palette-related variables */
static UINT8	pixel_mask;
static UINT16	palette_mask;
static UINT16 *	palette_lookup;
static UINT8 *	palette_reverse_lookup;

/* videoram-related variables */
static UINT16 *	local_videoram;
static UINT16 *	local_videoram_copy;
static UINT8	videobank_select;

/* update-related variables */
       UINT8	wms_partial_update_offset;
static UINT8	page_flipping;
static UINT8	skipping_this_frame;
static UINT32	last_display_addr;
static int		last_update_scanline;
static UINT32	autoerase_list[512];
static UINT32	autoerase_count;

/* DMA-related variables */
static UINT16 dma_register[16];
static struct
{
	UINT32		offset;			/* source offset, in bits */
	INT32 		rowbytes;		/* source bytes to skip each row */
	INT32 		xpos;			/* x position, clipped */
	INT32		ypos;			/* y position, clipped */
	INT32		width;			/* horizontal pixel count */
	INT32		height;			/* vertical pixel count */
	UINT16		palette;		/* palette base */
	UINT16		color;			/* current foreground color with palette */
} dma_state;



/* prototypes */
static void update_partial(int scanline);
       void wms_yunit_vh_stop(void);



/*************************************
 *
 *	Video startup
 *
 *************************************/

static int vh_start_common(void)
{
	/* allocate memory */
	wms_cmos_ram = (UINT8*)malloc(0x2000 * 4);
	local_videoram = (UINT16*)malloc(0x80000);
	local_videoram_copy = (UINT16*)malloc(0x80000);
	palette_lookup = (UINT16*)malloc(256 * sizeof(palette_lookup[0]));
	palette_reverse_lookup = (UINT8*)malloc(65536 * sizeof(palette_reverse_lookup[0]));
	
	/* handle failure */
	if (!wms_cmos_ram || !local_videoram || !local_videoram_copy || !palette_lookup || !palette_reverse_lookup)
	{
		wms_yunit_vh_stop();
		return 1;
	}
	
	/* we have to erase this since we rely on upper bits being 0 */
	memset(local_videoram, 0, 0x80000);
	memset(local_videoram_copy, 0, 0x80000);
	
	/* reset all the globals */
	wms_cmos_page = 0;
	
	autoerase_enable = 0;
	
	page_flipping = 0;
	skipping_this_frame = 0;
	last_display_addr = 0;
	last_update_scanline = 0;
	autoerase_count = 0;
	
	memset(dma_register, 0, sizeof(dma_register));
	memset(&dma_state, 0, sizeof(dma_state));

	return 0;
}

int wms_yunit_4bit_vh_start(void)
{
	int result = vh_start_common();
	int i;
	
	/* handle failure */
	if (result)
		return result;
	
	/* init for 4-bit */
	for (i = 0; i < 256; i++)
		palette_lookup[i] = i & 0xf0;
	for (i = 0; i < 65536; i++)
		palette_reverse_lookup[i] = i & 0xf0;
	pixel_mask = 0x000f;
	palette_mask = 0x00f0;
	
	return 0;
}

int wms_yunit_6bit_vh_start(void)
{
	int result = vh_start_common();
	int i;
	
	/* handle failure */
	if (result)
		return result;
	
	/* init for 6-bit */
	for (i = 0; i < 256; i++)
		palette_lookup[i] = (i & 0xc0) | ((i & 0x0f) << 8);
	for (i = 0; i < 65536; i++)
		palette_reverse_lookup[i] = ((i >> 8) & 0x0f) | (i & 0xc0);
	pixel_mask = 0x003f;
	palette_mask = 0x0fc0;

	return 0;
}

int wms_zunit_vh_start(void)
{
	int result = vh_start_common();
	int i;
	
	/* handle failure */
	if (result)
		return result;
	
	/* init for 8-bit */
	for (i = 0; i < 256; i++)
		palette_lookup[i] = (i << 8) & 0xff00;
	for (i = 0; i < 65536; i++)
		palette_reverse_lookup[i] = (i >> 8) & 0xff;
	pixel_mask = 0x00ff;
	palette_mask = 0xff00;
	
	return 0;
}



/*************************************
 *
 *	Video shutdown
 *
 *************************************/

void wms_yunit_vh_stop(void)
{
	if (wms_cmos_ram)
		free(wms_cmos_ram);
	wms_cmos_ram = NULL;

	if (local_videoram)
		free(local_videoram);
	local_videoram = NULL;

	if (local_videoram_copy)
		free(local_videoram_copy);
	local_videoram_copy = NULL;

	if (palette_lookup)
		free(palette_lookup);
	palette_lookup = NULL;

	if (palette_reverse_lookup)
		free(palette_reverse_lookup);
	palette_reverse_lookup = NULL;
}



/*************************************
 *
 *	Banked graphics ROM access
 *
 *************************************/

READ_HANDLER( wms_yunit_gfxrom_r )
{
	if (pixel_mask == 0x0f)
		return wms_gfx_rom[offset] | (wms_gfx_rom[offset] << 4) |
				(wms_gfx_rom[offset + 1] << 8) | (wms_gfx_rom[offset + 1] << 12);
	else
		return wms_gfx_rom[offset] | (wms_gfx_rom[offset + 1] << 8);
}



/*************************************
 *
 *	Video/color RAM read/write
 *
 *************************************/

WRITE_HANDLER( wms_yunit_vram_w )
{
	if (videobank_select)
	{
		if (!(data & 0x00ff0000))
			local_videoram[offset] = (data & pixel_mask) | palette_lookup[dma_register[DMA_PALETTE] & 0xff];
		if (!(data & 0xff000000))
			local_videoram[offset + 1] = ((data >> 8) & pixel_mask) | palette_lookup[dma_register[DMA_PALETTE] >> 8];
	}
	else
	{
		if (!(data & 0x00ff0000))
			local_videoram[offset] = (local_videoram[offset] & pixel_mask) | palette_lookup[data & 0xff];
		if (!(data & 0xff000000))
			local_videoram[offset + 1] = (local_videoram[offset + 1] & pixel_mask) | palette_lookup[(data >> 8) & 0xff];
	}
}


READ_HANDLER( wms_yunit_vram_r )
{
	if (videobank_select)
		return (local_videoram[offset] & pixel_mask) | ((local_videoram[offset + 1] & pixel_mask) << 8);
	else
		return (palette_reverse_lookup[local_videoram[offset]]) | (palette_reverse_lookup[local_videoram[offset + 1]] << 8);
}



/*************************************
 *
 *	Shift register read/write
 *
 *************************************/

void wms_yunit_to_shiftreg(UINT32 address, UINT16 *shiftreg)
{
	memcpy(shiftreg, &local_videoram[address >> 3], 2 * 512 * sizeof(UINT16));
}


void wms_yunit_from_shiftreg(UINT32 address, UINT16 *shiftreg)
{
	memcpy(&local_videoram[address >> 3], shiftreg, 2 * 512 * sizeof(UINT16));
}



/*************************************
 *
 *	Y/Z-unit control register
 *
 *************************************/

WRITE_HANDLER( wms_yunit_control_w )
{
	/*
	 * Narc system register
	 * ------------------
	 *
	 *   | Bit              | Use
	 * --+-FEDCBA9876543210-+------------
	 *   | xxxxxxxx-------- |   7 segment led on CPU board
	 *   | --------xx------ |   CMOS page
	 *   | ----------x----- | - OBJ PAL RAM select
	 *   | -----------x---- | - autoerase enable
	 *   | ---------------- | - watchdog
	 *
	 */

	/* CMOS page is bits 6-7 */
	wms_cmos_page = ((data >> 6) & 3) * 0x2000;

	/* video bank select is bit 5 */
	videobank_select = (data >> 5) & 1;

	/* handle autoerase disable (bit 4) */
	if (data & 0x10)
	{
		if (autoerase_enable)
			update_partial(cpu_getscanline());
		autoerase_enable = 0;
	}

	/* handle autoerase enable (bit 4)  */
	else
	{
		if (!autoerase_enable)
			update_partial(cpu_getscanline());
		autoerase_enable = 1;
	}
}



/*************************************
 *
 *	Palette handlers
 *
 *************************************/

WRITE_HANDLER( wms_yunit_paletteram_w )
{
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword, data);
	
	int r = (newword >> 10) & 0x1f;
	int g = (newword >>  5) & 0x1f;
	int b = (newword      ) & 0x1f;
	
	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);
	
	WRITE_WORD(&paletteram[offset], newword);
	palette_change_color((offset / 2) & (pixel_mask | palette_mask), r, g, b);
}



/*************************************
 *
 *	DMA drawing routines
 *
 *************************************/

/*** constant definitions ***/
#define	PIXEL_SKIP		0
#define PIXEL_COLOR		1
#define PIXEL_COPY		2

#define XFLIP_NO		0
#define XFLIP_YES		1


typedef void (*dma_draw_func)(void);


/*** core blitter routine macro ***/
#define DMA_DRAW_FUNC_BODY(name, xflip, zero, nonzero)				 			\
{																				\
	int height = dma_state.height;												\
	int width = dma_state.width;												\
	UINT8 *base = wms_gfx_rom;													\
	UINT32 offset = dma_state.offset >> 3;										\
	UINT16 pal = dma_state.palette;												\
	UINT16 color = pal | dma_state.color;										\
	int x, y;																	\
																				\
	/* loop over the height */													\
	for (y = 0; y < height; y++)												\
	{																			\
		int tx = dma_state.xpos;												\
		int ty = dma_state.ypos;												\
		UINT32 o = offset;														\
		UINT16 *d;																\
																				\
		/* determine Y position */												\
		ty = (ty + y) & 0x1ff;													\
		offset += dma_state.rowbytes;											\
																				\
		/* determine destination pointer */										\
		d = &local_videoram[ty * 512 + tx];										\
																				\
		/* loop until we draw the entire width */								\
		for (x = 0; x < width; x++, o++)										\
		{																		\
			/* special case similar handling of zero/non-zero */				\
			if (zero == nonzero)												\
			{																	\
				if (zero == PIXEL_COLOR)										\
					*d = color;													\
				else if (zero == PIXEL_COPY)									\
					*d = base[o] | pal;											\
			}																	\
																				\
			/* otherwise, read the pixel and look */							\
			else																\
			{																	\
				int pixel = base[o];											\
																				\
				/* non-zero pixel case */										\
				if (pixel)														\
				{																\
					if (nonzero == PIXEL_COLOR)									\
						*d = color;												\
					else if (nonzero == PIXEL_COPY)								\
						*d = pixel | pal;										\
				}																\
																				\
				/* zero pixel case */											\
				else															\
				{																\
					if (zero == PIXEL_COLOR)									\
						*d = color;												\
					else if (zero == PIXEL_COPY)								\
						*d = pal;												\
				}																\
			}																	\
																				\
			/* update pointers */												\
			if (xflip) d--;														\
			else d++;															\
		}																		\
	}																			\
}

/*** slightly simplified one for most blitters ***/
#define DMA_DRAW_FUNC(name, xflip, zero, nonzero)						\
static void name(void)													\
{																		\
	DMA_DRAW_FUNC_BODY(name, xflip, zero, nonzero)						\
}

/*** empty blitter ***/
static void dma_draw_none(void)
{
}

/*** super macro for declaring an entire blitter family ***/
#define DECLARE_BLITTER_SET(prefix)										\
DMA_DRAW_FUNC(prefix##_p0,      XFLIP_NO,  PIXEL_COPY,  PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_p1,      XFLIP_NO,  PIXEL_SKIP,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0,      XFLIP_NO,  PIXEL_COLOR, PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_c1,      XFLIP_NO,  PIXEL_SKIP,  PIXEL_COLOR)	\
DMA_DRAW_FUNC(prefix##_p0p1,    XFLIP_NO,  PIXEL_COPY,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0c1,    XFLIP_NO,  PIXEL_COLOR, PIXEL_COLOR)	\
DMA_DRAW_FUNC(prefix##_c0p1,    XFLIP_NO,  PIXEL_COLOR, PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_p0c1,    XFLIP_NO,  PIXEL_COPY,  PIXEL_COLOR)	\
																		\
DMA_DRAW_FUNC(prefix##_p0_xf,   XFLIP_YES, PIXEL_COPY,  PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_p1_xf,   XFLIP_YES, PIXEL_SKIP,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0_xf,   XFLIP_YES, PIXEL_COLOR, PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_c1_xf,   XFLIP_YES, PIXEL_SKIP,  PIXEL_COLOR)	\
DMA_DRAW_FUNC(prefix##_p0p1_xf, XFLIP_YES, PIXEL_COPY,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0c1_xf, XFLIP_YES, PIXEL_COLOR, PIXEL_COLOR)	\
DMA_DRAW_FUNC(prefix##_c0p1_xf, XFLIP_YES, PIXEL_COLOR, PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_p0c1_xf, XFLIP_YES, PIXEL_COPY,  PIXEL_COLOR)	\
																												\
static dma_draw_func prefix[32] =																				\
{																												\
/*	B0:N / B1:N			B0:Y / B1:N			B0:N / B1:Y			B0:Y / B1:Y */									\
	dma_draw_none,		prefix##_p0,		prefix##_p1,		prefix##_p0p1,		/* no color */ 				\
	prefix##_c0,		prefix##_c0,		prefix##_c0p1,		prefix##_c0p1,		/* color 0 pixels */		\
	prefix##_c1,		prefix##_p0c1,		prefix##_c1,		prefix##_p0c1,		/* color non-0 pixels */	\
	prefix##_c0c1,		prefix##_c0c1,		prefix##_c0c1,		prefix##_c0c1,		/* fill */ 					\
																												\
	dma_draw_none,		prefix##_p0_xf,		prefix##_p1_xf,		prefix##_p0p1_xf,	/* no color */ 				\
	prefix##_c0_xf,		prefix##_c0_xf,		prefix##_c0p1_xf,	prefix##_c0p1_xf,	/* color 0 pixels */ 		\
	prefix##_c1_xf,		prefix##_p0c1_xf,	prefix##_c1_xf,		prefix##_p0c1_xf,	/* color non-0 pixels */	\
	prefix##_c0c1_xf,	prefix##_c0c1_xf,	prefix##_c0c1_xf,	prefix##_c0c1_xf	/* fill */ 					\
};

/* allow for custom blitters */
#ifdef WMS_YUNIT_CUSTOM_BLITTERS
#include "wmsyblit.c"
#endif

/*** blitter family declarations ***/
DECLARE_BLITTER_SET(dma_draw)



/*************************************
 *
 *	DMA finished callback
 *
 *************************************/

static int temp_irq_callback(int irqline)
{
	tms34010_set_irq_line(0, CLEAR_LINE);
	return 0;
}


static void dma_callback(int is_in_34010_context)
{
	dma_register[DMA_COMMAND] &= ~0x8000; /* tell the cpu we're done */
	if (is_in_34010_context)
	{
		tms34010_set_irq_callback(temp_irq_callback);
		tms34010_set_irq_line(0, ASSERT_LINE);
	}
	else
		cpu_cause_interrupt(0,TMS34010_INT1);
}



/*************************************
 *
 *	DMA reader
 *
 *************************************/

READ_HANDLER( wms_yunit_dma_r )
{
	int result = READ_WORD(&dma_register[offset / 2]);
	
#if !FAST_DMA
	/* see if we're done */
	if (oofset / 2 == DMA_COMMAND && (result & 0x8000))
		switch (cpu_get_pc())
		{
			case 0xfff7aa20: /* narc */
			case 0xffe1c970: /* trog */
			case 0xffe1c9a0: /* trog3 */
			case 0xffe1d4a0: /* trogp */
			case 0xffe07690: /* smashtv */
			case 0xffe00450: /* hiimpact */
			case 0xffe14930: /* strkforc */
			case 0xffe02c20: /* strkforc */
			case 0xffc79890: /* mk */
			case 0xffc7a5a0: /* mk */
			case 0xffc063b0: /* term2 */
			case 0xffc00720: /* term2 */
			case 0xffc07a60: /* totcarn/totcarnp */
			case 0xff805200: /* mk2 */
			case 0xff8044e0: /* mk2 */
			case 0xff82e200: /* nbajam */
				cpu_spinuntil_int();
				break;
		}
#endif

	return result;
}



/*************************************
 *
 *	DMA write handler
 *
 *************************************/

/*
 * DMA registers
 * ------------------
 *
 *  Register | Bit              | Use
 * ----------+-FEDCBA9876543210-+------------
 *     0     | x--------------- | trigger write (or clear if zero)
 *           | ---184-1-------- | unknown
 *           | ----------x----- | flip y
 *           | -----------x---- | flip x
 *           | ------------x--- | blit nonzero pixels as color
 *           | -------------x-- | blit zero pixels as color
 *           | --------------x- | blit nonzero pixels
 *           | ---------------x | blit zero pixels
 *     1     | xxxxxxxxxxxxxxxx | width offset
 *     2     | xxxxxxxxxxxxxxxx | source address low word
 *     3     | xxxxxxxxxxxxxxxx | source address high word
 *     4     | xxxxxxxxxxxxxxxx | detination x
 *     5     | xxxxxxxxxxxxxxxx | destination y
 *     6     | xxxxxxxxxxxxxxxx | image columns
 *     7     | xxxxxxxxxxxxxxxx | image rows
 *     8     | xxxxxxxxxxxxxxxx | palette
 *     9     | xxxxxxxxxxxxxxxx | color
 */

WRITE_HANDLER( wms_yunit_dma_w )
{
	UINT32 gfxoffset;
	int command;
	
	/* blend with the current register contents */
	offset /= 2;
	COMBINE_WORD_MEM(&dma_register[offset], data);
	
	/* only writes to DMA_COMMAND actually cause actions */
	if (offset != DMA_COMMAND)
		return;

	/* high bit triggers action */
	command = dma_register[DMA_COMMAND];
	if (!(command & 0x8000))
	{
		tms34010_set_irq_line(0, CLEAR_LINE);
		return;
	}

#if LOG_DMA
	if (keyboard_pressed(KEYCODE_L))
	{
		logerror("----\n");
		logerror("DMA command %04X: (xflip=%d yflip=%d)\n",
				command, (command >> 4) & 1, (command >> 5) & 1);
		logerror("  offset=%08X pos=(%d,%d) w=%d h=%d\n", 
				dma_register[DMA_OFFSETLO] | (dma_register[DMA_OFFSETHI] << 16),
				(INT16)dma_register[DMA_XSTART], (INT16)dma_register[DMA_YSTART],
				dma_register[DMA_WIDTH], dma_register[DMA_HEIGHT]);
		logerror("  palette=%04X color=%04X\n",
				dma_register[DMA_PALETTE], dma_register[DMA_COLOR]);
	}
#endif
	
	profiler_mark(PROFILER_USER1);

	/* fill in the basic data */
	dma_state.rowbytes = (INT16)dma_register[DMA_ROWBYTES];
	dma_state.xpos = dma_register[DMA_XSTART] & 0x1ff;
	dma_state.ypos = dma_register[DMA_YSTART] & 0x1ff;
	dma_state.width = dma_register[DMA_WIDTH];
	dma_state.height = dma_register[DMA_HEIGHT];
	dma_state.palette = palette_lookup[dma_register[DMA_PALETTE] & 0xff];
	dma_state.color = dma_register[DMA_COLOR] & pixel_mask;
	
	/* determine the offset and adjust the rowbytes */
	gfxoffset = dma_register[DMA_OFFSETLO] | (dma_register[DMA_OFFSETHI] << 16);
	if (command & 0x10)
	{
		gfxoffset -= (dma_state.width - 1) * 8;
		dma_state.rowbytes = (dma_state.rowbytes - dma_state.width + 3) & ~3;
		dma_state.xpos += dma_state.width - 1;
	}
	else
		dma_state.rowbytes = (dma_state.rowbytes + dma_state.width + 3) & ~3;
	
	/* apply Y clipping */
	if (dma_state.ypos + dma_state.height > 512)
		dma_state.height = 512 - dma_state.ypos;
		
	/* special case: drawing mode C doesn't need to know about any pixel data */
	/* shimpact relies on this behavior */
	if ((command & 0x0f) == 0x0c)
		gfxoffset = 0;
	
	/* determine the location and draw */
	if (gfxoffset < 0x02000000)
		gfxoffset += 0x02000000;
	if (gfxoffset < 0x06000000)
	{
		dma_state.offset = gfxoffset - 0x02000000;
		(*dma_draw[command & 0x1f])();
	}

	/* signal we're done */
	if (FAST_DMA)
		dma_callback(1);
	else
		timer_set(TIME_IN_NSEC(41 * dma_state.width * dma_state.height), 0, dma_callback);

	profiler_mark(PROFILER_END);
}



/*************************************
 *
 *	Partial screen updater
 *
 *************************************/

static void update_partial(int scanline)
{
	int v, width, xoffs, copying;
	UINT32 offset;
	
	/* determine if we need to copy these scanlines into another buffer */
	copying = (!page_flipping && !skipping_this_frame);
	
	/* don't draw if we're already past the bottom of the screen */
	if (last_update_scanline >= Machine->visible_area.max_y)
		return;

	/* don't start before the top of the screen */
	if (last_update_scanline < Machine->visible_area.min_y)
		last_update_scanline = Machine->visible_area.min_y;

	/* bail if there's nothing to do */
	if (last_update_scanline > scanline)
		return;
	
	/* don't draw past the bottom scanline */
	if (scanline > Machine->visible_area.max_y)
		scanline = Machine->visible_area.max_y;

	/* determine the base of the videoram */
	offset = (~tms34010_get_DPYSTRT(0) & 0x1ff0) << 5;
	offset += 512 * (last_update_scanline - Machine->visible_area.min_y);
	offset &= 0x3ffff;

	/* determine how many pixels to copy */
	xoffs = Machine->visible_area.min_x;
	width = Machine->visible_area.max_x - xoffs + 1;
	offset += xoffs;

	/* loop over rows */
	for (v = last_update_scanline; v <= scanline; v++)
	{
		/* if we're not page flipping, and we're not skipping this frame, copy to the lookaside buffer */
		if (!page_flipping && !skipping_this_frame)
			memcpy(&local_videoram_copy[v * 512 + xoffs], &local_videoram[offset], width * sizeof(UINT16));

		/* if we're not page flipping, do autoerase immediately behind us */
		if (autoerase_enable)
		{
			if (!page_flipping)
				memcpy(&local_videoram[offset], &local_videoram[510 * 512], width * sizeof(UINT16));
			else
				autoerase_list[autoerase_count++] = offset;
		}

		/* point to the next row */
		offset = (offset + 512) & 0x3ffff;
	}

	/* remember where we left off */
	last_update_scanline = scanline + 1;
}



/*************************************
 *
 *	Screen updater
 *
 *************************************/

static void update_screen(struct osd_bitmap *bitmap)
{
	UINT16 *pens = Machine->pens;
	UINT16 *buffer;
	int v, h, width, xoffs;
	UINT32 offset;

	/* determine the base of the videoram */
	if (page_flipping)
	{
		buffer = local_videoram;
		offset = ((~tms34010_get_DPYSTRT(0) & 0x1ff0) << 5) & 0x3ffff;
	}
	else
	{
		buffer = local_videoram_copy;
		offset = Machine->visible_area.min_y * 512;
	}

	/* determine how many pixels to copy */
	xoffs = Machine->visible_area.min_x;
	width = Machine->visible_area.max_x - xoffs + 1;
	offset += xoffs;

	/* 16-bit refresh case */
	if (bitmap->depth == 16)
	{
		/* loop over rows */
		for (v = Machine->visible_area.min_y; v <= Machine->visible_area.max_y; v++)
		{
			/* handle the refresh */
			UINT16 *src = &buffer[offset];
			UINT16 *dst = &((UINT16 *)bitmap->line[v])[xoffs];
			UINT32 diff = dst - src;

			/* copy one row */
			for (h = 0; h < width; h++, src++)
				*(src + diff) = pens[*src];

			/* point to the next row */
			offset = (offset + 512) & 0x3ffff;
		}
	}

	/* 8-bit refresh case */
	else
	{
		/* loop over rows */
		for (v = Machine->visible_area.min_y; v <= Machine->visible_area.max_y; v++)
		{
			/* handle the refresh */
			UINT16 *src = &buffer[offset];
			UINT8 *dst = &bitmap->line[v][xoffs];

			for (h = 0; h < width; h++)
				*dst++ = pens[*src++];

			/* point to the next row */
			offset = (offset + 512) & 0x3ffff;
		}
	}
}



/*************************************
 *
 *	34010 display address callback
 *
 *************************************/

void wms_yunit_display_addr_changed(UINT32 offs, int rowbytes, int scanline)
{
	/* if nothing changed, nothing to do */
	if (offs == last_display_addr)
		return;
	
	/* attempt to detect page flipping case; this value decays by 1 each frame */
	if (last_display_addr != 0 && ((offs ^ last_display_addr) & 0x00100000))
		page_flipping = 2;
	last_display_addr = offs;
	//logerror("Display address = %08X\n", offs);
}



/*************************************
 *
 *	34010 interrupt callback
 *
 *************************************/

void wms_yunit_display_interrupt(int scanline)
{
	update_partial(scanline + wms_partial_update_offset);
}



/*************************************
 *
 *	End-of-frame update
 *
 *************************************/

void wms_yunit_vh_eof(void)
{
	int width = Machine->visible_area.max_x - Machine->visible_area.min_x + 1;
	int i;
	
	/* we require them to keep flipping in order to stay in this mode */
	if (page_flipping)
		page_flipping--;

	/* finish updating */
	update_partial(Machine->visible_area.max_y);
	last_update_scanline = 0;
	
	/* handle any autoerase */
	for (i = 0; i < autoerase_count; i++)
		memcpy(&local_videoram[autoerase_list[i]], &local_videoram[510 * 512], width * sizeof(UINT16));
	autoerase_count = 0;
	
	/* determine if we're going to skip this upcoming frame */
	skipping_this_frame = osd_skip_this_frame();
}

void wms_zunit_vh_eof(void)
{
	/* turn off the autoerase (NARC needs this) */
	wms_yunit_vh_eof();
	autoerase_enable = 0;
}



/*************************************
 *
 *	Core refresh routine
 *
 *************************************/

void wms_yunit_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	/* adjust the visible area if we haven't yet */
	if (wms_visible_area.max_x != 0)
	{
		set_visible_area(wms_visible_area.min_x, wms_visible_area.max_x, wms_visible_area.min_y, wms_visible_area.max_y);
		wms_visible_area.max_x = 0;
	}

	/* update the palette */
	palette_recalc();

	/* finish updating */
	update_partial(Machine->visible_area.max_y);
	update_screen(bitmap);
}
