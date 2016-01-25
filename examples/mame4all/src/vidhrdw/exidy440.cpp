/***************************************************************************

	Exidy 440 video system

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


#ifndef INCLUDE_DRAW_CORE


#define SPRITE_COUNT		40
#define SPRITERAM_SIZE		(SPRITE_COUNT * 4)
#define CHUNK_SIZE			8
#define MAX_SCANLINE		240
#define TOTAL_CHUNKS		(MAX_SCANLINE / CHUNK_SIZE)


/* external globals */
extern UINT8 exidy440_bank;
extern UINT8 exidy440_topsecret;


/* globals */
UINT8 *exidy440_scanline;
UINT8 *exidy440_imageram;
UINT8 exidy440_firq_vblank;
UINT8 exidy440_firq_beam;
UINT8 topsecex_yscroll;

/* local allocated storage */
static UINT8 exidy440_latched_x;
static UINT8 *local_videoram;
static UINT8 *local_paletteram;
static UINT8 *scanline_dirty;
static UINT8 *spriteram_buffer;

/* local variables */
static UINT8 firq_enable;
static UINT8 firq_select;
static UINT8 palettebank_io;
static UINT8 palettebank_vis;
static UINT8 topsecex_last_yscroll;

/* function prototypes */
void exidy440_vh_stop(void);
void exidy440_update_firq(void);
void exidy440_update_callback(int param);

static void scanline_callback(int param);

static void update_screen_8(struct osd_bitmap *bitmap, int scroll_offset);
static void update_screen_16(struct osd_bitmap *bitmap, int scroll_offset);



/*************************************
 *
 *	Initialize the video system
 *
 *************************************/

int exidy440_vh_start(void)
{
	/* reset the system */
	firq_enable = 0;
	firq_select = 0;
	palettebank_io = 0;
	palettebank_vis = 0;
	exidy440_firq_vblank = 0;
	exidy440_firq_beam = 0;

	/* reset Top Secret variables */
	topsecex_yscroll = 0;
	topsecex_last_yscroll = 0;

	/* allocate a buffer for VRAM */
	local_videoram = (UINT8*)malloc(256 * 256 * 2);
	if (!local_videoram)
	{
		exidy440_vh_stop();
		return 1;
	}

	/* clear it */
	memset(local_videoram, 0, 256 * 256 * 2);

	/* allocate a buffer for palette RAM */
	local_paletteram = (UINT8*)malloc(512 * 2);
	if (!local_paletteram)
	{
		exidy440_vh_stop();
		return 1;
	}

	/* clear it */
	memset(local_paletteram, 0, 512 * 2);

	/* allocate a scanline dirty array */
	scanline_dirty = (UINT8*)malloc(256);
	if (!scanline_dirty)
	{
		exidy440_vh_stop();
		return 1;
	}

	/* mark everything dirty to start */
	memset(scanline_dirty, 1, 256);

	/* allocate a sprite cache */
	spriteram_buffer = (UINT8*)malloc(SPRITERAM_SIZE * TOTAL_CHUNKS);
	if (!spriteram_buffer)
	{
		exidy440_vh_stop();
		return 1;
	}

	/* start the scanline timer */
	timer_set(TIME_NOW, 0, scanline_callback);

	return 0;
}



/*************************************
 *
 *	Tear down the video system
 *
 *************************************/

void exidy440_vh_stop(void)
{
	/* free VRAM */
	if (local_videoram)
		free(local_videoram);
	local_videoram = NULL;

	/* free palette RAM */
	if (local_paletteram)
		free(local_paletteram);
	local_paletteram = NULL;

	/* free the scanline dirty array */
	if (scanline_dirty)
		free(scanline_dirty);
	scanline_dirty = NULL;

	/* free the sprite cache */
	if (spriteram_buffer)
		free(spriteram_buffer);
	spriteram_buffer = NULL;
}



/*************************************
 *
 *	Periodic scanline update
 *
 *************************************/

static void scanline_callback(int scanline)
{
	/* copy the spriteram */
	memcpy(spriteram_buffer + SPRITERAM_SIZE * (scanline / CHUNK_SIZE), spriteram, SPRITERAM_SIZE);

	/* fire after the next 8 scanlines */
	scanline += CHUNK_SIZE;
	if (scanline >= MAX_SCANLINE)
		scanline = 0;
	timer_set(cpu_getscanlinetime(scanline), scanline, scanline_callback);
}



/*************************************
 *
 *	Video RAM read/write
 *
 *************************************/

READ_HANDLER( exidy440_videoram_r )
{
	UINT8 *base = &local_videoram[(*exidy440_scanline * 256 + offset) * 2];

	/* combine the two pixel values into one byte */
	return (base[0] << 4) | base[1];
}


