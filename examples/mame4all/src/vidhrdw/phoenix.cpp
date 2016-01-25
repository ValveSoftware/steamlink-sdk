/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* from sndhrdw/pleiads.c */
WRITE_HANDLER( pleiads_sound_control_c_w );

static unsigned char *ram_page1;
static unsigned char *ram_page2;
static unsigned char *current_ram_page;
static int current_ram_page_index;
static unsigned char bg_scroll;
static int palette_bank;
static int protection_question;


#define BACKGROUND_VIDEORAM_OFFSET   0x0800


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Phoenix has two 256x4 palette PROMs, one containing the high bits and the
  other the low bits (2x2x2 color space).
  The palette PROMs are connected to the RGB output this way:

  bit 3 --
        -- 270 ohm resistor  -- GREEN
        -- 270 ohm resistor  -- BLUE
  bit 0 -- 270 ohm resistor  -- RED

  bit 3 --
        -- GREEN
        -- BLUE
  bit 0 -- RED

  plus 270 ohm pullup and pulldown resistors on all lines

***************************************************************************/
void phoenix_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i;
	#define TOTAL_COLORS(gfxn) (Machine->gfx[gfxn]->total_colors * Machine->gfx[gfxn]->color_granularity)
	#define COLOR(gfxn,offs) (colortable[Machine->drv->gfxdecodeinfo[gfxn].color_codes_start + offs])


	for (i = 0;i < Machine->drv->total_colors;i++)
	{
		int bit0,bit1;


		bit0 = (color_prom[0] >> 0) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 0) & 0x01;
		*(palette++) = 0x55 * bit0 + 0xaa * bit1;
		bit0 = (color_prom[0] >> 2) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 2) & 0x01;
		*(palette++) = 0x55 * bit0 + 0xaa * bit1;
		bit0 = (color_prom[0] >> 1) & 0x01;
		bit1 = (color_prom[Machine->drv->total_colors] >> 1) & 0x01;
		*(palette++) = 0x55 * bit0 + 0xaa * bit1;

		color_prom++;
	}

	/* first bank of characters use colors 0-31 and 64-95 */
	for (i = 0;i < 8;i++)
	{
		int j;


		for (j = 0;j < 2;j++)
		{
			COLOR(0,4*i + j*4*8) = i + j*64;
			COLOR(0,4*i + j*4*8 + 1) = 8 + i + j*64;
			COLOR(0,4*i + j*4*8 + 2) = 2*8 + i + j*64;
			COLOR(0,4*i + j*4*8 + 3) = 3*8 + i + j*64;
		}
	}

	/* second bank of characters use colors 32-63 and 96-127 */
	for (i = 0;i < 8;i++)
	{
		int j;


		for (j = 0;j < 2;j++)
		{
			COLOR(1,4*i + j*4*8) = i + 32 + j*64;
			COLOR(1,4*i + j*4*8 + 1) = 8 + i + 32 + j*64;
			COLOR(1,4*i + j*4*8 + 2) = 2*8 + i + 32 + j*64;
			COLOR(1,4*i + j*4*8 + 3) = 3*8 + i + 32 + j*64;
		}
	}
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int phoenix_vh_start(void)
{
	if ((ram_page1 = (unsigned char*)malloc(0x1000)) == 0)
		return 1;

	if ((ram_page2 = (unsigned char*)malloc(0x1000)) == 0)
		return 1;

    current_ram_page = 0;
    current_ram_page_index = -1;

	videoram_size = 0x0340;
	return generic_vh_start();
}



/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void phoenix_vh_stop(void)
{
	free(ram_page1);
	free(ram_page2);

	ram_page1 = 0;
	ram_page2 = 0;

	generic_vh_stop();
}



READ_HANDLER( phoenix_paged_ram_r )
{
	return current_ram_page[offset];
}


WRITE_HANDLER( phoenix_paged_ram_w )
{
	if ((offset >= BACKGROUND_VIDEORAM_OFFSET) &&
		(offset <  BACKGROUND_VIDEORAM_OFFSET + videoram_size))
	{
		/* Background video RAM */
		if (data != current_ram_page[offset])
		{
			dirtybuffer[offset - BACKGROUND_VIDEORAM_OFFSET] = 1;
		}
	}

	current_ram_page[offset] = data;
}


WRITE_HANDLER( phoenix_videoreg_w )
{
    if (current_ram_page_index != (data & 1))
	{
		/* Set memory bank */
		current_ram_page_index = data & 1;

		current_ram_page = current_ram_page_index ? ram_page2 : ram_page1;

		memset(dirtybuffer,1,videoram_size);
	}

	if (palette_bank != ((data >> 1) & 1))
	{
		palette_bank = (data >> 1) & 1;

		memset(dirtybuffer,1,videoram_size);
	}

	protection_question = data & 0xfc;

	/* I think bits 2 and 3 are used for something else in Pleiads as well,
	   they are set in the routine starting at location 0x06bc */

	/* send two bits to sound control C (not sure if they are there) */
	pleiads_sound_control_c_w(offset, data);
}


WRITE_HANDLER( phoenix_scroll_w )
{
	bg_scroll = data;
}


READ_HANDLER( phoenix_input_port_0_r )
{
	int ret = input_port_0_r(0) & 0xf7;

	/* handle Pleiads protection */
	switch (protection_question)
	{
	case 0x00:
	case 0x20:
		/* Bit 3 is 0 */
		break;
	case 0x0c:
	case 0x30:
		/* Bit 3 is 1 */
		ret	|= 0x08;
		break;
	default:
		//logerror("Unknown protection question %02X at %04X\n", protection_question, cpu_get_pc());
		break;
	}

	return ret;
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void phoenix_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		if (dirtybuffer[offs])
		{
			int sx,sy,code;


			dirtybuffer[offs] = 0;

			code = current_ram_page[offs + BACKGROUND_VIDEORAM_OFFSET];

			sx = offs % 32;
			sy = offs / 32;

			drawgfx(tmpbitmap,Machine->gfx[0],
					code,
					(code >> 5) + 8 * palette_bank,
					0,0,
					8*sx,8*sy,
					0,TRANSPARENCY_NONE,0);
		}
	}


	/* copy the character mapped graphics */
	{
		int scroll;


		scroll = -bg_scroll;

		copyscrollbitmap(bitmap,tmpbitmap,1,&scroll,0,0,&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */
	for (offs = videoram_size - 1;offs >= 0;offs--)
	{
		int sx,sy,code;


		code = current_ram_page[offs];

		sx = offs % 32;
		sy = offs / 32;

		if (sx >= 1)
			drawgfx(bitmap,Machine->gfx[1],
					code,
					(code >> 5) + 8 * palette_bank,
					0,0,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_PEN,0);
		else
			drawgfx(bitmap,Machine->gfx[1],
					code,
					(code >> 5) + 8 * palette_bank,
					0,0,
					8*sx,8*sy,
					&Machine->visible_area,TRANSPARENCY_NONE,0);
	}
}
