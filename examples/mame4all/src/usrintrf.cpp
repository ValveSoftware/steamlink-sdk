/*********************************************************************

  usrintrf.c

  Functions used to handle MAME's crude user interface.

*********************************************************************/

#include "driver.h"
#include "info.h"
#include "vidhrdw/vector.h"
#include "datafile.h"
#include <stdarg.h>
#include "ui_text.h"

#ifdef MESS
  #include "mess/mess.h"
#endif

extern int mame_debug;

extern int bitmap_dirty;	/* set by osd_clearbitmap() */

/* Variables for stat menu */
extern char build_version[];
extern unsigned int dispensed_tickets;
extern unsigned int coins[COIN_COUNTERS];
extern unsigned int coinlockedout[COIN_COUNTERS];

/* MARTINEZ.F 990207 Memory Card */
#ifndef MESS
int 		memcard_menu(struct osd_bitmap *bitmap, int);
extern int	mcd_action;
extern int	mcd_number;
extern int	memcard_status;
extern int	memcard_number;
extern int	memcard_manager;
#endif

extern int neogeo_memcard_load(int);
extern void neogeo_memcard_save(void);
extern void neogeo_memcard_eject(void);
extern int neogeo_memcard_create(int);
/* MARTINEZ.F 990207 Memory Card End */

#ifdef ENABLE_AUTOFIRE
int autofire_menu(struct osd_bitmap *bitmap, int selected);
#endif


static int setup_selected;
static int osd_selected;
static int jukebox_selected;
static int single_step;
static int trueorientation;
static int orientation_count;


static void switch_ui_orientation(void)
{
	if (orientation_count == 0)
	{
		trueorientation = Machine->orientation;
		Machine->orientation = Machine->ui_orientation;
		set_pixel_functions();
	}

	orientation_count++;
}

static void switch_true_orientation(void)
{
	orientation_count--;

	if (orientation_count == 0)
	{
		Machine->orientation = trueorientation;
		set_pixel_functions();
	}
}


void set_ui_visarea (int xmin, int ymin, int xmax, int ymax)
{
	int temp,w,h;

	/* special case for vectors */
	if(Machine->drv->video_attributes == VIDEO_TYPE_VECTOR)
	{
		if (Machine->ui_orientation & ORIENTATION_SWAP_XY)
		{
			temp=xmin; xmin=ymin; ymin=temp;
			temp=xmax; xmax=ymax; ymax=temp;
		}
	}
	else
	{
		if (Machine->orientation & ORIENTATION_SWAP_XY)
		{
			w = Machine->drv->screen_height;
			h = Machine->drv->screen_width;
		}
		else
		{
			w = Machine->drv->screen_width;
			h = Machine->drv->screen_height;
		}

		if (Machine->ui_orientation & ORIENTATION_FLIP_X)
		{
			temp = w - xmin - 1;
			xmin = w - xmax - 1;
			xmax = temp ;
		}

		if (Machine->ui_orientation & ORIENTATION_FLIP_Y)
		{
			temp = h - ymin - 1;
			ymin = h - ymax - 1;
			ymax = temp;
		}

		if (Machine->ui_orientation & ORIENTATION_SWAP_XY)
		{
			temp = xmin; xmin = ymin; ymin = temp;
			temp = xmax; xmax = ymax; ymax = temp;
		}

	}
	Machine->uiwidth = xmax-xmin+1;
	Machine->uiheight = ymax-ymin+1;
	Machine->uixmin = xmin;
	Machine->uiymin = ymin;
}



