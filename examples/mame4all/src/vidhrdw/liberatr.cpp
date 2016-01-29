/***************************************************************************

  liberator.c - 'vidhrdw.c'

  Functions to emulate the video hardware of the machine.

   Liberator's screen is 256 pixels by 256 pixels.  The
     round planet in the middle of the screen is 128 pixels
     tall by 96 equivalent (192 at double pixel rate).  The
     emulator needs to account for the aspect ratio of 4/3
     from the arcade video system in order to make the planet
     appear round.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


unsigned char *liberatr_bitmapram;


void liberatr_vh_stop(void);


/*
	The following structure describes the (up to 32) line segments
	that make up one horizontal line (latitude) for one display frame of the planet.
	Note: this and the following structure is only used to collect the
	data before it is packed for actual use.
*/
typedef struct
{
	UINT8	segment_count;		/* the number of segments on this line */
	UINT8	max_x;				/* the maximum value of x_array for this line */
	UINT8	color_array[32];	/* the color values  */
	UINT8	x_array[32];		/* and maximum x values for each segment  */
} Liberator_Segs;

/*
	The following structure describes the lines (latitudes)
	that make up one complete display frame of the planet.
	Note: this and the previous structure is only used to collect the
	data before it is packed for actual use.
*/
typedef struct
{
	Liberator_Segs line[ 0x80 ];
} Liberator_Frame;


/*
	The following structure collects the 256 frames of the
	planet (one per value of longitude).
	The data is packed segment_count,segment_start,color,length,color,length,...  then
                       segment_count,segment_start,color,length,color,length...  for the next line, etc
	for the 128 lines.
*/
typedef struct
{
	UINT8 *frame[256];
} Liberator_Planet;


/*
	The following two arrays are Prom dumps off the processor
	board.
*/

static UINT8 latitude_scale[] = {
	0x25,0x3A,0x4A,0x55,0x5F,0x6A,0x75,0x7A,0x80,0x8A,0x8F,0x95,0x9A,0xA0,0xA5,0xAA,
	0xB0,0xB5,0xB5,0xBA,0xC0,0xC0,0xC5,0xCA,0xCA,0xCF,0xCF,0xD5,0xD5,0xDA,0xDA,0xDF,
	0xDF,0xE5,0xE5,0xE5,0xEA,0xEA,0xEA,0xEF,0xEF,0xEF,0xEF,0xF5,0xF5,0xF5,0xF5,0xFA,
	0xFA,0xFA,0xFA,0xFA,0xFA,0xFA,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFA,0xFA,0xFA,0xFA,0xFA,0xFA,
	0xFA,0xF5,0xF5,0xF5,0xF5,0xEF,0xEF,0xEF,0xEF,0xEA,0xEA,0xEA,0xE5,0xE5,0xE5,0xDF,
	0xDF,0xDA,0xDA,0xD5,0xD5,0xCF,0xCF,0xCA,0xCA,0xC5,0xC0,0xC0,0xBA,0xB5,0xB5,0xB0,
	0xAA,0xA5,0xA0,0x9A,0x95,0x8F,0x8A,0x80,0x7A,0x75,0x6A,0x5F,0x55,0x4A,0x3A,0x25 };

