/***************************************************************************

	Cinemat/Leland driver

	Leland video hardware
	driver by Aaron Giles and Paul Leaman

***************************************************************************/

#ifndef INCLUDE_DRAW_CORE

#include "driver.h"
#include "vidhrdw/generic.h"

#include "osdepend.h"


/* constants */
#define VRAM_LO 	0x00000
#define VRAM_HI 	0x08000
#define VRAM_SIZE	0x10000

#define QRAM_SIZE	0x10000

#define VIDEO_WIDTH  0x28
#define VIDEO_HEIGHT 0x1e


/* debugging */
#define LOG_COMM	0



struct vram_state_data
{
	UINT16	addr;
	UINT8	plane;
	UINT8	latch[2];
};

struct scroll_position
{
	UINT16 	scanline;
	UINT16 	x, y;
	UINT8 	gfxbank;
};


/* video RAM */
UINT8 *leland_video_ram;
UINT8 *ataxx_qram;
UINT8 leland_last_scanline_int;

/* video RAM bitmap drawing */
static struct vram_state_data vram_state[2];
static UINT8 sync_next_write;

/* partial screen updating */
static UINT8 *video_ram_copy;
static int next_update_scanline;

/* scroll background registers */
static UINT16 xscroll;
static UINT16 yscroll;
static UINT8 gfxbank;
static UINT8 scroll_index;
static struct scroll_position scroll_pos[VIDEO_HEIGHT];

static UINT32 *ataxx_pen_usage;


/* sound routines */
extern UINT8 leland_dac_control;

extern void leland_dac_update(int indx, UINT8 *base);

/* bitmap blending routines */
static void draw_bitmap_8(struct osd_bitmap *bitmap);
static void draw_bitmap_16(struct osd_bitmap *bitmap);



/*************************************
 *
 *	Start video hardware
 *
 *************************************/

int leland_vh_start(void)
{
	void leland_vh_stop(void);

	/* allocate memory */
    leland_video_ram = (UINT8*)malloc(VRAM_SIZE);
    video_ram_copy = (UINT8*)malloc(VRAM_SIZE);

	/* error cases */
    if (!leland_video_ram || !video_ram_copy)
    {
    	leland_vh_stop();
		return 1;
	}

	/* reset videoram */
    memset(leland_video_ram, 0, VRAM_SIZE);
    memset(video_ram_copy, 0, VRAM_SIZE);

	/* reset scrolling */
	scroll_index = 0;
	memset(scroll_pos, 0, sizeof(scroll_pos));

	return 0;
}


int ataxx_vh_start(void)
{
	void ataxx_vh_stop(void);

	const struct GfxElement *gfx = Machine->gfx[0];
	UINT32 usage[2];
	int i, x, y;

	/* first do the standard stuff */
	if (leland_vh_start())
		return 1;

	/* allocate memory */
	ataxx_qram = (UINT8*)malloc(QRAM_SIZE);
	ataxx_pen_usage = (UINT32*)malloc(gfx->total_elements * 2 * sizeof(UINT32));

	/* error cases */
    if (!ataxx_qram || !ataxx_pen_usage)
    {
    	ataxx_vh_stop();
		return 1;
	}

	/* build up color usage */
	for (i = 0; i < gfx->total_elements; i++)
	{
		UINT8 *src = gfx->gfxdata + i * gfx->char_modulo;

		usage[0] = usage[1] = 0;
		for (y = 0; y < gfx->height; y++)
		{
			for (x = 0; x < gfx->width; x++)
			{
				int color = src[x];
				usage[color >> 5] |= 1 << (color & 31);
			}
			src += gfx->line_modulo;
		}
		ataxx_pen_usage[i * 2 + 0] = usage[0];
		ataxx_pen_usage[i * 2 + 1] = usage[1];
	}

	/* reset QRAM */
	memset(ataxx_qram, 0, QRAM_SIZE);
	return 0;
}



/*************************************
 *
 *	Stop video hardware
 *
 *************************************/

void leland_vh_stop(void)
{
	if (leland_video_ram)
		free(leland_video_ram);
	leland_video_ram = NULL;

	if (video_ram_copy)
		free(video_ram_copy);
	video_ram_copy = NULL;
}


void ataxx_vh_stop(void)
{
	leland_vh_stop();

	if (ataxx_qram)
		free(ataxx_qram);
	ataxx_qram = NULL;

	if (ataxx_pen_usage)
		free(ataxx_pen_usage);
	ataxx_pen_usage = NULL;
}



/*************************************
 *
 *	Scrolling and banking
 *
 *************************************/

