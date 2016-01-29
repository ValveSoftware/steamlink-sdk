/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/tms34061.h"
#include "cpu/m6809/m6809.h"

unsigned char *capbowl_rowaddress;

static unsigned char *raw_video_ram;
static unsigned int  color_count[4096];
static unsigned char dirty_row[256];

static int max_col, max_row, max_col_offset;

static int  capbowl_tms34061_getfunction(int offset);
static int  capbowl_tms34061_getrowaddress(int offset);
static int  capbowl_tms34061_getcoladdress(int offset);
static int  capbowl_tms34061_getpixel(int col, int row);
static void capbowl_tms34061_setpixel(int col, int row, int pixel);

#define PAL_SIZE  0x20

/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
static int capbowl_vertical_interrupt(void)
{
	return M6809_INT_FIRQ;
}

static struct TMS34061interface tms34061_interface =
{
	capbowl_tms34061_getfunction,
	capbowl_tms34061_getrowaddress,
	capbowl_tms34061_getcoladdress,
	capbowl_tms34061_getpixel,
	capbowl_tms34061_setpixel,
	0,
    capbowl_vertical_interrupt,  /* Vertical interrupt causes a FIRQ */
};

int capbowl_vh_start(void)
{
	int i;

	if ((raw_video_ram = (unsigned char*)malloc(256 * 256)) == 0)
	{
		return 1;
	}

	// Initialize TMS34061 emulation
    if (TMS34061_start(&tms34061_interface))
	{
		free(raw_video_ram);
		return 1;
	}

	max_row = Machine->visible_area.max_y;
	max_col = Machine->visible_area.max_x;
	max_col_offset = (max_col + 1) / 2 + PAL_SIZE;

	// Initialize color areas. The screen is blank
	memset(raw_video_ram, 0, 256*256);
	palette_init_used_colors();
	memset(color_count, 0, sizeof(color_count));
	memset(dirty_row, 1, sizeof(dirty_row));

	for (i = 0; i < max_row * 16; i+=16)
	{
		palette_used_colors[i] = PALETTE_COLOR_USED;
		color_count[i] = max_col + 1;  // All the pixels are pen 0
	}

	return 0;
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void capbowl_vh_stop(void)
{
	free(raw_video_ram);

	TMS34061_stop();
}


/***************************************************************************

  TMS34061 callbacks

***************************************************************************/

static int capbowl_tms34061_getfunction(int offset)
{
	/* The function inputs (FS0-FS2) are hooked up the following way:

	   FS0 = A8
	   FS1 = A9
	   FS2 = grounded
	 */

	return (offset >> 8) & 0x03;
}


static int capbowl_tms34061_getrowaddress(int offset)
{
	/* Row address (RA0-RA8) is not dependent on the offset */
	return *capbowl_rowaddress;
}


static int capbowl_tms34061_getcoladdress(int offset)
{
	/* Column address (CA0-CA8) is hooked up the A0-A7, with A1 being inverted
	   during register access. CA8 is ignored */
	int col = (offset & 0xff);

	if (!(offset & 0x300))
	{
		col ^= 0x02;
	}

	return col;
}


static void capbowl_tms34061_setpixel(int col, int row, int pixel)
{
	int off = ((row << 8) | col);
	int penstart = row << 4;

	int oldpixel = raw_video_ram[off];

	raw_video_ram[off] = pixel;

	if (row > max_row || col >= max_col_offset) return;

	if (col >= PAL_SIZE)
	{
		int oldpen1 = penstart | (oldpixel >> 4);
		int oldpen2 = penstart | (oldpixel & 0x0f);
		int newpen1 = penstart | (pixel >> 4);
		int newpen2 = penstart | (pixel & 0x0f);

		if (oldpen1 != newpen1)
		{
			dirty_row[row] = 1;

			color_count[oldpen1]--;
			if (!color_count[oldpen1]) palette_used_colors[oldpen1] = PALETTE_COLOR_UNUSED;

			color_count[newpen1]++;
			palette_used_colors[newpen1] = PALETTE_COLOR_USED;
		}

		if (oldpen2 != newpen2)
		{
			dirty_row[row] = 1;

			color_count[oldpen2]--;
			if (!color_count[oldpen2]) palette_used_colors[oldpen2] = PALETTE_COLOR_UNUSED;

			color_count[newpen2]++;
			palette_used_colors[newpen2] = PALETTE_COLOR_USED;
		}
	}
	else
	{
		/* Offsets 0-1f are the palette */

		int r = (raw_video_ram[off & ~1] & 0x0f);
		int g = (raw_video_ram[off |  1] >> 4);
		int b = (raw_video_ram[off |  1] & 0x0f);
		r = (r << 4) + r;
		g = (g << 4) + g;
		b = (b << 4) + b;

		palette_change_color(penstart | (col >> 1),r,g,b);
	}
}


static int capbowl_tms34061_getpixel(int col, int row)
{
	return raw_video_ram[row << 8 | col];
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void capbowl_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int col, row;
	const unsigned char *remapped;


	if (full_refresh)
	{
		for (row = 0; row <= max_row; row++)  dirty_row[row] = 1;
	}

	if (TMS34061_display_blanked())
	{
		fillbitmap(bitmap,palette_transparent_pen,&Machine->visible_area);
		return;
	}

	if ((remapped = palette_recalc()) != 0)
	{
		for (row = 0; row <= max_row; row++)
		{
			if (dirty_row[row] == 0)
			{
				int i;

				for (i = 0;i < 16;i++)
				{
					if (remapped[16 * row + i] != 0)
					{
						dirty_row[row] = 1;
						break;
					}
				}
			}
		}
	}

	for (row = 0; row <= max_row; row++)
	{
		if (dirty_row[row])
		{
			int col1 = 0;
			int row1 = (row << 8 | PAL_SIZE);
			int row2 =  row << 4;

			dirty_row[row] = 0;

			for (col = PAL_SIZE; col < max_col_offset; col++)
			{
				int pixel = raw_video_ram[row1++];

				plot_pixel(bitmap, col1++,row,Machine->pens[row2 | (pixel >> 4)  ]);
				plot_pixel(bitmap, col1++,row,Machine->pens[row2 | (pixel & 0x0f)]);
			}
		}
	}
}