struct GfxElement *builduifont(void)
{
    static unsigned char fontdata6x8[] =
	{
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x7c,0x80,0x98,0x90,0x80,0xbc,0x80,0x7c,0xf8,0x04,0x64,0x44,0x04,0xf4,0x04,0xf8,
		0x7c,0x80,0x98,0x88,0x80,0xbc,0x80,0x7c,0xf8,0x04,0x64,0x24,0x04,0xf4,0x04,0xf8,
		0x7c,0x80,0x88,0x98,0x80,0xbc,0x80,0x7c,0xf8,0x04,0x24,0x64,0x04,0xf4,0x04,0xf8,
		0x7c,0x80,0x90,0x98,0x80,0xbc,0x80,0x7c,0xf8,0x04,0x44,0x64,0x04,0xf4,0x04,0xf8,
		0x30,0x48,0x84,0xb4,0xb4,0x84,0x48,0x30,0x30,0x48,0x84,0x84,0x84,0x84,0x48,0x30,
		0x00,0xfc,0x84,0x8c,0xd4,0xa4,0xfc,0x00,0x00,0xfc,0x84,0x84,0x84,0x84,0xfc,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x68,0x78,0x78,0x30,0x00,0x00,
		0x80,0xc0,0xe0,0xf0,0xe0,0xc0,0x80,0x00,0x04,0x0c,0x1c,0x3c,0x1c,0x0c,0x04,0x00,
		0x20,0x70,0xf8,0x20,0x20,0xf8,0x70,0x20,0x48,0x48,0x48,0x48,0x48,0x00,0x48,0x00,
		0x00,0x00,0x30,0x68,0x78,0x30,0x00,0x00,0x00,0x30,0x68,0x78,0x78,0x30,0x00,0x00,
		0x70,0xd8,0xe8,0xe8,0xf8,0xf8,0x70,0x00,0x1c,0x7c,0x74,0x44,0x44,0x4c,0xcc,0xc0,
		0x20,0x70,0xf8,0x70,0x70,0x70,0x70,0x00,0x70,0x70,0x70,0x70,0xf8,0x70,0x20,0x00,
		0x00,0x10,0xf8,0xfc,0xf8,0x10,0x00,0x00,0x00,0x20,0x7c,0xfc,0x7c,0x20,0x00,0x00,
		0xb0,0x54,0xb8,0xb8,0x54,0xb0,0x00,0x00,0x00,0x28,0x6c,0xfc,0x6c,0x28,0x00,0x00,
		0x00,0x30,0x30,0x78,0x78,0xfc,0x00,0x00,0xfc,0x78,0x78,0x30,0x30,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,0x00,0x20,0x00,
		0x50,0x50,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x50,0xf8,0x50,0xf8,0x50,0x00,0x00,
		0x20,0x70,0xc0,0x70,0x18,0xf0,0x20,0x00,0x40,0xa4,0x48,0x10,0x20,0x48,0x94,0x08,
		0x60,0x90,0xa0,0x40,0xa8,0x90,0x68,0x00,0x10,0x20,0x40,0x00,0x00,0x00,0x00,0x00,
		0x20,0x40,0x40,0x40,0x40,0x40,0x20,0x00,0x10,0x08,0x08,0x08,0x08,0x08,0x10,0x00,
		0x20,0xa8,0x70,0xf8,0x70,0xa8,0x20,0x00,0x00,0x20,0x20,0xf8,0x20,0x20,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x60,0x00,0x00,0x00,0xf8,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00,0x00,0x08,0x10,0x20,0x40,0x80,0x00,0x00,
		0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00,0x10,0x30,0x10,0x10,0x10,0x10,0x10,0x00,
		0x70,0x88,0x08,0x10,0x20,0x40,0xf8,0x00,0x70,0x88,0x08,0x30,0x08,0x88,0x70,0x00,
		0x10,0x30,0x50,0x90,0xf8,0x10,0x10,0x00,0xf8,0x80,0xf0,0x08,0x08,0x88,0x70,0x00,
		0x70,0x80,0xf0,0x88,0x88,0x88,0x70,0x00,0xf8,0x08,0x08,0x10,0x20,0x20,0x20,0x00,
		0x70,0x88,0x88,0x70,0x88,0x88,0x70,0x00,0x70,0x88,0x88,0x88,0x78,0x08,0x70,0x00,
		0x00,0x00,0x30,0x30,0x00,0x30,0x30,0x00,0x00,0x00,0x30,0x30,0x00,0x30,0x30,0x60,
		0x10,0x20,0x40,0x80,0x40,0x20,0x10,0x00,0x00,0x00,0xf8,0x00,0xf8,0x00,0x00,0x00,
		0x40,0x20,0x10,0x08,0x10,0x20,0x40,0x00,0x70,0x88,0x08,0x10,0x20,0x00,0x20,0x00,
		0x30,0x48,0x94,0xa4,0xa4,0x94,0x48,0x30,0x70,0x88,0x88,0xf8,0x88,0x88,0x88,0x00,
		0xf0,0x88,0x88,0xf0,0x88,0x88,0xf0,0x00,0x70,0x88,0x80,0x80,0x80,0x88,0x70,0x00,
		0xf0,0x88,0x88,0x88,0x88,0x88,0xf0,0x00,0xf8,0x80,0x80,0xf0,0x80,0x80,0xf8,0x00,
		0xf8,0x80,0x80,0xf0,0x80,0x80,0x80,0x00,0x70,0x88,0x80,0x98,0x88,0x88,0x70,0x00,
		0x88,0x88,0x88,0xf8,0x88,0x88,0x88,0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00,
		0x08,0x08,0x08,0x08,0x88,0x88,0x70,0x00,0x88,0x90,0xa0,0xc0,0xa0,0x90,0x88,0x00,
		0x80,0x80,0x80,0x80,0x80,0x80,0xf8,0x00,0x88,0xd8,0xa8,0x88,0x88,0x88,0x88,0x00,
		0x88,0xc8,0xa8,0x98,0x88,0x88,0x88,0x00,0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00,
		0xf0,0x88,0x88,0xf0,0x80,0x80,0x80,0x00,0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x08,
		0xf0,0x88,0x88,0xf0,0x88,0x88,0x88,0x00,0x70,0x88,0x80,0x70,0x08,0x88,0x70,0x00,
		0xf8,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00,
		0x88,0x88,0x88,0x88,0x88,0x50,0x20,0x00,0x88,0x88,0x88,0x88,0xa8,0xd8,0x88,0x00,
		0x88,0x50,0x20,0x20,0x20,0x50,0x88,0x00,0x88,0x88,0x88,0x50,0x20,0x20,0x20,0x00,
		0xf8,0x08,0x10,0x20,0x40,0x80,0xf8,0x00,0x30,0x20,0x20,0x20,0x20,0x20,0x30,0x00,
		0x40,0x40,0x20,0x20,0x10,0x10,0x08,0x08,0x30,0x10,0x10,0x10,0x10,0x10,0x30,0x00,
		0x20,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfc,
		0x40,0x20,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0x08,0x78,0x88,0x78,0x00,
		0x80,0x80,0xf0,0x88,0x88,0x88,0xf0,0x00,0x00,0x00,0x70,0x88,0x80,0x80,0x78,0x00,
		0x08,0x08,0x78,0x88,0x88,0x88,0x78,0x00,0x00,0x00,0x70,0x88,0xf8,0x80,0x78,0x00,
		0x18,0x20,0x70,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0x78,0x88,0x88,0x78,0x08,0x70,
		0x80,0x80,0xf0,0x88,0x88,0x88,0x88,0x00,0x20,0x00,0x20,0x20,0x20,0x20,0x20,0x00,
		0x20,0x00,0x20,0x20,0x20,0x20,0x20,0xc0,0x80,0x80,0x90,0xa0,0xe0,0x90,0x88,0x00,
		0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xf0,0xa8,0xa8,0xa8,0xa8,0x00,
		0x00,0x00,0xb0,0xc8,0x88,0x88,0x88,0x00,0x00,0x00,0x70,0x88,0x88,0x88,0x70,0x00,
		0x00,0x00,0xf0,0x88,0x88,0xf0,0x80,0x80,0x00,0x00,0x78,0x88,0x88,0x78,0x08,0x08,
		0x00,0x00,0xb0,0xc8,0x80,0x80,0x80,0x00,0x00,0x00,0x78,0x80,0x70,0x08,0xf0,0x00,
		0x20,0x20,0x70,0x20,0x20,0x20,0x18,0x00,0x00,0x00,0x88,0x88,0x88,0x98,0x68,0x00,
		0x00,0x00,0x88,0x88,0x88,0x50,0x20,0x00,0x00,0x00,0xa8,0xa8,0xa8,0xa8,0x50,0x00,
		0x00,0x00,0x88,0x50,0x20,0x50,0x88,0x00,0x00,0x00,0x88,0x88,0x88,0x78,0x08,0x70,
		0x00,0x00,0xf8,0x10,0x20,0x40,0xf8,0x00,0x08,0x10,0x10,0x20,0x10,0x10,0x08,0x00,
		0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x40,0x20,0x20,0x10,0x20,0x20,0x40,0x00,
		0x00,0x68,0xb0,0x00,0x00,0x00,0x00,0x00,0x20,0x50,0x20,0x50,0xa8,0x50,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x40,0x0C,0x10,0x38,0x10,0x20,0x20,0xC0,0x00,
		0x00,0x00,0x00,0x00,0x00,0x28,0x28,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0xA8,0x00,
		0x70,0xA8,0xF8,0x20,0x20,0x20,0x20,0x00,0x70,0xA8,0xF8,0x20,0x20,0xF8,0xA8,0x70,
		0x20,0x50,0x88,0x00,0x00,0x00,0x00,0x00,0x44,0xA8,0x50,0x20,0x68,0xD4,0x28,0x00,
		0x88,0x70,0x88,0x60,0x30,0x88,0x70,0x00,0x00,0x10,0x20,0x40,0x20,0x10,0x00,0x00,
		0x78,0xA0,0xA0,0xB0,0xA0,0xA0,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x20,0x20,0x00,0x00,0x00,0x00,0x00,
		0x10,0x10,0x20,0x00,0x00,0x00,0x00,0x00,0x28,0x50,0x50,0x00,0x00,0x00,0x00,0x00,
		0x28,0x28,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x78,0x78,0x30,0x00,0x00,
		0x00,0x00,0x00,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFC,0x00,0x00,0x00,0x00,
		0x68,0xB0,0x00,0x00,0x00,0x00,0x00,0x00,0xF4,0x5C,0x54,0x54,0x00,0x00,0x00,0x00,
		0x88,0x70,0x78,0x80,0x70,0x08,0xF0,0x00,0x00,0x40,0x20,0x10,0x20,0x40,0x00,0x00,
		0x00,0x00,0x70,0xA8,0xB8,0xA0,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x50,0x88,0x88,0x50,0x20,0x20,0x20,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x20,0x20,0x20,0x20,0x20,0x00,
		0x00,0x20,0x70,0xA8,0xA0,0xA8,0x70,0x20,0x30,0x48,0x40,0xE0,0x40,0x48,0xF0,0x00,
		0x00,0x48,0x30,0x48,0x48,0x30,0x48,0x00,0x88,0x88,0x50,0xF8,0x20,0xF8,0x20,0x00,
		0x20,0x20,0x20,0x00,0x20,0x20,0x20,0x00,0x78,0x80,0x70,0x88,0x70,0x08,0xF0,0x00,
		0xD8,0xD8,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x48,0x94,0xA4,0xA4,0x94,0x48,0x30,
		0x60,0x10,0x70,0x90,0x70,0x00,0x00,0x00,0x00,0x28,0x50,0xA0,0x50,0x28,0x00,0x00,
		0x00,0x00,0x00,0xF8,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x78,0x00,0x00,0x00,0x00,
		0x30,0x48,0xB4,0xB4,0xA4,0xB4,0x48,0x30,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x60,0x90,0x90,0x60,0x00,0x00,0x00,0x00,0x20,0x20,0xF8,0x20,0x20,0x00,0xF8,0x00,
		0x60,0x90,0x20,0x40,0xF0,0x00,0x00,0x00,0x60,0x90,0x20,0x90,0x60,0x00,0x00,0x00,
		0x10,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x88,0x88,0xC8,0xB0,0x80,
		0x78,0xD0,0xD0,0xD0,0x50,0x50,0x50,0x00,0x00,0x00,0x00,0x30,0x30,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x10,0x20,0x00,0x20,0x60,0x20,0x20,0x70,0x00,0x00,0x00,
		0x20,0x50,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0xA0,0x50,0x28,0x50,0xA0,0x00,0x00,
		0x40,0x48,0x50,0x28,0x58,0xA8,0x38,0x08,0x40,0x48,0x50,0x28,0x44,0x98,0x20,0x3C,
		0xC0,0x28,0xD0,0x28,0xD8,0xA8,0x38,0x08,0x20,0x00,0x20,0x40,0x80,0x88,0x70,0x00,
		0x40,0x20,0x70,0x88,0xF8,0x88,0x88,0x00,0x10,0x20,0x70,0x88,0xF8,0x88,0x88,0x00,
		0x70,0x00,0x70,0x88,0xF8,0x88,0x88,0x00,0x68,0xB0,0x70,0x88,0xF8,0x88,0x88,0x00,
		0x50,0x00,0x70,0x88,0xF8,0x88,0x88,0x00,0x20,0x50,0x70,0x88,0xF8,0x88,0x88,0x00,
		0x78,0xA0,0xA0,0xF0,0xA0,0xA0,0xB8,0x00,0x70,0x88,0x80,0x80,0x88,0x70,0x08,0x70,
		0x40,0x20,0xF8,0x80,0xF0,0x80,0xF8,0x00,0x10,0x20,0xF8,0x80,0xF0,0x80,0xF8,0x00,
		0x70,0x00,0xF8,0x80,0xF0,0x80,0xF8,0x00,0x50,0x00,0xF8,0x80,0xF0,0x80,0xF8,0x00,
		0x40,0x20,0x70,0x20,0x20,0x20,0x70,0x00,0x10,0x20,0x70,0x20,0x20,0x20,0x70,0x00,
		0x70,0x00,0x70,0x20,0x20,0x20,0x70,0x00,0x50,0x00,0x70,0x20,0x20,0x20,0x70,0x00,
		0x70,0x48,0x48,0xE8,0x48,0x48,0x70,0x00,0x68,0xB0,0x88,0xC8,0xA8,0x98,0x88,0x00,
		0x40,0x20,0x70,0x88,0x88,0x88,0x70,0x00,0x10,0x20,0x70,0x88,0x88,0x88,0x70,0x00,
		0x70,0x00,0x70,0x88,0x88,0x88,0x70,0x00,0x68,0xB0,0x70,0x88,0x88,0x88,0x70,0x00,
		0x50,0x00,0x70,0x88,0x88,0x88,0x70,0x00,0x00,0x88,0x50,0x20,0x50,0x88,0x00,0x00,
		0x00,0x74,0x88,0x90,0xA8,0x48,0xB0,0x00,0x40,0x20,0x88,0x88,0x88,0x88,0x70,0x00,
		0x10,0x20,0x88,0x88,0x88,0x88,0x70,0x00,0x70,0x00,0x88,0x88,0x88,0x88,0x70,0x00,
		0x50,0x00,0x88,0x88,0x88,0x88,0x70,0x00,0x10,0xA8,0x88,0x50,0x20,0x20,0x20,0x00,
		0x00,0x80,0xF0,0x88,0x88,0xF0,0x80,0x80,0x60,0x90,0x90,0xB0,0x88,0x88,0xB0,0x00,
		0x40,0x20,0x70,0x08,0x78,0x88,0x78,0x00,0x10,0x20,0x70,0x08,0x78,0x88,0x78,0x00,
		0x70,0x00,0x70,0x08,0x78,0x88,0x78,0x00,0x68,0xB0,0x70,0x08,0x78,0x88,0x78,0x00,
		0x50,0x00,0x70,0x08,0x78,0x88,0x78,0x00,0x20,0x50,0x70,0x08,0x78,0x88,0x78,0x00,
		0x00,0x00,0xF0,0x28,0x78,0xA0,0x78,0x00,0x00,0x00,0x70,0x88,0x80,0x78,0x08,0x70,
		0x40,0x20,0x70,0x88,0xF8,0x80,0x70,0x00,0x10,0x20,0x70,0x88,0xF8,0x80,0x70,0x00,
		0x70,0x00,0x70,0x88,0xF8,0x80,0x70,0x00,0x50,0x00,0x70,0x88,0xF8,0x80,0x70,0x00,
		0x40,0x20,0x00,0x60,0x20,0x20,0x70,0x00,0x10,0x20,0x00,0x60,0x20,0x20,0x70,0x00,
		0x20,0x50,0x00,0x60,0x20,0x20,0x70,0x00,0x50,0x00,0x00,0x60,0x20,0x20,0x70,0x00,
		0x50,0x60,0x10,0x78,0x88,0x88,0x70,0x00,0x68,0xB0,0x00,0xF0,0x88,0x88,0x88,0x00,
		0x40,0x20,0x00,0x70,0x88,0x88,0x70,0x00,0x10,0x20,0x00,0x70,0x88,0x88,0x70,0x00,
		0x20,0x50,0x00,0x70,0x88,0x88,0x70,0x00,0x68,0xB0,0x00,0x70,0x88,0x88,0x70,0x00,
		0x00,0x50,0x00,0x70,0x88,0x88,0x70,0x00,0x00,0x20,0x00,0xF8,0x00,0x20,0x00,0x00,
		0x00,0x00,0x68,0x90,0xA8,0x48,0xB0,0x00,0x40,0x20,0x88,0x88,0x88,0x98,0x68,0x00,
		0x10,0x20,0x88,0x88,0x88,0x98,0x68,0x00,0x70,0x00,0x88,0x88,0x88,0x98,0x68,0x00,
		0x50,0x00,0x88,0x88,0x88,0x98,0x68,0x00,0x10,0x20,0x88,0x88,0x88,0x78,0x08,0x70,
		0x80,0xF0,0x88,0x88,0xF0,0x80,0x80,0x80,0x50,0x00,0x88,0x88,0x88,0x78,0x08,0x70
    };
#if 0
	static unsigned char fontdata6x8[] =
	{
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x7c,0x80,0x98,0x90,0x80,0xbc,0x80,0x7c,0xf8,0x04,0x64,0x44,0x04,0xf4,0x04,0xf8,
		0x7c,0x80,0x98,0x88,0x80,0xbc,0x80,0x7c,0xf8,0x04,0x64,0x24,0x04,0xf4,0x04,0xf8,
		0x7c,0x80,0x88,0x98,0x80,0xbc,0x80,0x7c,0xf8,0x04,0x24,0x64,0x04,0xf4,0x04,0xf8,
		0x7c,0x80,0x90,0x98,0x80,0xbc,0x80,0x7c,0xf8,0x04,0x44,0x64,0x04,0xf4,0x04,0xf8,
		0x30,0x48,0x84,0xb4,0xb4,0x84,0x48,0x30,0x30,0x48,0x84,0x84,0x84,0x84,0x48,0x30,
		0x00,0xfc,0x84,0x8c,0xd4,0xa4,0xfc,0x00,0x00,0xfc,0x84,0x84,0x84,0x84,0xfc,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x68,0x78,0x78,0x30,0x00,0x00,
		0x80,0xc0,0xe0,0xf0,0xe0,0xc0,0x80,0x00,0x04,0x0c,0x1c,0x3c,0x1c,0x0c,0x04,0x00,
		0x20,0x70,0xf8,0x20,0x20,0xf8,0x70,0x20,0x48,0x48,0x48,0x48,0x48,0x00,0x48,0x00,
		0x00,0x00,0x30,0x68,0x78,0x30,0x00,0x00,0x00,0x30,0x68,0x78,0x78,0x30,0x00,0x00,
		0x70,0xd8,0xe8,0xe8,0xf8,0xf8,0x70,0x00,0x1c,0x7c,0x74,0x44,0x44,0x4c,0xcc,0xc0,
		0x20,0x70,0xf8,0x70,0x70,0x70,0x70,0x00,0x70,0x70,0x70,0x70,0xf8,0x70,0x20,0x00,
		0x00,0x10,0xf8,0xfc,0xf8,0x10,0x00,0x00,0x00,0x20,0x7c,0xfc,0x7c,0x20,0x00,0x00,
		0xb0,0x54,0xb8,0xb8,0x54,0xb0,0x00,0x00,0x00,0x28,0x6c,0xfc,0x6c,0x28,0x00,0x00,
		0x00,0x30,0x30,0x78,0x78,0xfc,0x00,0x00,0xfc,0x78,0x78,0x30,0x30,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,0x00,0x20,0x00,
		0x50,0x50,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x50,0xf8,0x50,0xf8,0x50,0x00,0x00,
		0x20,0x70,0xc0,0x70,0x18,0xf0,0x20,0x00,0x40,0xa4,0x48,0x10,0x20,0x48,0x94,0x08,
		0x60,0x90,0xa0,0x40,0xa8,0x90,0x68,0x00,0x10,0x20,0x40,0x00,0x00,0x00,0x00,0x00,
		0x20,0x40,0x40,0x40,0x40,0x40,0x20,0x00,0x10,0x08,0x08,0x08,0x08,0x08,0x10,0x00,
		0x20,0xa8,0x70,0xf8,0x70,0xa8,0x20,0x00,0x00,0x20,0x20,0xf8,0x20,0x20,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x60,0x00,0x00,0x00,0xf8,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00,0x00,0x08,0x10,0x20,0x40,0x80,0x00,0x00,
		0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00,0x10,0x30,0x10,0x10,0x10,0x10,0x10,0x00,
		0x70,0x88,0x08,0x10,0x20,0x40,0xf8,0x00,0x70,0x88,0x08,0x30,0x08,0x88,0x70,0x00,
		0x10,0x30,0x50,0x90,0xf8,0x10,0x10,0x00,0xf8,0x80,0xf0,0x08,0x08,0x88,0x70,0x00,
		0x70,0x80,0xf0,0x88,0x88,0x88,0x70,0x00,0xf8,0x08,0x08,0x10,0x20,0x20,0x20,0x00,
		0x70,0x88,0x88,0x70,0x88,0x88,0x70,0x00,0x70,0x88,0x88,0x88,0x78,0x08,0x70,0x00,
		0x00,0x00,0x30,0x30,0x00,0x30,0x30,0x00,0x00,0x00,0x30,0x30,0x00,0x30,0x30,0x60,
		0x10,0x20,0x40,0x80,0x40,0x20,0x10,0x00,0x00,0x00,0xf8,0x00,0xf8,0x00,0x00,0x00,
		0x40,0x20,0x10,0x08,0x10,0x20,0x40,0x00,0x70,0x88,0x08,0x10,0x20,0x00,0x20,0x00,
		0x30,0x48,0x94,0xa4,0xa4,0x94,0x48,0x30,0x70,0x88,0x88,0xf8,0x88,0x88,0x88,0x00,
		0xf0,0x88,0x88,0xf0,0x88,0x88,0xf0,0x00,0x70,0x88,0x80,0x80,0x80,0x88,0x70,0x00,
		0xf0,0x88,0x88,0x88,0x88,0x88,0xf0,0x00,0xf8,0x80,0x80,0xf0,0x80,0x80,0xf8,0x00,
		0xf8,0x80,0x80,0xf0,0x80,0x80,0x80,0x00,0x70,0x88,0x80,0x98,0x88,0x88,0x70,0x00,
		0x88,0x88,0x88,0xf8,0x88,0x88,0x88,0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00,
		0x08,0x08,0x08,0x08,0x88,0x88,0x70,0x00,0x88,0x90,0xa0,0xc0,0xa0,0x90,0x88,0x00,
		0x80,0x80,0x80,0x80,0x80,0x80,0xf8,0x00,0x88,0xd8,0xa8,0x88,0x88,0x88,0x88,0x00,
		0x88,0xc8,0xa8,0x98,0x88,0x88,0x88,0x00,0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00,
		0xf0,0x88,0x88,0xf0,0x80,0x80,0x80,0x00,0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x08,
		0xf0,0x88,0x88,0xf0,0x88,0x88,0x88,0x00,0x70,0x88,0x80,0x70,0x08,0x88,0x70,0x00,
		0xf8,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00,
		0x88,0x88,0x88,0x88,0x88,0x50,0x20,0x00,0x88,0x88,0x88,0x88,0xa8,0xd8,0x88,0x00,
		0x88,0x50,0x20,0x20,0x20,0x50,0x88,0x00,0x88,0x88,0x88,0x50,0x20,0x20,0x20,0x00,
		0xf8,0x08,0x10,0x20,0x40,0x80,0xf8,0x00,0x30,0x20,0x20,0x20,0x20,0x20,0x30,0x00,
		0x40,0x40,0x20,0x20,0x10,0x10,0x08,0x08,0x30,0x10,0x10,0x10,0x10,0x10,0x30,0x00,
		0x20,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfc,
		0x40,0x20,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0x08,0x78,0x88,0x78,0x00,
		0x80,0x80,0xf0,0x88,0x88,0x88,0xf0,0x00,0x00,0x00,0x70,0x88,0x80,0x80,0x78,0x00,
		0x08,0x08,0x78,0x88,0x88,0x88,0x78,0x00,0x00,0x00,0x70,0x88,0xf8,0x80,0x78,0x00,
		0x18,0x20,0x70,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0x78,0x88,0x88,0x78,0x08,0x70,
		0x80,0x80,0xf0,0x88,0x88,0x88,0x88,0x00,0x20,0x00,0x20,0x20,0x20,0x20,0x20,0x00,
		0x20,0x00,0x20,0x20,0x20,0x20,0x20,0xc0,0x80,0x80,0x90,0xa0,0xe0,0x90,0x88,0x00,
		0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xf0,0xa8,0xa8,0xa8,0xa8,0x00,
		0x00,0x00,0xb0,0xc8,0x88,0x88,0x88,0x00,0x00,0x00,0x70,0x88,0x88,0x88,0x70,0x00,
		0x00,0x00,0xf0,0x88,0x88,0xf0,0x80,0x80,0x00,0x00,0x78,0x88,0x88,0x78,0x08,0x08,
		0x00,0x00,0xb0,0xc8,0x80,0x80,0x80,0x00,0x00,0x00,0x78,0x80,0x70,0x08,0xf0,0x00,
		0x20,0x20,0x70,0x20,0x20,0x20,0x18,0x00,0x00,0x00,0x88,0x88,0x88,0x98,0x68,0x00,
		0x00,0x00,0x88,0x88,0x88,0x50,0x20,0x00,0x00,0x00,0xa8,0xa8,0xa8,0xa8,0x50,0x00,
		0x00,0x00,0x88,0x50,0x20,0x50,0x88,0x00,0x00,0x00,0x88,0x88,0x88,0x78,0x08,0x70,
		0x00,0x00,0xf8,0x10,0x20,0x40,0xf8,0x00,0x08,0x10,0x10,0x20,0x10,0x10,0x08,0x00,
		0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x40,0x20,0x20,0x10,0x20,0x20,0x40,0x00,
		0x00,0x68,0xb0,0x00,0x00,0x00,0x00,0x00,0x20,0x50,0x20,0x50,0xa8,0x50,0x00,0x00,
	};
#endif

	static struct GfxLayout fontlayout6x8 =
	{
		6,8,	/* 6*8 characters */
		256,	/* 256 characters */
		1,	/* 1 bit per pixel */
		{ 0 },
		{ 0, 1, 2, 3, 4, 5, 6, 7 }, /* straightforward layout */
		{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
		8*8 /* every char takes 8 consecutive bytes */
	};
	static struct GfxLayout fontlayout12x8 =
	{
		12,8,	/* 12*8 characters */
		256,	/* 256 characters */
		1,	/* 1 bit per pixel */
		{ 0 },
		{ 0,0, 1,1, 2,2, 3,3, 4,4, 5,5, 6,6, 7,7 }, /* straightforward layout */
		{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
		8*8 /* every char takes 8 consecutive bytes */
	};
	static struct GfxLayout fontlayout12x16 =
	{
		12,16,	/* 6*8 characters */
		256,	/* 256 characters */
		1,	/* 1 bit per pixel */
		{ 0 },
		{ 0,0, 1,1, 2,2, 3,3, 4,4, 5,5, 6,6, 7,7 }, /* straightforward layout */
		{ 0*8,0*8, 1*8,1*8, 2*8,2*8, 3*8,3*8, 4*8,4*8, 5*8,5*8, 6*8,6*8, 7*8,7*8 },
		8*8 /* every char takes 8 consecutive bytes */
	};

