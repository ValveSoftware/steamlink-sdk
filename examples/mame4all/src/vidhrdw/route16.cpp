/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *route16_sharedram;
unsigned char *route16_videoram1;
unsigned char *route16_videoram2;
size_t route16_videoram_size;

static struct osd_bitmap *tmpbitmap1;
static struct osd_bitmap *tmpbitmap2;

static int video_flip;
static int video_color_select_1;
static int video_color_select_2;
static int video_disable_1 = 0;
static int video_disable_2 = 0;
static int video_remap_1;
static int video_remap_2;
static const unsigned char *route16_color_prom;
static int route16_hardware;

/* Local functions */
static void modify_pen(int pen, int colorindex);
static void common_videoram_w(int offset,int data,
                              int coloroffset, struct osd_bitmap *bitmap);



void route16_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	route16_color_prom = color_prom;	/* we'll need this later */
}


/***************************************************************************

  Set hardware dependent flag.

***************************************************************************/
void init_route16b(void)
{
    route16_hardware = 1;
}

void init_route16(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);


	/* patch the protection */
	rom[0x00e9] = 0x3a;

	rom[0x0754] = 0xc3;
	rom[0x0755] = 0x63;
	rom[0x0756] = 0x07;

	init_route16b();
}

void init_stratvox(void)
{
    route16_hardware = 0;
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int route16_vh_start(void)
{
	if ((tmpbitmap1 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{
		return 1;
	}

	if ((tmpbitmap2 = bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height)) == 0)
	{

		bitmap_free(tmpbitmap1);
		tmpbitmap1 = 0;
		return 1;
	}

	video_flip = 0;
	video_color_select_1 = 0;
	video_color_select_2 = 0;
	video_disable_1 = 0;
	video_disable_2 = 0;
	video_remap_1 = 1;
	video_remap_2 = 1;

	return 0;
}

/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void route16_vh_stop(void)
{
	bitmap_free(tmpbitmap1);
	bitmap_free(tmpbitmap2);
}


/***************************************************************************
  route16_out0_w
***************************************************************************/
WRITE_HANDLER( route16_out0_w )
{
	static int last_write = 0;

	if (data == last_write) return;

	video_disable_1 = ((data & 0x02) << 6) && route16_hardware;
	video_color_select_1 = ((data & 0x1f) << 2);

	/* Bit 5 is the coin counter. */
	coin_counter_w(0, data & 0x20);

	video_remap_1 = 1;
	last_write = data;
}

/***************************************************************************
  route16_out1_w
***************************************************************************/
WRITE_HANDLER( route16_out1_w )
{
	static int last_write = 0;

	if (data == last_write) return;

	video_disable_2 = ((data & 0x02) << 6 ) && route16_hardware;
	video_color_select_2 = ((data & 0x1f) << 2);

	if (video_flip != ((data & 0x20) >> 5))
	{
		video_flip = (data & 0x20) >> 5;
	}

	video_remap_2 = 1;
	last_write = data;
}

/***************************************************************************

  Handle Stratovox's extra sound effects.

***************************************************************************/
WRITE_HANDLER( stratvox_sn76477_w )
{
	/* get out for Route 16 */
	if (route16_hardware) return;

    /***************************************************************
     * AY8910 output bits are connected to...
     * 7    - direct: 5V * 30k/(100+30k) = 1.15V - via DAC??
     * 6    - SN76477 mixer a
     * 5    - SN76477 mixer b
     * 4    - SN76477 mixer c
     * 3    - SN76477 envelope 1
	 * 2	- SN76477 envelope 2
     * 1    - SN76477 vco
     * 0    - SN76477 enable
     ***************************************************************/
    SN76477_mixer_w(0,(data >> 4) & 7);
	SN76477_envelope_w(0,(data >> 2) & 3);
    SN76477_vco_w(0,(data >> 1) & 1);
    SN76477_enable_w(0,data & 1);
}

/***************************************************************************
  route16_sharedram_r
***************************************************************************/
READ_HANDLER( route16_sharedram_r )
{
	return route16_sharedram[offset];
}

/***************************************************************************
  route16_sharedram_w
***************************************************************************/
WRITE_HANDLER( route16_sharedram_w )
{
	route16_sharedram[offset] = data;

	// 4313-4319 are used in Route 16 as triggers to wake the other CPU
	if (offset >= 0x0313 && offset <= 0x0319 && data == 0xff && route16_hardware)
	{
		// Let the other CPU run
		cpu_yield();
	}
}

/***************************************************************************
  route16_videoram1_r
***************************************************************************/
READ_HANDLER( route16_videoram1_r )
{
	return route16_videoram1[offset];
}

/***************************************************************************
  route16_videoram2_r
***************************************************************************/
READ_HANDLER( route16_videoram2_r )
{
	return route16_videoram1[offset];
}

/***************************************************************************
  route16_videoram1_w
***************************************************************************/
WRITE_HANDLER( route16_videoram1_w )
{
	route16_videoram1[offset] = data;

	common_videoram_w(offset, data, 0, tmpbitmap1);
}

/***************************************************************************
  route16_videoram2_w
***************************************************************************/
WRITE_HANDLER( route16_videoram2_w )
{
	route16_videoram2[offset] = data;

	common_videoram_w(offset, data, 4, tmpbitmap2);
}

/***************************************************************************
  common_videoram_w
***************************************************************************/
static void common_videoram_w(int offset,int data,
                              int coloroffset, struct osd_bitmap *bitmap)
{
	int x, y, color1, color2, color3, color4;

	x = ((offset & 0x3f) << 2);
	y = (offset & 0xffc0) >> 6;

	if (video_flip)
	{
		x = 255 - x;
		y = 255 - y;
	}

	color4 = ((data & 0x80) >> 6) | ((data & 0x08) >> 3);
	color3 = ((data & 0x40) >> 5) | ((data & 0x04) >> 2);
	color2 = ((data & 0x20) >> 4) | ((data & 0x02) >> 1);
	color1 = ((data & 0x10) >> 3) | ((data & 0x01)     );

	if (video_flip)
	{
		plot_pixel(bitmap, x  , y, Machine->pens[color1 | coloroffset]);
		plot_pixel(bitmap, x-1, y, Machine->pens[color2 | coloroffset]);
		plot_pixel(bitmap, x-2, y, Machine->pens[color3 | coloroffset]);
		plot_pixel(bitmap, x-3, y, Machine->pens[color4 | coloroffset]);
	}
	else
	{
		plot_pixel(bitmap, x  , y, Machine->pens[color1 | coloroffset]);
		plot_pixel(bitmap, x+1, y, Machine->pens[color2 | coloroffset]);
		plot_pixel(bitmap, x+2, y, Machine->pens[color3 | coloroffset]);
		plot_pixel(bitmap, x+3, y, Machine->pens[color4 | coloroffset]);
	}
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void route16_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
    if (video_remap_1)
	{
		modify_pen(0, video_color_select_1 + 0);
		modify_pen(1, video_color_select_1 + 1);
		modify_pen(2, video_color_select_1 + 2);
		modify_pen(3, video_color_select_1 + 3);
	}

	if (video_remap_2)
	{
		modify_pen(4, video_color_select_2 + 0);
		modify_pen(5, video_color_select_2 + 1);
		modify_pen(6, video_color_select_2 + 2);
		modify_pen(7, video_color_select_2 + 3);
	}


	if (palette_recalc() || video_remap_1 || video_remap_2)
	{
		int offs;

		// redraw bitmaps
		for (offs = 0; offs < route16_videoram_size; offs++)
		{
			route16_videoram1_w(offs, route16_videoram1[offs]);
			route16_videoram2_w(offs, route16_videoram2[offs]);
		}
	}

	video_remap_1 = 0;
	video_remap_2 = 0;


	if (!video_disable_2)
	{
		copybitmap(bitmap,tmpbitmap2,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}

	if (!video_disable_1)
	{
		if (video_disable_2)
			copybitmap(bitmap,tmpbitmap1,0,0,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
		else
			copybitmap(bitmap,tmpbitmap1,0,0,0,0,&Machine->visible_area,TRANSPARENCY_COLOR,0);
	}
}

/***************************************************************************
  mofify_pen
 ***************************************************************************/
static void modify_pen(int pen, int colorindex)
{
	int r,g,b,color;

	color = route16_color_prom[colorindex];

	r = ((color & 1) ? 0xff : 0x00);
	g = ((color & 2) ? 0xff : 0x00);
	b = ((color & 4) ? 0xff : 0x00);

	palette_change_color(pen,r,g,b);
}