WRITE_HANDLER( leland_gfx_port_w )
{
	int scanline = leland_last_scanline_int;
	struct scroll_position *scroll;

	/* treat anything during the VBLANK as scanline 0 */
	if (scanline > Machine->visible_area.max_y)
		scanline = 0;

	/* adjust the proper scroll value */
    switch (offset)
    {
    	case -1:
    		gfxbank = data;
    		break;
		case 0:
			xscroll = (xscroll & 0xff00) | (data & 0x00ff);
			break;
		case 1:
			xscroll = (xscroll & 0x00ff) | ((data << 8) & 0xff00);
			break;
		case 2:
			yscroll = (yscroll & 0xff00) | (data & 0x00ff);
			break;
		case 3:
			yscroll = (yscroll & 0x00ff) | ((data << 8) & 0xff00);
			break;
	}

	/* update if necessary */
	scroll = &scroll_pos[scroll_index];
	if (xscroll != scroll->x || yscroll != scroll->y || gfxbank != scroll->gfxbank)
	{
		/* determine which entry to use */
		if (scroll->scanline != scanline && scroll_index < VIDEO_HEIGHT - 1)
			scroll++, scroll_index++;

		/* fill in the data */
		scroll->scanline = scanline;
		scroll->x = xscroll;
		scroll->y = yscroll;
		scroll->gfxbank = gfxbank;
	}
}



/*************************************
 *
 *	Video address setting
 *
 *************************************/

void leland_video_addr_w(int offset, int data, int num)
{
	struct vram_state_data *state = vram_state + num;

	if (!offset)
    {
		state->addr = (state->addr & 0x7f00) | (data & 0x00ff);
		state->plane = 0;
    }
	else
	{
		state->addr = ((data << 8) & 0x7f00) | (state->addr & 0x00ff);
		state->plane = 0;
	}

	if (num == 0)
		sync_next_write = (state->addr >= 0x7800);
}



/*************************************
 *
 *	Flush data from VRAM into our copy
 *
 *************************************/

static void update_for_scanline(int scanline)
{
	int i;

	/* skip if we're behind the times */
	if (scanline <= next_update_scanline)
		return;

	/* update all scanlines */
	for (i = next_update_scanline; i < scanline; i++)
	{
		memcpy(&video_ram_copy[i * 128 + VRAM_LO], &leland_video_ram[i * 128 + VRAM_LO], 0x51);
		memcpy(&video_ram_copy[i * 128 + VRAM_HI], &leland_video_ram[i * 128 + VRAM_HI], 0x51);
	}

	/* set the new last update */
	next_update_scanline = scanline;
}



/*************************************
 *
 *	Common video RAM read
 *
 *************************************/

int leland_vram_port_r(int offset, int num)
{
	struct vram_state_data *state = vram_state + num;
	int addr = state->addr;
	int plane = state->plane;
	int inc = (offset >> 3) & 1;
    int ret;

    switch (offset & 7)
    {
        case 3:	/* read hi/lo (alternating) */
        	ret = leland_video_ram[addr + plane * VRAM_HI];
        	addr += inc & plane;
        	plane ^= 1;
            break;

        case 5:	/* read hi */
		    ret = leland_video_ram[addr + VRAM_HI];
		    addr += inc;
            break;

        case 6:	/* read lo */
		    ret = leland_video_ram[addr + VRAM_LO];
		    addr += inc;
            break;

        default:
            /*logerror("CPU #%d %04x Warning: Unknown video port %02x read (address=%04x)\n",
                        cpu_getactivecpu(),cpu_get_pc(), offset, addr);*/
            ret = 0;
            break;
    }
    state->addr = addr & 0x7fff;
    state->plane = plane;

	/*if (LOG_COMM && addr >= 0x7800)
		logerror("%04X:%s comm read %04X = %02X\n", cpu_getpreviouspc(), num ? "slave" : "master", addr, ret);*/

    return ret;
}



/*************************************
 *
 *	Common video RAM write
 *
 *************************************/