	struct GfxElement *font;
	static unsigned short colortable[2*2];	/* ASG 980209 */


	switch_ui_orientation();

	if ((Machine->drv->video_attributes & VIDEO_PIXEL_ASPECT_RATIO_MASK)
			== VIDEO_PIXEL_ASPECT_RATIO_1_2)
	{
		font = decodegfx(fontdata6x8,&fontlayout12x8);
		Machine->uifontwidth = 12;
		Machine->uifontheight = 8;
	}
	else if (Machine->uiwidth >= 420 && Machine->uiheight >= 420)
	{
		font = decodegfx(fontdata6x8,&fontlayout12x16);
		Machine->uifontwidth = 12;
		Machine->uifontheight = 16;
	}
	else
	{
		font = decodegfx(fontdata6x8,&fontlayout6x8);
		Machine->uifontwidth = 6;
		Machine->uifontheight = 8;
	}

	if (font)
	{
		/* colortable will be set at run time */
		memset(colortable,0,sizeof(colortable));
		font->colortable = colortable;
		font->total_colors = 2;
	}

	switch_true_orientation();

	return font;
}



/***************************************************************************

  Display text on the screen. If erase is 0, it superimposes the text on
  the last frame displayed.

***************************************************************************/

void displaytext(struct osd_bitmap *bitmap,const struct DisplayText *dt,int erase,int update_screen)
{
	if (erase)
		osd_clearbitmap(bitmap);


	switch_ui_orientation();

	osd_mark_dirty (0,0,Machine->uiwidth-1,Machine->uiheight-1,1);	/* ASG 971011 */

	while (dt->text)
	{
		int x,y;
		const char *c;


		x = dt->x;
		y = dt->y;
		c = dt->text;

		while (*c)
		{
			int wrapped;


			wrapped = 0;

			if (*c == '\n')
			{
				x = dt->x;
				y += Machine->uifontheight + 1;
				wrapped = 1;
			}
			else if (*c == ' ')
			{
				/* don't try to word wrap at the beginning of a line (this would cause */
				/* an endless loop if a word is longer than a line) */
				if (x != dt->x)
				{
					int nextlen=0;
					const char *nc;


					nc = c+1;
					while (*nc && *nc != ' ' && *nc != '\n')
					{
						nextlen += Machine->uifontwidth;
						nc++;
					}

					/* word wrap */
					if (x + Machine->uifontwidth + nextlen > Machine->uiwidth)
					{
						x = dt->x;
						y += Machine->uifontheight + 1;
						wrapped = 1;
					}
				}
			}

			if (!wrapped)
			{
				drawgfx(bitmap,Machine->uifont,*c,dt->color,0,0,x+Machine->uixmin,y+Machine->uiymin,0,TRANSPARENCY_NONE,0);
				x += Machine->uifontwidth;
			}

			c++;
		}

		dt++;
	}

	switch_true_orientation();

	if (update_screen) update_video_and_audio();
}

/* Writes messages on the screen. */
static void ui_text_ex(struct osd_bitmap *bitmap,const char* buf_begin, const char* buf_end, int x, int y, int color)
{
	switch_ui_orientation();

	for (;buf_begin != buf_end; ++buf_begin)
	{
		drawgfx(bitmap,Machine->uifont,*buf_begin,color,0,0,
				x + Machine->uixmin,
				y + Machine->uiymin, 0,TRANSPARENCY_NONE,0);
		x += Machine->uifontwidth;
	}

	switch_true_orientation();
}

/* Writes messages on the screen. */
void ui_text(struct osd_bitmap *bitmap,const char *buf,int x,int y)
{
	ui_text_ex(bitmap, buf, buf + strlen(buf), x, y, UI_COLOR_NORMAL);
}


void ui_drawbox(struct osd_bitmap *bitmap,int leftx,int topy,int width,int height)
{
	unsigned short black,white;


	switch_ui_orientation();

	if (leftx < 0) leftx = 0;
	if (topy < 0) topy = 0;
	if (width > Machine->uiwidth) width = Machine->uiwidth;
	if (height > Machine->uiheight) height = Machine->uiheight;

	leftx += Machine->uixmin;
	topy += Machine->uiymin;

	black = Machine->uifont->colortable[0];
	white = Machine->uifont->colortable[1];

	plot_box(bitmap,leftx,        topy,         width,  1,       white);
	plot_box(bitmap,leftx,        topy+height-1,width,  1,       white);
	plot_box(bitmap,leftx,        topy,         1,      height,  white);
	plot_box(bitmap,leftx+width-1,topy,         1,      height,  white);
	plot_box(bitmap,leftx+1,      topy+1,       width-2,height-2,black);

	switch_true_orientation();
}


static void drawbar(struct osd_bitmap *bitmap,int leftx,int topy,int width,int height,int percentage,int default_percentage)
{
	unsigned short black,white;


	switch_ui_orientation();

	if (leftx < 0) leftx = 0;
	if (topy < 0) topy = 0;
	if (width > Machine->uiwidth) width = Machine->uiwidth;
	if (height > Machine->uiheight) height = Machine->uiheight;

	leftx += Machine->uixmin;
	topy += Machine->uiymin;

	black = Machine->uifont->colortable[0];
	white = Machine->uifont->colortable[1];

	plot_box(bitmap,leftx+(width-1)*default_percentage/100,topy,1,height/8,white);

	plot_box(bitmap,leftx,topy+height/8,width,1,white);

	plot_box(bitmap,leftx,topy+height/8,1+(width-1)*percentage/100,height-2*(height/8),white);

	plot_box(bitmap,leftx,topy+height-height/8-1,width,1,white);

	plot_box(bitmap,leftx+(width-1)*default_percentage/100,topy+height-height/8,1,height/8,white);

	switch_true_orientation();
}

/* Extract one line from a multiline buffer */
/* Return the characters number of the line, pbegin point to the start of the next line */
static unsigned multiline_extract(const char** pbegin, const char* end, unsigned max)
{
	unsigned mac = 0;
	const char* begin = *pbegin;
	while (begin != end && mac < max)
	{
		if (*begin == '\n')
		{
			*pbegin = begin + 1; /* strip final space */
			return mac;
		}
		else if (*begin == ' ')
		{
			const char* word_end = begin + 1;
			while (word_end != end && *word_end != ' ' && *word_end != '\n')
				++word_end;
			if (mac + word_end - begin > max)
			{
				if (mac)
				{
					*pbegin = begin + 1;
					return mac; /* strip final space */
				} else {
					*pbegin = begin + max;
					return max;
				}
			}
			mac += word_end - begin;
			begin = word_end;
		} else {
			++mac;
			++begin;
		}
	}
	if (begin != end && (*begin == '\n' || *begin == ' '))
		++begin;
	*pbegin = begin;
	return mac;
}

/* Compute the output size of a multiline string */
static void multiline_size(int* dx, int* dy, const char* begin, const char* end, unsigned max)
{
	unsigned rows = 0;
	unsigned cols = 0;
	while (begin != end)
	{
		unsigned len;
		len = multiline_extract(&begin,end,max);
		if (len > cols)
			cols = len;
		++rows;
	}
	*dx = cols * Machine->uifontwidth;
	*dy = (rows-1) * 3*Machine->uifontheight/2 + Machine->uifontheight;
}

/* Compute the output size of a multiline string with box */
static void multilinebox_size(int* dx, int* dy, const char* begin, const char* end, unsigned max)
{
	multiline_size(dx,dy,begin,end,max);
	*dx += Machine->uifontwidth;
	*dy += Machine->uifontheight;
}

/* Display a multiline string */
static void ui_multitext_ex(struct osd_bitmap *bitmap, const char* begin, const char* end, unsigned max, int x, int y, int color)
{
	while (begin != end)
	{
		const char* line_begin = begin;
		unsigned len = multiline_extract(&begin,end,max);
		ui_text_ex(bitmap, line_begin, line_begin + len,x,y,color);
		y += 3*Machine->uifontheight/2;
	}
}

/* Display a multiline string with box */
static void ui_multitextbox_ex(struct osd_bitmap *bitmap,const char* begin, const char* end, unsigned max, int x, int y, int dx, int dy, int color)
{
	ui_drawbox(bitmap,x,y,dx,dy);
	x += Machine->uifontwidth/2;
	y += Machine->uifontheight/2;
	ui_multitext_ex(bitmap,begin,end,max,x,y,color);
}

void ui_displaymenu(struct osd_bitmap *bitmap,const char **items,const char **subitems,char *flag,int selected,int arrowize_subitem)
{
	struct DisplayText dt[256];
	int curr_dt;
	const char *lefthilight = ui_getstring (UI_lefthilight);
	const char *righthilight = ui_getstring (UI_righthilight);
	const char *uparrow = ui_getstring (UI_uparrow);
	const char *downarrow = ui_getstring (UI_downarrow);
	const char *leftarrow = ui_getstring (UI_leftarrow);
	const char *rightarrow = ui_getstring (UI_rightarrow);
	int i,count,len,maxlen,highlen;
	int leftoffs,topoffs,visible,topitem;
	int selected_long;


	i = 0;
	maxlen = 0;
	highlen = Machine->uiwidth / Machine->uifontwidth;
	while (items[i])
	{
		len = 3 + strlen(items[i]);
		if (subitems && subitems[i])
			len += 2 + strlen(subitems[i]);
		if (len > maxlen && len <= highlen)
			maxlen = len;
		i++;
	}
	count = i;

	visible = Machine->uiheight / (3 * Machine->uifontheight / 2) - 1;
	topitem = 0;
	if (visible > count) visible = count;
	else
	{
		topitem = selected - visible / 2;
		if (topitem < 0) topitem = 0;
		if (topitem > count - visible) topitem = count - visible;
	}

	leftoffs = (Machine->uiwidth - maxlen * Machine->uifontwidth) / 2;
	topoffs = (Machine->uiheight - (3 * visible + 1) * Machine->uifontheight / 2) / 2;

	/* black background */
	ui_drawbox(bitmap,leftoffs,topoffs,maxlen * Machine->uifontwidth,(3 * visible + 1) * Machine->uifontheight / 2);

	selected_long = 0;
	curr_dt = 0;
	for (i = 0;i < visible;i++)
	{
		int item = i + topitem;

		if (i == 0 && item > 0)
		{
			dt[curr_dt].text = uparrow;
			dt[curr_dt].color = UI_COLOR_NORMAL;
			dt[curr_dt].x = (Machine->uiwidth - Machine->uifontwidth * strlen(uparrow)) / 2;
			dt[curr_dt].y = topoffs + (3*i+1)*Machine->uifontheight/2;
			curr_dt++;
		}
		else if (i == visible - 1 && item < count - 1)
		{
			dt[curr_dt].text = downarrow;
			dt[curr_dt].color = UI_COLOR_NORMAL;
			dt[curr_dt].x = (Machine->uiwidth - Machine->uifontwidth * strlen(downarrow)) / 2;
			dt[curr_dt].y = topoffs + (3*i+1)*Machine->uifontheight/2;
			curr_dt++;
		}
		else
		{
			if (subitems && subitems[item])
			{
				int sublen;
				len = strlen(items[item]);
				dt[curr_dt].text = items[item];
				dt[curr_dt].color = UI_COLOR_NORMAL;
				dt[curr_dt].x = leftoffs + 3*Machine->uifontwidth/2;
				dt[curr_dt].y = topoffs + (3*i+1)*Machine->uifontheight/2;
				curr_dt++;
				sublen = strlen(subitems[item]);
				if (sublen > maxlen-5-len)
				{
					dt[curr_dt].text = "...";
					sublen = strlen(dt[curr_dt].text);
					if (item == selected)
						selected_long = 1;
				} else {
					dt[curr_dt].text = subitems[item];
				}
				/* If this item is flagged, draw it in inverse print */
				dt[curr_dt].color = (flag && flag[item]) ? UI_COLOR_INVERSE : UI_COLOR_NORMAL;
				dt[curr_dt].x = leftoffs + Machine->uifontwidth * (maxlen-1-sublen) - Machine->uifontwidth/2;
				dt[curr_dt].y = topoffs + (3*i+1)*Machine->uifontheight/2;
				curr_dt++;
			}
			else
			{
				dt[curr_dt].text = items[item];
				dt[curr_dt].color = UI_COLOR_NORMAL;
				dt[curr_dt].x = (Machine->uiwidth - Machine->uifontwidth * strlen(items[item])) / 2;
				dt[curr_dt].y = topoffs + (3*i+1)*Machine->uifontheight/2;
				curr_dt++;
			}
		}
	}

	i = selected - topitem;
	if (subitems && subitems[selected] && arrowize_subitem)
	{
		if (arrowize_subitem & 1)
		{
			dt[curr_dt].text = leftarrow;
			dt[curr_dt].color = UI_COLOR_NORMAL;
			dt[curr_dt].x = leftoffs + Machine->uifontwidth * (maxlen-2 - strlen(subitems[selected])) - Machine->uifontwidth/2 - 1;
			dt[curr_dt].y = topoffs + (3*i+1)*Machine->uifontheight/2;
			curr_dt++;
		}
		if (arrowize_subitem & 2)
		{
			dt[curr_dt].text = rightarrow;
			dt[curr_dt].color = UI_COLOR_NORMAL;
			dt[curr_dt].x = leftoffs + Machine->uifontwidth * (maxlen-1) - Machine->uifontwidth/2;
			dt[curr_dt].y = topoffs + (3*i+1)*Machine->uifontheight/2;
			curr_dt++;
		}
	}
	else
	{
		dt[curr_dt].text = righthilight;
		dt[curr_dt].color = UI_COLOR_NORMAL;
		dt[curr_dt].x = leftoffs + Machine->uifontwidth * (maxlen-1) - Machine->uifontwidth/2;
		dt[curr_dt].y = topoffs + (3*i+1)*Machine->uifontheight/2;
		curr_dt++;
	}
	dt[curr_dt].text = lefthilight;
	dt[curr_dt].color = UI_COLOR_NORMAL;
	dt[curr_dt].x = leftoffs + Machine->uifontwidth/2;
	dt[curr_dt].y = topoffs + (3*i+1)*Machine->uifontheight/2;
	curr_dt++;

	dt[curr_dt].text = 0;	/* terminate array */

	displaytext(bitmap,dt,0,0);

	if (selected_long)
	{
		int long_dx;
		int long_dy;
		int long_x;
		int long_y;
		unsigned long_max;

		long_max = (Machine->uiwidth / Machine->uifontwidth) - 2;
		multilinebox_size(&long_dx,&long_dy,subitems[selected],subitems[selected] + strlen(subitems[selected]), long_max);

		long_x = Machine->uiwidth - long_dx;
		long_y = topoffs + (i+1) * 3*Machine->uifontheight/2;

		/* if too low display up */
		if (long_y + long_dy > Machine->uiheight)
			long_y = topoffs + i * 3*Machine->uifontheight/2 - long_dy;

		ui_multitextbox_ex(bitmap,subitems[selected],subitems[selected] + strlen(subitems[selected]), long_max, long_x,long_y,long_dx,long_dy, UI_COLOR_NORMAL);
	}
}


void ui_displaymessagewindow(struct osd_bitmap *bitmap,const char *text)
{
	struct DisplayText dt[256];
	int curr_dt;
	char *c,*c2;
	int i,len,maxlen,lines;
	char textcopy[2048];
	int leftoffs,topoffs;
	int maxcols,maxrows;

	maxcols = (Machine->uiwidth / Machine->uifontwidth) - 1;
	maxrows = (2 * Machine->uiheight - Machine->uifontheight) / (3 * Machine->uifontheight);

	/* copy text, calculate max len, count lines, wrap long lines and crop height to fit */
	maxlen = 0;
	lines = 0;
	c = (char *)text;
	c2 = textcopy;
	while (*c)
	{
		len = 0;
		while (*c && *c != '\n')
		{
			*c2++ = *c++;
			len++;
			if (len == maxcols && *c != '\n')
			{
				/* attempt word wrap */
				char *csave = c, *c2save = c2;
				int lensave = len;

				/* back up to last space or beginning of line */
				while (*c != ' ' && *c != '\n' && c > text)
					--c, --c2, --len;

				/* if no space was found, hard wrap instead */
				if (*c != ' ')
					c = csave, c2 = c2save, len = lensave;
				else
					c++;

				*c2++ = '\n'; /* insert wrap */
				break;
			}
		}

		if (*c == '\n')
			*c2++ = *c++;

		if (len > maxlen) maxlen = len;

		lines++;
		if (lines == maxrows)
			break;
	}
	*c2 = '\0';

	maxlen += 1;

	leftoffs = (Machine->uiwidth - Machine->uifontwidth * maxlen) / 2;
	if (leftoffs < 0) leftoffs = 0;
	topoffs = (Machine->uiheight - (3 * lines + 1) * Machine->uifontheight / 2) / 2;

	/* black background */
	ui_drawbox(bitmap,leftoffs,topoffs,maxlen * Machine->uifontwidth,(3 * lines + 1) * Machine->uifontheight / 2);

	curr_dt = 0;
	c = textcopy;
	i = 0;
	while (*c)
	{
		c2 = c;
		while (*c && *c != '\n')
			c++;

		if (*c == '\n')
		{
			*c = '\0';
			c++;
		}

		if (*c2 == '\t')    /* center text */
		{
			c2++;
			dt[curr_dt].x = (Machine->uiwidth - Machine->uifontwidth * (c - c2)) / 2;
		}
		else
			dt[curr_dt].x = leftoffs + Machine->uifontwidth/2;

		dt[curr_dt].text = c2;
		dt[curr_dt].color = UI_COLOR_NORMAL;
		dt[curr_dt].y = topoffs + (3*i+1)*Machine->uifontheight/2;
		curr_dt++;

		i++;
	}

	dt[curr_dt].text = 0;	/* terminate array */

	displaytext(bitmap,dt,0,0);
}



#ifndef MESS
extern int no_of_tiles;
void NeoMVSDrawGfx(unsigned char **line,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		int zx,int zy,const struct rectangle *clip);
void NeoMVSDrawGfx16(unsigned char **line,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		int zx,int zy,const struct rectangle *clip);
extern struct GameDriver driver_neogeo;
#endif

static void showcharset(struct osd_bitmap *bitmap)
{
	int i;
	char buf[80];
	int bank,color,firstdrawn;
	int palpage;
	int changed;
	int game_is_neogeo=0;
	unsigned char *orig_used_colors=0;


	if (palette_used_colors)
	{
		orig_used_colors = (unsigned char*) malloc(Machine->drv->total_colors * sizeof(unsigned char));
		if (!orig_used_colors) return;

		memcpy(orig_used_colors,palette_used_colors,Machine->drv->total_colors * sizeof(unsigned char));
	}

#ifndef MESS
	if (Machine->gamedrv->clone_of == &driver_neogeo ||
			(Machine->gamedrv->clone_of &&
				Machine->gamedrv->clone_of->clone_of == &driver_neogeo))
		game_is_neogeo=1;
#endif

	bank = -1;
	color = 0;
	firstdrawn = 0;
	palpage = 0;

	changed = 1;

	do
	{
		int cpx,cpy,skip_chars;

		if (bank >= 0)
		{
			cpx = Machine->uiwidth / Machine->gfx[bank]->width;
			cpy = (Machine->uiheight - Machine->uifontheight) / Machine->gfx[bank]->height;
			skip_chars = cpx * cpy;
		}
		else cpx = cpy = skip_chars = 0;

		if (changed)
		{
			int lastdrawn=0;

			osd_clearbitmap(bitmap);

			/* validity chack after char bank change */
			if (bank >= 0)
			{
				if (firstdrawn >= Machine->gfx[bank]->total_elements)
				{
					firstdrawn = Machine->gfx[bank]->total_elements - skip_chars;
					if (firstdrawn < 0) firstdrawn = 0;
				}
			}

			if(bank!=2 || !game_is_neogeo)
			{
				switch_ui_orientation();

				if (bank >= 0)
				{
					int table_offs;
					int flipx,flipy;

					if (palette_used_colors)
					{
						memset(palette_used_colors,PALETTE_COLOR_TRANSPARENT,Machine->drv->total_colors * sizeof(unsigned char));
						table_offs = Machine->gfx[bank]->colortable - Machine->remapped_colortable
								+ Machine->gfx[bank]->color_granularity * color;
						for (i = 0;i < Machine->gfx[bank]->color_granularity;i++)
							palette_used_colors[Machine->game_colortable[table_offs + i]] = PALETTE_COLOR_USED;
						palette_recalc();	/* do it twice in case of previous overflow */
						palette_recalc();	/*(we redraw the screen only when it changes) */
					}

#ifndef PREROTATE_GFX
					flipx = (Machine->orientation ^ trueorientation) & ORIENTATION_FLIP_X;
					flipy = (Machine->orientation ^ trueorientation) & ORIENTATION_FLIP_Y;

					if (Machine->orientation & ORIENTATION_SWAP_XY)
					{
						int t;
						t = flipx; flipx = flipy; flipy = t;
					}
#else
					flipx = 0;
					flipy = 0;
#endif

					for (i = 0; i+firstdrawn < Machine->gfx[bank]->total_elements && i<cpx*cpy; i++)
					{
						drawgfx(bitmap,Machine->gfx[bank],
								i+firstdrawn,color,  /*sprite num, color*/
								flipx,flipy,
								(i % cpx) * Machine->gfx[bank]->width + Machine->uixmin,
								Machine->uifontheight + (i / cpx) * Machine->gfx[bank]->height + Machine->uiymin,
								0,TRANSPARENCY_NONE,0);

						lastdrawn = i+firstdrawn;
					}
				}
				else
				{
					int sx,sy,colors;

					colors = Machine->drv->total_colors - 256 * palpage;
					if (colors > 256) colors = 256;
					if (palette_used_colors)
					{
						memset(palette_used_colors,PALETTE_COLOR_UNUSED,Machine->drv->total_colors * sizeof(unsigned char));
						memset(palette_used_colors+256*palpage,PALETTE_COLOR_USED,colors * sizeof(unsigned char));
						palette_recalc();	/* do it twice in case of previous overflow */
						palette_recalc();	/*(we redraw the screen only when it changes) */
					}

					for (i = 0;i < 16;i++)
					{
						char bf[40];

						sx = 3*Machine->uifontwidth + (Machine->uifontwidth*4/3)*(i % 16);
						sprintf(bf,"%X",i);
						ui_text(bitmap,bf,sx,2*Machine->uifontheight);
						if (16*i < colors)
						{
							sy = 3*Machine->uifontheight + (Machine->uifontheight)*(i % 16);
							sprintf(bf,"%3X",i+16*palpage);
							ui_text(bitmap,bf,0,sy);
						}
					}

					for (i = 0;i < colors;i++)
					{
						sx = Machine->uixmin + 3*Machine->uifontwidth + (Machine->uifontwidth*4/3)*(i % 16);
						sy = Machine->uiymin + 2*Machine->uifontheight + (Machine->uifontheight)*(i / 16) + Machine->uifontheight;
						plot_box(bitmap,sx,sy,Machine->uifontwidth*4/3,Machine->uifontheight,Machine->pens[i + 256*palpage]);
					}
				}

				switch_true_orientation();
			}
#ifndef MESS
			else	/* neogeo sprite tiles */
			{
				struct rectangle clip;

				clip.min_x = Machine->uixmin;
				clip.max_x = Machine->uixmin + Machine->uiwidth - 1;
				clip.min_y = Machine->uiymin;
				clip.max_y = Machine->uiymin + Machine->uiheight - 1;

				if (palette_used_colors)
				{
					memset(palette_used_colors,PALETTE_COLOR_TRANSPARENT,Machine->drv->total_colors * sizeof(unsigned char));
					memset(palette_used_colors+Machine->gfx[bank]->color_granularity*color,PALETTE_COLOR_USED,Machine->gfx[bank]->color_granularity * sizeof(unsigned char));
					palette_recalc();	/* do it twice in case of previous overflow */
					palette_recalc();	/*(we redraw the screen only when it changes) */
				}

				for (i = 0; i+firstdrawn < no_of_tiles && i<cpx*cpy; i++)
				{
					if (bitmap->depth == 16)
						NeoMVSDrawGfx16(bitmap->line,Machine->gfx[bank],
							i+firstdrawn,color,  /*sprite num, color*/
							0,0,
							(i % cpx) * Machine->gfx[bank]->width + Machine->uixmin,
							Machine->uifontheight+1 + (i / cpx) * Machine->gfx[bank]->height + Machine->uiymin,
							16,16,&clip);
					else
						NeoMVSDrawGfx(bitmap->line,Machine->gfx[bank],
							i+firstdrawn,color,  /*sprite num, color*/
							0,0,
							(i % cpx) * Machine->gfx[bank]->width + Machine->uixmin,
							Machine->uifontheight+1 + (i / cpx) * Machine->gfx[bank]->height + Machine->uiymin,
							16,16,&clip);

					lastdrawn = i+firstdrawn;
				}
			}
#endif

			if (bank >= 0)
				sprintf(buf,"GFXSET %d COLOR %2X CODE %X-%X",bank,color,firstdrawn,lastdrawn);
			else
				strcpy(buf,"PALETTE");
			ui_text(bitmap,buf,0,0);

			changed = 0;
		}

		update_video_and_audio();
		osd_poll_joysticks();

		if (code_pressed(KEYCODE_LCONTROL) || code_pressed(KEYCODE_RCONTROL))
		{
			skip_chars = cpx;
		}
		if (code_pressed(KEYCODE_LSHIFT) || code_pressed(KEYCODE_RSHIFT))
		{
			skip_chars = 1;
		}


		if (input_ui_pressed_repeat(IPT_UI_RIGHT,8))
		{
			if (bank+1 < MAX_GFX_ELEMENTS && Machine->gfx[bank + 1])
			{
				bank++;
//				firstdrawn = 0;
				changed = 1;
			}
		}

		if (input_ui_pressed_repeat(IPT_UI_LEFT,8))
		{
			if (bank > -1)
			{
				bank--;
//				firstdrawn = 0;
				changed = 1;
			}
		}

		if (code_pressed_memory_repeat(KEYCODE_PGDN,4))
		{
			if (bank >= 0)
			{
				if (firstdrawn + skip_chars < Machine->gfx[bank]->total_elements)
				{
					firstdrawn += skip_chars;
					changed = 1;
				}
			}
			else
			{
				if (256 * (palpage + 1) < Machine->drv->total_colors)
				{
					palpage++;
					changed = 1;
				}
			}
		}

		if (code_pressed_memory_repeat(KEYCODE_PGUP,4))
		{
			if (bank >= 0)
			{
				firstdrawn -= skip_chars;
				if (firstdrawn < 0) firstdrawn = 0;
				changed = 1;
			}
			else
			{
				if (palpage > 0)
				{
					palpage--;
					changed = 1;
				}
			}
		}

		if (input_ui_pressed_repeat(IPT_UI_UP,6))
		{
			if (bank >= 0)
			{
				if (color < Machine->gfx[bank]->total_colors - 1)
				{
					color++;
					changed = 1;
				}
			}
		}

		if (input_ui_pressed_repeat(IPT_UI_DOWN,6))
		{
			if (color > 0)
			{
				color--;
				changed = 1;
			}
		}

		if (input_ui_pressed(IPT_UI_SNAPSHOT))
			osd_save_snapshot(bitmap);
	} while (!input_ui_pressed(IPT_UI_SHOW_GFX) &&
			!input_ui_pressed(IPT_UI_CANCEL));

	/* clear the screen before returning */
	osd_clearbitmap(bitmap);

	if (palette_used_colors)
	{
		/* this should force a full refresh by the video driver */
		memset(palette_used_colors,PALETTE_COLOR_TRANSPARENT,Machine->drv->total_colors * sizeof(unsigned char));
		palette_recalc();
		/* restore the game used colors array */
		memcpy(palette_used_colors,orig_used_colors,Machine->drv->total_colors * sizeof(unsigned char));
		free(orig_used_colors);
	}

	return;
}

static int setdipswitches(struct osd_bitmap *bitmap,int selected)
{
	const char *menu_item[128];
	const char *menu_subitem[128];
	struct InputPort *entry[128];
	char flag[40];
	int i,sel;
	struct InputPort *in;
	int total;
	int arrowize;


	sel = selected - 1;


	in = Machine->input_ports;

	total = 0;
	while (in->type != IPT_END)
	{
		if ((in->type & ~IPF_MASK) == IPT_DIPSWITCH_NAME && input_port_name(in) != 0 &&
				(in->type & IPF_UNUSED) == 0 &&
				!(!options.cheat && (in->type & IPF_CHEAT)))
		{
			entry[total] = in;
			menu_item[total] = input_port_name(in);

			total++;
		}

		in++;
	}

	if (total == 0) return 0;

	menu_item[total] = ui_getstring (UI_returntomain);
	menu_item[total + 1] = 0;	/* terminate array */
	total++;


	for (i = 0;i < total;i++)
	{
		flag[i] = 0; /* TODO: flag the dip if it's not the real default */
		if (i < total - 1)
		{
			in = entry[i] + 1;
			while ((in->type & ~IPF_MASK) == IPT_DIPSWITCH_SETTING &&
					in->default_value != entry[i]->default_value)
				in++;

			if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING)
				menu_subitem[i] = ui_getstring (UI_INVALID);
			else menu_subitem[i] = input_port_name(in);
		}
		else menu_subitem[i] = 0;	/* no subitem */
	}