static UINT8 longitude_scale[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x02,0x03,
	0x03,0x03,0x04,0x04,0x05,0x05,0x05,0x06,0x06,0x07,0x07,0x08,0x08,0x09,0x09,0x0A,
	0x0A,0x0B,0x0B,0x0C,0x0C,0x0D,0x0E,0x0E,0x0F,0x10,0x10,0x11,0x12,0x12,0x13,0x14,
	0x15,0x15,0x16,0x17,0x18,0x19,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x1F,0x20,0x21,
	0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,
	0x32,0x33,0x34,0x35,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x40,0x41,0x42,0x43,
	0x44,0x45,0x46,0x48,0x49,0x4A,0x4B,0x4C,0x4E,0x4F,0x50,0x51,0x52,0x54,0x55,0x56,
	0x57,0x58,0x5A,0x5B,0x5C,0x5D,0x5F,0x60,0x61,0x62,0x63,0x65,0x66,0x67,0x68,0x6A,
	0x6B,0x6C,0x6D,0x6F,0x70,0x71,0x72,0x73,0x75,0x76,0x77,0x78,0x79,0x7B,0x7C,0x7D,
	0x7E,0x7F,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
	0x90,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0xA0,
	0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAE,
	0xAF,0xB0,0xB1,0xB2,0xB2,0xB3,0xB4,0xB5,0xB5,0xB6,0xB7,0xB7,0xB8,0xB9,0xB9,0xBA,
	0xBB,0xBB,0xBC,0xBC,0xBD,0xBD,0xBE,0xBE,0xBF,0xBF,0xC0,0xC0,0xC1,0xC1,0xC2,0xC2,
	0xC2,0xC3,0xC3,0xC4,0xC4,0xC4,0xC5,0xC5,0xC5,0xC5,0xC6,0xC6,0xC6,0xC6,0xC6,0xC7,
	0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,0xFF,0xFF,0xFF,0xFF,0xFF };


UINT8 *liberatr_base_ram;
UINT8 *liberatr_planet_frame;
UINT8 *liberatr_planet_select;
UINT8 *liberatr_x;
UINT8 *liberatr_y;

static UINT8 *liberatr_videoram;

/*
	The following array collects the 2 different planet
	descriptions, which are selected by liberatr_planetbit
*/
static Liberator_Planet *liberatr_planet_segs[2];


INLINE void bitmap_common_w(UINT8 x, UINT8 y, int data)
{
	int pen;

	liberatr_videoram[(y<<8) | x] = data & 0xe0;

	pen = Machine->pens[(data >> 5) + 0x10];
	plot_pixel(Machine->scrbitmap, x, y, pen);
}

WRITE_HANDLER( liberatr_bitmap_xy_w )
{
	bitmap_common_w(*liberatr_x, *liberatr_y, data);
}

WRITE_HANDLER( liberatr_bitmap_w )
{
	UINT8 x = (offset & 0x3f) << 2;
	UINT8 y = (offset >> 6);

	liberatr_bitmapram[offset] = data;

    bitmap_common_w(x , y, data);
    bitmap_common_w(x+1, y, data);
    bitmap_common_w(x+2, y, data);
    bitmap_common_w(x+3, y, data);
}


READ_HANDLER( liberatr_bitmap_xy_r )
{
	return liberatr_videoram[((*liberatr_y)<<8) | (*liberatr_x)];
}


WRITE_HANDLER( liberatr_colorram_w )
{
	UINT8 r,g,b;

	/* handle the hardware flip of the bit order from 765 to 576 that
	   hardware does between vram and color ram */
	static UINT8 penmap[] = {0x10,0x12,0x14,0x16,0x11,0x13,0x15,0x17};


	/* scale it from 0x00-0xff */
	r = ((~data >> 3) & 0x07) * 0x24 + 3;  if (r == 3)  r = 0;
	g = ((~data     ) & 0x07) * 0x24 + 3;  if (g == 3)  g = 0;
	b = ((~data >> 5) & 0x06) * 0x24 + 3;  if (b == 3)  b = 0;

	if (offset & 0x10)
	{
		/* bitmap colorram values */
		offset = penmap[offset & 0x07];
 	}
	else
	{
		offset ^= 0x0f;
	}

	palette_change_color(offset,r,g,b);
}


/********************************************************************************************
  liberatr_init_planet()

  The data for the planet is stored in ROM using a run-length type of encoding.  This
  function does the conversion to the above structures and then a smaller
  structure which is quicker to use in real time.

  Its a multi-step process, reflecting the history of the code.  Not quite as efficient
  as it might be, but this is not realtime stuff, so who cares...
 ********************************************************************************************/