WRITE_HANDLER( exidy440_videoram_w )
{
	UINT8 *base = &local_videoram[(*exidy440_scanline * 256 + offset) * 2];

	/* expand the two pixel values into two bytes */
	base[0] = (data >> 4) & 15;
	base[1] = data & 15;

	/* mark the scanline dirty */
	scanline_dirty[*exidy440_scanline] = 1;
}



/*************************************
 *
 *	Palette RAM read/write
 *
 *************************************/

READ_HANDLER( exidy440_paletteram_r )
{
	return local_paletteram[palettebank_io * 512 + offset];
}


WRITE_HANDLER( exidy440_paletteram_w )
{
	/* update palette ram in the I/O bank */
	local_paletteram[palettebank_io * 512 + offset] = data;

	/* if we're modifying the active palette, change the color immediately */
	if (palettebank_io == palettebank_vis)
	{
		int word;

		/* combine two bytes into a word */
		offset = palettebank_vis * 512 + (offset & 0x1fe);
		word = (local_paletteram[offset] << 8) + local_paletteram[offset + 1];

		/* extract the 5-5-5 RGB colors */
		palette_change_color(offset / 2, ((word >> 10) & 31) << 3, ((word >> 5) & 31) << 3, (word & 31) << 3);
	}
}



/*************************************
 *
 *	Horizontal/vertical positions
 *
 *************************************/

READ_HANDLER( exidy440_horizontal_pos_r )
{
	/* clear the FIRQ on a read here */
	exidy440_firq_beam = 0;
	exidy440_update_firq();

	/* according to the schems, this value is only latched on an FIRQ
	 * caused by collision or beam */
	return exidy440_latched_x;
}


READ_HANDLER( exidy440_vertical_pos_r )
{
	int result;

	/* according to the schems, this value is latched on any FIRQ
	 * caused by collision or beam, ORed together with CHRCLK,
	 * which probably goes off once per scanline; for now, we just
	 * always return the current scanline */
	result = cpu_getscanline();
	return (result < 255) ? result : 255;
}



/*************************************
 *
 *	Interrupt and I/O control regs
 *
 *************************************/

WRITE_HANDLER( exidy440_control_w )
{
	int oldvis = palettebank_vis;

	/* extract the various bits */
	exidy440_bank = data >> 4;
	firq_enable = (data >> 3) & 1;
	firq_select = (data >> 2) & 1;
	palettebank_io = (data >> 1) & 1;
	palettebank_vis = data & 1;

	/* set the memory bank for the main CPU from the upper 4 bits */
	cpu_setbank(1, &memory_region(REGION_CPU1)[0x10000 + exidy440_bank * 0x4000]);

	/* update the FIRQ in case we enabled something */
	exidy440_update_firq();

	/* if we're swapping palettes, change all the colors */
	if (oldvis != palettebank_vis)
	{
		int i;

		/* pick colors from the visible bank */
		offset = palettebank_vis * 512;
		for (i = 0; i < 256; i++, offset += 2)
		{
			/* extract a word and the 5-5-5 RGB components */
			int word = (local_paletteram[offset] << 8) + local_paletteram[offset + 1];
			palette_change_color(i, ((word >> 10) & 31) << 3, ((word >> 5) & 31) << 3, (word & 31) << 3);
		}
	}
}


WRITE_HANDLER( exidy440_interrupt_clear_w )
{
	/* clear the VBLANK FIRQ on a write here */
	exidy440_firq_vblank = 0;
	exidy440_update_firq();
}



/*************************************
 *
 *	Interrupt generators
 *
 *************************************/

void exidy440_update_firq(void)
{
	if (exidy440_firq_vblank || (firq_enable && exidy440_firq_beam))
		cpu_set_irq_line(0, 1, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 1, CLEAR_LINE);
}


int exidy440_vblank_interrupt(void)
{
	/* set the FIRQ line on a VBLANK */
	exidy440_firq_vblank = 1;
	exidy440_update_firq();

	/* allocate a timer to go off just before the refresh (but not for Top Secret */
	if (!exidy440_topsecret)
		timer_set(TIME_IN_USEC(Machine->drv->vblank_duration - 50), 0, exidy440_update_callback);

	return 0;
}



/*************************************
 *
 *	IRQ callback handlers
 *
 *************************************/

void beam_firq_callback(int param)
{
	/* generate the interrupt, if we're selected */
	if (firq_select && firq_enable)
	{
		exidy440_firq_beam = 1;
		exidy440_update_firq();
	}

	/* round the x value to the nearest byte */
	param = (param + 1) / 2;

	/* latch the x value; this convolution comes from the read routine */
	exidy440_latched_x = (param + 3) ^ 2;
}


void collide_firq_callback(int param)
{
	/* generate the interrupt, if we're selected */
	if (!firq_select && firq_enable)
	{
		exidy440_firq_beam = 1;
		exidy440_update_firq();
	}

	/* round the x value to the nearest byte */
	param = (param + 1) / 2;

	/* latch the x value; this convolution comes from the read routine */
	exidy440_latched_x = (param + 3) ^ 2;
}