	arrowize = 0;
	if (sel < total - 1)
	{
		in = entry[sel] + 1;
		while ((in->type & ~IPF_MASK) == IPT_DIPSWITCH_SETTING &&
				in->default_value != entry[sel]->default_value)
			in++;

		if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING)
			/* invalid setting: revert to a valid one */
			arrowize |= 1;
		else
		{
			if (((in-1)->type & ~IPF_MASK) == IPT_DIPSWITCH_SETTING &&
					!(!options.cheat && ((in-1)->type & IPF_CHEAT)))
				arrowize |= 1;
		}
	}
	if (sel < total - 1)
	{
		in = entry[sel] + 1;
		while ((in->type & ~IPF_MASK) == IPT_DIPSWITCH_SETTING &&
				in->default_value != entry[sel]->default_value)
			in++;

		if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING)
			/* invalid setting: revert to a valid one */
			arrowize |= 2;
		else
		{
			if (((in+1)->type & ~IPF_MASK) == IPT_DIPSWITCH_SETTING &&
					!(!options.cheat && ((in+1)->type & IPF_CHEAT)))
				arrowize |= 2;
		}
	}

	ui_displaymenu(bitmap,menu_item,menu_subitem,flag,sel,arrowize);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		sel = (sel + 1) % total;

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
		sel = (sel + total - 1) % total;

	if (input_ui_pressed_repeat(IPT_UI_RIGHT,8))
	{
		if (sel < total - 1)
		{
			in = entry[sel] + 1;
			while ((in->type & ~IPF_MASK) == IPT_DIPSWITCH_SETTING &&
					in->default_value != entry[sel]->default_value)
				in++;

			if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING)
				/* invalid setting: revert to a valid one */
				entry[sel]->default_value = (entry[sel]+1)->default_value & entry[sel]->mask;
			else
			{
				if (((in+1)->type & ~IPF_MASK) == IPT_DIPSWITCH_SETTING &&
						!(!options.cheat && ((in+1)->type & IPF_CHEAT)))
					entry[sel]->default_value = (in+1)->default_value & entry[sel]->mask;
			}

			/* tell updatescreen() to clean after us (in case the window changes size) */
			need_to_clear_bitmap = 1;
		}
	}

	if (input_ui_pressed_repeat(IPT_UI_LEFT,8))
	{
		if (sel < total - 1)
		{
			in = entry[sel] + 1;
			while ((in->type & ~IPF_MASK) == IPT_DIPSWITCH_SETTING &&
					in->default_value != entry[sel]->default_value)
				in++;

			if ((in->type & ~IPF_MASK) != IPT_DIPSWITCH_SETTING)
				/* invalid setting: revert to a valid one */
				entry[sel]->default_value = (entry[sel]+1)->default_value & entry[sel]->mask;
			else
			{
				if (((in-1)->type & ~IPF_MASK) == IPT_DIPSWITCH_SETTING &&
						!(!options.cheat && ((in-1)->type & IPF_CHEAT)))
					entry[sel]->default_value = (in-1)->default_value & entry[sel]->mask;
			}

			/* tell updatescreen() to clean after us (in case the window changes size) */
			need_to_clear_bitmap = 1;
		}
	}

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == total - 1) sel = -1;
	}

	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}