static int liberatr_init_planet(int planet_select)
{
	UINT16 longitude;
	UINT8 *planet_rom;


	planet_rom = memory_region(REGION_GFX1);

	/* for each starting longitude */
	for (longitude = 0; longitude < 0x100; longitude++)
	{
		UINT8  i, latitude, start_segment, segment_count;
		UINT16 total_segment_count;
		UINT8  *buffer;
		Liberator_Frame frame;
		Liberator_Segs *line = 0;


		total_segment_count = 0;

		/* for each latitude */
		for (latitude = 0; latitude < 0x80; latitude++)
		{
			UINT8 segment, longitude_scale_factor, latitude_scale_factor, color, x=0;
			UINT8 x_array[32], color_array[32], visible_array[32];


			/* point to the structure which will hold the data for this line */
			line = &frame.line[ latitude ];

			latitude_scale_factor = latitude_scale[ latitude ];

			/* for this latitude, load the 32 segments into the arrays */
			for (segment = 0; segment < 0x20; segment++)
			{
				UINT16 length, planet_data, address;


				/*
				   read the planet picture ROM and get the
				   latitude and longitude scaled from the scaling PROMS
				*/
				address = (latitude << 5) + segment;
				if (planet_select)
					planet_data = (planet_rom[0x0000+address] << 8) + planet_rom[0x1000+address];
				else
					planet_data = (planet_rom[0x2000+address] << 8) + planet_rom[0x3000+address];

				color  =  (planet_data >> 8) & 0x0f;
				length = ((planet_data << 1) & 0x1fe) + ((planet_data >> 15) & 0x01);


				/* scale the longitude limit (adding the starting longitude) */
				address = longitude + ( length >> 1 ) + ( length & 1 );		/* shift with rounding */
				visible_array[segment] = (( address & 0x100 ) ? 1 : 0);
				if (address & 0x80)
				{
					longitude_scale_factor = 0xff;
				}
				else
				{
					address = ((address & 0x7f) << 1) + (((length & 1) || visible_array[segment]) ? 0 : 1);
					longitude_scale_factor = longitude_scale[ address ];
				}

				x_array[segment] = (((UINT16)latitude_scale_factor * (UINT16)longitude_scale_factor) + 0x80) >> 8;	/* round it */
				color_array[segment] = color;
			}

			/*
			   determine which segment is the western horizon and
			     leave 'segment' indexing it.
			*/
			for (segment = 0; segment < 0x1f; segment++)	/* if not found, 'segment' = 0x1f */
				if (visible_array[segment]) break;

			/* transfer from the temporary arrays to the structure */
			line->max_x = (latitude_scale_factor * 0xc0) >> 8;
			if (line->max_x & 1)
				line->max_x += 1; 				/* make it even */

			/*
			   as part of the quest to reduce memory usage (and to a lesser degree
			     execution time), stitch together segments that have the same color
			*/
			segment_count = 0;
			i = 0;
			start_segment = segment;
			do
			{
				color = color_array[segment];
				while (color == color_array[segment])
				{
					x = x_array[segment];
					segment = (segment+1) & 0x1f;
					if (segment == start_segment)
						break;
				}
				line->color_array[ i ] = color;
				line->x_array[ i ]     = (x > line->max_x) ? line->max_x : x;
				i++;
				segment_count++;
			} while ((i < 32) && (x <= line->max_x));

			total_segment_count += segment_count;
			line->segment_count = segment_count;
		}

		/* now that the all the lines have been processed, and we know how
		   many segments it will take to store the description, allocate the
		   space for it and copy the data to it.
		*/
		if ((buffer = (UINT8 *)malloc(2*(128 + total_segment_count))) == 0)
			return 1;

		liberatr_planet_segs[ planet_select ]->frame[ longitude ] = buffer;

		for (latitude = 0; latitude < 0x80; latitude++)
		{
			UINT8 last_x;


			line = &frame.line[ latitude ];
			segment_count = line->segment_count;
			*buffer++ = segment_count;
			last_x = 0;

			/* calculate the bitmap's x coordinate for the western horizon
			   center of bitmap - (the number of planet pixels) / 4 */
			*buffer++ = Machine->drv->screen_width/2 - (line->max_x + 2) / 4;

			for (i = 0; i < segment_count; i++)
			{
				UINT8 current_x = (line->x_array[ i ] + 1) / 2;
				*buffer++ = line->color_array[ i ];
				*buffer++ = current_x - last_x;
				last_x = current_x;
			}
		}
	}

	return 0;
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int liberatr_vh_start(void)
{
    liberatr_videoram = 0;
    liberatr_planet_segs[0] = 0;
    liberatr_planet_segs[1] = 0;


	if ((liberatr_videoram = (UINT8*)malloc(Machine->drv->screen_width * Machine->drv->screen_height)) == 0)
	{
		liberatr_vh_stop();
		return 1;
	}

	/* allocate the planet descriptor structure */
	if (((liberatr_planet_segs[0] = (Liberator_Planet*)malloc(sizeof(Liberator_Planet))) == 0) ||
	    ((liberatr_planet_segs[1] = (Liberator_Planet*)malloc(sizeof(Liberator_Planet))) == 0))
	{
		liberatr_vh_stop();
		return 1;
	}

	memset(liberatr_planet_segs[0], 0, sizeof(Liberator_Planet));
	memset(liberatr_planet_segs[1], 0, sizeof(Liberator_Planet));

	/* for each planet in the planet ROMs */
	if (liberatr_init_planet(0) ||
		liberatr_init_planet(1))
	{
		liberatr_vh_stop();
		return 1;
	}

	return 0;
}

/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void liberatr_vh_stop(void)
{
	int i;

	if (liberatr_videoram)
	{
		free(liberatr_videoram);
		liberatr_videoram = 0;
	}
	if (liberatr_planet_segs[0])
	{
		for (i = 0; i < 256; i++)
			if (liberatr_planet_segs[0]->frame[i])
				free(liberatr_planet_segs[0]->frame[i]);
		free(liberatr_planet_segs[0]);
		liberatr_planet_segs[0] = 0;
	}
	if (liberatr_planet_segs[1])
	{
		for (i = 0; i < 256; i++)
			if (liberatr_planet_segs[1]->frame[i])
				free(liberatr_planet_segs[1]->frame[i]);
		free(liberatr_planet_segs[1]);
		liberatr_planet_segs[1] = 0;
	}
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/

static void liberatr_draw_planet(void)
{
	UINT8 latitude;
	UINT8 *buffer;


	buffer = liberatr_planet_segs[ (*liberatr_planet_select >> 4) & 0x01 ]->frame[ *liberatr_planet_frame ];

	/* for each latitude */
	for (latitude = 0; latitude < 0x80; latitude++)
	{
		UINT8 base_color, segment, segment_count, x, y;

		/* grab the color value for the base (if any) at this latitude */
		base_color = liberatr_base_ram[latitude>>3] ^ 0x0f;

		segment_count = *buffer++;
		x             = *buffer++;
		y             = 64 + latitude;

		/* run through the segments, drawing its color until its x_array value comes up. */
		for (segment = 0; segment < segment_count; segment++)
		{
			UINT8 color, segment_length, i;
			UINT16 pen;

			color = *buffer++;
			if ((color & 0x0c) == 0x0c)
				color = base_color;

			pen = Machine->pens[color];

			segment_length = *buffer++;

			for (i = 0; i < segment_length; i++, x++)
			{
				/* only plot pixels that don't cover the foreground up */
				if (!liberatr_videoram[(y<<8) | x])
					plot_pixel(Machine->scrbitmap, x, y, pen);
			}
		}
	}
}


void liberatr_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	if (palette_recalc() || full_refresh)
	{
		UINT8 liberatr_y_save = *liberatr_y;
		UINT8 liberatr_x_save = *liberatr_x;

		/* redraw bitmap */
		for (*liberatr_y = Machine->visible_area.min_y; *liberatr_y < Machine->visible_area.max_y; (*liberatr_y)++)
		{
			for (*liberatr_x = Machine->visible_area.min_x; *liberatr_x < Machine->visible_area.max_x; (*liberatr_x)++)
			{
				liberatr_bitmap_xy_w(0, liberatr_bitmap_xy_r(0));
			}
		}

		*liberatr_y = liberatr_y_save;
		*liberatr_x = liberatr_x_save;
	}

	/* draw the planet */
	liberatr_draw_planet();
}