/*************************************
 *
 *	Determine the time when the beam
 *	will intersect a given pixel
 *
 *************************************/

timer_tm compute_pixel_time(int x, int y)
{
	/* assuming this is called at refresh time, compute how long until we
	 * hit the given x,y position */
	return cpu_getscanlinetime(y) + (cpu_getscanlineperiod() * (timer_tm)x * (TIME_ONE_SEC / 320));
}



/*************************************
 *
 *	Update handling for shooters
 *
 *************************************/

void exidy440_update_callback(int param)
{
	/* note: we do most of the work here, because collision detection and beam detection need
		to happen at 60Hz, whether or not we're frameskipping; in order to do those, we pretty
		much need to do all the update handling */

	struct osd_bitmap *bitmap = Machine->scrbitmap;

	int y, i;
	int xoffs, yoffs;
	timer_tm time, increment;
	int beamx, beamy;

	/* make sure color 256 is white for our crosshair */
	palette_change_color(256, 0xff, 0xff, 0xff);

	/* redraw the screen */
	if (bitmap->depth == 8)
		update_screen_8(bitmap, 0);
	else
		update_screen_16(bitmap, 0);

	/* update the analog x,y values */
	beamx = ((input_port_4_r(0) & 0xff) * 320) >> 8;
	beamy = ((input_port_5_r(0) & 0xff) * 240) >> 8;

	/* The timing of this FIRQ is very important. The games look for an FIRQ
		and then wait about 650 cycles, clear the old FIRQ, and wait a
		very short period of time (~130 cycles) for another one to come in.
		From this, it appears that they are expecting to get beams over
		a 12 scanline period, and trying to pick roughly the middle one.
		This is how it is implemented. */
	increment = cpu_getscanlineperiod();
	time = compute_pixel_time(beamx, beamy) - increment * 6;
	for (i = 0; i <= 12; i++, time += increment)
		timer_set(time, beamx, beam_firq_callback);

	/* draw a crosshair */
	xoffs = beamx - 3;
	yoffs = beamy - 3;
	for (y = -3; y <= 3; y++, yoffs++, xoffs++)
	{
		if (yoffs >= 0 && yoffs < 240 && beamx >= 0 && beamx < 320)
		{
			plot_pixel(bitmap, beamx, yoffs, Machine->pens[256]);
			scanline_dirty[yoffs] = 1;
		}
		if (xoffs >= 0 && xoffs < 320 && beamy >= 0 && beamy < 240)
			plot_pixel(bitmap, xoffs, beamy, Machine->pens[256]);
	}
}



/*************************************
 *
 *	Standard screen refresh callback
 *
 *************************************/

void exidy440_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	/* if we need a full refresh, mark all scanlines dirty */
	if (full_refresh)
		memset(scanline_dirty, 1, 256);

	/* if we're Top Secret, do our refresh here; others are done in the update function above */
	if (exidy440_topsecret)
	{
		/* if the scroll changed, mark everything dirty */
		if (topsecex_yscroll != topsecex_last_yscroll)
		{
			topsecex_last_yscroll = topsecex_yscroll;
			memset(scanline_dirty, 1, 256);
		}

		/* redraw the screen */
		if (bitmap->depth == 8)
			update_screen_8(bitmap, topsecex_yscroll);
		else
			update_screen_16(bitmap, topsecex_yscroll);
	}
}


/*************************************
 *
 *		Depth-specific refresh
 *
 *************************************/

#define ADJUST_FOR_ORIENTATION(orientation, bitmap, dst, x, y, xadv)	\
	if (orientation)													\
	{																	\
		int dy = bitmap->line[1] - bitmap->line[0];						\
		int tx = x, ty = y, temp;										\
		if (orientation & ORIENTATION_SWAP_XY)							\
		{																\
			temp = tx; tx = ty; ty = temp;								\
			xadv = dy / (bitmap->depth / 8);							\
		}																\
		if (orientation & ORIENTATION_FLIP_X)							\
		{																\
			tx = bitmap->width - 1 - tx;								\
			if (!(orientation & ORIENTATION_SWAP_XY)) xadv = -xadv;		\
		}																\
		if (orientation & ORIENTATION_FLIP_Y)							\
		{																\
			ty = bitmap->height - 1 - ty;								\
			if ((orientation & ORIENTATION_SWAP_XY)) xadv = -xadv;		\
		}																\
		/* can't lookup line because it may be negative! */				\
		dst = (TYPE *)(bitmap->line[0] + dy * ty) + tx;					\
	}

#define INCLUDE_DRAW_CORE

#define DRAW_FUNC update_screen_8
#define TYPE UINT8
#include "exidy440.cpp"
#undef TYPE
#undef DRAW_FUNC