/* This flag is used for record OR sequence of key/joy */
/* when is !=0 the first sequence is record, otherwise the first free */
/* it's used byt setdefkeysettings, setdefjoysettings, setkeysettings, setjoysettings */
static int record_first_insert = 1;

static char menu_subitem_buffer[400][96];

static int setdefcodesettings(struct osd_bitmap *bitmap,int selected)
{
	const char *menu_item[400];
	const char *menu_subitem[400];
	struct ipd *entry[400];
	char flag[400];
	int i,sel;
	struct ipd *in;
	int total;
	extern struct ipd inputport_defaults[];

	sel = selected - 1;


	if (Machine->input_ports == 0)
		return 0;

	in = inputport_defaults;

	total = 0;
	while (in->type != IPT_END)
	{
		if (in->name != 0  && (in->type & ~IPF_MASK) != IPT_UNKNOWN && (in->type & IPF_UNUSED) == 0
			&& !(!options.cheat && (in->type & IPF_CHEAT)))
		{
			entry[total] = in;
			menu_item[total] = in->name;

			total++;
		}

		in++;
	}

	if (total == 0) return 0;

	menu_item[total] = ui_getstring (UI_returntomain);
	menu_item[total + 1] = 0;	/* terminate array */
	total++;

	for (i = 0;i < total;i++)
	{
		if (i < total - 1)
		{
			seq_name(&entry[i]->seq,menu_subitem_buffer[i],sizeof(menu_subitem_buffer[0]));
			menu_subitem[i] = menu_subitem_buffer[i];
		} else
			menu_subitem[i] = 0;	/* no subitem */
		flag[i] = 0;
	}

	if (sel > SEL_MASK)   /* are we waiting for a new key? */
	{
		int ret;

		menu_subitem[sel & SEL_MASK] = "    ";
		ui_displaymenu(bitmap,menu_item,menu_subitem,flag,sel & SEL_MASK,3);

		ret = seq_read_async(&entry[sel & SEL_MASK]->seq,record_first_insert);

		if (ret >= 0)
		{
			sel &= 0xff;

			if (ret > 0 || seq_get_1(&entry[sel]->seq) == CODE_NONE)
			{
				seq_set_1(&entry[sel]->seq,CODE_NONE);
				ret = 1;
			}

			/* tell updatescreen() to clean after us (in case the window changes size) */
			need_to_clear_bitmap = 1;

			record_first_insert = ret != 0;
		}


		return sel + 1;
	}


	ui_displaymenu(bitmap,menu_item,menu_subitem,flag,sel,0);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
	{
		sel = (sel + 1) % total;
		record_first_insert = 1;
	}

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
	{
		sel = (sel + total - 1) % total;
		record_first_insert = 1;
	}

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == total - 1) sel = -1;
		else
		{
			seq_read_async_start();

			sel |= 1 << SEL_BITS;	/* we'll ask for a key */

			/* tell updatescreen() to clean after us (in case the window changes size) */
			need_to_clear_bitmap = 1;
		}
	}

	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;

		record_first_insert = 1;
	}

	return sel + 1;
}



static int setcodesettings(struct osd_bitmap *bitmap,int selected)
{
	const char *menu_item[400];
	const char *menu_subitem[400];
	struct InputPort *entry[400];
	char flag[400];
	int i,sel;
	struct InputPort *in;
	int total;


	sel = selected - 1;


	if (Machine->input_ports == 0)
		return 0;

	in = Machine->input_ports;

	total = 0;
	while (in->type != IPT_END)
	{
		if (input_port_name(in) != 0 && seq_get_1(&in->seq) != CODE_NONE && (in->type & ~IPF_MASK) != IPT_UNKNOWN)
		{
			entry[total] = in;
			menu_item[total] = input_port_name(in);

			total++;
		}

		in++;
	}

	if (total == 0) return 0;

	menu_item[total] = ui_getstring (UI_returntomain);
	menu_item[total + 1] = 0;	/* terminate array */
	total++;

	for (i = 0;i < total;i++)
	{
		if (i < total - 1)
		{
			seq_name(input_port_seq(entry[i]),menu_subitem_buffer[i],sizeof(menu_subitem_buffer[0]));
			menu_subitem[i] = menu_subitem_buffer[i];

			/* If the key isn't the default, flag it */
			if (seq_get_1(&entry[i]->seq) != CODE_DEFAULT)
				flag[i] = 1;
			else
				flag[i] = 0;

		} else
			menu_subitem[i] = 0;	/* no subitem */
	}

	if (sel > SEL_MASK)   /* are we waiting for a new key? */
	{
		int ret;

		menu_subitem[sel & SEL_MASK] = "    ";
		ui_displaymenu(bitmap,menu_item,menu_subitem,flag,sel & SEL_MASK,3);

		ret = seq_read_async(&entry[sel & SEL_MASK]->seq,record_first_insert);

		if (ret >= 0)
		{
			sel &= 0xff;

			if (ret > 0 || seq_get_1(&entry[sel]->seq) == CODE_NONE)
			{
				seq_set_1(&entry[sel]->seq, CODE_DEFAULT);
				ret = 1;
			}

			/* tell updatescreen() to clean after us (in case the window changes size) */
			need_to_clear_bitmap = 1;

			record_first_insert = ret != 0;
		}

		return sel + 1;
	}


	ui_displaymenu(bitmap,menu_item,menu_subitem,flag,sel,0);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
	{
		sel = (sel + 1) % total;
		record_first_insert = 1;
	}

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
	{
		sel = (sel + total - 1) % total;
		record_first_insert = 1;
	}

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == total - 1) sel = -1;
		else
		{
			seq_read_async_start();

			sel |= 1 << SEL_BITS;	/* we'll ask for a key */

			/* tell updatescreen() to clean after us (in case the window changes size) */
			need_to_clear_bitmap = 1;
		}
	}

	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;

		record_first_insert = 1;
	}

	return sel + 1;
}


static int calibratejoysticks(struct osd_bitmap *bitmap,int selected)
{
	char *msg;
	char buf[2048];
	int sel;
	static int calibration_started = 0;

	sel = selected - 1;

	if (calibration_started == 0)
	{
		osd_joystick_start_calibration();
		calibration_started = 1;
		strcpy (buf, "");
	}

	if (sel > SEL_MASK) /* Waiting for the user to acknowledge joystick movement */
	{
		if (input_ui_pressed(IPT_UI_CANCEL))
		{
			calibration_started = 0;
			sel = -1;
		}
		else if (input_ui_pressed(IPT_UI_SELECT))
		{
			osd_joystick_calibrate();
			sel &= 0xff;
		}

		ui_displaymessagewindow(bitmap,buf);
	}
	else
	{
		msg = osd_joystick_calibrate_next();
		need_to_clear_bitmap = 1;
		if (msg == 0)
		{
			calibration_started = 0;
			osd_joystick_end_calibration();
			sel = -1;
		}
		else
		{
			strcpy (buf, msg);
			ui_displaymessagewindow(bitmap,buf);
			sel |= 1 << SEL_BITS;
		}
	}

	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}


static int settraksettings(struct osd_bitmap *bitmap,int selected)
{
	const char *menu_item[40];
	const char *menu_subitem[40];
	struct InputPort *entry[40];
	int i,sel;
	struct InputPort *in;
	int total,total2;
	int arrowize;


	sel = selected - 1;


	if (Machine->input_ports == 0)
		return 0;

	in = Machine->input_ports;

	/* Count the total number of analog controls */
	total = 0;
	while (in->type != IPT_END)
	{
		if (((in->type & 0xff) > IPT_ANALOG_START) && ((in->type & 0xff) < IPT_ANALOG_END)
				&& !(!options.cheat && (in->type & IPF_CHEAT)))
		{
			entry[total] = in;
			total++;
		}
		in++;
	}

	if (total == 0) return 0;

	/* Each analog control has 3 entries - key & joy delta, reverse, sensitivity */

#define ENTRIES 3

	total2 = total * ENTRIES;

	menu_item[total2] = ui_getstring (UI_returntomain);
	menu_item[total2 + 1] = 0;	/* terminate array */
	total2++;

	arrowize = 0;
	for (i = 0;i < total2;i++)
	{
		if (i < total2 - 1)
		{
			char label[30][40];
			char setting[30][40];
			int sensitivity,delta;
			int reverse;

			strcpy (label[i], input_port_name(entry[i/ENTRIES]));
			sensitivity = IP_GET_SENSITIVITY(entry[i/ENTRIES]);
			delta = IP_GET_DELTA(entry[i/ENTRIES]);
			reverse = (entry[i/ENTRIES]->type & IPF_REVERSE);

			strcat (label[i], " ");
			switch (i%ENTRIES)
			{
				case 0:
					strcat (label[i], ui_getstring (UI_keyjoyspeed));
					sprintf(setting[i],"%d",delta);
					if (i == sel) arrowize = 3;
					break;
				case 1:
					strcat (label[i], ui_getstring (UI_reverse));
					if (reverse)
						sprintf(setting[i],ui_getstring (UI_on));
					else
						sprintf(setting[i],ui_getstring (UI_off));
					if (i == sel) arrowize = 3;
					break;
				case 2:
					strcat (label[i], ui_getstring (UI_sensitivity));
					sprintf(setting[i],"%3d%%",sensitivity);
					if (i == sel) arrowize = 3;
					break;
			}

			menu_item[i] = label[i];
			menu_subitem[i] = setting[i];

			in++;
		}
		else menu_subitem[i] = 0;	/* no subitem */
	}

	ui_displaymenu(bitmap,menu_item,menu_subitem,0,sel,arrowize);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		sel = (sel + 1) % total2;

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
		sel = (sel + total2 - 1) % total2;

	if (input_ui_pressed_repeat(IPT_UI_LEFT,8))
	{
		if ((sel % ENTRIES) == 0)
		/* keyboard/joystick delta */
		{
			int val = IP_GET_DELTA(entry[sel/ENTRIES]);

			val --;
			if (val < 1) val = 1;
			IP_SET_DELTA(entry[sel/ENTRIES],val);
		}
		else if ((sel % ENTRIES) == 1)
		/* reverse */
		{
			int reverse = entry[sel/ENTRIES]->type & IPF_REVERSE;
			if (reverse)
				reverse=0;
			else
				reverse=IPF_REVERSE;
			entry[sel/ENTRIES]->type &= ~IPF_REVERSE;
			entry[sel/ENTRIES]->type |= reverse;
		}
		else if ((sel % ENTRIES) == 2)
		/* sensitivity */
		{
			int val = IP_GET_SENSITIVITY(entry[sel/ENTRIES]);

			val --;
			if (val < 1) val = 1;
			IP_SET_SENSITIVITY(entry[sel/ENTRIES],val);
		}
	}

	if (input_ui_pressed_repeat(IPT_UI_RIGHT,8))
	{
		if ((sel % ENTRIES) == 0)
		/* keyboard/joystick delta */
		{
			int val = IP_GET_DELTA(entry[sel/ENTRIES]);

			val ++;
			if (val > 255) val = 255;
			IP_SET_DELTA(entry[sel/ENTRIES],val);
		}
		else if ((sel % ENTRIES) == 1)
		/* reverse */
		{
			int reverse = entry[sel/ENTRIES]->type & IPF_REVERSE;
			if (reverse)
				reverse=0;
			else
				reverse=IPF_REVERSE;
			entry[sel/ENTRIES]->type &= ~IPF_REVERSE;
			entry[sel/ENTRIES]->type |= reverse;
		}
		else if ((sel % ENTRIES) == 2)
		/* sensitivity */
		{
			int val = IP_GET_SENSITIVITY(entry[sel/ENTRIES]);

			val ++;
			if (val > 255) val = 255;
			IP_SET_SENSITIVITY(entry[sel/ENTRIES],val);
		}
	}

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		if (sel == total2 - 1) sel = -1;
	}

	if (input_ui_pressed(IPT_UI_CANCEL))
		sel = -1;

	if (input_ui_pressed(IPT_UI_CONFIGURE))
		sel = -2;

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}

#ifndef MESS
static int mame_stats(struct osd_bitmap *bitmap,int selected)
{
	char temp[10];
	char buf[2048];
	int sel, i;


	sel = selected - 1;

	buf[0] = 0;

	if (dispensed_tickets)
	{
		strcat(buf, ui_getstring (UI_tickets));
		strcat(buf, ": ");
		sprintf(temp, "%d\n\n", dispensed_tickets);
		strcat(buf, temp);
	}

	for (i=0; i<COIN_COUNTERS; i++)
	{
		strcat(buf, ui_getstring (UI_coin));
		sprintf(temp, " %c: ", i+'A');
		strcat(buf, temp);
		if (!coins[i])
			strcat (buf, ui_getstring (UI_NA));
		else
		{
			sprintf (temp, "%d", coins[i]);
			strcat (buf, temp);
		}
		if (coinlockedout[i])
		{
			strcat(buf, " ");
			strcat(buf, ui_getstring (UI_locked));
			strcat(buf, "\n");
		}
		else
		{
			strcat(buf, "\n");
		}
	}

	{
		/* menu system, use the normal menu keys */
		strcat(buf,"\n\t");
		strcat(buf,ui_getstring (UI_lefthilight));
		strcat(buf," ");
		strcat(buf,ui_getstring (UI_returntomain));
		strcat(buf," ");
		strcat(buf,ui_getstring (UI_righthilight));

		ui_displaymessagewindow(bitmap,buf);

		if (input_ui_pressed(IPT_UI_SELECT))
			sel = -1;

		if (input_ui_pressed(IPT_UI_CANCEL))
			sel = -1;

		if (input_ui_pressed(IPT_UI_CONFIGURE))
			sel = -2;
	}

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}
#endif

int showcopyright(struct osd_bitmap *bitmap)
{
	int done;
	char buf[1000];
	char buf2[256];

	strcpy (buf, ui_getstring(UI_copyright1));
	strcat (buf, "\n\n");
	sprintf(buf2, ui_getstring(UI_copyright2), Machine->gamedrv->description);
	strcat (buf, buf2);
	strcat (buf, "\n\n");
	strcat (buf, ui_getstring(UI_copyright3));

	ui_displaymessagewindow(bitmap,buf);

	setup_selected = -1;////
	done = 0;
	do
	{
		update_video_and_audio();
		osd_poll_joysticks();
		if (input_ui_pressed(IPT_UI_CANCEL))
		{
			setup_selected = 0;////
			return 1;
		}
		if (keyboard_pressed_memory(KEYCODE_O) ||
				input_ui_pressed(IPT_UI_LEFT))
			done = 1;
		if (done == 1 && (keyboard_pressed_memory(KEYCODE_K) ||
				input_ui_pressed(IPT_UI_RIGHT)))
			done = 2;
	} while (done < 2);

	setup_selected = 0;////
	osd_clearbitmap(bitmap);
	update_video_and_audio();

	return 0;
}