void leland_vram_port_w(int offset, int data, int num)
{
	struct vram_state_data *state = vram_state + num;
	int addr = state->addr;
	int plane = state->plane;
	int inc = (offset >> 3) & 1;
	int trans = (offset >> 4) & num;

	/* if we're writing "behind the beam", make sure we've cached what was there */
	if (addr < 0x7800)
	{
		int cur_scanline = cpu_getscanline();
		int mod_scanline = addr / 0x80;

		if (cur_scanline != next_update_scanline && mod_scanline < cur_scanline)
			update_for_scanline(cur_scanline);
	}

	/*if (LOG_COMM && addr >= 0x7800)
		logerror("%04X:%s comm write %04X = %02X\n", cpu_getpreviouspc(), num ? "slave" : "master", addr, data);*/

	/* based on the low 3 bits of the offset, update the destination */
    switch (offset & 7)
    {
        case 1:	/* write hi = data, lo = latch */
        	leland_video_ram[addr + VRAM_HI] = data;
        	leland_video_ram[addr + VRAM_LO] = state->latch[0];
        	addr += inc;
        	break;

        case 2:	/* write hi = latch, lo = data */
        	leland_video_ram[addr + VRAM_HI] = state->latch[1];
        	leland_video_ram[addr + VRAM_LO] = data;
        	addr += inc;
        	break;

        case 3:	/* write hi/lo = data (alternating) */
        	if (trans)
        	{
        		if (!(data & 0xf0)) data |= leland_video_ram[addr + plane * VRAM_HI] & 0xf0;
        		if (!(data & 0x0f)) data |= leland_video_ram[addr + plane * VRAM_HI] & 0x0f;
        	}
       		leland_video_ram[addr + plane * VRAM_HI] = data;
        	addr += inc & plane;
        	plane ^= 1;
            break;

        case 5:	/* write hi = data */
        	state->latch[1] = data;
        	if (trans)
        	{
        		if (!(data & 0xf0)) data |= leland_video_ram[addr + VRAM_HI] & 0xf0;
        		if (!(data & 0x0f)) data |= leland_video_ram[addr + VRAM_HI] & 0x0f;
        	}
		    leland_video_ram[addr + VRAM_HI] = data;
		    addr += inc;
            break;

        case 6:	/* write lo = data */
        	state->latch[0] = data;
        	if (trans)
        	{
        		if (!(data & 0xf0)) data |= leland_video_ram[addr + VRAM_LO] & 0xf0;
        		if (!(data & 0x0f)) data |= leland_video_ram[addr + VRAM_LO] & 0x0f;
        	}
		    leland_video_ram[addr + VRAM_LO] = data;
		    addr += inc;
            break;

        default:
            /*logerror("CPU #%d %04x Warning: Unknown video port %02x write (address=%04x value=%02x)\n",
                        cpu_getactivecpu(),cpu_get_pc(), offset, addr);*/
            break;
    }

    /* update the address and plane */
    state->addr = addr & 0x7fff;
    state->plane = plane;
}



/*************************************
 *
 *	Master video RAM read/write
 *
 *************************************/

WRITE_HANDLER( leland_master_video_addr_w )
{
    leland_video_addr_w(offset, data, 0);
}


static void leland_delayed_mvram_w(int param)
{
	int num = (param >> 16) & 1;
	int offset = (param >> 8) & 0xff;
	int data = param & 0xff;
	leland_vram_port_w(offset, data, num);
}


WRITE_HANDLER( leland_mvram_port_w )
{
	if (sync_next_write)
	{
		timer_set(TIME_NOW, 0x00000 | (offset << 8) | data, leland_delayed_mvram_w);
		sync_next_write = 0;
	}
	else
	    leland_vram_port_w(offset, data, 0);
}


READ_HANDLER( leland_mvram_port_r )
{
    return leland_vram_port_r(offset, 0);
}



/*************************************
 *
 *	Slave video RAM read/write
 *
 *************************************/

WRITE_HANDLER( leland_slave_video_addr_w )
{
    leland_video_addr_w(offset, data, 1);
}

WRITE_HANDLER( leland_svram_port_w )
{
    leland_vram_port_w(offset, data, 1);
}

READ_HANDLER( leland_svram_port_r )
{
    return leland_vram_port_r(offset, 1);
}



/*************************************
 *
 *	Ataxx master video RAM read/write
 *
 *************************************/

WRITE_HANDLER( ataxx_mvram_port_w )
{
	offset = ((offset >> 1) & 0x07) | ((offset << 3) & 0x08) | (offset & 0x10);
	if (sync_next_write)
	{
		timer_set(TIME_NOW, 0x00000 | (offset << 8) | data, leland_delayed_mvram_w);
		sync_next_write = 0;
	}
	else
		leland_vram_port_w(offset, data, 0);
}


WRITE_HANDLER( ataxx_svram_port_w )
{
	offset = ((offset >> 1) & 0x07) | ((offset << 3) & 0x08) | (offset & 0x10);
	leland_vram_port_w(offset, data, 1);
}



/*************************************
 *
 *	Ataxx slave video RAM read/write
 *
 *************************************/