#define DRAW_FUNC update_screen_16
#define TYPE UINT16
#include "exidy440.cpp"
#undef TYPE
#undef DRAW_FUNC


#else


/*************************************
 *
 *		Core refresh routine
 *
 *************************************/

void DRAW_FUNC(struct osd_bitmap *bitmap, int scroll_offset)
{
	int orientation = Machine->orientation;
	int xoffs, yoffs, count, scanline;
	int x, y, i, sy;
	UINT8 *palette;
	UINT8 *sprite;

	/* recompute the palette, and mark all scanlines dirty if we need to redraw */
	if (palette_recalc())
		memset(scanline_dirty, 1, 256);

	/* draw any dirty scanlines from the VRAM directly */
	sy = scroll_offset;
	for (y = 0; y < 240; y++, sy++)
	{
		/* wrap at the bottom of the screen */
		if (sy >= 240)
			sy -= 240;

		/* only redraw if dirty */
		if (scanline_dirty[sy])
		{
			UINT8 *src = &local_videoram[sy * 512];
			TYPE *dst = (TYPE *)bitmap->line[y];
			int xadv = 1;

			/* adjust if we're oriented oddly */
			ADJUST_FOR_ORIENTATION(orientation, bitmap, dst, 0, y, xadv);

			/* redraw the scanline */
			for (x = 0; x < 320; x++, dst += xadv)
				*dst = Machine->pens[*src++];
			scanline_dirty[sy] = 0;
		}
	}

	/* get a pointer to the palette to look for collision flags */
	palette = &local_paletteram[palettebank_vis * 512];

	/* start the count high for topsecret, which doesn't use collision flags */
	count = exidy440_topsecret ? 128 : 0;

	/* draw the sprite images, checking for collisions along the way */
	for (scanline = 0; scanline < MAX_SCANLINE; scanline += CHUNK_SIZE)
	{
		sprite = spriteram_buffer + SPRITERAM_SIZE * (scanline / CHUNK_SIZE) + (SPRITE_COUNT - 1) * 4;
		for (i = 0; i < SPRITE_COUNT; i++, sprite -= 4)
		{
			UINT8 *src;
			int image = (~sprite[3] & 0x3f);
			xoffs = (~((sprite[1] << 8) | sprite[2]) & 0x1ff);
			yoffs = (~sprite[0] & 0xff) + 1;

			/* skip if out of range */
			if (yoffs < scanline || yoffs >= scanline + 16 + CHUNK_SIZE - 1)
				continue;

			/* get a pointer to the source image */
			src = &exidy440_imageram[image * 128];

			/* account for large positive offsets meaning small negative values */
			if (xoffs >= 0x1ff - 16)
				xoffs -= 0x1ff;

			/* loop over y */
			sy = yoffs + scroll_offset;
			for (y = 0; y < 16; y++, yoffs--, sy--)
			{
				/* wrap at the top and bottom of the screen */
				if (sy >= 240)
					sy -= 240;
				else if (sy < 0)
					sy += 240;

				/* stop if we get before the current scanline */
				if (yoffs < scanline)
					break;

				/* only draw scanlines that are in this chunk */
				if (yoffs < scanline + CHUNK_SIZE)
				{
					UINT8 *old = &local_videoram[sy * 512 + xoffs];
					TYPE *dst = &((TYPE *)bitmap->line[yoffs])[xoffs];
					int currx = xoffs, xadv = 1;

					/* adjust if we're oriented oddly */
					ADJUST_FOR_ORIENTATION(orientation, bitmap, dst, xoffs, yoffs, xadv);

					/* mark this scanline dirty */
					scanline_dirty[sy] = 1;

					/* loop over x */
					for (x = 0; x < 8; x++, dst += xadv * 2, old += 2)
					{
						int ipixel = *src++;
						int left = ipixel & 0xf0;
						int right = (ipixel << 4) & 0xf0;
						int pen;

						/* left pixel */
						if (left && currx >= 0 && currx < 320)
						{
							/* combine with the background */
							pen = left | old[0];
							dst[0] = Machine->pens[pen];

							/* check the collisions bit */
							if ((palette[2 * pen] & 0x80) && count++ < 128)
								timer_set(compute_pixel_time(currx, yoffs), currx, collide_firq_callback);
						}
						currx++;

						/* right pixel */
						if (right && currx >= 0 && currx < 320)
						{
							/* combine with the background */
							pen = right | old[1];
							dst[xadv] = Machine->pens[pen];

							/* check the collisions bit */
							if ((palette[2 * pen] & 0x80) && count++ < 128)
								timer_set(compute_pixel_time(currx, yoffs), currx, collide_firq_callback);
						}
						currx++;
					}
				}
				else
					src += 8;
			}
		}
	}
}

#endif