static int displaygameinfo(struct osd_bitmap *bitmap,int selected)
{
	int i;
	char buf[2048];
	char buf2[32];
	int sel;


	sel = selected - 1;


	sprintf(buf,"%s\n%s %s\n\n%s:\n",Machine->gamedrv->description,Machine->gamedrv->year,Machine->gamedrv->manufacturer,
		ui_getstring (UI_cpu));
	i = 0;
	while (i < MAX_CPU && Machine->drv->cpu[i].cpu_type)
	{

		if (Machine->drv->cpu[i].cpu_clock >= 1000000)
			sprintf(&buf[strlen(buf)],"%s %d.%06d MHz",
					cputype_name(Machine->drv->cpu[i].cpu_type),
					Machine->drv->cpu[i].cpu_clock / 1000000,
					Machine->drv->cpu[i].cpu_clock % 1000000);
		else
			sprintf(&buf[strlen(buf)],"%s %d.%03d kHz",
					cputype_name(Machine->drv->cpu[i].cpu_type),
					Machine->drv->cpu[i].cpu_clock / 1000,
					Machine->drv->cpu[i].cpu_clock % 1000);

		if (Machine->drv->cpu[i].cpu_type & CPU_AUDIO_CPU)
		{
			sprintf (buf2, " (%s)", ui_getstring (UI_sound_lc));
			strcat(buf, buf2);
		}

		strcat(buf,"\n");

		i++;
	}

	sprintf (buf2, "\n%s", ui_getstring (UI_sound));
	strcat (buf, buf2);
	if (Machine->drv->sound_attributes & SOUND_SUPPORTS_STEREO)
		sprintf(&buf[strlen(buf)]," (%s)", ui_getstring (UI_stereo));
	strcat(buf,":\n");

	i = 0;
	while (i < MAX_SOUND && Machine->drv->sound[i].sound_type)
	{
		if (sound_num(&Machine->drv->sound[i]))
			sprintf(&buf[strlen(buf)],"%dx",sound_num(&Machine->drv->sound[i]));

		sprintf(&buf[strlen(buf)],"%s",sound_name(&Machine->drv->sound[i]));

		if (sound_clock(&Machine->drv->sound[i]))
		{
			if (sound_clock(&Machine->drv->sound[i]) >= 1000000)
				sprintf(&buf[strlen(buf)]," %d.%06d MHz",
						sound_clock(&Machine->drv->sound[i]) / 1000000,
						sound_clock(&Machine->drv->sound[i]) % 1000000);
			else
				sprintf(&buf[strlen(buf)]," %d.%03d kHz",
						sound_clock(&Machine->drv->sound[i]) / 1000,
						sound_clock(&Machine->drv->sound[i]) % 1000);
		}

		strcat(buf,"\n");

		i++;
	}

	if (Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
		sprintf(&buf[strlen(buf)],"\n%s\n", ui_getstring (UI_vectorgame));
	else
	{
		int pixelx,pixely,tmax,tmin,rem;

		pixelx = 4 * (Machine->visible_area.max_y - Machine->visible_area.min_y + 1);
		pixely = 3 * (Machine->visible_area.max_x - Machine->visible_area.min_x + 1);

		/* calculate MCD */
		if (pixelx >= pixely)
		{
			tmax = pixelx;
			tmin = pixely;
		}
		else
		{
			tmax = pixely;
			tmin = pixelx;
		}
		while ( (rem = tmax % tmin) )
		{
			tmax = tmin;
			tmin = rem;
		}
		/* tmin is now the MCD */

		pixelx /= tmin;
		pixely /= tmin;

		sprintf(&buf[strlen(buf)],"\n%s:\n", ui_getstring (UI_screenres));
		sprintf(&buf[strlen(buf)],"%d x %d (%s) %f Hz\n",
				Machine->visible_area.max_x - Machine->visible_area.min_x + 1,
				Machine->visible_area.max_y - Machine->visible_area.min_y + 1,
				(Machine->gamedrv->flags & ORIENTATION_SWAP_XY) ? "V" : "H",
				Machine->drv->frames_per_second);
#if 0
		sprintf(&buf[strlen(buf)],"pixel aspect ratio %d:%d\n",
				pixelx,pixely);
		sprintf(&buf[strlen(buf)],"%d colors ",Machine->drv->total_colors);
		if (Machine->gamedrv->flags & GAME_REQUIRES_16BIT)
			strcat(buf,"(16-bit required)\n");
		else if (Machine->drv->video_attributes & VIDEO_MODIFIES_PALETTE)
			strcat(buf,"(dynamic)\n");
		else strcat(buf,"(static)\n");
#endif
	}


	if (sel == -1)
	{
		/* startup info, print MAME version and ask for any key */

		sprintf (buf2, "\n\t%s ", ui_getstring (UI_mame));	/* \t means that the line will be centered */
		strcat(buf, buf2);

		strcat(buf,build_version);
		sprintf (buf2, "\n\t%s", ui_getstring (UI_anykey));
		strcat(buf,buf2);
		ui_drawbox(bitmap,0,0,Machine->uiwidth,Machine->uiheight);
		ui_displaymessagewindow(bitmap,buf);

		sel = 0;
		if (code_read_async() != CODE_NONE)
			sel = -1;
	}
	else
	{
		/* menu system, use the normal menu keys */
		strcat(buf,"\n\t");
		strcat(buf,ui_getstring (UI_lefthilight));
		strcat(buf," ");
		strcat(buf,ui_getstring (UI_returntomain));
		strcat(buf," ");
		strcat(buf,ui_getstring (UI_righthilight));

		ui_displaymessagewindow(bitmap,buf);

		if (input_ui_pressed(IPT_UI_SELECT))
			sel = -1;

		if (input_ui_pressed(IPT_UI_CANCEL))
			sel = -1;

		if (input_ui_pressed(IPT_UI_CONFIGURE))
			sel = -2;
	}

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}


int showgamewarnings(struct osd_bitmap *bitmap)
{
	int i;
	char buf[2048];

	if (Machine->gamedrv->flags &
			(GAME_NOT_WORKING | GAME_UNEMULATED_PROTECTION | GAME_WRONG_COLORS | GAME_IMPERFECT_COLORS |
			  GAME_NO_SOUND | GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL))
	{
		int done;

		strcpy(buf, ui_getstring (UI_knownproblems));
		strcat(buf, "\n\n");

#ifdef MESS
		if (Machine->gamedrv->flags & GAME_COMPUTER)
		{
			strcpy(buf, ui_getstring (UI_comp1));
			strcat(buf, "\n\n");
			strcat(buf, ui_getstring (UI_comp2));
			strcat(buf, "\n");
		}
#endif

		if (Machine->gamedrv->flags & GAME_IMPERFECT_COLORS)
		{
			strcat(buf, ui_getstring (UI_imperfectcolors));
			strcat(buf, "\n");
		}

		if (Machine->gamedrv->flags & GAME_WRONG_COLORS)
		{
			strcat(buf, ui_getstring (UI_wrongcolors));
			strcat(buf, "\n");
		}

		if (Machine->gamedrv->flags & GAME_IMPERFECT_SOUND)
		{
			strcat(buf, ui_getstring (UI_imperfectsound));
			strcat(buf, "\n");
		}

		if (Machine->gamedrv->flags & GAME_NO_SOUND)
		{
			strcat(buf, ui_getstring (UI_nosound));
			strcat(buf, "\n");
		}

		if (Machine->gamedrv->flags & GAME_NO_COCKTAIL)
		{
			strcat(buf, ui_getstring (UI_nococktail));
			strcat(buf, "\n");
		}

		if (Machine->gamedrv->flags & (GAME_NOT_WORKING | GAME_UNEMULATED_PROTECTION))
		{
			const struct GameDriver *maindrv;
			int foundworking;

			if (Machine->gamedrv->flags & GAME_NOT_WORKING)
				strcpy(buf, ui_getstring (UI_brokengame));
			if (Machine->gamedrv->flags & GAME_UNEMULATED_PROTECTION)
				strcat(buf, ui_getstring (UI_brokenprotection));

			if (Machine->gamedrv->clone_of && !(Machine->gamedrv->clone_of->flags & NOT_A_DRIVER))
				maindrv = Machine->gamedrv->clone_of;
			else maindrv = Machine->gamedrv;

			foundworking = 0;
			i = 0;
			while (drivers[i])
			{
				if (drivers[i] == maindrv || drivers[i]->clone_of == maindrv)
				{
					if ((drivers[i]->flags & (GAME_NOT_WORKING | GAME_UNEMULATED_PROTECTION)) == 0)
					{
						if (foundworking == 0)
						{
							strcat(buf,"\n\n");
							strcat(buf, ui_getstring (UI_workingclones));
							strcat(buf,"\n\n");
						}
						foundworking = 1;

						sprintf(&buf[strlen(buf)],"%s\n",drivers[i]->name);
					}
				}
				i++;
			}
		}

		strcat(buf,"\n\n");
		strcat(buf,ui_getstring (UI_typeok));

		ui_displaymessagewindow(bitmap,buf);

		done = 0;
		do
		{
			update_video_and_audio();
			osd_poll_joysticks();
			if (input_ui_pressed(IPT_UI_CANCEL))
				return 1;
			if (code_pressed_memory(KEYCODE_O) ||
					input_ui_pressed(IPT_UI_LEFT))
				done = 1;
			if (done == 1 && (code_pressed_memory(KEYCODE_K) ||
					input_ui_pressed(IPT_UI_RIGHT)))
				done = 2;
		} while (done < 2);
	}


	osd_clearbitmap(bitmap);

	/* clear the input memory */
	while (code_read_async() != CODE_NONE) {};

	while (displaygameinfo(bitmap,0) == 1)
	{
		update_video_and_audio();
		osd_poll_joysticks();
	}

	#ifdef MESS
	while (displayimageinfo(bitmap,0) == 1)
	{
		update_video_and_audio();
		osd_poll_joysticks();
	}
	#endif

	osd_clearbitmap(bitmap);
	/* make sure that the screen is really cleared, in case autoframeskip kicked in */
	update_video_and_audio();
	update_video_and_audio();
	update_video_and_audio();
	update_video_and_audio();

	return 0;
}

/* Word-wraps the text in the specified buffer to fit in maxwidth characters per line.
   The contents of the buffer are modified.
   Known limitations: Words longer than maxwidth cause the function to fail. */
static void wordwrap_text_buffer (char *buffer, int maxwidth)
{
	int width = 0;

	while (*buffer)
	{
		if (*buffer == '\n')
		{
			buffer++;
			width = 0;
			continue;
		}

		width++;

		if (width > maxwidth)
		{
			/* backtrack until a space is found */
			while (*buffer != ' ')
			{
				buffer--;
				width--;
			}
			if (width < 1) return;	/* word too long */

			/* replace space with a newline */
			*buffer = '\n';
		}
		else
			buffer++;
	}
}

static int count_lines_in_buffer (char *buffer)
{
	int lines = 0;
	char c;

	while ( (c = *buffer++) )
		if (c == '\n') lines++;

	return lines;
}

/* Display lines from buffer, starting with line 'scroll', in a width x height text window */
static void display_scroll_message (struct osd_bitmap *bitmap, int *scroll, int width, int height, char *buf)
{
	struct DisplayText dt[256];
	int curr_dt = 0;
	const char *uparrow = ui_getstring (UI_uparrow);
	const char *downarrow = ui_getstring (UI_downarrow);
	char textcopy[2048];
	char *copy;
	int leftoffs,topoffs;
	int first = *scroll;
	int buflines,showlines;
	int i;


	/* draw box */
	leftoffs = (Machine->uiwidth - Machine->uifontwidth * (width + 1)) / 2;
	if (leftoffs < 0) leftoffs = 0;
	topoffs = (Machine->uiheight - (3 * height + 1) * Machine->uifontheight / 2) / 2;
	ui_drawbox(bitmap,leftoffs,topoffs,(width + 1) * Machine->uifontwidth,(3 * height + 1) * Machine->uifontheight / 2);

	buflines = count_lines_in_buffer (buf);
	if (first > 0)
	{
		if (buflines <= height)
			first = 0;
		else
		{
			height--;
			if (first > (buflines - height))
				first = buflines - height;
		}
		*scroll = first;
	}

	if (first != 0)
	{
		/* indicate that scrolling upward is possible */
		dt[curr_dt].text = uparrow;
		dt[curr_dt].color = UI_COLOR_NORMAL;
		dt[curr_dt].x = (Machine->uiwidth - Machine->uifontwidth * strlen(uparrow)) / 2;
		dt[curr_dt].y = topoffs + (3*curr_dt+1)*Machine->uifontheight/2;
		curr_dt++;
	}

	if ((buflines - first) > height)
		showlines = height - 1;
	else
		showlines = height;

	/* skip to first line */
	while (first > 0)
	{
		char c;

		while ( (c = *buf++) )
		{
			if (c == '\n')
			{
				first--;
				break;
			}
		}
	}

	/* copy 'showlines' lines from buffer, starting with line 'first' */
	copy = textcopy;
	for (i = 0; i < showlines; i++)
	{
		char *copystart = copy;

		while (*buf && *buf != '\n')
		{
			*copy = *buf;
			copy++;
			buf++;
		}
		*copy = '\0';
		copy++;
		if (*buf == '\n')
			buf++;

		if (*copystart == '\t') /* center text */
		{
			copystart++;
			dt[curr_dt].x = (Machine->uiwidth - Machine->uifontwidth * (copy - copystart)) / 2;
		}
		else
			dt[curr_dt].x = leftoffs + Machine->uifontwidth/2;

		dt[curr_dt].text = copystart;
		dt[curr_dt].color = UI_COLOR_NORMAL;
		dt[curr_dt].y = topoffs + (3*curr_dt+1)*Machine->uifontheight/2;
		curr_dt++;
	}

	if (showlines == (height - 1))
	{
		/* indicate that scrolling downward is possible */
		dt[curr_dt].text = downarrow;
		dt[curr_dt].color = UI_COLOR_NORMAL;
		dt[curr_dt].x = (Machine->uiwidth - Machine->uifontwidth * strlen(downarrow)) / 2;
		dt[curr_dt].y = topoffs + (3*curr_dt+1)*Machine->uifontheight/2;
		curr_dt++;
	}

	dt[curr_dt].text = 0;	/* terminate array */

	displaytext(bitmap,dt,0,0);
}


/* Display text entry for current driver from history.dat and mameinfo.dat. */
static int displayhistory (struct osd_bitmap *bitmap, int selected)
{
	static int scroll = 0;
	static char *buf = 0;
	int maxcols,maxrows;
	int sel;


	sel = selected - 1;


	maxcols = (Machine->uiwidth / Machine->uifontwidth) - 1;
	maxrows = (2 * Machine->uiheight - Machine->uifontheight) / (3 * Machine->uifontheight);
	maxcols -= 2;
	maxrows -= 8;

	if (!buf)
	{
		/* allocate a buffer for the text */
		buf = (char*) malloc(8192);
		if (buf)
		{
			/* try to load entry */
			if (load_driver_history (Machine->gamedrv, buf, 8192) == 0)
			{
				scroll = 0;
				wordwrap_text_buffer (buf, maxcols);
				strcat(buf,"\n\t");
				strcat(buf,ui_getstring (UI_lefthilight));
				strcat(buf," ");
				strcat(buf,ui_getstring (UI_returntomain));
				strcat(buf," ");
				strcat(buf,ui_getstring (UI_righthilight));
				strcat(buf,"\n");
			}
			else
			{
				free(buf);
				buf = 0;
			}
		}
	}

	{
		if (buf)
			display_scroll_message (bitmap, &scroll, maxcols, maxrows, buf);
		else
		{
			char msg[80];

			strcpy(msg,"\t");
			strcat(msg,ui_getstring(UI_historymissing));
			strcat(msg,"\n\n\t");
			strcat(msg,ui_getstring (UI_lefthilight));
			strcat(msg," ");
			strcat(msg,ui_getstring (UI_returntomain));
			strcat(msg," ");
			strcat(msg,ui_getstring (UI_righthilight));
			ui_displaymessagewindow(bitmap,msg);
		}

		if ((scroll > 0) && input_ui_pressed_repeat(IPT_UI_UP,4))
		{
			if (scroll == 2) scroll = 0;	/* 1 would be the same as 0, but with arrow on top */
			else scroll--;
		}

		if (input_ui_pressed_repeat(IPT_UI_DOWN,4))
		{
			if (scroll == 0) scroll = 2;	/* 1 would be the same as 0, but with arrow on top */
			else scroll++;
		}

		if (input_ui_pressed(IPT_UI_SELECT))
			sel = -1;

		if (input_ui_pressed(IPT_UI_CANCEL))
			sel = -1;

		if (input_ui_pressed(IPT_UI_CONFIGURE))
			sel = -2;
	}

	if (sel == -1 || sel == -2)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;

		/* force buffer to be recreated */
		if (buf)
		{
			free(buf);
			buf = 0;
		}
	}

	return sel + 1;

}