READ_HANDLER( ataxx_mvram_port_r )
{
	offset = ((offset >> 1) & 0x07) | ((offset << 3) & 0x08) | (offset & 0x10);
    return leland_vram_port_r(offset, 0);
}


READ_HANDLER( ataxx_svram_port_r )
{
	offset = ((offset >> 1) & 0x07) | ((offset << 3) & 0x08) | (offset & 0x10);
    return leland_vram_port_r(offset, 1);
}



/*************************************
 *
 *	End-of-frame routine
 *
 *************************************/

static void scanline_reset(int param)
{
	/* flush the remaining scanlines */
	update_for_scanline(256);
	next_update_scanline = 0;

	/* update the DACs if they're on */
	if (!(leland_dac_control & 0x01))
		leland_dac_update(0, &video_ram_copy[VRAM_LO + 0x50]);
	if (!(leland_dac_control & 0x02))
		leland_dac_update(1, &video_ram_copy[VRAM_HI + 0x50]);
	leland_dac_control = 3;
}


void leland_vh_eof(void)
{
	/* reset scrolling */
	scroll_index = 0;
	scroll_pos[0].scanline = 0;
	scroll_pos[0].x = xscroll;
	scroll_pos[0].y = yscroll;
	scroll_pos[0].gfxbank = gfxbank;

	/* update anything remaining */
	update_for_scanline(VIDEO_HEIGHT * 8);

	/* set a timer to go off at the top of the frame */
	timer_set(cpu_getscanlinetime(0), 0, scanline_reset);
}



/*************************************
 *
 *	ROM-based refresh routine
 *
 *************************************/

void leland_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	const UINT8 *background_prom = memory_region(REGION_USER1);
	const struct GfxElement *gfx = Machine->gfx[0];
	int total_elements = gfx->total_elements;
	UINT8 background_usage[8];
	int x, y, chunk;

	/* update anything remaining */
	update_for_scanline(VIDEO_HEIGHT * 8);

	/* loop over scrolling chunks */
	/* it's okay to do this before the palette calc because */
	/* these values are raw indexes, not pens */
	memset(background_usage, 0, sizeof(background_usage));
	for (chunk = 0; chunk <= scroll_index; chunk++)
	{
		int char_bank = ((scroll_pos[chunk].gfxbank >> 4) & 0x03) * 0x0400;
		int prom_bank = ((scroll_pos[chunk].gfxbank >> 3) & 0x01) * 0x2000;

		/* determine scrolling parameters */
		int xfine = scroll_pos[chunk].x % 8;
		int yfine = scroll_pos[chunk].y % 8;
		int xcoarse = scroll_pos[chunk].x / 8;
		int ycoarse = scroll_pos[chunk].y / 8;
		struct rectangle clip;

		/* make a clipper */
		clip = Machine->visible_area;
		if (chunk != 0)
			clip.min_y = scroll_pos[chunk].scanline;
		if (chunk != scroll_index)
			clip.max_y = scroll_pos[chunk + 1].scanline - 1;

		/* draw what's visible to the main bitmap */
		for (y = clip.min_y / 8; y < clip.max_y / 8 + 2; y++)
		{
			int ysum = ycoarse + y;
			for (x = 0; x < VIDEO_WIDTH + 1; x++)
			{
				int xsum = xcoarse + x;
				int offs = ((xsum << 0) & 0x000ff) |
				           ((ysum << 8) & 0x01f00) |
				           prom_bank |
				           ((ysum << 9) & 0x1c000);
				int code = background_prom[offs] |
				           ((ysum << 2) & 0x300) |
				           char_bank;
				int color = (code >> 5) & 7;

				/* draw to the bitmap */
				drawgfx(bitmap, gfx,
						code, 8 * color, 0, 0,
						8 * x - xfine, 8 * y - yfine,
						&clip, TRANSPARENCY_NONE_RAW, 0);

				/* update color usage */
				background_usage[color] |= gfx->pen_usage[code & (total_elements - 1)];
			}
		}
	}

	/* build the palette */
	palette_init_used_colors();
	for (y = 0; y < 8; y++)
	{
		UINT8 usage = background_usage[y];
		for (x = 0; x < 8; x++)
			if (usage & (1 << x))
			{
				int p;
				for (p = 0; p < 16; p++)
					palette_used_colors[p * 64 + y * 8 + x] = PALETTE_COLOR_USED;
			}
	}
	palette_recalc();

	/* Merge the two bitmaps together */
	if (bitmap->depth == 8)
		draw_bitmap_8(bitmap);
	else
		draw_bitmap_16(bitmap);
}



/*************************************
 *
 *	RAM-based refresh routine
 *
 *************************************/

