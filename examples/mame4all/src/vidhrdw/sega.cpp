#ifndef PI
#define PI 3.14159265357989
#endif
/***************************************************************************

  vidhrdw.c

  Generic functions used by the Sega Vector games

***************************************************************************/

/*
 * History:
 *
 * 97???? Converted Al Kossow's G80 sources. LBO
 * 970807 Scaling support and dynamic sin/cos tables. ASG
 * 980124 Suport for antialiasing. .ac
 * 980203 cleaned up and interfaced to generic vector routines. BW
 *
 * TODO: use floating point math instead of fixed point.
 */

#include "driver.h"
#include "avgdvg.h"
#include "vector.h"
#include <math.h>

#define VEC_SHIFT 15	/* do not use a higher value. Values will overflow */

static int width, height, cent_x, cent_y, min_x, min_y, max_x, max_y;
static long *sinTable, *cosTable;
static int intensity;

void sega_generate_vector_list (void)
{
	int deltax, deltay;
	int currentX, currentY;

	int vectorIndex;
	int symbolIndex;

	int rotate, scale;
	int attrib;

	int angle, length;
	int color;

	int draw;

	vector_clear_list();

	symbolIndex = 0;	/* Reset vector PC to 0 */

	/*
	 * walk the symbol list until 'last symbol' set
	 */

	do {
		draw = vectorram[symbolIndex++];

		if (draw & 1)	/* if symbol active */
		{
			currentX    = vectorram[symbolIndex + 0] | (vectorram[symbolIndex + 1] << 8);
			currentY    = vectorram[symbolIndex + 2] | (vectorram[symbolIndex + 3] << 8);
			vectorIndex = vectorram[symbolIndex + 4] | (vectorram[symbolIndex + 5] << 8);
			rotate      = vectorram[symbolIndex + 6] | (vectorram[symbolIndex + 7] << 8);
			scale       = vectorram[symbolIndex + 8];

			currentX = ((currentX & 0x7ff) - min_x) << VEC_SHIFT;
			currentY = (max_y - (currentY & 0x7ff)) << VEC_SHIFT;
			vector_add_point ( currentX, currentY, 0, 0);
			vectorIndex &= 0xfff;

			/* walk the vector list until 'last vector' bit */
			/* is set in attributes */

			do
			{
				attrib = vectorram[vectorIndex + 0];
				length = vectorram[vectorIndex + 1];
				angle  = vectorram[vectorIndex + 2] | (vectorram[vectorIndex + 3] << 8);

				vectorIndex += 4;

				/* calculate deltas based on len, angle(s), and scale factor */

				angle = (angle + rotate) & 0x3ff;
				deltax = sinTable[angle] * scale * length;
				deltay = cosTable[angle] * scale * length;

				currentX += deltax >> 7;
				currentY -= deltay >> 7;

				color = attrib & 0x7e;
				if ((attrib & 1) && color)
				{
					if (translucency)
						intensity = 0xa0; /* leave room for translucency */
					else
						intensity = 0xff;
				}
				else
					intensity = 0;
				vector_add_point ( currentX, currentY, color, intensity );

			} while (!(attrib & 0x80));
		}

		symbolIndex += 9;
		if (symbolIndex >= vectorram_size)
			break;

	} while (!(draw & 0x80));
}
/***************************************************************************

  The Sega vector games don't have a color PROM, it uses RGB values for the
  vector guns.
  This routine sets up the color tables to simulate it.

***************************************************************************/


void sega_init_colors (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i,r,g,b;

	/* Bits are-> Red: 6&5 (0x60), Green: 4&3 (0x18), Blue: 2&1 (0x06) */
	for (i=0; i<128; i+=2)
	{
		palette[3*i  ] = 85 * ((i>>5)&0x3);
		palette[3*i+1] = 85 * ((i>>3)&0x3);
		palette[3*i+2] = 85 * ((i>>1)&0x3);
		/* Set the color table */
		colortable[i] = i;
	}
	/*
	 * Fill in the holes with good anti-aliasing colors.  This is a very good
	 * range of colors based on the previous palette entries.     .ac JAN2498
	 */
	i=1;
	for (r=0; r<=6; r++)
	{
		for (g=0; g<=6; g++)
		{
			for (b=0; b<=6; b++)
			{
				if (!((r|g|b)&0x1) ) continue;
				if ((g==5 || g==6) && (b==1 || b==2 || r==1 || r==2)) continue;
				if ((g==3 || g==4) && (b==1         || r==1        )) continue;
				if ((b==6 || r==6) && (g==1 || g==2                )) continue;
				if ((r==5)         && (b==1)                        ) continue;
				if ((b==5)         && (r==1)                        ) continue;
				palette[3*i  ] = (255*r) / 6;
				palette[3*i+1] = (255*g) / 6;
				palette[3*i+2] = (255*b) / 6;
				colortable[i]  = i;
				if (i < 128)
					i+=2;
				else
					i++;
			}
		}
	}
	/* There are still 4 colors left, just going to put some grays in. */
	for (i=252; i<=255; i++)
	{
		palette[3*i  ] =
		palette[3*i+1] =
		palette[3*i+2] = 107 + (42*(i-252));
	}
}

/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int sega_vh_start (void)
{
	int i;

	if (vectorram_size == 0)
		return 1;
	min_x =Machine->visible_area.min_x;
	min_y =Machine->visible_area.min_y;
	max_x =Machine->visible_area.max_x;
	max_y =Machine->visible_area.max_y;
	width =max_x-min_x;
	height=max_y-min_y;
	cent_x=(max_x+min_x)/2;
	cent_y=(max_y+min_y)/2;

	vector_set_shift (VEC_SHIFT);

	/* allocate memory for the sine and cosine lookup tables ASG 080697 */
	sinTable = (long int*)malloc(0x400 * sizeof (long));
	if (!sinTable)
		return 1;
	cosTable = (long int*)malloc(0x400 * sizeof (long));
	if (!cosTable)
	{
		free(sinTable);
		return 1;
	}

	/* generate the sine/cosine lookup tables */
	for (i = 0; i < 0x400; i++)
	{
		double angle = ((2. * PI) / (double)0x400) * (double)i;
		double temp;

		temp = sin (angle);
		if (temp < 0)
			sinTable[i] = (long)(temp * (double)(1 << VEC_SHIFT) - 0.5);
		else
			sinTable[i] = (long)(temp * (double)(1 << VEC_SHIFT) + 0.5);

		temp = cos (angle);
		if (temp < 0)
			cosTable[i] = (long)(temp * (double)(1 << VEC_SHIFT) - 0.5);
		else
			cosTable[i] = (long)(temp * (double)(1 << VEC_SHIFT) + 0.5);

	}

	return vector_vh_start();
}

/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void sega_vh_stop (void)
{
	if (sinTable)
		free(sinTable);
	sinTable = NULL;
	if (cosTable)
		free(cosTable);
	cosTable = NULL;

	vector_vh_stop();
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void sega_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	sega_generate_vector_list();
	vector_vh_update(bitmap,full_refresh);
}