#ifndef MESS
int memcard_menu(struct osd_bitmap *bitmap, int selection)
{
	int sel;
	int menutotal = 0;
	const char *menuitem[10];
	char buf[256];
	char buf2[256];

	sel = selection - 1 ;

	sprintf(buf, "%s %03d", ui_getstring (UI_loadcard), mcd_number);
	menuitem[menutotal++] = buf;
	menuitem[menutotal++] = ui_getstring (UI_ejectcard);
	menuitem[menutotal++] = ui_getstring (UI_createcard);
	menuitem[menutotal++] = ui_getstring (UI_resetcard);
	menuitem[menutotal++] = ui_getstring (UI_returntomain);
	menuitem[menutotal] = 0;

	if (mcd_action!=0)
	{
		strcpy (buf2, "\n");

		switch(mcd_action)
		{
			case 1:
				strcat (buf2, ui_getstring (UI_loadfailed));
				break;
			case 2:
				strcat (buf2, ui_getstring (UI_loadok));
				break;
			case 3:
				strcat (buf2, ui_getstring (UI_cardejected));
				break;
			case 4:
				strcat (buf2, ui_getstring (UI_cardcreated));
				break;
			case 5:
				strcat (buf2, ui_getstring (UI_cardcreatedfailed));
				strcat (buf2, "\n");
				strcat (buf2, ui_getstring (UI_cardcreatedfailed2));
				break;
			default:
				strcat (buf2, ui_getstring (UI_carderror));
				break;
		}

		strcat (buf2, "\n\n");
		ui_displaymessagewindow(bitmap,buf2);
		if (input_ui_pressed(IPT_UI_SELECT))
			mcd_action = 0;
	}
	else
	{
		ui_displaymenu(bitmap,menuitem,0,0,sel,0);

		if (input_ui_pressed_repeat(IPT_UI_RIGHT,8))
			mcd_number = (mcd_number + 1) % 1000;

		if (input_ui_pressed_repeat(IPT_UI_LEFT,8))
			mcd_number = (mcd_number + 999) % 1000;

		if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
			sel = (sel + 1) % menutotal;

		if (input_ui_pressed_repeat(IPT_UI_UP,8))
			sel = (sel + menutotal - 1) % menutotal;

		if (input_ui_pressed(IPT_UI_SELECT))
		{
			switch(sel)
			{
			case 0:
				neogeo_memcard_eject();
				if (neogeo_memcard_load(mcd_number))
				{
					memcard_status=1;
					memcard_number=mcd_number;
					mcd_action = 2;
				}
				else
					mcd_action = 1;
				break;
			case 1:
				neogeo_memcard_eject();
				mcd_action = 3;
				break;
			case 2:
				if (neogeo_memcard_create(mcd_number))
					mcd_action = 4;
				else
					mcd_action = 5;
				break;
			case 3:
				memcard_manager=1;
				sel=-2;
				machine_reset();
				break;
			case 4:
				sel=-1;
				break;
			}
		}

		if (input_ui_pressed(IPT_UI_CANCEL))
			sel = -1;

		if (input_ui_pressed(IPT_UI_CONFIGURE))
			sel = -2;

		if (sel == -1 || sel == -2)
		{
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
		}
	}

	return sel + 1;
}
#endif


#ifndef MESS
enum { UI_SWITCH = 0,UI_DEFCODE,UI_CODE,UI_ANALOG,UI_CALIBRATE,
		UI_STATS,UI_GAMEINFO, UI_HISTORY,
#ifdef ENABLE_AUTOFIRE
		UI_CHEAT,UI_AUTOFIRE,UI_RESET,UI_MEMCARD,UI_EXIT };
#else
		UI_CHEAT,UI_RESET,UI_MEMCARD,UI_EXIT };
#endif
#else
enum { UI_SWITCH = 0,UI_DEFCODE,UI_CODE,UI_ANALOG,UI_CALIBRATE,
		UI_GAMEINFO, UI_IMAGEINFO,UI_FILEMANAGER,UI_TAPECONTROL,
		UI_HISTORY,UI_CHEAT,UI_RESET,UI_MEMCARD,UI_EXIT };
#endif


#define MAX_SETUPMENU_ITEMS 20
static const char *menu_item[MAX_SETUPMENU_ITEMS];
static int menu_action[MAX_SETUPMENU_ITEMS];
static int menu_total;


static void setup_menu_init(void)
{
	menu_total = 0;

	menu_item[menu_total] = ui_getstring (UI_inputgeneral); menu_action[menu_total++] = UI_DEFCODE;
	menu_item[menu_total] = ui_getstring (UI_inputspecific); menu_action[menu_total++] = UI_CODE;
	menu_item[menu_total] = ui_getstring (UI_dipswitches); menu_action[menu_total++] = UI_SWITCH;

	/* Determine if there are any analog controls */
	{
		struct InputPort *in;
		int num;

		in = Machine->input_ports;

		num = 0;
		while (in->type != IPT_END)
		{
			if (((in->type & 0xff) > IPT_ANALOG_START) && ((in->type & 0xff) < IPT_ANALOG_END)
					&& !(!options.cheat && (in->type & IPF_CHEAT)))
				num++;
			in++;
		}

		if (num != 0)
		{
			menu_item[menu_total] = ui_getstring (UI_analogcontrols); menu_action[menu_total++] = UI_ANALOG;
		}
	}

	/* Joystick calibration possible? */
	if ((osd_joystick_needs_calibration()) != 0)
	{
		menu_item[menu_total] = ui_getstring (UI_calibrate); menu_action[menu_total++] = UI_CALIBRATE;
	}

#ifndef MESS
	menu_item[menu_total] = ui_getstring (UI_bookkeeping); menu_action[menu_total++] = UI_STATS;
	menu_item[menu_total] = ui_getstring (UI_gameinfo); menu_action[menu_total++] = UI_GAMEINFO;
	menu_item[menu_total] = ui_getstring (UI_history); menu_action[menu_total++] = UI_HISTORY;
#else
	menu_item[menu_total] = ui_getstring (UI_imageinfo); menu_action[menu_total++] = UI_IMAGEINFO;
	menu_item[menu_total] = ui_getstring (UI_filemanager); menu_action[menu_total++] = UI_FILEMANAGER;
	menu_item[menu_total] = ui_getstring (UI_tapecontrol); menu_action[menu_total++] = UI_TAPECONTROL;
	menu_item[menu_total] = ui_getstring (UI_history); menu_action[menu_total++] = UI_HISTORY;
#endif

	if (options.cheat)
	{
		menu_item[menu_total] = ui_getstring (UI_cheat); menu_action[menu_total++] = UI_CHEAT;
	}

#ifndef MESS
	if (Machine->gamedrv->clone_of == &driver_neogeo ||
			(Machine->gamedrv->clone_of &&
				Machine->gamedrv->clone_of->clone_of == &driver_neogeo))
	{
		menu_item[menu_total] = ui_getstring (UI_memorycard); menu_action[menu_total++] = UI_MEMCARD;
	}
#endif

#ifdef ENABLE_AUTOFIRE
	menu_item[menu_total] = "Auto-Fire"; menu_action[menu_total++] = UI_AUTOFIRE;
#endif
	menu_item[menu_total] = ui_getstring (UI_resetgame); menu_action[menu_total++] = UI_RESET;
	menu_item[menu_total] = ui_getstring (UI_returntogame); menu_action[menu_total++] = UI_EXIT;
	menu_item[menu_total] = 0; /* terminate array */
}