void ataxx_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	const struct GfxElement *gfx = Machine->gfx[0];
	int total_elements = gfx->total_elements;
	UINT32 background_usage[2];
	int x, y, chunk;

	/* update anything remaining */
	update_for_scanline(VIDEO_HEIGHT * 8);

	/* loop over scrolling chunks */
	/* it's okay to do this before the palette calc because */
	/* these values are raw indexes, not pens */
	memset(background_usage, 0, sizeof(background_usage));
	for (chunk = 0; chunk <= scroll_index; chunk++)
	{
		/* determine scrolling parameters */
		int xfine = scroll_pos[chunk].x % 8;
		int yfine = scroll_pos[chunk].y % 8;
		int xcoarse = scroll_pos[chunk].x / 8;
		int ycoarse = scroll_pos[chunk].y / 8;
		struct rectangle clip;

		/* make a clipper */
		clip = Machine->visible_area;
		if (chunk != 0)
			clip.min_y = scroll_pos[chunk].scanline;
		if (chunk != scroll_index)
			clip.max_y = scroll_pos[chunk + 1].scanline - 1;

		/* draw what's visible to the main bitmap */
		for (y = clip.min_y / 8; y < clip.max_y / 8 + 2; y++)
		{
			int ysum = ycoarse + y;
			for (x = 0; x < VIDEO_WIDTH + 1; x++)
			{
				int xsum = xcoarse + x;
				int offs = ((ysum & 0x40) << 9) + ((ysum & 0x3f) << 8) + (xsum & 0xff);
				int code = ataxx_qram[offs] | ((ataxx_qram[offs + 0x4000] & 0x7f) << 8);

				/* draw to the bitmap */
				drawgfx(bitmap, gfx,
						code, 0, 0, 0,
						8 * x - xfine, 8 * y - yfine,
						&clip, TRANSPARENCY_NONE_RAW, 0);

				/* update color usage */
				background_usage[0] |= ataxx_pen_usage[(code & (total_elements - 1)) * 2 + 0];
				background_usage[1] |= ataxx_pen_usage[(code & (total_elements - 1)) * 2 + 1];
			}
		}
	}

	/* build the palette */
	palette_init_used_colors();
	for (y = 0; y < 2; y++)
	{
		UINT32 usage = background_usage[y];
		for (x = 0; x < 32; x++)
			if (usage & (1 << x))
			{
				int p;
				for (p = 0; p < 16; p++)
					palette_used_colors[p * 64 + y * 32 + x] = PALETTE_COLOR_USED;
			}
	}
	palette_recalc();

	/* Merge the two bitmaps together */
	if (bitmap->depth == 8)
		draw_bitmap_8(bitmap);
	else
		draw_bitmap_16(bitmap);
}



/*************************************
 *
 *	Depth-specific refresh
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

#define DRAW_FUNC draw_bitmap_8
#define TYPE UINT8
#include "leland.cpp"
#undef TYPE
#undef DRAW_FUNC

#define DRAW_FUNC draw_bitmap_16
#define TYPE UINT16
#include "leland.cpp"
#undef TYPE
#undef DRAW_FUNC


#else


/*************************************
 *
 *	Bitmap blending routine
 *
 *************************************/

void DRAW_FUNC(struct osd_bitmap *bitmap)
{
	const UINT16 *pens = &Machine->pens[0];
	int orientation = Machine->orientation;
	int x, y;

	/* draw any non-transparent scanlines from the VRAM directly */
	for (y = Machine->visible_area.min_y; y <= Machine->visible_area.max_y; y++)
	{
		UINT8 *srclo = &video_ram_copy[y * 128 + VRAM_LO];
		UINT8 *srchi = &video_ram_copy[y * 128 + VRAM_HI];
		TYPE *dst = (TYPE *)bitmap->line[y];
		int xadv = 1;

		/* adjust in case we're oddly oriented */
		ADJUST_FOR_ORIENTATION(orientation, bitmap, dst, 0, y, xadv);

		/* redraw the scanline */
		for (x = 0; x < VIDEO_WIDTH*2; x++)
		{
			UINT16 data = (*srclo++ << 8) | *srchi++;

			*dst = pens[*dst | ((data & 0xf000) >> 6)];
			dst += xadv;
			*dst = pens[*dst | ((data & 0x0f00) >> 2)];
			dst += xadv;
			*dst = pens[*dst | ((data & 0x00f0) << 2)];
			dst += xadv;
			*dst = pens[*dst | ((data & 0x000f) << 6)];
			dst += xadv;
		}
	}
}

#endif
