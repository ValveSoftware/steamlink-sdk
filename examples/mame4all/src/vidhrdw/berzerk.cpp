/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *berzerk_magicram;
extern int berzerk_irq_end_of_screen;

static int magicram_control = 0xff;
static int magicram_latch = 0xff;
static int collision = 0;


INLINE void copy_byte(int x, int y, int data, int col)
{
	UINT16 fore, back;


	if (y < 32)  return;

	fore  = Machine->pens[(col >> 4) & 0x0f];
	back  = Machine->pens[0];

	plot_pixel(Machine->scrbitmap, x  , y, (data & 0x80) ? fore : back);
	plot_pixel(Machine->scrbitmap, x+1, y, (data & 0x40) ? fore : back);
	plot_pixel(Machine->scrbitmap, x+2, y, (data & 0x20) ? fore : back);
	plot_pixel(Machine->scrbitmap, x+3, y, (data & 0x10) ? fore : back);

	fore  = Machine->pens[col & 0x0f];

	plot_pixel(Machine->scrbitmap, x+4, y, (data & 0x08) ? fore : back);
	plot_pixel(Machine->scrbitmap, x+5, y, (data & 0x04) ? fore : back);
	plot_pixel(Machine->scrbitmap, x+6, y, (data & 0x02) ? fore : back);
	plot_pixel(Machine->scrbitmap, x+7, y, (data & 0x01) ? fore : back);
}


WRITE_HANDLER( berzerk_videoram_w )
{
	int coloroffset, x, y;

	videoram[offset] = data;

	/* Get location of color RAM for this offset */
	coloroffset = ((offset & 0xff80) >> 2) | (offset & 0x1f);

	y = (offset >> 5);
	x = (offset & 0x1f) << 3;

    copy_byte(x, y, data, colorram[coloroffset]);
}


WRITE_HANDLER( berzerk_colorram_w )
{
	int x, y, i;

	colorram[offset] = data;

	/* Need to change the affected pixels' colors */

	y = ((offset >> 3) & 0xfc);
	x = (offset & 0x1f) << 3;

	for (i = 0; i < 4; i++, y++)
	{
		int byte = videoram[(y << 5) | (x >> 3)];

		if (byte)
		{
			copy_byte(x, y, byte, data);
		}
	}
}

INLINE unsigned char reverse_byte(unsigned char data)
{
	return ((data & 0x01) << 7) |
		   ((data & 0x02) << 5) |
		   ((data & 0x04) << 3) |
		   ((data & 0x08) << 1) |
		   ((data & 0x10) >> 1) |
		   ((data & 0x20) >> 3) |
		   ((data & 0x40) >> 5) |
		   ((data & 0x80) >> 7);
}

INLINE int shifter_flopper(unsigned char data)
{
	int shift_amount, outval;

	/* Bits 0-2 are the shift amount */

	shift_amount = magicram_control & 0x06;

	outval = ((data >> shift_amount) | (magicram_latch << (8 - shift_amount))) & 0x1ff;
	outval >>= (magicram_control & 0x01);


	/* Bit 3 is the flip bit */
	if (magicram_control & 0x08)
	{
		outval = reverse_byte(outval);
	}

	return outval;
}


WRITE_HANDLER( berzerk_magicram_w )
{
	int data2;

	data2 = shifter_flopper(data);

	magicram_latch = data;

	/* Check for collision */
	if (!collision)
	{
		collision = (data2 & videoram[offset]);
	}

	switch (magicram_control & 0xf0)
	{
	case 0x00: berzerk_magicram[offset] = data2; 						break;
	case 0x10: berzerk_magicram[offset] = data2 |  videoram[offset]; 	break;
	case 0x20: berzerk_magicram[offset] = data2 | ~videoram[offset]; 	break;
	case 0x30: berzerk_magicram[offset] = 0xff;  						break;
	case 0x40: berzerk_magicram[offset] = data2 & videoram[offset]; 	break;
	case 0x50: berzerk_magicram[offset] = videoram[offset]; 			break;
	case 0x60: berzerk_magicram[offset] = ~(data2 ^ videoram[offset]); 	break;
	case 0x70: berzerk_magicram[offset] = ~data2 | videoram[offset]; 	break;
	case 0x80: berzerk_magicram[offset] = data2 & ~videoram[offset];	break;
	case 0x90: berzerk_magicram[offset] = data2 ^ videoram[offset];		break;
	case 0xa0: berzerk_magicram[offset] = ~videoram[offset];			break;
	case 0xb0: berzerk_magicram[offset] = ~(data2 & videoram[offset]); 	break;
	case 0xc0: berzerk_magicram[offset] = 0x00; 						break;
	case 0xd0: berzerk_magicram[offset] = ~data2 & videoram[offset]; 	break;
	case 0xe0: berzerk_magicram[offset] = ~(data2 | videoram[offset]); 	break;
	case 0xf0: berzerk_magicram[offset] = ~data2; 						break;
	}

	berzerk_videoram_w(offset, berzerk_magicram[offset]);
}


WRITE_HANDLER( berzerk_magicram_control_w )
{
	magicram_control = data;
	magicram_latch = 0;
	collision = 0;
}


READ_HANDLER( berzerk_collision_r )
{
	int ret = (collision ? 0x80 : 0x00);

	return ret | berzerk_irq_end_of_screen;
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  To be used by bitmapped games not using sprites.

***************************************************************************/
void berzerk_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	if (full_refresh)
	{
		/* redraw bitmap */

		int offs;

		for (offs = 0; offs < videoram_size; offs++)
		{
			berzerk_videoram_w(offs, videoram[offs]);
		}
	}
}