static int setup_menu(struct osd_bitmap *bitmap, int selected)
{
	int sel,res=-1;
	static int menu_lastselected = 0;


	if (selected == -1)
		sel = menu_lastselected;
	else sel = selected - 1;

	if (sel > SEL_MASK)
	{
		switch (menu_action[sel & SEL_MASK])
		{
#ifdef ENABLE_AUTOFIRE
			case UI_AUTOFIRE:
				res = autofire_menu(bitmap, sel >> SEL_BITS);
				break;
#endif
			case UI_SWITCH:
				res = setdipswitches(bitmap, sel >> SEL_BITS);
				break;
			case UI_DEFCODE:
				res = setdefcodesettings(bitmap, sel >> SEL_BITS);
				break;
			case UI_CODE:
				res = setcodesettings(bitmap, sel >> SEL_BITS);
				break;
			case UI_ANALOG:
				res = settraksettings(bitmap, sel >> SEL_BITS);
				break;
			case UI_CALIBRATE:
				res = calibratejoysticks(bitmap, sel >> SEL_BITS);
				break;
#ifndef MESS
			case UI_STATS:
				res = mame_stats(bitmap, sel >> SEL_BITS);
				break;
			case UI_GAMEINFO:
				res = displaygameinfo(bitmap, sel >> SEL_BITS);
				break;
#endif
#ifdef MESS
			case UI_IMAGEINFO:
				res = displayimageinfo(bitmap, sel >> SEL_BITS);
				break;
			case UI_FILEMANAGER:
				res = filemanager(bitmap, sel >> SEL_BITS);
				break;
			case UI_TAPECONTROL:
				res = tapecontrol(bitmap, sel >> SEL_BITS);
				break;
#endif
			case UI_HISTORY:
				res = displayhistory(bitmap, sel >> SEL_BITS);
				break;
			case UI_CHEAT:
				res = cheat_menu(bitmap, sel >> SEL_BITS);
				break;
#ifndef MESS
			case UI_MEMCARD:
				res = memcard_menu(bitmap, sel >> SEL_BITS);
				break;
#endif
		}

		if (res == -1)
		{
			menu_lastselected = sel;
			sel = -1;
		}
		else
			sel = (sel & SEL_MASK) | (res << SEL_BITS);

		return sel + 1;
	}


	ui_displaymenu(bitmap,menu_item,0,0,sel,0);

	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		sel = (sel + 1) % menu_total;

	if (input_ui_pressed_repeat(IPT_UI_UP,8))
		sel = (sel + menu_total - 1) % menu_total;

	if (input_ui_pressed(IPT_UI_SELECT))
	{
		switch (menu_action[sel])
		{
#ifdef ENABLE_AUTOFIRE
			case UI_AUTOFIRE:
#endif
			case UI_SWITCH:
			case UI_DEFCODE:
			case UI_CODE:
			case UI_ANALOG:
			case UI_CALIBRATE:
			#ifndef MESS
			case UI_STATS:
			case UI_GAMEINFO:
			#else
			case UI_GAMEINFO:
			case UI_IMAGEINFO:
			case UI_FILEMANAGER:
			case UI_TAPECONTROL:
			#endif
			case UI_HISTORY:
			case UI_CHEAT:
			case UI_MEMCARD:
				sel |= 1 << SEL_BITS;
				/* tell updatescreen() to clean after us */
				need_to_clear_bitmap = 1;
				break;

			case UI_RESET:
				machine_reset();
				break;

			case UI_EXIT:
				menu_lastselected = 0;
				sel = -1;
				break;
		}
	}

	if (input_ui_pressed(IPT_UI_CANCEL) ||
			input_ui_pressed(IPT_UI_CONFIGURE))
	{
		menu_lastselected = sel;
		sel = -1;
	}

	if (sel == -1)
	{
		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}



/*********************************************************************

  start of On Screen Display handling

*********************************************************************/

static void displayosd(struct osd_bitmap *bitmap,const char *text,int percentage,int default_percentage)
{
	struct DisplayText dt[2];
	int avail;


	avail = (Machine->uiwidth / Machine->uifontwidth) * 19 / 20;

	ui_drawbox(bitmap,(Machine->uiwidth - Machine->uifontwidth * avail) / 2,
			(Machine->uiheight - 7*Machine->uifontheight/2),
			avail * Machine->uifontwidth,
			3*Machine->uifontheight);

	avail--;

	drawbar(bitmap,(Machine->uiwidth - Machine->uifontwidth * avail) / 2,
			(Machine->uiheight - 3*Machine->uifontheight),
			avail * Machine->uifontwidth,
			Machine->uifontheight,
			percentage,default_percentage);

	dt[0].text = text;
	dt[0].color = UI_COLOR_NORMAL;
	dt[0].x = (Machine->uiwidth - Machine->uifontwidth * strlen(text)) / 2;
	dt[0].y = (Machine->uiheight - 2*Machine->uifontheight) + 2;
	dt[1].text = 0; /* terminate array */
	displaytext(bitmap,dt,0,0);
}



static void onscrd_volume(struct osd_bitmap *bitmap,int increment,int arg)
{
	char buf[20];
	int attenuation;

	if (increment)
	{
		attenuation = osd_get_mastervolume();
		attenuation += increment;
		if (attenuation > 0) attenuation = 0;
		if (attenuation < -32) attenuation = -32;
		osd_set_mastervolume(attenuation);
	}
	attenuation = osd_get_mastervolume();

	sprintf(buf,"%s %3ddB", ui_getstring (UI_volume), attenuation);
	displayosd(bitmap,buf,100 * (attenuation + 32) / 32,100);
}

static void onscrd_mixervol(struct osd_bitmap *bitmap,int increment,int arg)
{
	static void *driver = 0;
	char buf[40];
	int volume,ch;
	int doallchannels = 0;
	int proportional = 0;


	if (code_pressed(KEYCODE_LSHIFT) || code_pressed(KEYCODE_RSHIFT))
		doallchannels = 1;
	if (!code_pressed(KEYCODE_LCONTROL) && !code_pressed(KEYCODE_RCONTROL))
		increment *= 5;
	if (code_pressed(KEYCODE_LALT) || code_pressed(KEYCODE_RALT))
		proportional = 1;

	if (increment)
	{
		if (proportional)
		{
			static int old_vol[MIXER_MAX_CHANNELS];
			float ratio = 1.0;
			int overflow = 0;

			if (driver != Machine->drv)
			{
				driver = (void *)Machine->drv;
				for (ch = 0; ch < MIXER_MAX_CHANNELS; ch++)
					old_vol[ch] = mixer_get_mixing_level(ch);
			}

			volume = mixer_get_mixing_level(arg);
			if (old_vol[arg])
				ratio = (float)(volume + increment) / (float)old_vol[arg];

			for (ch = 0; ch < MIXER_MAX_CHANNELS; ch++)
			{
				if (mixer_get_name(ch) != 0)
				{
					volume = ratio * old_vol[ch];
					if (volume < 0 || volume > 100)
						overflow = 1;
				}
			}

			if (!overflow)
			{
				for (ch = 0; ch < MIXER_MAX_CHANNELS; ch++)
				{
					volume = ratio * old_vol[ch];
					mixer_set_mixing_level(ch,volume);
				}
			}
		}
		else
		{
			driver = 0; /* force reset of saved volumes */

			volume = mixer_get_mixing_level(arg);
			volume += increment;
			if (volume > 100) volume = 100;
			if (volume < 0) volume = 0;

			if (doallchannels)
			{
				for (ch = 0;ch < MIXER_MAX_CHANNELS;ch++)
					mixer_set_mixing_level(ch,volume);
			}
			else
				mixer_set_mixing_level(arg,volume);
		}
	}
	volume = mixer_get_mixing_level(arg);

	if (proportional)
		sprintf(buf,"%s %s %3d%%", ui_getstring (UI_allchannels), ui_getstring (UI_relative), volume);
	else if (doallchannels)
		sprintf(buf,"%s %s %3d%%", ui_getstring (UI_allchannels), ui_getstring (UI_volume), volume);
	else
		sprintf(buf,"%s %s %3d%%",mixer_get_name(arg), ui_getstring (UI_volume), volume);
	displayosd(bitmap,buf,volume,mixer_get_default_mixing_level(arg));
}

static void onscrd_brightness(struct osd_bitmap *bitmap,int increment,int arg)
{
	char buf[20];
	int brightness;


	if (increment)
	{
		brightness = osd_get_brightness();
		brightness += 5 * increment;
		if (brightness < 0) brightness = 0;
		if (brightness > 100) brightness = 100;
		osd_set_brightness(brightness);
	}
	brightness = osd_get_brightness();

	sprintf(buf,"%s %3d%%", ui_getstring (UI_brightness), brightness);
	displayosd(bitmap,buf,brightness,100);
}

static void onscrd_gamma(struct osd_bitmap *bitmap,int increment,int arg)
{
	char buf[20];
	float gamma_correction;

	if (increment)
	{
		gamma_correction = osd_get_gamma();

		gamma_correction += 0.05 * increment;
		if (gamma_correction < 0.5) gamma_correction = 0.5;
		if (gamma_correction > 2.0) gamma_correction = 2.0;

		osd_set_gamma(gamma_correction);
	}
	gamma_correction = osd_get_gamma();

	sprintf(buf,"%s %1.2f", ui_getstring (UI_gamma), gamma_correction);
	displayosd(bitmap,buf,100*(gamma_correction-0.5)/(2.0-0.5),100*(1.0-0.5)/(2.0-0.5));
}

static void onscrd_vector_intensity(struct osd_bitmap *bitmap,int increment,int arg)
{
	char buf[30];
	float intensity_correction;

	if (increment)
	{
		intensity_correction = vector_get_intensity();

		intensity_correction += 0.05 * increment;
		if (intensity_correction < 0.5) intensity_correction = 0.5;
		if (intensity_correction > 3.0) intensity_correction = 3.0;

		vector_set_intensity(intensity_correction);
	}
	intensity_correction = vector_get_intensity();

	sprintf(buf,"%s %1.2f", ui_getstring (UI_vectorintensity), intensity_correction);
	displayosd(bitmap,buf,100*(intensity_correction-0.5)/(3.0-0.5),100*(1.5-0.5)/(3.0-0.5));
}


static void onscrd_overclock(struct osd_bitmap *bitmap,int increment,int arg)
{
	char buf[30];
	float overclock;
	int cpu, doallcpus = 0, oc;

	if (code_pressed(KEYCODE_LSHIFT) || code_pressed(KEYCODE_RSHIFT))
		doallcpus = 1;
	if (!code_pressed(KEYCODE_LCONTROL) && !code_pressed(KEYCODE_RCONTROL))
		increment *= 5;
	if( increment )
	{
		overclock = timer_get_overclock(arg);
		overclock += 0.01 * increment;
		if (overclock < 0.01) overclock = 0.01;
		if (overclock > 2.0) overclock = 2.0;
		if( doallcpus )
			for( cpu = 0; cpu < cpu_gettotalcpu(); cpu++ )
				timer_set_overclock(cpu, overclock);
		else
			timer_set_overclock(arg, overclock);
	}

	oc = 100 * timer_get_overclock(arg) + 0.5;

	if( doallcpus )
		sprintf(buf,"%s %s %3d%%", ui_getstring (UI_allcpus), ui_getstring (UI_overclock), oc);
	else
		sprintf(buf,"%s %s%d %3d%%", ui_getstring (UI_overclock), ui_getstring (UI_cpu), arg, oc);
	displayosd(bitmap,buf,oc/2,100/2);
}

#define MAX_OSD_ITEMS 30
static void (*onscrd_fnc[MAX_OSD_ITEMS])(struct osd_bitmap *bitmap,int increment,int arg);
static int onscrd_arg[MAX_OSD_ITEMS];
static int onscrd_total_items;

static void onscrd_init(void)
{
	int item,ch;


	item = 0;

	onscrd_fnc[item] = onscrd_volume;
	onscrd_arg[item] = 0;
	item++;

	for (ch = 0;ch < MIXER_MAX_CHANNELS;ch++)
	{
		if (mixer_get_name(ch) != 0)
		{
			onscrd_fnc[item] = onscrd_mixervol;
			onscrd_arg[item] = ch;
			item++;
		}
	}

	if (options.cheat)
	{
		for (ch = 0;ch < cpu_gettotalcpu();ch++)
		{
			onscrd_fnc[item] = onscrd_overclock;
			onscrd_arg[item] = ch;
			item++;
		}
	}

	onscrd_fnc[item] = onscrd_brightness;
	onscrd_arg[item] = 0;
	item++;

	onscrd_fnc[item] = onscrd_gamma;
	onscrd_arg[item] = 0;
	item++;

	if (Machine->drv->video_attributes & VIDEO_TYPE_VECTOR)
	{
		onscrd_fnc[item] = onscrd_vector_intensity;
		onscrd_arg[item] = 0;
		item++;
	}

	onscrd_total_items = item;
}

static int on_screen_display(struct osd_bitmap *bitmap, int selected)
{
	int increment,sel;
	static int lastselected = 0;


	if (selected == -1)
		sel = lastselected;
	else sel = selected - 1;

	increment = 0;
	if (input_ui_pressed_repeat(IPT_UI_LEFT,8))
		increment = -1;
	if (input_ui_pressed_repeat(IPT_UI_RIGHT,8))
		increment = 1;
	if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		sel = (sel + 1) % onscrd_total_items;
	if (input_ui_pressed_repeat(IPT_UI_UP,8))
		sel = (sel + onscrd_total_items - 1) % onscrd_total_items;

	(*onscrd_fnc[sel])(bitmap,increment,onscrd_arg[sel]);

	lastselected = sel;

	if (input_ui_pressed(IPT_UI_ON_SCREEN_DISPLAY))
	{
		sel = -1;

		/* tell updatescreen() to clean after us */
		need_to_clear_bitmap = 1;
	}

	return sel + 1;
}

/*********************************************************************

  end of On Screen Display handling

*********************************************************************/


static void displaymessage(struct osd_bitmap *bitmap,const char *text)
{
	struct DisplayText dt[2];
	int avail;


	if (Machine->uiwidth < Machine->uifontwidth * strlen(text))
	{
		ui_displaymessagewindow(bitmap,text);
		return;
	}

	avail = strlen(text)+2;

	ui_drawbox(bitmap,(Machine->uiwidth - Machine->uifontwidth * avail) / 2,
			Machine->uiheight - 3*Machine->uifontheight,
			avail * Machine->uifontwidth,
			2*Machine->uifontheight);

	dt[0].text = text;
	dt[0].color = UI_COLOR_NORMAL;
	dt[0].x = (Machine->uiwidth - Machine->uifontwidth * strlen(text)) / 2;
	dt[0].y = Machine->uiheight - 5*Machine->uifontheight/2;
	dt[1].text = 0; /* terminate array */
	displaytext(bitmap,dt,0,0);
}


static char messagetext[80];
static int messagecounter;

void CLIB_DECL usrintf_showmessage(const char *text,...)
{
	va_list arg;
	va_start(arg,text);
	vsprintf(messagetext,text,arg);
	va_end(arg);
	messagecounter = 2 * Machine->drv->frames_per_second;
}

void CLIB_DECL usrintf_showmessage_secs(int seconds, const char *text,...)
{
	va_list arg;
	va_start(arg,text);
	vsprintf(messagetext,text,arg);
	va_end(arg);
	messagecounter = seconds * Machine->drv->frames_per_second;
}



int handle_user_interface(struct osd_bitmap *bitmap)
{
	static int show_profiler;

#ifdef MESS
if (Machine->gamedrv->flags & GAME_COMPUTER)
{
	static int ui_active = 0, ui_toggle_key = 0;
	static int ui_display_count = 4 * 60;

	if( input_ui_pressed(IPT_UI_TOGGLE_UI) )
	{
		if( !ui_toggle_key )
		{
			ui_toggle_key = 1;
			ui_active = !ui_active;
			ui_display_count = 4 * 60;
			bitmap_dirty = 1;
		 }
	}
	else
	{
		ui_toggle_key = 0;
	}

	if( ui_active )
	{
		if( ui_display_count > 0 )
		{
			char text[] = "KBD: UI  (ScrLock)";
			int x, x0 = Machine->uiwidth - sizeof(text) * Machine->uifont->width - 2;
			int y0 = Machine->uiymin + Machine->uiheight - Machine->uifont->height - 2;
			for( x = 0; text[x]; x++ )
			{
				drawgfx(bitmap,
					Machine->uifont,text[x],0,0,0,
					x0+x*Machine->uifont->width,
					y0,0,TRANSPARENCY_NONE,0);
			}
			if( --ui_display_count == 0 )
				bitmap_dirty = 1;
		}
	}
	else
	{
		if( ui_display_count > 0 )
		{
			char text[] = "KBD: EMU (ScrLock)";
			int x, x0 = Machine->uiwidth - sizeof(text) * Machine->uifont->width - 2;
			int y0 = Machine->uiymin + Machine->uiheight - Machine->uifont->height - 2;
			for( x = 0; text[x]; x++ )
			{
				drawgfx(bitmap,
					Machine->uifont,text[x],0,0,0,
					x0+x*Machine->uifont->width,
					y0,0,TRANSPARENCY_NONE,0);
			}
			if( --ui_display_count == 0 )
				bitmap_dirty = 1;
		}
		return 0;
	}
}
#endif

	/* if the user pressed F12, save the screen to a file */
	if (input_ui_pressed(IPT_UI_SNAPSHOT))
		osd_save_snapshot(bitmap);

	/* This call is for the cheat, it must be called once a frame */
	if (options.cheat) DoCheat(bitmap);

	/* if the user pressed ESC, stop the emulation */
	/* but don't quit if the setup menu is on screen */
	if (setup_selected == 0 && input_ui_pressed(IPT_UI_CANCEL))
		return 1;

	if (setup_selected == 0 && input_ui_pressed(IPT_UI_CONFIGURE))
	{
		setup_selected = -1;
		if (osd_selected != 0)
		{
			osd_selected = 0;	/* disable on screen display */
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
		}
	}
	if (setup_selected != 0) setup_selected = setup_menu(bitmap, setup_selected);

	if (!mame_debug && osd_selected == 0 && input_ui_pressed(IPT_UI_ON_SCREEN_DISPLAY))
	{
		osd_selected = -1;
		if (setup_selected != 0)
		{
			setup_selected = 0; /* disable setup menu */
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
		}
	}
	if (osd_selected != 0) osd_selected = on_screen_display(bitmap, osd_selected);


#if 0
	if (keyboard_pressed_memory(KEYCODE_BACKSPACE))
	{
		if (jukebox_selected != -1)
		{
			jukebox_selected = -1;
			cpu_halt(0,1);
		}
		else
		{
			jukebox_selected = 0;
			cpu_halt(0,0);
		}
	}

	if (jukebox_selected != -1)
	{
		char buf[40];
		watchdog_reset_w(0,0);
		if (keyboard_pressed_memory(KEYCODE_LCONTROL))
		{
#include "cpu/z80/z80.h"
			soundlatch_w(0,jukebox_selected);
			cpu_cause_interrupt(1,Z80_NMI_INT);
		}
		if (input_ui_pressed_repeat(IPT_UI_RIGHT,8))
		{
			jukebox_selected = (jukebox_selected + 1) & 0xff;
		}
		if (input_ui_pressed_repeat(IPT_UI_LEFT,8))
		{
			jukebox_selected = (jukebox_selected - 1) & 0xff;
		}
		if (input_ui_pressed_repeat(IPT_UI_UP,8))
		{
			jukebox_selected = (jukebox_selected + 16) & 0xff;
		}
		if (input_ui_pressed_repeat(IPT_UI_DOWN,8))
		{
			jukebox_selected = (jukebox_selected - 16) & 0xff;
		}
		sprintf(buf,"sound cmd %02x",jukebox_selected);
		displaymessage(buf);
	}
#endif


	/* if the user pressed F3, reset the emulation */
	if (input_ui_pressed(IPT_UI_RESET_MACHINE))
		machine_reset();


	if (single_step || input_ui_pressed(IPT_UI_PAUSE)) /* pause the game */
	{
/*		osd_selected = 0;	   disable on screen display, since we are going   */
							/* to change parameters affected by it */

		if (single_step == 0)
		{
			osd_sound_enable(0);
			osd_pause(1);
		}

		while (!input_ui_pressed(IPT_UI_PAUSE))
		{
#ifdef MAME_NET
			osd_net_sync();
#endif /* MAME_NET */
			profiler_mark(PROFILER_VIDEO);
			if (osd_skip_this_frame() == 0)
			{
				if (need_to_clear_bitmap || bitmap_dirty)
				{
					osd_clearbitmap(bitmap);
					need_to_clear_bitmap = 0;
					draw_screen(bitmap_dirty);
					bitmap_dirty = 0;
				}
			}
			profiler_mark(PROFILER_END);

			if (input_ui_pressed(IPT_UI_SNAPSHOT))
				osd_save_snapshot(bitmap);

			if (setup_selected == 0 && input_ui_pressed(IPT_UI_CANCEL))
				return 1;

			if (setup_selected == 0 && input_ui_pressed(IPT_UI_CONFIGURE))
			{
				setup_selected = -1;
				if (osd_selected != 0)
				{
					osd_selected = 0;	/* disable on screen display */
					/* tell updatescreen() to clean after us */
					need_to_clear_bitmap = 1;
				}
			}
			if (setup_selected != 0) setup_selected = setup_menu(bitmap, setup_selected);

			if (!mame_debug && osd_selected == 0 && input_ui_pressed(IPT_UI_ON_SCREEN_DISPLAY))
			{
				osd_selected = -1;
				if (setup_selected != 0)
				{
					setup_selected = 0; /* disable setup menu */
					/* tell updatescreen() to clean after us */
					need_to_clear_bitmap = 1;
				}
			}
			if (osd_selected != 0) osd_selected = on_screen_display(bitmap, osd_selected);

			/* show popup message if any */
			if (messagecounter > 0) displaymessage(bitmap, messagetext);

			update_video_and_audio();
			osd_poll_joysticks();
		}

		if (code_pressed(KEYCODE_LSHIFT) || code_pressed(KEYCODE_RSHIFT))
			single_step = 1;
		else
		{
			single_step = 0;
			osd_pause(0);
			osd_sound_enable(1);
		}
	}


	/* show popup message if any */
	if (messagecounter > 0)
	{
		displaymessage(bitmap, messagetext);

		if (--messagecounter == 0)
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
	}


	if (input_ui_pressed(IPT_UI_SHOW_PROFILER))
	{
		show_profiler ^= 1;
		if (show_profiler)
			profiler_start();
		else
		{
			profiler_stop();
			/* tell updatescreen() to clean after us */
			need_to_clear_bitmap = 1;
		}
	}

	if (show_profiler) profiler_show(bitmap);


	/* if the user pressed F4, show the character set */
	if (input_ui_pressed(IPT_UI_SHOW_GFX))
	{
		osd_sound_enable(0);

		showcharset(bitmap);

		osd_sound_enable(1);
	}

	return 0;
}


void init_user_interface(void)
{
	extern int snapno;	/* in common.c */

	snapno = 0; /* reset snapshot counter */

	setup_menu_init();
	setup_selected = 0;

	onscrd_init();
	osd_selected = 0;

	jukebox_selected = -1;

	single_step = 0;

	orientation_count = 0;
}

int onscrd_active(void)
{
	return osd_selected;
}

int setup_active(void)
{
	return setup_selected;
}

